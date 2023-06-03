/**
 * \file ui-death.c
 * \brief Handle the UI bits that happen after the character dies.
 *
 * Copyright (c) 1987 - 2007 Angband contributors
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
#include "effects.h"
#include "game-input.h"
#include "init.h"
#include "obj-desc.h"
#include "obj-info.h"
#include "obj-knowledge.h"
#include "player-calcs.h"
#include "savefile.h"
#include "score.h"
#include "ui-death.h"
#include "ui-history.h"
#include "ui-input.h"
#include "ui-knowledge.h"
#include "ui-map.h"
#include "ui-menu.h"
#include "ui-object.h"
#include "ui-player.h"
#include "ui-score.h"
#include "ui-spoil.h"

/**
 * Display a "tomb-stone"
 */
static void print_tomb(struct high_score *score)
{
	if (player->escaped) {
		Term_putstr(15, 2, -1, COLOUR_L_BLUE, "You have escaped");
	} else {
		Term_putstr(15, 2, -1, COLOUR_L_BLUE, "You have been slain");
	}
	display_single_score(score, 1, 0, COLOUR_WHITE);
	Term_putstr( 3, 10, -1, COLOUR_L_DARK, "____________________________________________________");
	prt_mini_screenshot(5, 14);
}

/**
 * Menu command: see top twenty scores.
 */
static void death_scores(const char *title, int row)
{
	screen_save();
	show_scores();
	screen_load();
}

/**
 * Menu command: examine items in the inventory.
 */
static void death_examine(const char *title, int row)
{
	struct object *obj;
	const char *q, *s;

	/* Get an item */
	q = "Examine which item? ";
	s = "You have nothing to examine.";

	while (get_item(&obj, q, s, 0, NULL, (USE_INVEN | USE_QUIVER | USE_EQUIP | IS_HARMLESS))) {
		char header[120];

		textblock *tb;
		region area = { 0, 0, 0, 0 };

		tb = object_info(obj, OINFO_NONE);
		object_desc(header, sizeof(header), obj,
			ODESC_PREFIX | ODESC_FULL | ODESC_CAPITAL, player);

		textui_textblock_show(tb, area, header);
		textblock_free(tb);
	}
}

/**
 * Menu command: Look at the dungeon.
 */
static void death_dungeon(const char *title, int row)
{
	int i;
	struct object *obj;
				
	/* Save screen */
	screen_save();
			
	/* Dungeon objects */
	for (i = 1; i < cave->obj_max; i++) {
		/* Get the next object from the dungeon */
		obj = cave->objects[i];

		/* Skip dead objects */
		if (!obj || !obj->kind) continue;

		/* ID it */
		object_flavor_aware(player, obj);
		object_know(obj);
	}

	/* Light the level, show all monsters and redraw */
	Term_clear();
	wiz_light(cave, player);
	effect_simple(EF_DETECT_MONSTERS, source_player(), "0",
		0, 0, 0, NULL);
	player->upkeep->redraw |= 0x0FFFFFFFL;
	handle_stuff(player);

	/* Allow the player to look around */
	prt_map();
	do_cmd_look();

	/* Load screen */
	screen_load();
}

/**
 * Menu command: peruse pre-death messages.
 */
static void death_messages(const char *title, int row)
{
	screen_save();
	do_cmd_messages();
	screen_load();
}

/**
 * Menu command: view character dump and inventory.
 */
static void death_info(const char *title, int row)
{
	screen_save();

	/* Display player */
	display_player(0);

	/* Prompt for inventory */
	prt("Hit any key to see more information: ", 0, 0);

	/* Allow abort at this point */
	(void)anykey();


	/* Show equipment and inventory */

	/* Equipment -- if any */
	if (player->upkeep->equip_cnt) {
		Term_clear();
		show_equip(OLIST_WEIGHT | OLIST_SEMPTY | OLIST_DEATH, NULL);
		prt("You are using: -more-", 0, 0);
		(void)anykey();
	}

	/* Inventory -- if any */
	if (player->upkeep->inven_cnt) {
		Term_clear();
		show_inven(OLIST_WEIGHT | OLIST_DEATH, NULL);
		prt("You are carrying: -more-", 0, 0);
		(void)anykey();
	}

	screen_load();
}

/**
 * Menu command: view character history.
 */
static void death_history(const char *title, int row)
{
	history_display();
}

/**
 * Menu command: add to character history.
 */
static void death_note(const char *title, int row)
{
	do_cmd_note();
}

/**
 * Menu command: dump character dump to file.
 */
static void death_file(const char *title, int row)
{
	char buf[1024];
	char ftmp[80];

	/* Get the filesystem-safe name and append .txt */
	player_safe_name(ftmp, sizeof(ftmp), player->full_name, false);
	my_strcat(ftmp, ".txt", sizeof(ftmp));

	if (get_file(ftmp, buf, sizeof buf)) {
		bool success;

		/* Dump a character file */
		screen_save();
		success = dump_save(buf);
		screen_load();

		/* Check result */
		if (success)
			msg("Character dump successful.");
		else
			msg("Character dump failed!");

		/* Flush messages */
		event_signal(EVENT_MESSAGE_FLUSH);
	}
}


/**
 * Menu command: allow spoiler generation (mainly for randarts).
 */
static void death_spoilers(const char *title, int row)
{
	do_cmd_spoilers();
}

/**
 * Menu structures for the death menu. Note that Quit must always be the
 * last option, due to a hard-coded check in death_screen
 */
static menu_action death_actions[] =
{
	{ 0, 'v', "View scores",                  death_scores    },
	{ 0, 'x', "View inventory and equipment", death_examine   },
	{ 0, 'd', "View dungeon",                 death_dungeon   },
	{ 0, 'm', "View final messages",          death_messages  },
	{ 0, 'c', "View character sheet",         death_info      },
	{ 0, 'h', "View character history",       death_history   },
	{ 0, 'a', "Add comment to history",       death_note      },
	{ 0, 'f', "Save character sheet",         death_file      },
	{ 0, 's', "Spoilers",                     death_spoilers  },
	{ 0, 'q', "Quit",                         NULL            },
};

/**
 * Handle character death
 */
void death_screen(void)
{
	struct menu *death_menu;
	bool done = false;
	const region area = { 15, 12, 0, N_ELEMENTS(death_actions) };
	time_t death_time = (time_t)0;
	struct high_score score;

	/* Get time of death, prepare score */
	(void)time(&death_time);
	build_score(&score, player, player->died_from, &death_time);

	/* Tombstone */
	clear_from(0);
	print_tomb(&score);

	/* Flush all input and output */
	event_signal(EVENT_INPUT_FLUSH);
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Display and use the death menu */
	death_menu = menu_new_action(death_actions,
			N_ELEMENTS(death_actions));

	death_menu->flags = MN_CASELESS_TAGS;

	menu_layout(death_menu, &area);

	while (!done)
	{
		ui_event e = menu_select(death_menu, EVT_KBRD, false);
		if (e.type == EVT_KBRD)
		{
			if (e.key.code == KTRL('X')) break;
		}
		else if (e.type == EVT_SELECT)
		{
			done = true;
		}
	}

	menu_free(death_menu);
}
