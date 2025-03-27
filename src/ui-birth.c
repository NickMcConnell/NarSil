/**
 * \file ui-birth.c
 * \brief Text-based user interface for character creation
 *
 * Copyright (c) 1987 - 2015 Angband contributors
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cmds.h"
#include "cmd-core.h"
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "obj-tval.h"
#include "player.h"
#include "player-birth.h"
#include "ui-birth.h"
#include "ui-display.h"
#include "ui-game.h"
#include "ui-help.h"
#include "ui-input.h"
#include "ui-menu.h"
#include "ui-options.h"
#include "ui-player.h"
#include "ui-prefs.h"
#include "ui-skills.h"
#include "ui-target.h"

/**
 * Overview
 * ========
 * This file implements the user interface side of the birth process
 * for the classic terminal-based UI of Angband.
 *
 * It models birth as a series of steps which must be carried out in 
 * a specified order, with the option of stepping backwards to revisit
 * past choices.
 *
 * It starts when we receive the EVENT_ENTER_BIRTH event from the game,
 * and ends when we receive the EVENT_LEAVE_BIRTH event.  In between,
 * we will repeatedly be asked to supply a game command, which change
 * the state of the character being rolled.  Once the player is happy
 * with their character, we send the CMD_ACCEPT_CHARACTER command.
 */

/**
 * A local-to-this-file global to hold the most important bit of state
 * between calls to the game proper.  Probably not strictly necessary,
 * but reduces complexity a bit. */
enum birth_stage
{
	BIRTH_BACK = -1,
	BIRTH_RESET = 0,
	BIRTH_QUICKSTART,
	BIRTH_RACE_CHOICE,
	BIRTH_HOUSE_CHOICE,
	BIRTH_SEX_CHOICE,
	BIRTH_STAT_POINTS,
	BIRTH_SKILL_POINTS,
	BIRTH_NAME_CHOICE,
	BIRTH_AHW_CHOICE,
	BIRTH_HISTORY_CHOICE,
	BIRTH_FINAL_CONFIRM,
	BIRTH_COMPLETE
};


enum birth_questions
{
	BQ_METHOD = 0,
	BQ_RACE,
	BQ_HOUSE,
	MAX_BIRTH_QUESTIONS
};

const char *list_player_flag_names[] = {
	#define PF(a, b) b,
	#include "list-player-flags.h"
	#undef PF
	NULL
};

static const char *skill_names[] = {
	#define SKILL(a, b) b,
	#include "list-skills.h"
	#undef SKILL
	""
};

static bool quickstart_allowed = false;
bool arg_force_name;

/**
 * ------------------------------------------------------------------------
 * Quickstart? screen.
 * ------------------------------------------------------------------------ */
static enum birth_stage textui_birth_quickstart(void)
//phantom name change changes
{
	const char *prompt = "['Y': use as is; 'N': redo; 'C': change name/history; '=': set birth options]";

	enum birth_stage next = BIRTH_QUICKSTART;

	/* Prompt for it */
	prt("New character based on previous one:", 0, 0);
	prt(prompt, Term->hgt - 1, Term->wid / 2 - strlen(prompt) / 2);

	do {
		/* Get a key */
		struct keypress ke = inkey();
		
		if (ke.code == 'N' || ke.code == 'n') {
			cmdq_push(CMD_BIRTH_RESET);
			/*
			 * If the player rejects the quickstart, also reset
			 * the stat buy that was used for the previous
			 * character.
			 */
			cmdq_push(CMD_RESET_STATS);
			next = BIRTH_RACE_CHOICE;
		} else if (ke.code == KTRL('X')) {
			quit(NULL);
		} else if ( !arg_force_name && (ke.code == 'C' || ke.code == 'c')) {
			next = BIRTH_NAME_CHOICE;
		} else if (ke.code == '=') {
			do_cmd_options_birth();
		} else if (ke.code == 'Y' || ke.code == 'y') {
			cmdq_push(CMD_ACCEPT_CHARACTER);
			next = BIRTH_COMPLETE;
		}
	} while (next == BIRTH_QUICKSTART);

	/* Clear prompt */
	clear_from(23);

	return next;
}

/**
 * ------------------------------------------------------------------------
 * The various "menu" bits of the birth process - namely choice of race,
 * house, and sex.
 * ------------------------------------------------------------------------ */

/**
 * The various menus
 */
static struct menu race_menu, house_menu, sex_menu;
static int house_start = 0;

/**
 * Locations of the menus, etc. on the screen
 */
#define HEADER_ROW       1
#define QUESTION_ROW     7
#define TABLE_ROW        9

#define QUESTION_COL     2
#define RACE_COL         2
#define RACE_AUX_COL    19
#define HOUSE_COL       19
#define HOUSE_AUX_COL   42
#define SEX_COL         42
#define HELP_ROW        14
#define HIST_INSTRUCT_ROW 18

#define MENU_ROWS TABLE_ROW + 14

/**
 * upper left column and row, width, and lower column
 */
static region race_region = {RACE_COL, TABLE_ROW, 17, MENU_ROWS};
static region house_region = {HOUSE_COL, TABLE_ROW, 17, MENU_ROWS};
static region sex_region = {SEX_COL, TABLE_ROW, 34, MENU_ROWS};

/**
 * We use different menu "browse functions" to display the help text
 * sometimes supplied with the menu items - currently just the list
 * of bonuses, etc, corresponding to each race and house.
 */
typedef void (*browse_f) (int oid, void *db, const region *l);

/**
 * We have one of these structures for each menu we display - it holds
 * the useful information for the menu - text of the menu items, "help"
 * text, current (or default) selection, whether random selection is allowed,
 * and the current stage of the process for setting up a context menu and
 * relaying the reuslt of a selection in that menu.
 */
struct birthmenu_data 
{
	const char **items;
	const char *hint;
	bool allow_random;
	enum birth_stage stage_inout;
};

/**
 * A custom "display" function for our menus that simply displays the
 * text from our stored data in a different colour if it's currently
 * selected.
 */
static void birthmenu_display(struct menu *menu, int oid, bool cursor,
			      int row, int col, int width)
{
	struct birthmenu_data *data = menu->menu_data;

	uint8_t attr = curs_attrs[CURS_KNOWN][0 != cursor];
	c_put_str(attr, data->items[oid], row, col);
}

/**
 * Our custom menu iterator, only really needed to allow us to override
 * the default handling of "commands" in the standard iterators (hence
 * only defining the display and handler parts).
 */
static const menu_iter birth_iter = { NULL, NULL, birthmenu_display, NULL, NULL };

static int stat_attr(int adj)
{
	int attr;
	if (adj < 0) {
		attr = COLOUR_RED;
	} else if (adj == 0) {
		attr = COLOUR_L_DARK;
	} else if (adj == 1) {
		attr = COLOUR_GREEN;
	} else if (adj == 2) {
		attr = COLOUR_L_GREEN;
	} else {
		attr = COLOUR_L_BLUE;
	}
	return attr;
}

static void race_help(int i, void *db, const region *l)
{
	int j;
	struct player_race *r = player_id2race(i);

	if (!r) return;

	/* Output to the screen */
	text_out_hook = text_out_to_screen;
	
	clear_from(HELP_ROW);
	
	/* Indent output */
	text_out_indent = RACE_AUX_COL;
	Term_gotoxy(RACE_AUX_COL, TABLE_ROW);

	for (j = 0; j < STAT_MAX; j++) {  
		const char *name = stat_names_reduced[j];
		int adj = r->stat_adj[j];

		text_out_e("%s", name);
		text_out_c(stat_attr(adj), "%+3d", adj);
		text_out("\n");
	}

	text_out_e("\n");
	for (j = 0; j < SKILL_MAX; j++) {  
		int adj = r->skill_adj[j];
		if (adj > 0) {
			text_out_c(COLOUR_GREEN, "%s affinity\n", skill_names[j]);
		} else if (adj < 0) {
			text_out_c(COLOUR_RED, "%s penalty\n", skill_names[j]);
		}
	}
	for (j = 0; j < PF_MAX; j++) {
		if (pf_has(r->pflags, j)) {
			text_out_c(COLOUR_GREEN, "%s\n", list_player_flag_names[j]);
		}
	}

	Term_gotoxy(RACE_AUX_COL, HIST_INSTRUCT_ROW);
	text_out_c(COLOUR_L_WHITE, "%s", r->desc);

	/* Reset text_out() indentation */
	text_out_indent = 0;
}

static void house_help(int i, void *db, const region *l)
{
	int j;
	const struct player_race *r = player->race;
	struct player_house *h = player_house_from_count(i);

	if (!h) return;

	/* Output to the screen */
	text_out_hook = text_out_to_screen;

	clear_from(HELP_ROW);
	
	/* Indent output */
	text_out_indent = HOUSE_AUX_COL;
	Term_gotoxy(HOUSE_AUX_COL, TABLE_ROW);

	for (j = 0; j < STAT_MAX; j++) {  
		const char *name = stat_names_reduced[j];
		int adj = r->stat_adj[j] + h->stat_adj[j];

		text_out_e("%s", name);
		text_out_c(stat_attr(adj), "%+3d", adj);
		text_out("\n");
	}

	text_out_e("\n");
	for (j = 0; j < SKILL_MAX; j++) {  
		int adj = r->skill_adj[j] + h->skill_adj[j];
		if (adj > 1) {
			text_out_c(COLOUR_L_GREEN, "%s mastery\n", skill_names[j]);
		} else if (adj > 0) {
			text_out_c(COLOUR_GREEN, "%s affinity\n", skill_names[j]);
		} else if (adj < 0) {
			text_out_c(COLOUR_RED, "%s penalty\n", skill_names[j]);
		}
	}
	for (j = 0; j < PF_MAX; j++) {
		if (pf_has(r->pflags, j)) {
			text_out_c(COLOUR_GREEN, "%s\n", list_player_flag_names[j]);
		}
	}

	Term_gotoxy(HOUSE_AUX_COL, HIST_INSTRUCT_ROW);
	text_out_c(COLOUR_L_WHITE, "%s", h->desc);

	/* Reset text_out() indentation */
	text_out_indent = 0;
}

static void sex_help(int i, void *db, const region *l)
{
	clear_from(HELP_ROW);
}

/**
 * Display and handle user interaction with a context menu appropriate for the
 * current stage.  That way actions available with certain keys are also
 * available if only using the mouse.
 *
 * \param current_menu is the standard (not contextual) menu for the stage.
 * \param in is the event triggering the context menu.  in->type must be
 * EVT_MOUSE.
 * \param out is the event to be passed upstream (to internal handling in
 * menu_select() or, potentially, menu_select()'s caller).
 * \return true if the event was handled; otherwise, return false.
 *
 * The logic here overlaps with what's done to handle cmd_keys in
 * menu_question().
 */
static bool use_context_menu_birth(struct menu *current_menu,
		const ui_event *in, ui_event *out)
{
	enum {
		ACT_CTX_BIRTH_OPT,
		ACT_CTX_BIRTH_RAND,
		ACT_CTX_BIRTH_QUIT,
		ACT_CTX_BIRTH_HELP
	};
	struct birthmenu_data *menu_data = menu_priv(current_menu);
	char *labels;
	struct menu *m;
	int selected;

	assert(in->type == EVT_MOUSE);
	if (in->mouse.y != QUESTION_ROW && in->mouse.y != QUESTION_ROW + 1) {
		return false;
	}

	labels = string_make(lower_case);
	m = menu_dynamic_new();

	m->selections = labels;
	menu_dynamic_add_label(m, "Show birth options", '=',
		ACT_CTX_BIRTH_OPT, labels);
	if (menu_data->allow_random) {
		menu_dynamic_add_label(m, "Select one at random", '*',
			ACT_CTX_BIRTH_RAND, labels);
	}
	menu_dynamic_add_label(m, "Quit", 'q', ACT_CTX_BIRTH_QUIT, labels);
	menu_dynamic_add_label(m, "Help", '?', ACT_CTX_BIRTH_HELP, labels);

	screen_save();

	menu_dynamic_calc_location(m, in->mouse.x, in->mouse.y);
	region_erase_bordered(&m->boundary);

	selected = menu_dynamic_select(m);

	menu_dynamic_free(m);
	string_free(labels);

	screen_load();

	switch (selected) {
	case ACT_CTX_BIRTH_OPT:
		do_cmd_options_birth();
		/* The stage remains the same so leave stage_inout as is. */
		out->type = EVT_SWITCH;
		break;

	case ACT_CTX_BIRTH_RAND:
		current_menu->cursor = randint0(current_menu->count);
		out->type = EVT_SELECT;
		break;

	case ACT_CTX_BIRTH_QUIT:
		quit(NULL);
		break;

	case ACT_CTX_BIRTH_HELP:
		do_cmd_help();
		menu_data->stage_inout = BIRTH_RESET;
		out->type = EVT_SWITCH;

	default:
		/* There's nothing to do. */
		break;
	}

	return true;
}

/**
 * Set up one of our menus ready to display choices for a birth question.
 * This is slightly involved.
 */
static void init_birth_menu(struct menu *menu, int n_choices,
							int initial_choice, const region *reg,
							bool allow_random, browse_f aux)
{
	struct birthmenu_data *menu_data;

	/* Initialise a basic menu */
	menu_init(menu, MN_SKIN_SCROLL, &birth_iter);

	/* A couple of behavioural flags - we want selections as letters
	   skipping the rogue-like cardinal direction movements and a
	   double tap to act as a selection. */
	menu->selections = all_letters_nohjkl;
	menu->flags = MN_DBL_TAP;

	/* Copy across the game's suggested initial selection, etc. */
	menu->cursor = initial_choice;

	/* Allocate sufficient space for our own bits of menu information. */
	menu_data = mem_alloc(sizeof *menu_data);

	/* Allocate space for an array of menu item texts and help texts
	   (where applicable) */
	menu_data->items = mem_alloc(n_choices * sizeof *menu_data->items);
	menu_data->allow_random = allow_random;

	/* Set private data */
	menu_setpriv(menu, n_choices, menu_data);

	/* Set up the "browse" hook to display help text (where applicable). */
	menu->browse_hook = aux;

	/*
	 * All use the same hook to display a context menu so that
	 * functionality driven by keyboard input (see how cmd_keys is used
	 * in menu_question()) is also available using the mouse.
	 */
	menu->context_hook = use_context_menu_birth;

	/* Lay out the menu appropriately */
	menu_layout(menu, reg);
}



static void setup_menus(void)
{
	int n;
	struct player_sex *s;
	struct player_race *r;

	struct birthmenu_data *mdata;

	/* Count the races */
	n = 0;
	for (r = races; r; r = r->next) n++;

	/* Race menu. */
	init_birth_menu(&race_menu, n, player->race ? player->race->ridx : 0,
	                &race_region, true, race_help);
	mdata = race_menu.menu_data;

	for (r = races; r; r = r->next) {
		mdata->items[r->ridx] = r->name;
	}
	mdata->hint = "Race affects stats, skills, and other character traits.";

	/* Count the sexes */
	n = 0;
	for (s = sexes; s; s = s->next) n++;

	/* Sex menu similar to race. */
	init_birth_menu(&sex_menu, n, player->sex ? player->sex->sidx : 0,
					&sex_region, true, sex_help);
	mdata = sex_menu.menu_data;

	for (s = sexes; s; s = s->next) {
		mdata->items[s->sidx] = s->name;
	}
	mdata->hint = "Sex has no gameplay effect.";
}

static void setup_house_menu(const struct player_race *r)
{
	int i, n;
	struct player_house *h;

	struct birthmenu_data *mdata;

	/* Count the houses */
	n = 0;
	for (h = houses; h; h = h->next) {
		if (h->race == r) n++;
	}

	/* House menu similar to race. */
	init_birth_menu(&house_menu, n, house_start, &house_region, true,
					house_help);
	mdata = house_menu.menu_data;

	for (i = n - 1, h = houses; h; h = h->next) {
		if (h->race == r) {
			mdata->items[i] = h->name;
			i--;
		}
	}
	mdata->hint = "House affects stats, skills, and other character traits.";
}

/**
 * Cleans up our stored menu info when we've finished with it.
 */
static void free_birth_menu(struct menu *menu)
{
	struct birthmenu_data *data = menu->menu_data;

	if (data) {
		mem_free(data->items);
		mem_free(data);
		menu->menu_data = NULL;
	}
}

static void free_birth_menus(void)
{
	/* We don't need these any more. */
	free_birth_menu(&race_menu);
	free_birth_menu(&house_menu);
	free_birth_menu(&sex_menu);
}

/**
 * Clear the previous question
 */
static void clear_question(void)
{
	int i;

	for (i = QUESTION_ROW; i < TABLE_ROW; i++)
		/* Clear line, position cursor */
		Term_erase(0, i, 255);
}


#define BIRTH_MENU_HELPTEXT \
	"{light blue}Please select your character traits from the menus below:{/}\n\n" \
	"Use the {light green}movement keys{/} to scroll the menu, " \
	"{light green}Enter{/} to select the current menu item, '{light green}*{/}' " \
	"for a random menu item, " \
	"'{light green}ESC{/}' to step back through the birth process, " \
	"'{light green}={/}' for the birth options, '{light green}?{/}' " \
	"for help, or '{light green}Ctrl-X{/}' to quit."

/**
 * Show the birth instructions on an otherwise blank screen
 */	
static void print_menu_instructions(void)
{
	/* Clear screen */
	Term_clear();
	
	/* Output to the screen */
	text_out_hook = text_out_to_screen;
	
	/* Indent output */
	text_out_indent = QUESTION_COL;
	Term_gotoxy(QUESTION_COL, HEADER_ROW);
	
	/* Display some helpful information */
	text_out_e(BIRTH_MENU_HELPTEXT);
	
	/* Reset text_out() indentation */
	text_out_indent = 0;
}

/**
 * Allow the user to select from the current menu, and return the 
 * corresponding command to the game.  Some actions are handled entirely
 * by the UI (displaying help text, for instance).
 */
static enum birth_stage menu_question(enum birth_stage current,
									  struct menu *current_menu,
									  cmd_code choice_command)
{
	struct birthmenu_data *menu_data = menu_priv(current_menu);
	ui_event cx;

	enum birth_stage next = BIRTH_RESET;
	
	/* Print the question currently being asked. */
	clear_question();
	Term_putstr(QUESTION_COL, QUESTION_ROW, -1, COLOUR_YELLOW, menu_data->hint);

	current_menu->cmd_keys = "?=*@\x18";	 /* ?, =, *, @, <ctl-X> */

	while (next == BIRTH_RESET) {
		/* Display the menu, wait for a selection of some sort to be made. */
		menu_data->stage_inout = current;
		cx = menu_select(current_menu, EVT_KBRD, false);

		/* As all the menus are displayed in "hierarchical" style, we allow
		   use of "back" (left arrow key or equivalent) to step back in 
		   the proces as well as "escape". */
		if (cx.type == EVT_ESCAPE) {
			next = BIRTH_BACK;
		} else if (cx.type == EVT_SELECT) {
			cmdq_push(choice_command);
			cmd_set_arg_choice(cmdq_peek(), "choice", current_menu->cursor);
			if (current == BIRTH_HOUSE_CHOICE) {
				house_start = current_menu->cursor;
			}
			next = current + 1;
		} else if (cx.type == EVT_SWITCH) {
			next = menu_data->stage_inout;
		} else if (cx.type == EVT_KBRD) {
			/* '*' chooses an option at random from those the game's provided */
			if (cx.key.code == '*' && menu_data->allow_random) {
				current_menu->cursor = randint0(current_menu->count);
				cmdq_push(choice_command);
				cmd_set_arg_choice(cmdq_peek(), "choice", current_menu->cursor);

				if (current == BIRTH_HOUSE_CHOICE) {
					house_start = current_menu->cursor;
				}
				menu_refresh(current_menu, false);
				next = current + 1;
			} else if (cx.key.code == '=') {
				do_cmd_options_birth();
				next = current;
			} else if (cx.key.code == KTRL('X')) {
				quit(NULL);
			} else if (cx.key.code == '?') {
				do_cmd_help();
			}
		}
	}
	
	return next;
}

/**
 * ------------------------------------------------------------------------
 * Point-based stat allocation.
 * ------------------------------------------------------------------------ */

/* The locations of the stat costs area on the birth screen. */
#define STAT_COSTS_ROW 2
#define COSTS_COL     (42 + 32)
#define TOTAL_COL     (42 + 19)

/*
 * Remember what's possible for a given stat.  0 means can't buy or sell.
 * 1 means can sell.  2 means can buy.  3 means can buy or sell.
 */
static int buysell[STAT_MAX];

/**
 * This is called whenever a stat changes.  We take the easy road, and just
 * redisplay them all using the standard function.
 */
static void point_stats(game_event_type type, game_event_data *data,
							  void *user)
{
	display_player_stat_info();
}

/**
 * This is called whenever any of the other miscellaneous stat-dependent things
 * changed.  We redisplay everything because it's easier.
 */
static void point_misc(game_event_type type, game_event_data *data,
							 void *user)
{
	display_player_xtra_info();
}


/**
 * This is called whenever the points totals are changed (in birth.c), so
 * that we can update our display of how many points have been spent and
 * are available.
 */
static void stat_points(game_event_type type, game_event_data *data,
						void *user)
{
	int i;
	int sum = 0;
	const int *spent = data->points.points;
	const int *inc = data->points.inc_points;
	int remaining = data->points.remaining;

	/* Display the costs header */
	put_str("Cost", STAT_COSTS_ROW - 1, COSTS_COL);

	for (i = 0; i < STAT_MAX; i++) {
		/* Remember what's allowed. */
		buysell[i] = 0;
		if (spent[i] > 0) {
			buysell[i] |= 1;
		}
		if (inc[i] <= remaining) {
			buysell[i] |= 2;
		}
		/* Display cost */
		put_str(format("%4d", spent[i]), STAT_COSTS_ROW + i, COSTS_COL);
		sum += spent[i];
	}

	put_str(format("Total Cost: %2d/%2d", sum, remaining + sum),
		STAT_COSTS_ROW + STAT_MAX, TOTAL_COL);
}

static void stat_points_start(void)
{
	const char *prompt = "[up/down to move, left/right to modify, 'r' to reset, 'Enter' to accept]";
	int i;

	/* Clear */
	Term_clear();

	/* Display the player */
	display_player_xtra_info();
	display_player_stat_info();
	display_player_skill_info();

	prt(prompt, Term->hgt - 1, Term->wid / 2 - strlen(prompt) / 2);

	for (i = 0; i < STAT_MAX; ++i) {
		buysell[i] = 0;
	}

	/* Register handlers for various events - cheat a bit because we redraw
	   the lot at once rather than each bit at a time. */
	event_add_handler(EVENT_STATPOINTS, stat_points, NULL);	
	event_add_handler(EVENT_STATS, point_stats, NULL);	
	event_add_handler(EVENT_EXP_CHANGE, point_misc, NULL);
}

static void stat_points_stop(void)
{
	event_remove_handler(EVENT_STATPOINTS, stat_points, NULL);	
	event_remove_handler(EVENT_STATS, point_stats, NULL);	
	event_remove_handler(EVENT_EXP_CHANGE, point_misc, NULL);	
}

static enum birth_stage stat_points_command(void)
{
	static int stat = 0;
	enum {
		ACT_CTX_BIRTH_PTS_NONE,
		ACT_CTX_BIRTH_PTS_BUY,
		ACT_CTX_BIRTH_PTS_SELL,
		ACT_CTX_BIRTH_PTS_ESCAPE,
		ACT_CTX_BIRTH_PTS_RESET,
		ACT_CTX_BIRTH_PTS_ACCEPT,
		ACT_CTX_BIRTH_PTS_QUIT
	};
	int action = ACT_CTX_BIRTH_PTS_NONE;
	ui_event in;
	enum birth_stage next = BIRTH_STAT_POINTS;

	/* Place cursor just after cost of current stat */
	Term_gotoxy(COSTS_COL + 4, STAT_COSTS_ROW + stat);

	/*
	 * Get input.  Emulate what inkey() does without coercing mouse events
	 * to look like keystrokes.
	 */
	while (1) {
		in = inkey_ex();
		if (in.type == EVT_KBRD || in.type == EVT_MOUSE) {
			break;
		}
		if (in.type == EVT_BUTTON) {
			in.type = EVT_KBRD;
		}
		if (in.type == EVT_ESCAPE) {
			in.type = EVT_KBRD;
			in.key.code = ESCAPE;
			in.key.mods = 0;
			break;
		}
	}

	/* Figure out what to do. */
	if (in.type == EVT_KBRD) {
		if (in.key.code == KTRL('X')) {
			action = ACT_CTX_BIRTH_PTS_QUIT;
		} else if (in.key.code == ESCAPE) {
			action = ACT_CTX_BIRTH_PTS_ESCAPE;
		} else if (in.key.code == 'r' || in.key.code == 'R') {
			action = ACT_CTX_BIRTH_PTS_RESET;
		} else if (in.key.code == KC_ENTER) {
			action = ACT_CTX_BIRTH_PTS_ACCEPT;
		} else {
			int dir;

			if (in.key.code == '-') {
				dir = 4;
			} else if (in.key.code == '+') {
				dir = 6;
			} else {
				dir = target_dir(in.key);
			}

			/*
			 * Go to previous stat.  Loop back to the last if at
			 * the first.
			 */
			if (dir == 8) {
				stat = (stat + STAT_MAX - 1) % STAT_MAX;
			}

			/*
			 * Go to next stat.  Loop back to the first if at the
			 * last.
			 */
			if (dir == 2) {
				stat = (stat + 1) % STAT_MAX;
			}

			/* Decrease stat (if possible). */
			if (dir == 4) {
				action = ACT_CTX_BIRTH_PTS_SELL;
			}

			/* Increase stat (if possible). */
			if (dir == 6) {
				action = ACT_CTX_BIRTH_PTS_BUY;
			}
		}
	} else if (in.type == EVT_MOUSE) {
		assert(stat >= 0 && stat < STAT_MAX);
		if (in.mouse.button == 2) {
			action = ACT_CTX_BIRTH_PTS_ESCAPE;
		} else if (in.mouse.y >= STAT_COSTS_ROW
				&& in.mouse.y < STAT_COSTS_ROW + STAT_MAX
				&& in.mouse.y != STAT_COSTS_ROW + stat) {
			/*
			 * Make that stat the current one if buying or selling.
			 */
			stat = in.mouse.y - STAT_COSTS_ROW;
		} else {
			/* Present a context menu with the other actions. */
			char *labels = string_make(lower_case);
			struct menu *m = menu_dynamic_new();

			m->selections = labels;
			if (in.mouse.y == STAT_COSTS_ROW + stat
					&& (buysell[stat] & 1)) {
				menu_dynamic_add_label(m, "Sell", 's',
					ACT_CTX_BIRTH_PTS_SELL, labels);
			}
			if (in.mouse.y == STAT_COSTS_ROW + stat
					&& (buysell[stat] & 2)) {
				menu_dynamic_add_label(m, "Buy", 'b',
					ACT_CTX_BIRTH_PTS_BUY, labels);
			}
			menu_dynamic_add_label(m, "Accept", 'a',
				ACT_CTX_BIRTH_PTS_ACCEPT, labels);
			menu_dynamic_add_label(m, "Reset", 'r',
				ACT_CTX_BIRTH_PTS_RESET, labels);
			menu_dynamic_add_label(m, "Quit", 'q',
				ACT_CTX_BIRTH_PTS_QUIT, labels);

			screen_save();

			menu_dynamic_calc_location(m, in.mouse.x, in.mouse.y);
			region_erase_bordered(&m->boundary);

			action = menu_dynamic_select(m);

			menu_dynamic_free(m);
			string_free(labels);

			screen_load();
		}
	}

	/* Do it. */
	switch (action) {
	case ACT_CTX_BIRTH_PTS_SELL:
		assert(stat >= 0 && stat < STAT_MAX);
		cmdq_push(CMD_SELL_STAT);
		cmd_set_arg_choice(cmdq_peek(), "choice", stat);
		break;

	case ACT_CTX_BIRTH_PTS_BUY:
		assert(stat >= 0 && stat < STAT_MAX);
		cmdq_push(CMD_BUY_STAT);
		cmd_set_arg_choice(cmdq_peek(), "choice", stat);
		break;

	case ACT_CTX_BIRTH_PTS_ESCAPE:
		/* Go back a step or back to the start of this step. */
		next = BIRTH_BACK;
		break;

	case ACT_CTX_BIRTH_PTS_RESET:
		cmdq_push(CMD_RESET_STATS);
		cmd_set_arg_choice(cmdq_peek(), "choice", false);
		break;

	case ACT_CTX_BIRTH_PTS_ACCEPT:
		/* Done with this stage.  Proceed to the next. */
		next = BIRTH_SKILL_POINTS;
		break;

	case ACT_CTX_BIRTH_PTS_QUIT:
		quit(NULL);
		break;

	default:
		/* Do nothing and remain at this stage. */
		break;
	}

	return next;
}
	
/**
 * ------------------------------------------------------------------------
 * Asking for the player's chosen name.
 * ------------------------------------------------------------------------ */
//phantom changes for server
static enum birth_stage get_name_command(void)
{
	enum birth_stage next;
	char name[PLAYER_NAME_LEN];

	/* Use frontend-provided savefile name if requested */
	if (arg_name[0]) {
		my_strcpy(player->full_name, arg_name, sizeof(player->full_name));
	}

	/*
	 * If not forcing the character's name, the front end didn't set the
	 * savefile to use, and the chosen name for the character would lead
	 * to overwriting an existing savefile, confirm that's okay with the
	 * player.
	 */
	if (arg_force_name) {
		next = BIRTH_HISTORY_CHOICE;
	} else if (get_character_name(name, sizeof(name))
			&& (savefile[0]
			|| !savefile_name_already_used(name, true, true)
			|| get_check("A savefile for that name exists.  Overwrite it? "))) {
		cmdq_push(CMD_NAME_CHOICE);
		cmd_set_arg_string(cmdq_peek(), "name", name);
		next = BIRTH_HISTORY_CHOICE;
	} else {
		next = BIRTH_BACK;
	}

	
	return next;
}

static void get_screen_loc(size_t cursor, int *x, int *y, size_t n_lines,
	size_t *line_starts, size_t *line_lengths)
{
	size_t lengths_so_far = 0;
	size_t i;

	if (!line_starts || !line_lengths) return;

	for (i = 0; i < n_lines; i++) {
		if (cursor >= line_starts[i]) {
			if (cursor <= (line_starts[i] + line_lengths[i])) {
				*y = i;
				*x = cursor - lengths_so_far;
				break;
			}
		}
		/* +1 for the space */
		lengths_so_far += line_lengths[i] + 1;
	}
}

static int edit_text(char *buffer, int buflen) {
	int len = strlen(buffer);
	bool done = false;
	int cursor = 0;

	while (!done) {
		int x = 0, y = 0;
		struct keypress ke;

		region area = { 1, HIST_INSTRUCT_ROW + 1, 71, 5 };
		textblock *tb = textblock_new();

		size_t *line_starts = NULL, *line_lengths = NULL;
		size_t n_lines;
		/*
		 * This is the total number of UTF-8 characters; can be less
		 * less than len, the number of 8-bit units in the buffer,
		 * if a single character is encoded with more than one 8-bit
		 * unit.
		 */
		int ulen;

		/* Display on screen */
		clear_from(HIST_INSTRUCT_ROW);
		textblock_append(tb, "%s", buffer);
		textui_textblock_place(tb, area, NULL);

		n_lines = textblock_calculate_lines(tb,
				&line_starts, &line_lengths, area.width);
		ulen = (n_lines > 0) ? line_starts[n_lines - 1] +
			line_lengths[n_lines - 1]: 0;

		/* Set cursor to current editing position */
		get_screen_loc(cursor, &x, &y, n_lines, line_starts, line_lengths);
		Term_gotoxy(1 + x, 19 + y);

		ke = inkey();
		switch (ke.code) {
			case ESCAPE:
				return -1;

			case KC_ENTER:
				done = true;
				break;

			case ARROW_LEFT:
				if (cursor > 0) cursor--;
				break;

			case ARROW_RIGHT:
				if (cursor < ulen) cursor++;
				break;

			case ARROW_DOWN: {
				int add = line_lengths[y] + 1;
				if (cursor + add < ulen) cursor += add;
				break;
			}

			case ARROW_UP:
				if (y > 0) {
					int up = line_lengths[y - 1] + 1;
					if (cursor - up >= 0) cursor -= up;
				}
				break;

			case KC_END:
				cursor = MAX(0, ulen);
				break;

			case KC_HOME:
				cursor = 0;
				break;

			case KC_BACKSPACE:
			case KC_DELETE: {
				char *ocurs, *oshift;

				/* Refuse to backspace into oblivion */
				if ((ke.code == KC_BACKSPACE && cursor == 0) ||
						(ke.code == KC_DELETE && cursor >= ulen))
					break;

				/*
				 * Move the string from k to nul along to the
				 * left by 1.  First, have to get offset
				 * corresponding to the cursor position.
				 */
				ocurs = utf8_fskip(buffer, cursor, NULL);
				assert(ocurs);
				if (ke.code == KC_BACKSPACE) {
					/* Get offset of the previous character. */
					oshift = utf8_rskip(ocurs, 1, buffer);
					assert(oshift);
					memmove(oshift, ocurs,
						len - (ocurs - buffer));
					/* Decrement */
					--cursor;
					len -= ocurs - oshift;
				} else {
					/* Get offset of the next character. */
					oshift = utf8_fskip(ocurs, 1, NULL);
					assert(oshift);
					memmove(ocurs, oshift,
						len - (oshift - buffer));
					/* Decrement. */
					len -= oshift - ocurs;
				}

				/* Terminate */
				buffer[len] = '\0';

				break;
			}
			
			default: {
				bool atnull = (cursor == ulen);
				char encoded[5];
				int n_enc;
				char *ocurs;

				if (!keycode_isprint(ke.code))
					break;

				n_enc = utf32_to_utf8(encoded,
					N_ELEMENTS(encoded), &ke.code, 1, NULL);

				/*
				 * Make sure we have something to add and have
				 * enough space.
				 */
				if (n_enc == 0 || n_enc + len >= buflen) {
					break;
				}

				/* Insert the encoded character. */
				if (atnull) {
					ocurs = buffer + len;
				} else {
					ocurs = utf8_fskip(buffer, cursor, NULL);
					assert(ocurs);
					/*
					 * Move the rest of the buffer along
					 * to make room.
					 */
					memmove(ocurs + n_enc, ocurs,
						len - (ocurs - buffer));
				}
				memcpy(ocurs, encoded, n_enc);

				/* Update cursor position and length. */
				++cursor;
				len += n_enc;

				/* Terminate */
				buffer[len] = '\0';

				break;
			}
		}

		mem_free(line_starts);
		mem_free(line_lengths);
		textblock_free(tb);
	}

	return 0;
}

/**
 * ------------------------------------------------------------------------
 * Allowing the player to reroll their age, height, weight.
 * ------------------------------------------------------------------------ */
static enum birth_stage get_ahw_command(void)
{
	enum birth_stage next = 0;
	struct keypress ke;

	/* Ask ahw */
	prt("Accept age, height and weight? [y/n]", 0, 0);
	ke = inkey();

	/* Quit, go back, change history, or accept */
	if (ke.code == KTRL('X')) {
		quit(NULL);
	} else if (ke.code == ESCAPE) {
		next = BIRTH_BACK;
	} else if (ke.code == 'N' || ke.code == 'n') {
		get_ahw(player);
		next = BIRTH_AHW_CHOICE;
	} else {
		next = BIRTH_HISTORY_CHOICE;
	}

	return next;
}

/**
 * ------------------------------------------------------------------------
 * Allowing the player to choose their history.
 * ------------------------------------------------------------------------ */
static enum birth_stage get_history_command(void)
{
	enum birth_stage next = 0;
	struct keypress ke;
	char old_history[240];

	/* Save the original history */
	my_strcpy(old_history, player->history, sizeof(old_history));

	/* Ask for some history */
	prt("Accept character history? [y/n]", 0, 0);
	ke = inkey();

	/* Quit, go back, change history, or accept */
	if (ke.code == KTRL('X')) {
		quit(NULL);
	} else if (ke.code == ESCAPE) {
		next = BIRTH_BACK;
	} else if (ke.code == 'N' || ke.code == 'n') {
		char history[240];
		my_strcpy(history, player->history, sizeof(history));

		switch (edit_text(history, sizeof(history))) {
			case -1:
				next = BIRTH_BACK;
				break;
			case 0:
				cmdq_push(CMD_HISTORY_CHOICE);
				cmd_set_arg_string(cmdq_peek(), "history", history);
				next = BIRTH_HISTORY_CHOICE;
		}
	} else {
		next = BIRTH_FINAL_CONFIRM;
	}

	return next;
}

/**
 * ------------------------------------------------------------------------
 * Final confirmation of character.
 * ------------------------------------------------------------------------ */
static enum birth_stage get_confirm_command(void)
{
	const char *prompt = "['ESC' to step back, 'S' to start over, or any other key to continue]";
	struct keypress ke;

	enum birth_stage next = BIRTH_RESET;

	/* Prompt for it */
	prt(prompt, Term->hgt - 1, Term->wid / 2 - strlen(prompt) / 2);

	/* Get a key */
	ke = inkey();
	
	/* Start over */
	if (ke.code == 'S' || ke.code == 's') {
		next = BIRTH_RESET;
	} else if (ke.code == KTRL('X')) {
		quit(NULL);
	} else if (ke.code == ESCAPE) {
		next = BIRTH_BACK;
	} else {
		cmdq_push(CMD_ACCEPT_CHARACTER);
		next = BIRTH_COMPLETE;
	}

	/* Clear prompt */
	clear_from(23);

	return next;
}



/**
 * ------------------------------------------------------------------------
 * Things that relate to the world outside this file: receiving game events
 * and being asked for game commands.
 * ------------------------------------------------------------------------ */

/**
 * This is called when we receive a request for a command in the birth 
 * process.

 * The birth process continues until we send a final character confirmation
 * command (or quit), so this is effectively called in a loop by the main
 * game.
 *
 * We're imposing a step-based system onto the main game here, so we need
 * to keep track of where we're up to, where each step moves on to, etc.
 */
int textui_do_birth(void)
{
	enum birth_stage current_stage = BIRTH_RESET;
	enum birth_stage prev = BIRTH_BACK;
	enum birth_stage next = current_stage;

	bool done = false;

	cmdq_push(CMD_BIRTH_INIT);
	cmdq_execute(CTX_BIRTH);

	while (!done) {

		switch (current_stage)
		{
			case BIRTH_RESET:
			{
				cmdq_push(CMD_BIRTH_RESET);

				if (quickstart_allowed)
					next = BIRTH_QUICKSTART;
				else
					next = BIRTH_RACE_CHOICE;

				break;
			}

			case BIRTH_QUICKSTART:
			{
				display_player(0);
				next = textui_birth_quickstart();
				if (next == BIRTH_COMPLETE)
					done = true;
				break;
			}

			case BIRTH_HOUSE_CHOICE:
			case BIRTH_RACE_CHOICE:
			case BIRTH_SEX_CHOICE:
			{
				struct menu *menu = &race_menu;
				cmd_code command = CMD_CHOOSE_RACE;

				Term_clear();
				print_menu_instructions();

				if (current_stage > BIRTH_RACE_CHOICE) {
					menu_refresh(&race_menu, false);
					free_birth_menu(&house_menu);
					setup_house_menu(player->race);
					menu = &house_menu;
					command = CMD_CHOOSE_HOUSE;
				}

				if (current_stage > BIRTH_HOUSE_CHOICE) {
					menu_refresh(&house_menu, false);
					menu = &sex_menu;
					command = CMD_CHOOSE_SEX;
				}

				next = menu_question(current_stage, menu, command);

				if (next == BIRTH_BACK) {
					next = current_stage - 1;
				}

				/* Make sure the character gets reset before quickstarting */
				if (next == BIRTH_QUICKSTART) 
					next = BIRTH_RESET;

				break;
			}

			case BIRTH_STAT_POINTS:
			{
				stat_points_start();
				/*
				 * Force a redraw of the point
				 * allocations but do not reset them.
				 */
				cmdq_push(CMD_REFRESH_STATS);
				cmdq_execute(CTX_BIRTH);

				next = stat_points_command();

				if (next == BIRTH_BACK)
					next = BIRTH_SEX_CHOICE;

				stat_points_stop();

				break;
			}

			case BIRTH_SKILL_POINTS:
			{
				int skill_action = gain_skills(CTX_BIRTH,
											   (prev == BIRTH_STAT_POINTS));
				if (skill_action > 0) {
					next = BIRTH_NAME_CHOICE;
				} else if (skill_action < 0) {
					next = BIRTH_STAT_POINTS;
				} else {
					/* Shouldn't get here */
					next = BIRTH_SKILL_POINTS;
				}

				break;
			}

			case BIRTH_NAME_CHOICE:
			{
				if (prev < BIRTH_NAME_CHOICE)
					display_player(0);

				next = get_name_command();
				if (next == BIRTH_BACK)
					next = BIRTH_SKILL_POINTS;

				break;
			}

			case BIRTH_AHW_CHOICE:
			{
				if (prev < BIRTH_AHW_CHOICE)
					display_player(0);

				next = get_ahw_command();
				if (next == BIRTH_BACK)
					next = BIRTH_NAME_CHOICE;

				break;
			}

			case BIRTH_HISTORY_CHOICE:
			{
				if (prev < BIRTH_HISTORY_CHOICE)
					display_player(0);

				next = get_history_command();
				if (next == BIRTH_BACK)
					next = BIRTH_AHW_CHOICE;

				break;
			}

			case BIRTH_FINAL_CONFIRM:
			{
				if (prev < BIRTH_FINAL_CONFIRM)
					display_player(0);

				next = get_confirm_command();
				if (next == BIRTH_BACK)
					next = BIRTH_HISTORY_CHOICE;

				if (next == BIRTH_COMPLETE)
					done = true;

				break;
			}

			default:
			{
				/* Remove dodgy compiler warning, */
			}
		}

		prev = current_stage;
		current_stage = next;

		/* Execute whatever commands have been sent */
		cmdq_execute(CTX_BIRTH);
	}

	return 0;
}

/**
 * Called when we enter the birth mode - so we set up handlers, command hooks,
 * etc, here.
 */
static void ui_enter_birthscreen(game_event_type type, game_event_data *data,
								 void *user)
{
	/* Set the ugly static global that tells us if quickstart's available. */
	quickstart_allowed = data->flag;

	setup_menus();
}

static void ui_leave_birthscreen(game_event_type type, game_event_data *data,
								 void *user)
{
	/* Set the savefile name if it's not already set */
	if (!savefile[0])
		savefile_set_name(player->full_name, true, true);

	free_birth_menus();
}


void ui_init_birthstate_handlers(void)
{
	event_add_handler(EVENT_ENTER_BIRTH, ui_enter_birthscreen, NULL);
	event_add_handler(EVENT_LEAVE_BIRTH, ui_leave_birthscreen, NULL);
}

