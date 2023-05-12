/**
 * \file ui-abilities.c
 * \brief Text-based user interface for player abilities
 *
 * Copyright (c) 1987 - 2022 Angband contributors
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
#include "monster.h"
#include "player-abilities.h"
#include "ui-abilities.h"
#include "ui-input.h"
#include "ui-menu.h"



#define COL_ABILITY		17
#define COL_DESCRIPTION	46

static struct ability **skill_abilities;

static struct bane_type {
	int race_flag;
	const char *name;
} bane_types[] = {
	#define BANE(a, b) { RF_##a, b },
	#include "list-bane-types.h"
	#undef BANE
};

static int get_skill_abilities(int skill)
{
	struct ability *a = abilities;
	int count = 0;
	while (a) {
		if (a->skill == skill) skill_abilities[count++] = a;
		a = a->next;
	}
	return count;
}

/**
 * Display an entry in the bane menu.
 */
static void bane_display(struct menu *menu, int oid, bool cursor, int row,
						 int col, int width)
{
	struct bane_type *choice = menu->menu_data;
	uint8_t attr = (cursor ? COLOUR_L_BLUE : COLOUR_WHITE);
	c_put_str(attr, choice[oid].name, row, col);	
}

/**
 * Handle keypresses in the bane menu.
 */
static bool bane_action(struct menu *m, const ui_event *event, int oid)
{
	if ((event->type == EVT_SELECT) && oid) {
		player->bane_type = oid;
		return true;
	}
	return false;
}

/**
 * Display the bane menu.
 */
static bool bane_menu(void)
{
	struct menu menu;
	menu_iter menu_f = { NULL, NULL, bane_display, bane_action, NULL };
	region area = { COL_DESCRIPTION, 4, 0, 0 };
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.title = "Enemy types";
	menu_setpriv(&menu, N_ELEMENTS(bane_types), bane_types);
	menu_layout(&menu, &area);
	menu_select(&menu, 0, true);
	return player->bane_type != RF_NONE;
}

/**
 * Display an entry in the ability menu.
 */
static void ability_display(struct menu *menu, int oid, bool cursor, int row,
							int col, int width)
{
	struct ability **choice = menu->menu_data;
	struct ability *innate = locate_ability(player->abilities, choice[oid]);
	struct ability *item = locate_ability(player->item_abilities, choice[oid]);
	uint8_t attr = COLOUR_L_DARK;
	int points = player->skill_base[choice[oid]->skill];
	int points_needed = choice[oid]->level;

	if (innate) {
		attr = innate->active ? COLOUR_WHITE : COLOUR_RED;
	} else if (item) {
		attr = item->active ? COLOUR_L_GREEN : COLOUR_RED;
	} else if (player_has_prereq_abilities(player, choice[oid]) &&
			   (points >= points_needed)) {
		attr = COLOUR_SLATE;
	}
	c_put_str(attr, choice[oid]->name, row, col);	
}

/**
 * Handle keypresses in the ability menu.
 */
static bool ability_action(struct menu *m, const ui_event *event, int oid)
{
	struct ability **choice = m->menu_data;
	struct ability *possessed = locate_ability(player->abilities, choice[oid]);
	bool points = player->skill_base[choice[oid]->skill] >= choice[oid]->level;

	/* Check for item abilities */
	if (!possessed) {
		possessed = locate_ability(player->item_abilities, choice[oid]);
	}

	if (event->type == EVT_SELECT) {
		if (possessed) {
			if (possessed->active) {
				possessed->active = false;
				put_str("Ability now switched off.", 0, 0);
			} else {
				possessed->active = true;
				put_str("Ability now switched on.", 0, 0);
			}
		} else if (player_has_prereq_abilities(player, choice[oid]) && points) {
			if (player_can_gain_ability(player, choice[oid])) {
				if (streq(choice[oid]->name, "Bane")) {
					if (!bane_menu()) return false;
				}
				if (player_gain_ability(player, choice[oid])) {
					put_str("Ability gained.", 0, 0);
				}
			} else {
				msg("You do not have enough experience to acquire this ability.");
			}
		} else {
			msg("Insufficient prerequisites for ability!");
		}
		return true;
	}
	return false;
}

/**
 * Show ability data
 */
static void ability_browser(int oid, void *data, const region *loc)
{
	struct ability **choice = data;
	struct ability *current = choice[oid];
	bool learned = player_has_ability(player, current->name);
	uint8_t attr = COLOUR_L_DARK;
	bool points = player->skill_base[current->skill] >= current->level;

	/* Redirect output to the screen */
	text_out_hook = text_out_to_screen;
	text_out_wrap = 79;
	text_out_indent = COL_DESCRIPTION;
	Term_gotoxy(text_out_indent, 4);

	/* Print the description of the current ability */
	if (current->desc) {
		text_out_c(COLOUR_L_WHITE, "%s", current->desc);
	}

	/* Print more info if you don't have the skill */
	if (!learned) {
		struct ability *prereq = current->prerequisites;
		bool ready = player_has_prereq_abilities(player, current);
		int line = 0;
		if (ready && points) {
			attr = COLOUR_SLATE;
		}
		Term_gotoxy(text_out_indent, 10);
		text_out_c(attr, "Prerequisites:");
		Term_gotoxy(text_out_indent, 12);
		if (points) {
			text_out_c(COLOUR_SLATE, "  %d skill points", current->level);
		} else {
			text_out_c(COLOUR_L_DARK, "  %d skill points (you have %d)",
					   current->level, player->skill_base[current->skill]);
		}
		Term_gotoxy(text_out_indent + 2, 13);
		while (prereq) {
			if (player_has_ability(player, prereq->name)) {
				attr = COLOUR_SLATE;
			} else {
				attr = COLOUR_L_DARK;
			}
			text_out_c(attr, "%s", prereq->name);
			prereq = prereq->next;
			line++;
			if (prereq) {
				Term_gotoxy(text_out_indent + 2, 13 + line);
				text_out_c(COLOUR_L_DARK, "or ");
			}
		}
		if (ready) {
			int exp_cost = player_ability_cost(player, current);
			bool have_exp = player_can_gain_ability(player, current);
			attr = have_exp ? COLOUR_SLATE : COLOUR_L_DARK;
			Term_gotoxy(text_out_indent, 16);
			text_out_c(attr, "Current price:");
			Term_gotoxy(text_out_indent + 2, 18);
			if (have_exp) {
				text_out_c(attr, "%d experience", exp_cost, player->new_exp);
			} else {
				text_out_c(attr, "%d experience (you have %d)", exp_cost,
						   player->new_exp);
			}
		}
	}
}

/**
 * Display an entry in the skill menu.
 */
static void skill_display(struct menu *menu, int oid, bool cursor, int row,
						 int col, int width)
{
	char **choice = menu->menu_data;
	uint8_t attr = (cursor ? COLOUR_L_BLUE : COLOUR_WHITE);
	c_put_str(attr, choice[oid], row, col);	
}


/**
 * Handle keypresses in the skill menu.
 */
static bool skill_action(struct menu *m, const ui_event *event, int oid)
{
	struct menu menu;
	menu_iter menu_f = { NULL, NULL, ability_display, ability_action, NULL };
	region area = { COL_ABILITY, 2, COL_DESCRIPTION - COL_ABILITY - 5, 0 };
	int count;
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.title = "Abilities";
	skill_abilities = mem_zalloc(100 * sizeof(struct ability*));
	count = get_skill_abilities(oid);
	if (count) {
		menu_setpriv(&menu, count, skill_abilities);
		menu.browse_hook = ability_browser;
		menu.selections = lower_case;
		menu.flags = MN_CASELESS_TAGS;
		menu_layout(&menu, &area);
		menu_select(&menu, 0, true);
	}
	mem_free(skill_abilities);
	return true;
}

/**
 * Display the abilities main menu.
 */
void do_cmd_abilities(void)
{
	struct menu menu;
	menu_iter menu_f = { NULL, NULL, skill_display, skill_action, NULL };
	const char *skill_names[] = {
		#define SKILL(a, b) b,
		#include "list-skills.h"
		#undef SKILL
	};
	ui_event evt = EVENT_EMPTY;

	screen_save();
	clear_from(0);

	/* Set up the menu */
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.title = "Skills";
	menu_setpriv(&menu, SKILL_MAX, skill_names);
	menu.selections = lower_case;
	menu.flags = MN_CASELESS_TAGS;
	menu_layout(&menu, &SCREEN_REGION);

	/* Select an entry */
	while (evt.type != EVT_ESCAPE)
		evt = menu_select(&menu, 0, false);

	screen_load();
}

