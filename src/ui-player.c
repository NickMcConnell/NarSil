/**
 * \file ui-player.c
 * \brief character screens and dumps
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "buildid.h"
#include "combat.h"
#include "game-world.h"
#include "init.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-info.h"
#include "obj-knowledge.h"
#include "obj-util.h"
#include "player.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "ui-abilities.h"
#include "ui-birth.h"
#include "ui-display.h"
#include "ui-history.h"
#include "ui-input.h"
#include "ui-map.h"
#include "ui-menu.h"
#include "ui-object.h"
#include "ui-output.h"
#include "ui-player.h"
#include "ui-skills.h"


static const char *skill_names[] = {
	#define SKILL(a, b) b,
	#include "list-skills.h"
	#undef SKILL
	""
};

/**
 * ------------------------------------------------------------------------
 * Panel utilities
 * ------------------------------------------------------------------------ */

/**
 * Panel line type
 */
struct panel_line {
	uint8_t attr;
	const char *label;
	char value[20];
};

/**
 * Panel holder type
 */
struct panel {
	size_t len;
	size_t max;
	struct panel_line *lines;
};

/**
 * Allocate some panel lines
 */
static struct panel *panel_allocate(int n) {
	struct panel *p = mem_zalloc(sizeof *p);

	p->len = 0;
	p->max = n;
	p->lines = mem_zalloc(p->max * sizeof *p->lines);

	return p;
}

/**
 * Free up panel lines
 */
static void panel_free(struct panel *p) {
	assert(p);
	mem_free(p->lines);
	mem_free(p);
}

/**
 * Add a new line to the panel
 */
static void panel_line(struct panel *p, uint8_t attr, const char *label,
		const char *fmt, ...) {
	va_list vp;

	struct panel_line *pl;

	/* Get the next panel line */
	assert(p);
	assert(p->len != p->max);
	pl = &p->lines[p->len++];

	/* Set the basics */
	pl->attr = attr;
	pl->label = label;

	/* Set the value */
	va_start(vp, fmt);
	vstrnfmt(pl->value, sizeof pl->value, fmt, vp);
	va_end(vp);
}

/**
 * Add a spacer line in a panel
 */
static void panel_space(struct panel *p) {
	assert(p);
	assert(p->len != p->max);
	p->len++;
}


/**
 * Special display, part 2b
 */
void display_player_stat_info(void)
{
	int i, row = 2, col = 41;
	char buf[80];

	/* Display the stats */
	for (i = 0; i < STAT_MAX; i++) {
		/* Reduced or normal */
		if (player->stat_drain[i] < 0)
			/* Use lowercase stat name */
			put_str(stat_names_reduced[i], row + i, col);
		else
			/* Assume uppercase stat name */
			put_str(stat_names[i], row+i, col);

		/* Resulting "modified" maximum value */
		strnfmt(buf, sizeof(buf), "%2d", player->state.stat_use[i]);
		if (player->stat_drain[i] < 0)
			c_put_str(COLOUR_YELLOW, buf, row + i, col + 5);
		else
			c_put_str(COLOUR_L_GREEN, buf, row + i, col + 5);

		/* Only display stat_equip_mod if not zero */
		if (player->state.stat_equip_mod[i] != 0) {
			c_put_str(COLOUR_SLATE, "=", row + i, col + 8);

			/* Internal "natural" maximum value */
			strnfmt(buf, sizeof(buf), "%2d", player->stat_base[i]);
			c_put_str(COLOUR_GREEN, buf, row+i, col + 10);
		
			/* Equipment Bonus */
			strnfmt(buf, sizeof(buf), "%+3d", player->state.stat_equip_mod[i]);
			c_put_str(COLOUR_SLATE, buf, row + i, col + 13);	
		}

		/* Only display stat_drain if not zero */
		if (player->stat_drain[i] != 0) {
			c_put_str(COLOUR_SLATE, "=", row + i, col + 8);

			/* Internal "natural" maximum value */
			strnfmt(buf, sizeof(buf), "%2d", player->stat_base[i]);
			c_put_str(COLOUR_GREEN, buf, row+i, col + 10);
		
			/* Reduction */
			strnfmt(buf, sizeof(buf), "%+3d", player->stat_drain[i]);
			c_put_str(COLOUR_SLATE, buf, row + i, col + 17);	
		}

		/* Only display stat_misc_mod if not zero */
		if (player->state.stat_misc_mod[i] != 0) {
			c_put_str(COLOUR_SLATE, "=", row + i, col + 8);

			/* Internal "natural" maximum value */
			strnfmt(buf, sizeof(buf), "%2d", player->stat_base[i]);
			c_put_str(COLOUR_GREEN, buf, row+i, col + 10);
		
			/* Modifier */
			strnfmt(buf, sizeof(buf), "%+3d", player->state.stat_misc_mod[i]);
			c_put_str(COLOUR_SLATE, buf, row + i, col + 21);	
		}
	}
}

/**
 * Skill breakdown
 */
void display_player_skill_info(void)
{
	int skill, row, col = 41;

	/* Display the skills */
	for (skill = 0; skill < SKILL_MAX; skill++) {
		int stat = player->state.skill_stat_mod[skill];
		int equip = player->state.skill_equip_mod[skill];
		int misc = player->state.skill_misc_mod[skill];
		row = 7 + skill;
		put_str(skill_names[skill], row, col);
		c_put_str(COLOUR_L_GREEN, format("%3d", player->state.skill_use[skill]),
				  row, col + 11);
		c_put_str(COLOUR_SLATE, "=", row, col + 15);
		c_put_str(COLOUR_GREEN, format("%2d", player->skill_base[skill]), row,
				  col+17);
		if (stat != 0)
			c_put_str(COLOUR_SLATE, format("%+3d", stat), row, col + 20);
		if (equip != 0)
			c_put_str(COLOUR_SLATE, format("%+3d", equip), row, col + 24);
		if (misc != 0)
			c_put_str(COLOUR_SLATE, format("%+3d", misc), row, col + 28);
	}
}


static void display_panel(const struct panel *p, bool left_adj,
		const region *bounds)
{
	size_t i;
	int col = bounds->col;
	int row = bounds->row;
	int w = bounds->width;
	int offset = 0;

	region_erase(bounds);

	if (left_adj) {
		for (i = 0; i < p->len; i++) {
			struct panel_line *pl = &p->lines[i];

			int len = pl->label ? strlen(pl->label) : 0;
			if (offset < len) offset = len;
		}
		offset += 2;
	}

	for (i = 0; i < p->len; i++, row++) {
		int len;
		struct panel_line *pl = &p->lines[i];

		if (!pl->label)
			continue;

		Term_putstr(col, row, strlen(pl->label), COLOUR_WHITE, pl->label);

		len = strlen(pl->value);
		len = len < w - offset ? len : w - offset - 1;

		if (left_adj)
			Term_putstr(col+offset, row, len, pl->attr, pl->value);
		else
			Term_putstr(col+w-len, row, len, pl->attr, pl->value);
	}
}

static uint8_t max_color(int val, int max)
{
	return val < max ? COLOUR_YELLOW : COLOUR_L_GREEN;
}

static struct panel *get_panel_topleft(void) {
	struct panel *p = panel_allocate(4);

	panel_line(p, COLOUR_L_BLUE, "Name", "%s", player->full_name);
	panel_line(p, COLOUR_L_BLUE, "Sex", "%s", player->sex->name);
	panel_line(p, COLOUR_L_BLUE, "Race", "%s", player->race->name);
	panel_line(p, COLOUR_L_BLUE, "House", "%s", player->house->short_name);

	return p;
}

static struct panel *get_panel_midleft(void) {
	struct panel *p = panel_allocate(8);

	panel_line(p, COLOUR_L_GREEN, "Game Turn", "%d", player->turn);
	panel_line(p, COLOUR_L_GREEN, "Exp Pool", "%d", player->new_exp);
	panel_line(p, COLOUR_L_GREEN, "Total Exp", "%d", player->exp);
	panel_line(p, max_color(weight_limit(player->state),
							player->upkeep->total_weight),
			   "Burden", "%.1f", player->upkeep->total_weight / 10.0F);
	panel_line(p, COLOUR_L_GREEN, "Max Burden", "%.1f",
			   weight_limit(player->state) / 10.0F);
	if (turn > 0) {
		panel_line(p, max_color(player->depth, player_min_depth(player)),
				   "Depth", "%3d'", player->depth * 50);
		panel_line(p, COLOUR_L_GREEN, "Min Depth", "%3d'",
				   player_min_depth(player) * 50);
	} else {
		panel_space(p);
		panel_space(p);
	}
	panel_line(p, COLOUR_L_GREEN, "Light Radius", "%3d",
			   player->upkeep->cur_light);

	return p;
}

static struct panel *get_panel_combat(void) {
	struct panel *p = panel_allocate(9);
	int mel, arc;
	int add_lines = 3;

	/* Melee */
	mel = player->state.skill_use[SKILL_MELEE];
	panel_line(p, COLOUR_L_BLUE, "Melee", "(%+d,%dd%d)", mel, player->state.mdd,
			   player->state.mds);
	if (player_active_ability(player, "Rapid Attack")) {
		add_lines--;
		panel_line(p, COLOUR_L_BLUE, "", "(%+d,%dd%d)", mel, player->state.mdd,
				   player->state.mds);
	}
	if (player->state.mds2 > 0) {
		add_lines--;
		mel += player->state.offhand_mel_mod;
		panel_line(p, COLOUR_L_BLUE, "", "(%+d,%dd%d)", mel, player->state.mdd2,
				   player->state.mds2);
	}

	/* Ranged */
	arc = player->state.skill_use[SKILL_ARCHERY];
	panel_line(p, COLOUR_L_BLUE, "Bows", "(%+d,%dd%d)", arc, player->state.add,
			   player->state.ads);
	if (player_active_ability(player, "Rapid Fire")) {
		add_lines--;
		panel_line(p, COLOUR_L_BLUE, "", "(%+d,%dd%d)", arc, player->state.add,
			   player->state.ads);
	}

	/* Armor */
	panel_line(p, COLOUR_L_BLUE, "Armor", "[%+d,%d-%d]",
			   player->state.skill_use[SKILL_EVASION],
			   protection_roll(player, PROJ_HURT, true, MINIMISE),
			   protection_roll(player, PROJ_HURT, true, MAXIMISE));

	if (add_lines) {
		panel_space(p);
		add_lines--;
	}

	/* Health */
	panel_line(p, COLOUR_L_BLUE, "Health", "%d:%d", player->chp, player->mhp);

	/* Voice */
	panel_line(p, COLOUR_L_BLUE, "Voice", "%d:%d", player->csp, player->msp);

	/* Song */
	if (player->song[SONG_MAIN]) {
		panel_line(p, COLOUR_L_BLUE, "Song", "%s",
				   player->song[SONG_MAIN]->name);
		if (player->song[SONG_MINOR]) {
			panel_line(p, COLOUR_L_BLUE, "", "%s",
					   player->song[SONG_MINOR]->name);
		}
	}
	/* Note potential trouble if two songs and two or three extra lines - NRM */

	return p;
}

static struct panel *get_panel_misc(void) {
	struct panel *p = panel_allocate(3);
	uint8_t attr = COLOUR_L_BLUE;

	panel_line(p, attr, "Age", "%d", player->age);
	panel_line(p, attr, "Height", "%d'%d\"", player->ht / 12, player->ht % 12);
	panel_line(p, attr, "Weight", "%d", player->wt);

	return p;
}

/**
 * Panels for main character screen
 */
static const struct {
	region bounds;
	bool align_left;
	struct panel *(*panel)(void);
} panels[] = {
	/*   x  y wid rows */
	{ {  1, 1, 18, 4 }, true,  get_panel_topleft },	/* Name, Class, ... */
	{ { 22, 1, 12, 3 }, false, get_panel_misc },	/* Age, ht, wt, ... */
	{ {  1, 6, 18, 9 }, false, get_panel_midleft },	/* Cur Exp, Max Exp, ... */
	{ { 22, 6, 16, 9 }, false, get_panel_combat },
};

void display_player_xtra_info(void)
{
	size_t i;
	for (i = 0; i < N_ELEMENTS(panels); i++) {
		struct panel *p = panels[i].panel();
		display_panel(p, panels[i].align_left, &panels[i].bounds);
		panel_free(p);
	}

	/* Indent output by 1 character, and wrap at column 72 */
	text_out_wrap = 72;
	text_out_indent = 1;

	/* History */
	Term_gotoxy(text_out_indent, 19);
	text_out_to_screen(COLOUR_WHITE, player->history);

	/* Reset text_out() vars */
	text_out_wrap = 0;
	text_out_indent = 0;

	return;
}

/**
 * Display the character on the screen (two different modes)
 *
 * The top two lines, and the bottom line (or two) are left blank.
 *
 * Mode 0 = standard display with skills/history
 * Mode 1 = special display with equipment flags
 */
void display_player(int mode)
{
	/* Erase screen */
	clear_from(0);

	/* When not playing, do not display in subwindows */
	if (Term != angband_term[0] && !player->upkeep->playing) return;

	/* Stat info */
	display_player_stat_info();

	if (mode) {
		struct panel *p = panels[0].panel();
		display_panel(p, panels[0].align_left, &panels[0].bounds);
		panel_free(p);

	} else {
		/* Extra info */
		display_player_xtra_info();

		/* Skill info */
		display_player_skill_info();
	}
}

/**
 * Write a character dump
 */
void write_character_dump(ang_file *fff)
{
	int i, x, y;

	int a;
	wchar_t c;

	char o_name[80];

	int n;
	char buf[1024];
	char *p;


	/* Begin dump */
	file_putf(fff, "  [%s Character Dump]\n\n", buildid);

	/* Display player basics */
	display_player(0);

	/* Dump part of the screen */
	for (y = 1; y < 23; y++) {
		p = buf;
		/* Dump each row */
		for (x = 0; x < 79; x++) {
			/* Get the attr/char */
			(void)(Term_what(x, y, &a, &c));

			/* Dump it */
			n = text_wctomb(p, c);
			if (n > 0) {
				p += n;
			} else {
				*p++ = ' ';
			}
		}

		/* Back up over spaces */
		while ((p > buf) && (p[-1] == ' ')) --p;

		/* Terminate */
		*p = '\0';

		/* End the row */
		file_putf(fff, "%s\n", buf);
	}

	/* Skip some lines */
	file_putf(fff, "\n\n");


	/* If dead, dump last messages -- Prfnoff */
	if (player->is_dead) {
		i = messages_num();
		if (i > 15) i = 15;
		file_putf(fff, "  [Last Messages]\n\n");
		while (i-- > 0)
		{
			file_putf(fff, "> %s\n", message_str((int16_t)i));
		}
		file_putf(fff, "\nKilled by %s.\n\n", player->died_from);

		file_putf(fff, "\n  [Screenshot]\n\n");
		file_mini_screenshot(fff);
		file_putf(fff, "\n");
	}


	/* Dump the equipment */
	file_putf(fff, "  [Character Equipment]\n\n");
	for (i = 0; i < player->body.count; i++) {
		struct object *obj = slot_object(player, i);
		if (!obj) continue;

		object_desc(o_name, sizeof(o_name), obj,
			ODESC_PREFIX | ODESC_FULL, player);
		file_putf(fff, "%c) %s\n", gear_to_label(player, obj), o_name);
		object_info_chardump(fff, obj, 5, 72);
	}
	file_putf(fff, "\n\n");

	/* Dump the inventory */
	file_putf(fff, "\n\n  [Character Inventory]\n\n");
	for (i = 0; i < z_info->pack_size; i++) {
		struct object *obj = player->upkeep->inven[i];
		if (!obj) break;

		object_desc(o_name, sizeof(o_name), obj,
			ODESC_PREFIX | ODESC_FULL, player);
		file_putf(fff, "%c) %s\n", gear_to_label(player, obj), o_name);
		object_info_chardump(fff, obj, 5, 72);
	}
	file_putf(fff, "\n\n");

	/* Dump character history */
	dump_history(fff);
	file_putf(fff, "\n\n");

	/* Dump options */
	file_putf(fff, "  [Options]\n\n");

	/* Dump options */
	for (i = 0; i < OP_MAX; i++) {
		int opt;
		const char *title = "";
		switch (i) {
			case OP_INTERFACE: title = "User interface"; break;
			case OP_BIRTH: title = "Birth"; break;
		    default: continue;
		}

		file_putf(fff, "  [%s]\n\n", title);
		for (opt = 0; opt < OPT_MAX; opt++) {
			if (option_type(opt) != i) continue;

			file_putf(fff, "%-45s: %s (%s)\n",
			        option_desc(opt),
			        player->opts.opt[opt] ? "yes" : "no ",
			        option_name(opt));
		}

		/* Skip some lines */
		file_putf(fff, "\n");
	}

	/*
	 * Display the randart seed, if applicable.  Use the same format as is
	 * used when constructing the randart file name.
	 */
	if (player->self_made_arts) {
		file_putf(fff, "  [Artefact label]\n\n");
		file_putf(fff, "%08x\n\n", seed_randart);
	}
}

/**
 * Save the lore to a file in the user directory.
 *
 * \param path is the path to the filename
 *
 * \returns true on success, false otherwise.
 */
bool dump_save(const char *path)
{
	if (text_lines_to_file(path, write_character_dump)) {
		msg("Failed to create file %s.new", path);
		return false;
	}

	return true;
}



#define INFO_SCREENS 2 /* Number of screens in character info mode */

/**
 * Hack -- change name
 */
void do_cmd_change_name(void)
{
	ui_event ke;
	int mode = 0;

	bool more = true;

	/* Save screen */
	screen_save();

	/* Forever */
	while (more) {
		/* Display the player */
		display_player(mode);

		/* Prompt */
		Term_putstr(1, 23, -1, COLOUR_SLATE, "history   change name   save to a file   abilities   increase skills   ESC");
		Term_putstr(1, 23, -1, COLOUR_L_WHITE, "h");
		Term_putstr(11, 23, -1, COLOUR_L_WHITE, "c");
		Term_putstr(25, 23, -1, COLOUR_L_WHITE, "s");
		Term_putstr(42, 23, -1, COLOUR_L_WHITE, "a");
		Term_putstr(54, 23, -1, COLOUR_L_WHITE, "i");
		Term_putstr(72, 23, -1, COLOUR_L_WHITE, "ESC");

		/* Query */
		ke = inkey_ex();

		if ((ke.type == EVT_KBRD)||(ke.type == EVT_BUTTON)) {
			switch (ke.key.code) {
				case ESCAPE: more = false; break;

				case 'h': {
					history_display();
					break;
				}

				case 'c': {
					if(arg_force_name)
						msg("You are not allowed to change your name!");
					else {
					char namebuf[32] = "";

					/* Set player name */
					if (get_character_name(namebuf, sizeof namebuf))
						my_strcpy(player->full_name, namebuf,
								  sizeof(player->full_name));
					}

					break;
				}

				case 's': {
					char buf[1024];
					char fname[80];

					/* Get the filesystem-safe name and append .txt */
					player_safe_name(fname, sizeof(fname), player->full_name, false);
					my_strcat(fname, ".txt", sizeof(fname));

					if (get_file(fname, buf, sizeof buf)) {
						if (dump_save(buf))
							msg("Character dump successful.");
						else
							msg("Character dump failed!");
					}
					break;
				}
				
				case 'a': {
					do_cmd_abilities();
					/*
					 * In case an ability was added, update
					 * the player's state.
					 */
					update_stuff(player);
					break;
				}

				case 'i': {
					(void) gain_skills(CTX_GAME, true);
					break;
				}

			}
		} else if (ke.type == EVT_MOUSE) {
			if (ke.mouse.button == 1) {
				/* Flip through the screens */			
				mode = (mode + 1) % INFO_SCREENS;
			} else if (ke.mouse.button == 2) {
				/* exit the screen */
				more = false;
			} else {
				/* Flip backwards through the screens */			
				mode = (mode - 1) % INFO_SCREENS;
			}
		}

		/* Flush messages */
		event_signal(EVENT_MESSAGE_FLUSH);
	}

	/* Load screen */
	screen_load();
}

