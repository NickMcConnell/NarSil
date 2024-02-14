/**
 * \file wiz-debug.c
 * \brief Implement miscellaneous debug mode functions
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
#include "cave.h"
#include "player-timed.h"
#include "player-util.h"
#include "wizard.h"


/**
 * What happens when you cheat death.  Tsk, tsk.
 */
void wiz_cheat_death(void)
{
	/* Mark social class, reset age, if needed */
	player->age = 1;
	player->noscore |= NOSCORE_WIZARD;

	player->is_dead = false;

	/* Restore hit & spell points */
	player->chp = player->mhp;
	player->csp = player->msp;

	/* Healing */
	(void)player_clear_timed(player, TMD_BLIND, true, false);
	(void)player_clear_timed(player, TMD_CONFUSED, true, false);
	(void)player_clear_timed(player, TMD_POISONED, true, false);
	(void)player_clear_timed(player, TMD_AFRAID, true, false);
	(void)player_clear_timed(player, TMD_ENTRANCED, true, false);
	(void)player_clear_timed(player, TMD_IMAGE, true, false);
	(void)player_clear_timed(player, TMD_STUN, true, false);
	(void)player_clear_timed(player, TMD_CUT, true, false);

	/* Prevent starvation */
	player_set_timed(player, TMD_FOOD, PY_FOOD_FULL - 1, false, false);

	/* Note cause of death XXX XXX XXX */
	my_strcpy(player->died_from, "Cheating death", sizeof(player->died_from));

	/* Back to the start */
	dungeon_change_level(player, 1);
}
