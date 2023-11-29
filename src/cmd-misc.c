/**
 * \file cmd-misc.c
 * \brief Deal with miscellaneous commands.
 *
 * Copyright (c) 2010 Andi Sidwell
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
#include "cave.h"
#include "cmd-core.h"
#include "cmds.h"
#include "game-input.h"
#include "init.h"
#include "mon-lore.h"
#include "mon-util.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "obj-util.h"
#include "target.h"


/**
 * Toggle wizard mode
 */
void do_cmd_wizard(void)
{
	/* Verify first time */
	if (!(player->noscore & NOSCORE_WIZARD)) {
		/* Mention effects */
		msg("You are about to enter 'wizard' mode for the very first time!");
		msg("This is a form of cheating, and your game will not be scored!");
		event_signal(EVENT_MESSAGE_FLUSH);

		/* Verify request */
		if (!get_check("Are you sure you want to enter wizard mode? "))
			return;

		/* Mark savefile */
		player->noscore |= NOSCORE_WIZARD;
	}

	/* Toggle mode */
	if (player->wizard) {
		player->wizard = false;
		msg("Wizard mode off.");
	} else {
		player->wizard = true;
		msg("Wizard mode on.");
	}

	/* Update monsters */
	player->upkeep->update |= (PU_MONSTERS);
}

/**
 * Escape from Angband
 */
void do_cmd_escape(void)
{
	time_t ct = time((time_t*)0);
	char long_day[40];
	char buf[120];

	/* Set the escaped flag */
	player->escaped = true;

	/* Flush input */
	event_signal(EVENT_INPUT_FLUSH);

	/* Treat escaping character as dead to satisfy end of game logic. */
	event_signal(EVENT_DEATH);
	player->is_dead = true;

	/* Get time */
	(void)strftime(long_day, 40, "%d %B %Y", localtime(&ct));
	
	/* Add notes to the history */
	strnfmt(buf, sizeof(buf), "You escaped the Iron Hells on %s.", long_day);
	history_add(player, buf, HIST_ESCAPE);
	switch (silmarils_possessed(player)) {
		case 0:	{
			history_add(player, "You returned empty handed.", HIST_ESCAPE);
			break;
		}
		case 1:	{
			history_add(player, "You brought back a Silmaril from Morgoth's crown!", HIST_ESCAPE);
			break;
		}
		case 2:	{
			history_add(player, "You brought back two Silmarils from Morgoth's crown!", HIST_ESCAPE);
			break;
		}
		case 3: {
			history_add(player, "You brought back all three Silmarils from Morgoth's crown!", HIST_ESCAPE);
			break;
		}
		default: {
			history_add(player, "You brought back so many Silmarils that people should be suspicious!", HIST_ESCAPE);
		}
	}

	/* Cause of death */
	my_strcpy(player->died_from, "ripe old age", sizeof(player->died_from));
}

/**
 * Retire
 */
void do_cmd_retire(struct command *cmd)
{
	/* Treat retired character as dead to satisfy end of game logic. */
	player->is_dead = true;
	my_strcpy(player->died_from, "Retiring", sizeof(player->died_from));
}

/**
 * Record the player's thoughts as a note.
 *
 * This both displays the note back to the player and adds it to the game log.
 * Two fancy note types are supported: notes beginning with "/say" will be
 * written as 'Frodo says: "____"', and notes beginning with "/me" will
 * be written as 'Frodo ____'.
 */
void do_cmd_note(void)
{
	/* Allocate/Initialize strings to get and format user input. */
	char tmp[70];
	char note[90];
	my_strcpy(tmp, "", sizeof(tmp));
	my_strcpy(note, "", sizeof(note));

	/* Read a line of input from the user */
	if (!get_string("Note: ", tmp, sizeof(tmp))) return;

	/* Ignore empty notes */
	if (!tmp[0] || (tmp[0] == ' ')) return;

	/* Format the note correctly, supporting some cute /me commands */
	if (strncmp(tmp, "/say ", 5) == 0)
		strnfmt(note, sizeof(note), "-- %s says: \"%s\"", player->full_name,
				&tmp[5]);
	else if (strncmp(tmp, "/me", 3) == 0)
		strnfmt(note, sizeof(note), "-- %s%s", player->full_name, &tmp[3]);
	else
		strnfmt(note, sizeof(note), "-- Note: %s", tmp);

	/* Display the note (omitting the "-- " prefix) */
	msg("%s", &note[3]);

	/* Add a history entry */
	history_add(player, note, HIST_USER_INPUT);
}
