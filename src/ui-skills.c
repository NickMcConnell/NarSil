/**
 * \file ui-skills.c
 * \brief Text-based user interface for skill point allocation
 *
 * Copyright (c) 1987 - 2023 Angband contributors
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
#include "cmd-core.h"
#include "game-world.h"
#include "player-calcs.h"
#include "player-skills.h"
#include "ui-menu.h"
#include "ui-input.h"
#include "ui-output.h"
#include "ui-player.h"
#include "ui-skills.h"
#include "ui-target.h"


/**
 * ------------------------------------------------------------------------
 * Point-based skill allocation.
 * ------------------------------------------------------------------------ */

#define COSTS_COL     (42 + 32)
#define TOTAL_COL     (42 + 15)
#define SKILL_COSTS_ROW 7
int skill_idx = 0;

/*
 * Remember what's possible for a given skill.  0 means can't buy or sell.
 * 1 means can sell.  2 means can buy.  3 means can buy or sell.
 */
static int buysell[SKILL_MAX];

/**
 * This is called whenever a skill changes.  We take the easy road, and just
 * redisplay them all using the standard function.
 */
static void point_skills(game_event_type type, game_event_data *data,
							  void *user)
{
	display_player_skill_info();
}

/**
 * This is called whenever any of the other miscellaneous skill-dependent things
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
static void skill_points(game_event_type type, game_event_data *data,
						 void *user)
{
	int i;
	int sum = 0;
	const int *spent = data->exp.exp;
	const int *inc = data->exp.inc_exp;
	int remaining = data->exp.remaining;

	/* Display the costs header */
	put_str("Cost", SKILL_COSTS_ROW - 1, COSTS_COL);

	for (i = 0; i < SKILL_MAX; i++) {
		/* Remember what's allowed. */
		buysell[i] = 0;
		if (spent[i] > 0) {
			buysell[i] |= 1;
		}
		if (inc[i] <= remaining) {
			buysell[i] |= 2;
		}
		/* Display cost */
		put_str(format("%4d", spent[i]), SKILL_COSTS_ROW + i, COSTS_COL);
		sum += spent[i];
	}

	put_str(format("Total Cost: %4d/%4d", sum, remaining + sum),
		SKILL_COSTS_ROW + SKILL_MAX, TOTAL_COL);
}

static void skill_points_start(cmd_context context, bool reset)
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

	for (i = 0; i < SKILL_MAX; ++i) {
		buysell[i] = 0;
	}

	/* Register handlers for various events - cheat a bit because we redraw
	   the lot at once rather than each bit at a time. */
	event_add_handler(EVENT_SKILLPOINTS, skill_points, NULL);	
	event_add_handler(EVENT_SKILLS, point_skills, NULL);	
	event_add_handler(EVENT_EXP_CHANGE, point_misc, NULL);
	init_skills(false, reset);
}

static void skill_points_stop(void)
{
	event_remove_handler(EVENT_SKILLPOINTS, skill_points, NULL);	
	event_remove_handler(EVENT_SKILLS, point_skills, NULL);	
	event_remove_handler(EVENT_EXP_CHANGE, point_misc, NULL);	
}

static int skill_points_command(void)
{
	enum {
		ACT_CTX_SKILL_PTS_NONE,
		ACT_CTX_SKILL_PTS_BUY,
		ACT_CTX_SKILL_PTS_SELL,
		ACT_CTX_SKILL_PTS_ESCAPE,
		ACT_CTX_SKILL_PTS_RESET,
		ACT_CTX_SKILL_PTS_ACCEPT,
		ACT_CTX_SKILL_PTS_QUIT
	};
	int action = ACT_CTX_SKILL_PTS_NONE;
	ui_event in;
	int next = 0;

	/* Place cursor just after cost of current skill */
	Term_gotoxy(COSTS_COL + 4, SKILL_COSTS_ROW + skill_idx);

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
			action = ACT_CTX_SKILL_PTS_QUIT;
		} else if (in.key.code == ESCAPE) {
			action = ACT_CTX_SKILL_PTS_ESCAPE;
		} else if (in.key.code == 'r' || in.key.code == 'R') {
			action = ACT_CTX_SKILL_PTS_RESET;
		} else if (in.key.code == KC_ENTER) {
			action = ACT_CTX_SKILL_PTS_ACCEPT;
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
			 * Go to previous skill.  Loop back to the last if at
			 * the first.
			 */
			if (dir == 8) {
				skill_idx = (skill_idx + SKILL_MAX - 1) % SKILL_MAX;
			}

			/*
			 * Go to next skill.  Loop back to the first if at the
			 * last.
			 */
			if (dir == 2) {
				skill_idx = (skill_idx + 1) % SKILL_MAX;
			}

			/* Decrease skill (if possible). */
			if (dir == 4) {
				action = ACT_CTX_SKILL_PTS_SELL;
			}

			/* Increase skill (if possible). */
			if (dir == 6) {
				action = ACT_CTX_SKILL_PTS_BUY;
			}
		}
	} else if (in.type == EVT_MOUSE) {
		assert(skill_idx >= 0 && skill_idx < SKILL_MAX);
		if (in.mouse.button == 2) {
			action = ACT_CTX_SKILL_PTS_ESCAPE;
		} else if (in.mouse.y >= SKILL_COSTS_ROW
				&& in.mouse.y < SKILL_COSTS_ROW + SKILL_MAX
				&& in.mouse.y != SKILL_COSTS_ROW + skill_idx) {
			/*
			 * Make that skill the current one if buying or selling.
			 */
			skill_idx = in.mouse.y - SKILL_COSTS_ROW;
		} else {
			/* Present a context menu with the other actions. */
			char *labels = string_make(lower_case);
			struct menu *m = menu_dynamic_new();

			m->selections = labels;
			if (in.mouse.y == SKILL_COSTS_ROW + skill_idx
					&& (buysell[skill_idx] & 1)) {
				menu_dynamic_add_label(m, "Sell", 's',
					ACT_CTX_SKILL_PTS_SELL, labels);
			}
			if (in.mouse.y == SKILL_COSTS_ROW + skill_idx
					&& (buysell[skill_idx] & 2)) {
				menu_dynamic_add_label(m, "Buy", 'b',
					ACT_CTX_SKILL_PTS_BUY, labels);
			}
			menu_dynamic_add_label(m, "Accept", 'a',
				ACT_CTX_SKILL_PTS_ACCEPT, labels);
			menu_dynamic_add_label(m, "Reset", 'r',
				ACT_CTX_SKILL_PTS_RESET, labels);
			menu_dynamic_add_label(m, "Quit", 'q',
				ACT_CTX_SKILL_PTS_QUIT, labels);

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
	case ACT_CTX_SKILL_PTS_SELL:
		assert(skill_idx >= 0 && skill_idx < SKILL_MAX);
		cmdq_push(CMD_SELL_SKILL);
		cmd_set_arg_choice(cmdq_peek(), "choice", skill_idx);
		break;

	case ACT_CTX_SKILL_PTS_BUY:
		assert(skill_idx >= 0 && skill_idx < SKILL_MAX);
		cmdq_push(CMD_BUY_SKILL);
		cmd_set_arg_choice(cmdq_peek(), "choice", skill_idx);
		break;

	case ACT_CTX_SKILL_PTS_ESCAPE:
		/* Undo any changes made and then exit from skill buying. */
		cmdq_push(CMD_RESET_SKILLS);
		cmd_set_arg_choice(cmdq_peek(), "choice", false);
		next = -1;
		break;

	case ACT_CTX_SKILL_PTS_RESET:
		cmdq_push(CMD_RESET_SKILLS);
		cmd_set_arg_choice(cmdq_peek(), "choice", false);
		break;

	case ACT_CTX_SKILL_PTS_ACCEPT:
		/* Finalise that we are buying the skills as allocated. */
		next = 1;
		break;

	case ACT_CTX_SKILL_PTS_QUIT:
		if (character_dungeon) {
			next = -1;
		} else {
			quit(NULL);
		}
		break;

	default:
		/* Do nothing and remain at this stage. */
		break;
	}

	return next;
}
	
/**
 * Increase your skills by spending experience points
 */
int gain_skills(cmd_context context, bool reset)
{
	int next = 0;
	skill_points_start(context, reset);
	while (!next) {
		next = skill_points_command();
		cmdq_push(CMD_REFRESH_SKILLS);
		cmdq_execute(context);
	}
	skill_points_stop();
	if (context == CTX_GAME) {
		finalise_skills();
		player->upkeep->redraw |= (PR_EXP);
	}
	return next;
}


