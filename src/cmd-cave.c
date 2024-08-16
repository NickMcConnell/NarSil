/**
 * \file cmd-cave.c
 * \brief Chest and door opening/closing, disarming, running, resting, &c.
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
#include "cmd-core.h"
#include "cmds.h"
#include "combat.h"
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-attack.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-move.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-chest.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-path.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "songs.h"
#include "trap.h"
#include "tutorial.h"

/**
 * Determines whether a staircase is 'trapped' like a false floor trap.
 * This means you fall a level below where you expected to end up (if you were
 * going upwards), take some minor damage, and have no stairs back.
 *
 * It gets more likely the more stairs you have recently taken.
 * It is designed to stop you stair-scumming.
 */
static bool trapped_stairs(void)
{
	int chance;
	
	chance = player->staircasiness / 100;
	chance = chance * chance * chance;	
	chance = chance / 10000;
	
	if (percent_chance(chance))	{
		msg("The stairs crumble beneath you!");
		event_signal(EVENT_MESSAGE_FLUSH);
		msg("You fall through...");
		event_signal(EVENT_MESSAGE_FLUSH);
		msg("...and land somewhere deeper in the Iron Hells.");
		event_signal(EVENT_MESSAGE_FLUSH);
		history_add(player, "Fell through a crumbling stair",
					HIST_TRAPPED_STAIRS);

		/* Take some damage */
		player_falling_damage(player, false);

		/* No stairs back */
		player->upkeep->create_stair = FEAT_NONE;

		return true;
	}

	return false;
}

/**
 * Go up one level
 */
static void do_cmd_go_up_aux(void)
{
	int new_depth, min;
	int change = square_isshaft(cave, player->grid) ? -2 : -1;

	/* Verify stairs */
	if (!square_isupstairs(cave, player->grid)) {
		msg("You see no up staircase here.");
		return;
	}

	/* Special handling for the tutorial */
	if (in_tutorial()) {
		player->upkeep->energy_use = z_info->move_energy;
		tutorial_leave_section(player);
		return;
	}

	/* Force descend */
	if (OPT(player, birth_force_descend) && (silmarils_possessed(player) == 0)){
		msg("You have vowed to not to return until you hold a Silmaril.");
		return;
	}
	
	/* Calculate the depth to aim for */
	new_depth = dungeon_get_next_level(player, player->depth, change);
	
	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;

	/* Cannot flee Morgoth's throne room without a Silmaril */
	if ((player->max_depth == z_info->dun_depth) &&
		(silmarils_possessed(player) == 0)) {
		msg("You enter a maze of staircases, but cannot find your way.");
		return;
	}

	/* Calculate the new depth to arrive at */
	min = player_min_depth(player);

	/* Create a way back */
	player->upkeep->create_stair = (change == -2) ? FEAT_MORE_SHAFT : FEAT_MORE;
	
	/* Deal with most cases where you can't find your way */
	if ((new_depth < min) && (player->max_depth != z_info->dun_depth)) {
		msgt(MSG_STAIRS_UP, "You enter a maze of up staircases, but cannot find your way.");

		/* Deal with trapped stairs when trying and failing to go upwards */
		if (!trapped_stairs()) {
			if (player->depth == min) {
				msgt(MSG_STAIRS_UP, "You emerge near where you began.");
			} else {
				msgt(MSG_STAIRS_UP, "You emerge even deeper in the dungeon.");
			}

			/* Change the way back */
			if (player->upkeep->create_stair == FEAT_MORE) {
				player->upkeep->create_stair = FEAT_LESS;
			} else {
				player->upkeep->create_stair = FEAT_LESS_SHAFT;
			}
		}

		new_depth = min;
	} else {
		/* Deal with cases where you can find your way */
		msgt(MSG_STAIRS_UP, "You enter a maze of up staircases.");

		/* Escaping */
		if (silmarils_possessed(player) > 0) {
			msgt(MSG_STAIRS_UP, "The divine light reveals the way.");
		}

		/* Flee Morgoth's throne room */
		if (player->depth == z_info->dun_depth) {
			if (!player->morgoth_slain) {
				msg("As you climb the stair, a great cry of rage and anguish comes from below.");
				msg("Make quick your escape: it shall be hard-won.");
			}

			/* Set the 'on the run' flag */
			player->on_the_run = true;

			/* Remove the 'truce' flag if it hasn't been done already */
			player->truce = false;
		} else if (trapped_stairs()) {
			/* Deal with trapped stairs when going upwards */
			new_depth++;
		}
	}

	/* Another staircase has been used... */
	player->stairs_taken++;
	player->staircasiness += 1000;

	if (OPT(player, birth_discon_stairs)) {
		player->upkeep->create_stair = FEAT_NONE;
	}
	
	/* Change level */
	dungeon_change_level(player, new_depth);
}


/**
 * Go up one level
 */
void do_cmd_go_up(struct command *cmd)
{
	do_cmd_go_up_aux();
}

/**
 * Go down one level
 */
static void do_cmd_go_down_aux(void)
{
	int new_depth, min;
	int change = square_isshaft(cave, player->grid) ? 2 : 1;

	/* Verify stairs */
	if (!square_isdownstairs(cave, player->grid)) {
		msg("I see no down staircase here.");
		return;
	}

	/* Special handling for the tutorial */
	if (in_tutorial()) {
		player->upkeep->energy_use = z_info->move_energy;
		tutorial_leave_section(player);
		return;
	}

	/* Do not descend from the Gates */
	if (player->depth == 0) {
		msg("You have made it to the very gates of Angband and can once more taste the freshness on the air.");
		msg("You will not re-enter that fell pit.");
		return;
	}

	/* Calculate the depth to aim for */
	new_depth = dungeon_get_next_level(player, player->depth, change);
	
	/* Calculate the new depth to arrive at */
	min = player_min_depth(player);

	/* Create a way back */
	player->upkeep->create_stair = (change == 2) ? FEAT_LESS_SHAFT : FEAT_LESS;
	
	/* Warn players if this could lead them to Morgoth's Throne Room */
	if (new_depth == z_info->dun_depth) {
		if (!player->on_the_run) {
			msg("From up this stair comes the harsh din of feasting in Morgoth's own hall.");
			if (!get_check("Are you completely sure you wish to descend? ")) {
				player->upkeep->create_stair = FEAT_NONE;
				return;
			}
		}
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;

	/* Success */
	msgt(MSG_STAIRS_DOWN, "You enter a maze of down staircases.");

	/* Can never return to the throne room... */
	if ((player->on_the_run) && (new_depth == z_info->dun_depth)) {
		msgt(MSG_STAIRS_DOWN, "Try though you might, you cannot find your way back to Morgoth's throne.");
		msgt(MSG_STAIRS_DOWN, "You emerge near where you began.");
		player->upkeep->create_stair = FEAT_MORE;
		new_depth = z_info->dun_depth - 1;
	} else if (!trapped_stairs() && (new_depth < min)) {
		msgt(MSG_STAIRS_DOWN, "You emerge much deeper in the dungeon.");
		new_depth = min;
	}

	/* Another staircase has been used... */
	player->stairs_taken++;
	player->staircasiness += 1000;

	if (OPT(player, birth_discon_stairs)) {
		player->upkeep->create_stair = FEAT_NONE;
	}
	
	/* Change level */
	dungeon_change_level(player, new_depth);
}


/**
 * Go down one level
 */
void do_cmd_go_down(struct command *cmd)
{
	do_cmd_go_down_aux();
}

/**
 * Toggle stealth mode
 */
void do_cmd_toggle_stealth(struct command *cmd)
{
	/* Toggle stealth mode */
	if (player->stealth_mode) {
		/* Clear the stealth mode flag */
		player->stealth_mode = STEALTH_MODE_OFF;

		/* Recalculate bonuses */
		player->upkeep->update |= (PU_BONUS);

		/* Redraw the state */
		player->upkeep->redraw |= (PR_STATE);
	} else {
		/* Set the stealth mode flag */
		player->stealth_mode = STEALTH_MODE_ON;

		/* Update stuff */
		player->upkeep->update |= (PU_BONUS);

		/* Redraw stuff */
		player->upkeep->redraw |= (PR_STATE | PR_SPEED);
	}
}

/**
 * Determine if a given grid may be "opened"
 */
static bool do_cmd_open_test(struct loc grid)
{
	/* Must have knowledge */
	if (!square_isknown(cave, grid)) {
		msg("You see nothing there.");
		return false;
	}

	/* Must be a closed door */
	if (!square_iscloseddoor(cave, grid) || square_issecretdoor(cave, grid)) {
		msgt(MSG_NOTHING_TO_OPEN, "You see nothing there to open.");
		return false;
	}

	return (true);
}


/**
 * Perform the basic "open" command on doors
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_open_aux(struct loc grid)
{
	int score, power, difficulty;
	bool more = false;

	/* Verify legality */
	if (!do_cmd_open_test(grid)) return (false);

	/* Check the type of door */
	if (square_isjammeddoor(cave, grid)) {
		/* Stuck */
		msg("The door appears to be stuck.");
	} else if (square_islockeddoor(cave, grid)) {
		/* Get the score in favour (=perception) */
		score = player->state.skill_use[SKILL_PERCEPTION];

		/* Extract the lock power */
		power = square_door_lock_power(cave, grid);

		/* Base difficulty is the door power + 5 */
		difficulty = power + 5;

		/* Penalize some conditions */
		if (player->timed[TMD_BLIND] || no_light(player) ||
			player->timed[TMD_IMAGE]) {
			difficulty += 5;
		}
		if (player->timed[TMD_CONFUSED]) difficulty += 5;

		/* Success */
		if (skill_check(source_player(), score, difficulty, source_none()) > 0){
			/* Message */
			msgt(MSG_LOCKPICK, "You have picked the lock.");

			/* Open the door */
			square_open_door(cave, grid);

			/* Update the visuals */
			player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		} else {
			/* Failure */
			event_signal(EVENT_INPUT_FLUSH);

			/* Message */
			msgt(MSG_LOCKPICK_FAIL, "You failed to pick the lock.");

			/* We may keep trying */
			more = true;
		}
	} else {
		/* Closed door */
		square_open_door(cave, grid);
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		sound(MSG_OPENDOOR);
	}

	/* Result */
	return more;
}



/**
 * Open a closed/locked/jammed door or a closed/locked chest.
 *
 * Unlocking a locked chest is worth one experience point; since doors are
 * player lockable, there is no experience for unlocking doors.
 */
void do_cmd_open(struct command *cmd)
{
	struct loc grid;
	int dir;
	struct object *obj;
	bool more = false;
	int err;
	struct monster *mon;

	/* Get arguments */
	err = cmd_get_arg_direction(cmd, "direction", &dir);
	if (err || dir == DIR_UNKNOWN) {
		struct loc grid1;
		int n_closed_doors, n_locked_chests;

		/* square_iscloseddoor() also includes secret doors. */
		n_closed_doors = count_feats(&grid1, square_iscloseddoor, false)
			- count_feats(&grid1, square_issecretdoor, false);
		assert(n_closed_doors >= 0);
		n_locked_chests = count_chests(&grid1, CHEST_OPENABLE);

		if (n_closed_doors + n_locked_chests == 1) {
			dir = motion_dir(player->grid, grid1);
			cmd_set_arg_direction(cmd, "direction", dir);
		} else if (cmd_get_direction(cmd, "direction", &dir, false)) {
			return;
		}
	}

	/* Get location */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Check for chest */
	obj = chest_check(player, grid, CHEST_OPENABLE);

	/* Check for door */
	if (!obj && !do_cmd_open_test(grid)) {
		msg("There is nothing in your square (or adjacent) to open.");
		/* Cancel repeat */
		disturb(player, false);
		return;
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->grid, ddgrid[dir]);

		/* Check for chest */
		obj = chest_check(player, grid, CHEST_OPENABLE);
	}

	/* Monster */
	mon = square_monster(cave, grid);
	if (mon) {
		/* Message */
		msg("There is a monster in the way!");

		/* Attack */
		py_attack(player, grid, ATT_MAIN);
	} else if (obj) {
		/* Chest */
		more = do_cmd_open_chest(grid, obj);
	} else {
		/* Door */
		more = do_cmd_open_aux(grid);
	}

	/* Cancel repeat unless we may continue */
	if (!more) disturb(player, false);
}


/**
 * Determine if a given grid may be "closed"
 */
static bool do_cmd_close_test(struct loc grid)
{
	/* Must have knowledge */
	if (!square_isknown(cave, grid)) {
		/* Message */
		msg("You see nothing there.");

		/* Nope */
		return (false);
	}

 	/* Require open/broken door */
	if (!square_isopendoor(cave, grid) && !square_isbrokendoor(cave, grid)) {
		/* Message */
		msg("You see nothing there to close.");

		/* Nope */
		return (false);
	}

	/* Dont allow if player is in the way. */
	if (square(cave, grid)->mon < 0) {
		/* Message */
		msg("To close the door you would need to move out from the doorway.");

		/* Nope */
		return (false);
	}

	/* Okay */
	return (true);
}


/**
 * Perform the basic "close" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_close_aux(struct loc grid)
{
	bool more = false;

	/* Verify legality */
	if (!do_cmd_close_test(grid)) return (false);

	/* Broken door */
	if (square_isbrokendoor(cave, grid)) {
		msg("The door appears to be broken.");
	} else {
		/* Close door */
		square_close_door(cave, grid);
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		sound(MSG_SHUTDOOR);
	}

	/* Result */
	return (more);
}


/**
 * Close an open door.
 */
void do_cmd_close(struct command *cmd)
{
	struct loc grid;
	int dir;
	int err;

	bool more = false;

	/* Get arguments */
	err = cmd_get_arg_direction(cmd, "direction", &dir);
	if (err || dir == DIR_UNKNOWN) {
		struct loc grid1;

		/* Count open doors */
		if (count_feats(&grid1, square_isopendoor, false) == 1) {
			dir = motion_dir(player->grid, grid1);
			cmd_set_arg_direction(cmd, "direction", dir);
		} else if (cmd_get_direction(cmd, "direction", &dir, false)) {
			return;
		}
	}

	/* Get location */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Verify legality */
	if (!do_cmd_close_test(grid)) {
		/* Cancel repeat */
		disturb(player, false);
		return;
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->grid, ddgrid[dir]);
	}

	/* Monster - alert, then attack */
	if (square(cave, grid)->mon > 0) {
		msg("There is a monster in the way!");
		py_attack(player, grid, ATT_MAIN);
	} else
		/* Door - close it */
		more = do_cmd_close_aux(grid);

	/* Cancel repeat unless told not to */
	if (!more) disturb(player, false);
}


/**
 * Exchange places with a monster.
 */
void do_cmd_exchange(struct command *cmd)
{
	int dir;
	struct loc grid;
	struct monster *mon;
	char m_name[80];

	if (!player_active_ability(player, "Exchange Places")) {
		msg("You need the ability 'exchange places' to use this command.");
		return;
	}

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	/* Get location */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Deal with overburdened characters */
	if (player->upkeep->total_weight > weight_limit(player->state) * 3 / 2) {
		/* Abort */
		msg("You are too burdened to move.");
		return;
	}

	/* Check terrain, traps, monsters */
	mon = square_monster(cave, grid);
	if (square_ispit(cave, player->grid)) {
		/* Can't exchange from within pits */
		msg("You would have to escape the pit before being able to exchange places.");
		return;
	} else if (square_iswebbed(cave, player->grid)) {
		/* Can't exchange from within webs */
		msg("You would have to escape the web before being able to exchange places.");
		return;
	} else if (!mon || !monster_is_visible(mon)) {
		/* No monster, or invisible */
		msg("You cannot see a monster there to exchange places with.");
		return;
	} else if (square_isrubble(cave, grid)) {
		/* Rubble */
		msg("You cannot enter the rubble.");
		return;
	} else if (square_isrock(cave, grid)) {
		/* Wall */
		msg("You cannot enter the wall.");
		return;
	} else if (square_iscloseddoor(cave, grid)) {
		/* Closed door */
		msg("You cannot enter the closed door.");
		return;
	} else {
		if (rf_has(mon->race->flags, RF_NEVER_MOVE) ||
			rf_has(mon->race->flags, RF_HIDDEN_MOVE)) {
			monster_desc(m_name, sizeof(m_name), mon, MDESC_DEFAULT);

			/* Message */
			msg("You cannot get past %s.", m_name);
			return;
		}
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->grid, ddgrid[dir]);
	}

	/* Re-check for a visible monster (in case confusion changed the move) */
	mon = square_monster(cave, grid);
	if (!mon || !monster_is_visible(mon)) {
		/* Message */
		msg("You cannot see a monster there to exchange places with.");
		return;
	} else if (square_isrubble(cave, grid)) {
		/* Rubble */
		msg("There is a pile of rubble in the way.");
		return;
	} else if (square_isrock(cave, grid)) {
		/* Wall */
		msg("There is a wall in the way.");
		return;
	} else if (square_iscloseddoor(cave, grid)) {
		/* Closed door */
		msg("There is a door in the way.");
		return;
	} else if (square_ischasm(cave, grid)) {
		/* Chasm */
		msg("You cannot exchange places over the chasm.");
		return;
	}
	
	/* Recalculate the monster name (in case confusion changed the move) */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_DEFAULT);

	/* Message */
	msg("You exchange places with %s.", m_name);

	/* Attack of opportunity */
	if ((mon->alertness >= ALERTNESS_ALERT) && !mon->m_timed[MON_TMD_CONF] &&
		!rf_has(mon->race->flags, RF_MINDLESS)) {
		char m_pronoun[80];

		/* Calculate pronoun for the first time */
		monster_desc(m_pronoun, sizeof(m_pronoun), mon,
					 MDESC_PRO_VIS | MDESC_CAPITAL);

		msg("%s attacks you as you slip past.", m_pronoun);
		make_attack_normal(mon, player);
	}

	/* Alert the monster */
	make_alert(mon, 0);

	/* Swap positions with the monster */
	monster_swap(player->grid, grid);

	/* Set off traps */
	if (square_isplayertrap(cave, grid)) {
		/* Hit the trap */
		square_reveal_trap(cave, grid, true);
		hit_trap(grid);
	} else if (square_ischasm(cave, grid)) {
		player_fall_in_chasm(player);
	}
}


/**
 * Determine if a given grid may be "tunneled"
 */
static bool do_cmd_tunnel_test(struct loc grid)
{

	/* Must have knowledge */
	if (!square_isknown(cave, grid)) {
		msg("You see nothing there.");
		return (false);
	}

	/* Titanium */
	if (square_isperm(cave, grid)) {
		msg("You cannot tunnel any further in that direction.");
		return (false);
	}

	/* Must be a wall/etc */
	if (!square_isdiggable(cave, grid)) {
		/* Doors get a more informative message. */
		if (square_iscloseddoor(cave, grid)) {
			msg("You cannot tunnel through a door. Try bashing it.");
		} else {
			msg("You see nothing there to tunnel.");
		}
		return (false);
	}

	/* Okay */
	return (true);
}


/**
 * Tunnel through wall.  Assumes valid location.
 *
 * Note that it is impossible to "extend" rooms past their
 * outer walls (which are actually part of the room).
 *
 * Attempting to do so will produce floor grids which are not part
 * of the room, and whose "illumination" status do not change with
 * the rest of the room.
 */
static bool twall(struct loc grid)
{
	/* Paranoia -- Require a wall or some such */
	if (!square_isdiggable(cave, grid))
		return (false);

	/* Sound */
	sound(MSG_DIG);

	/* Remove the feature */
	square_tunnel_wall(cave, grid);

	/* Update the visuals */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Result */
	return (true);
}


/**
 * Print a message when the player doesn't have the required digger for terrain.
 */
static void fail_message(struct feature *terrain, char *name)
{
	char buf[1024] = "\0";

	/* See if we have a message */
	if (!terrain->fail_msg) return;

	/* Insert */
	insert_name(buf, 1024, terrain->fail_msg, name);
	msg("%s", buf);
}

/**
 * Perform the basic "tunnel" command
 *
 * Assumes that no monster is blocking the destination.
 * Uses twall() (above) to do all "terrain feature changing".
 * Returns true if repeated commands may continue.
 */
static bool do_cmd_tunnel_aux(struct loc grid)
{
	bool more = false;
	int weapon_slot = slot_by_name(player, "weapon");
	struct object *current_weapon = slot_object(player, weapon_slot);
	struct object *digger = NULL;
	int digging_score = 0;
	int difficulty = square_digging(cave, grid);
    char o_name[80];

	/* Verify legality */
	if (!do_cmd_tunnel_test(grid)) return (false);

	/* Pick what we're digging with and our chance of success */
	if (obj_digging_score(current_weapon)) {
		/* If weapon is a digger, then use it */
		digging_score = obj_digging_score(current_weapon);
		digger = current_weapon;
	} else {
        /* Find one or more diggers in the pack */
		bool more_than_one = false;
		struct object *test;
		for (test = player->gear; test; test = test->next) {
			if (obj_digging_score(test)) {
				if (digging_score) {
					more_than_one = true;
				}
				digging_score = obj_digging_score(test);
				digger = test;
			}
		}

		/* Make a choice if needed */
		if (more_than_one) {
			/* Get arguments */
			if (!get_item(&digger, "Use which digger?",
						  "You are not carrying a shovel or mattock.",
						  CMD_TUNNEL, obj_can_dig, USE_INVEN))
				return false;
			digging_score = obj_digging_score(digger);
		}
	}

	/* Abort if you have no digger */
    if (digging_score == 0) {
        /* Confused players trying to dig without a digger waste their turn
		 * (otherwise control-dir is safe in a corridor) */
        if (player->timed[TMD_CONFUSED]) {
            if (square_isrubble(cave, grid)) {
				msg("You bump into the rubble.");
            } else {
				msg("You bump into the wall.");
            }
            return false;
        } else {
            msg("You are not carrying a shovel or mattock.");

            /* Reset the action type */
            player->previous_action[0] = ACTION_NOTHING;

            /* Don't take a turn */
            player->upkeep->energy_use = 0;

            return false;
        }
    }

    /* Get the short name of the item */
	object_desc(o_name, sizeof(o_name), digger, ODESC_BASE, player);

	/* Test for success */
	if (difficulty > digging_score) {
		fail_message(square_feat(cave, grid), o_name);

        /* Confused players trying to dig without a digger waste their turn
		 * (otherwise control-dir is safe in a corridor) */
        if (!player->timed[TMD_CONFUSED]) {

            /* Reset the action type */
            player->previous_action[0] = ACTION_NOTHING;

            /* Don't take a turn */
            player->upkeep->energy_use = 0;

            return false;
        }
	} else if (difficulty > player->state.stat_use[STAT_STR]) {
		msg(square_feat(cave, grid)->str_msg);

        /* Confused players trying to dig without a digger waste their turn
		 * (otherwise control-dir is safe in a corridor) */
        if (!player->timed[TMD_CONFUSED]) {

            /* Reset the action type */
            player->previous_action[0] = ACTION_NOTHING;

            /* Don't take a turn */
            player->upkeep->energy_use = 0;

            return false;
        }
	} else {
		/* Make a lot of noise */
		monsters_hear(true, false, -10);

		/* Success */
		msg(square_feat(cave, grid)->dig_msg);
		twall(grid);

		/* Possibly identify the digger */
		if (!object_is_known(digger) && digger->modifiers[OBJ_MOD_TUNNEL]) { 
            char o_short_name[80];
            char o_full_name[80];

            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), digger,
						ODESC_BASE, player);

            ident(digger);

            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), digger, ODESC_FULL,
				player);

            /* Print the messages */
            msg("You notice that your %s is especially suited to tunneling.", o_short_name);
            msg("You are wielding %s.", o_full_name);
		}
	}

	/* Break the truce if creatures see */
	break_truce(player, false);

    /* Provoke attacks of opportunity from adjacent monsters */
    attacks_of_opportunity(player, loc(0, 0));

	/* Result */
	return more;
}


/**
 * Tunnel through "walls" (including rubble and doors, secret or otherwise)
 *
 * Digging is very difficult without a "digger" weapon, but can be
 * accomplished by strong players using heavy weapons.
 */
void do_cmd_tunnel(struct command *cmd)
{
	struct loc grid;
	int dir;
	bool more = false;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false))
		return;

	/* Get location */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Oops */
	if (!do_cmd_tunnel_test(grid)) {
		/* Cancel repeat */
		disturb(player, false);
		return;
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->grid, ddgrid[dir]);
	}

	/* Attack any monster we run into */
	if (square(cave, grid)->mon > 0) {
		msg("There is a monster in the way!");
		py_attack(player, grid, ATT_MAIN);
	} else {
		/* Tunnel through walls */
		more = do_cmd_tunnel_aux(grid);
	}

	/* Cancel repetition unless we can continue */
	if (!more) disturb(player, false);
}

/**
 * Determine if a given grid may be "disarmed"
 */
static bool do_cmd_disarm_test(struct loc grid)
{
	/* Must have knowledge */
	if (!square_isknown(cave, grid)) {
		msg("You see nothing there.");
		return false;
	}

	/* Look for a trap or glyph of warding */
	if (!square_isdisarmabletrap(cave, grid) && !square_iswarded(cave, grid)) {
		msg("You see nothing there to disarm.");
		return false;
	}

	/* Okay */
	return true;
}


/**
 * Perform the basic "disarm" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_disarm_aux(struct loc grid)
{
	int skill, power, difficulty, result;
    struct trap *trap = square(cave, grid)->trap;
	bool more = false;

	/* Verify legality */
	if (!do_cmd_disarm_test(grid)) return (false);

    /* Choose first player trap or glyph */
	while (trap) {
		if (trf_has(trap->flags, TRF_TRAP))
			break;
		if (trf_has(trap->flags, TRF_GLYPH))
			break;
		trap = trap->next;
	}
	if (!trap)
		return false;

	/* Get the base disarming skill */
	skill = player->state.skill_use[SKILL_PERCEPTION];

	/* Special case: player is stuck in a web */
	if (square_iswebbed(cave, grid) && loc_eq(grid, player->grid)) {
		more = player_break_web(player);
		return !more;
	}

	/* Determine trap power; (uint8_t)-1 is undisarmable */
	power = trap->power;
	if (power == (uint8_t)-1) {
		msg("You cannot disarm the %s.", trap->kind->name);
		return false;
	}

	/* Base difficulty is the trap power */
	difficulty = power;

	/* Penalize some conditions */
	if (player->timed[TMD_BLIND] ||	no_light(player) ||
		player->timed[TMD_IMAGE])
		difficulty += 5;
	if (player->timed[TMD_CONFUSED])
		difficulty += 5;

	/* Perform the check */
	result = skill_check(source_player(), skill, difficulty, source_none());
	if (result > 0) {
		/* Success, always succeed with player trap */
		if (trf_has(trap->flags, TRF_GLYPH)) {
			msgt(MSG_DISARM, "You have scuffed the %s.", trap->kind->name);
		} else {
			msgt(MSG_DISARM, "You have disarmed the %s.", trap->kind->name);
		}

		/* Trap is gone */
		square_destroy_trap(cave, grid);
		square_unmark(cave, grid);
	} else if (result > -3) {
		/* Failure by a small amount allows one to keep trying */
		event_signal(EVENT_INPUT_FLUSH);
		msg("You failed to disarm the %s.", trap->kind->name);

		/* Player can try again */
		more = true;
	} else {
		/* Failure by a larger amount sets off the trap */
		monster_swap(player->grid, grid);
		msg("You set off the %s!", trap->kind->name);
		hit_trap(grid);
	}

	/* Result */
	return more;
}


/**
 * Disarms a trap, or a chest
 *
 * Traps must be visible, chests must be known trapped
 */
void do_cmd_disarm(struct command *cmd)
{
	struct loc grid;
	int dir;
	int err;

	struct object *obj;
	bool more = false;

	/* Get arguments */
	err = cmd_get_arg_direction(cmd, "direction", &dir);
	if (err || dir == DIR_UNKNOWN) {
		struct loc grid1;
		int n_traps, n_wards, n_chests;

		n_traps = count_feats(&grid1, square_isdisarmabletrap, true);
		n_wards = count_feats(&grid1, square_iswarded, true);
		n_chests = count_chests(&grid1, CHEST_TRAPPED);

		if (n_traps + n_wards + n_chests == 1) {
			dir = motion_dir(player->grid, grid1);
			cmd_set_arg_direction(cmd, "direction", dir);
		} else if (cmd_get_direction(cmd, "direction", &dir, true)) {
			return;
		}
	}

	/* Get location */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Check for chests */
	obj = chest_check(player, grid, CHEST_TRAPPED);

	/* Verify legality */
	if (!obj && !do_cmd_disarm_test(grid)) {
		/* Cancel repeat */
		disturb(player, false);
		return;
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->grid, ddgrid[dir]);

		/* Check for chests */
		obj = chest_check(player, grid, CHEST_TRAPPED);
	}


	/* Monster */
	if (square(cave, grid)->mon > 0) {
		msg("There is a monster in the way!");
		py_attack(player, grid, ATT_MAIN);
	} else if (obj) {
		/* Chest */
		more = do_cmd_disarm_chest(obj);
	} else {
		/* Disarm trap */
		more = do_cmd_disarm_aux(grid);
	}

	/* Cancel repeat unless told not to */
	if (!more) disturb(player, false);
}

/**
 * Determine if a given grid may be "bashed"
 */
static bool do_cmd_bash_test(struct loc grid)
{
	/* Must have knowledge */
	if (!square_ismark(cave, grid)) {
		/* Message */
		msg("You see nothing there.");

		/* Nope */
		return false;
	}

	/* Require a door */
	if (!square_iscloseddoor(cave, grid) || square_issecretdoor(cave, grid)) {
		/* Message */
		msg("You see no door there to bash.");

		/* Nope */
		return false;
	}

	/* Okay */
	return true;
}

/**
 * Perform the basic "bash" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_bash_aux(struct loc grid)
{
	int score, difficulty;
	bool more = false;
	bool success = false;

	/* Verify legality */
	if (!do_cmd_bash_test(grid)) return false;

	/* Get the score in favour (=str) */
	score = player->state.stat_use[STAT_STR] * 2;

	/* The base difficulty is the door power  */
	difficulty = square_door_jam_power(cave, grid);

	/* Message */
	msg("You slam into the door!");

	if (skill_check(source_player(), score, difficulty, source_none()) > 0) {
		success = true;
		if (player_is_singing(player, lookup_song("Silence"))) {
			/* Message */
			msgt(MSG_OPENDOOR, "The door opens with a muffled crash!");
		} else {
			/* Message */
			msgt(MSG_OPENDOOR, "The door crashes open!");
		}

		if (one_in_(2)) {
			/* Break down the door */
			square_set_feat(cave, grid, FEAT_BROKEN);
		} else {
			/* Open the door */
			square_set_feat(cave, grid, FEAT_OPEN);
		}

		/* Move the player onto the door square */
		monster_swap(player->grid, grid);

		/* Make a lot of noise */
		monsters_hear(true, false, -10);

		/* Update the visuals */
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	}

	if (!success) {
		int old_stun = player->timed[TMD_STUN];
		if (square_iscloseddoor(cave, grid)) {
			/* Message */
			msg("The door holds firm.");
		}

		/* Stuns */
		(void)player_inc_timed(player, TMD_STUN, 10, true, true);
		if (player->timed[TMD_STUN] > old_stun) {
			/* Allow repeated bashing */
			more = true;
		}

		/* Make some noise */
		monsters_hear(true, false, -5);
	}

	/* Result */
	return more;
}


/**
 * Bash open a door, success based on character strength
 *
 * For a closed door, pval is positive if locked; negative if stuck.
 *
 * For an open door, pval is positive for a broken door.
 *
 * A closed door can be opened - harder if locked. Any door might be
 * bashed open (and thereby broken). Bashing a door is (potentially)
 * faster! You move into the door way. To open a stuck door, it must
 * be bashed.
 *
 * Creatures can also open or bash doors, see elsewhere.
 */
void do_cmd_bash(struct command *cmd)
{
	int dir;
	struct loc grid;
	struct monster *mon;
	bool more = false;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	/* Get location */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Verify legality */
	if (!do_cmd_bash_test(grid)) return;

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->grid, ddgrid[dir]);
	}

	/* Monster */
	mon = square_monster(cave, grid);
	if (mon) {
		/* Message */
		msg("There is a monster in the way!");

		/* Attack */
		py_attack(player, grid, ATT_MAIN);
	} else {
		/* Door */
		more = do_cmd_bash_aux(grid);
	}

	/* Cancel repeat unless we may continue */
	if (!more) disturb(player, false);
}


/**
 * Manipulate an adjacent grid in some way
 *
 * Attack monsters, tunnel through walls, disarm traps, bash doors.
 *
 * This command must always take energy, to prevent free detection
 * of invisible monsters.
 *
 * The "semantics" of this command must be chosen before the player
 * is confused, and it must be verified against the new grid.
 */
static void do_cmd_alter_aux(int dir)
{
	struct loc grid;
	bool more = false;
	struct object *o_chest_closed;
	struct object *o_chest_trapped;
	struct object *obj = NULL;

	/* Get location */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->grid, ddgrid[dir]);
	}

	/* Check for closed chest */
	o_chest_closed = chest_check(player, grid, CHEST_OPENABLE);
	/* Check for trapped chest */
	o_chest_trapped = chest_check(player, grid, CHEST_TRAPPED);
	/* Check for any object */
	obj = square_object(cave, grid);

	/* Action depends on what's there */
	if (square(cave, grid)->mon > 0) {
		/* Attack monster */
		py_attack(player, grid, ATT_MAIN);
	} else if ((dir != DIR_NONE) && !square_ismark(cave, grid)) {
		/* Deal with players who can't see the square */
		if (square_isfloor(cave, grid)) {
			msg("You strike, but there is nothing there.");
		} else {
			msg("You hit something hard.");
			square_mark(cave, grid);
			square_light_spot(cave, grid);
		}
	} else if (square_isrock(cave, grid)) {
		/* Tunnel through walls and rubble */
		more = do_cmd_tunnel_aux(grid);
	} else if (square_iscloseddoor(cave, grid)) {
		/* Bash closed doors */
		more = do_cmd_bash_aux(grid);
	} else if (square_isdisarmabletrap(cave, grid) || square_iswarded(cave, grid)) {
		/* Disarm traps */
		more = do_cmd_disarm_aux(grid);
	} else if (o_chest_trapped) {
		/* Trapped chest */
		more = do_cmd_disarm_chest(o_chest_trapped);
	} else if (o_chest_closed) {
		/* Open chest */
		more = do_cmd_open_chest(grid, o_chest_closed);
	} else if (square_isopendoor(cave, grid)) {
		/* Close door */
		more = do_cmd_close_aux(grid);
	} else if ((dir == DIR_NONE) && square_isupstairs(cave, grid)) {
		/* Ascend */
		if (get_check("Are you sure you wish to ascend? ")) {
			do_cmd_go_up_aux();
		}
	} else if ((dir == DIR_NONE) && square_isdownstairs(cave, grid)) {
		/* Descend */
		if (get_check("Are you sure you wish to descend? ")) {
			do_cmd_go_down_aux();
		}
	} else if ((dir == DIR_NONE) && square_isforge(cave, grid)) {
		/* Cancel the alter command */
		cmd_cancel_repeat();

		/* Use forge */
		do_cmd_smith_aux(true);
		more = true;

		/* Don't take a turn... */
		player->upkeep->energy_use = 0;
	} else if ((dir == DIR_NONE) && obj) {
		/* Pick up items */
		player_pickup_item(player, obj, true);
	} else if (dir == DIR_NONE) {
		/* Oops */
		msg("There is nothing here to use.");

		/* Don't take a turn... */
		player->upkeep->energy_use = 0;
	} else {
		/* Oops */
		msg("You strike, but there is nothing there.");
	}

	/* Cancel repetition unless we can continue */
	if (!more) disturb(player, false);
}

void do_cmd_alter(struct command *cmd)
{
	int dir;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, true) != CMD_OK)
		return;

	do_cmd_alter_aux(dir);
}

/**
 * Confirm a player wants to leap if necessary
 */
static bool confirm_leap(struct loc grid, int dir)
{
	bool confirm = true;
	char prompt[80];
	struct loc end = loc_sum(loc_sum(player->grid, ddgrid[dir]), ddgrid[dir]);
	struct monster *mon = square_monster(cave, end);

	/* Prompt for confirmation */
	if (!(square_isseen(cave, end) || square_ismark(cave, end))) {
		/* Confirm if the destination is unknown */
		strnfmt(prompt, sizeof(prompt),
				"Are you sure you wish to leap into the unknown? ");
	} else if (square_ischasm(cave, end)) {
		/* Confirm if the destination is in the chasm */
		strnfmt(prompt, sizeof(prompt),
				"Are you sure you wish to leap into the abyss? ");
	} else if (mon && monster_is_visible(mon)) {
		/* Confirm if the destination has a visible monster */
		char m_name[80];

		/* Get the monster name */
		monster_desc(m_name, sizeof(m_name), mon, 0);
		strnfmt(prompt, sizeof(prompt),
				"Are you sure you wish to leap into %s? ", m_name);
	} else {
		/* No confirmation needed */
		confirm = false;
	}

	/* True if no need to confirm or confirmed, otherwise false */
	return (!confirm || get_check(prompt));
}

/**
 * Finish player leap
 */
static void player_land(struct player *p)
{
    /* Make some noise when landing */
    p->stealth_score -= 5;

	/* Set off traps */
	if (square_issecrettrap(cave, p->grid)) {
		disturb(player, false);
		square_reveal_trap(cave, p->grid, true);
		hit_trap(p->grid);
	} else if (square_isdisarmabletrap(cave, p->grid)) {
		disturb(player, false);
		hit_trap(p->grid);
	}

	/* Fall into chasms */
	if (square_ischasm(cave, p->grid)) {
		player_fall_in_chasm(p);
	}
}

/**
 * Continue player leap
 */
void do_cmd_leap(struct command *cmd)
{
    int dir = player->previous_action[1];
	struct loc end = loc_sum(player->grid, ddgrid[dir]);
	struct monster *mon = square_monster(cave, end);

	/* Knocked back player is handled separately */
	if (player->upkeep->knocked_back) return;

    /* Display a message until player input is received */
    msg("You fly through the air.");

	/* Flush messages */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = dir;

    /* Solid objects end the leap */
	if (!square_ispassable(cave, end)) {
		if (square_isrubble(cave, end)) {
			msgt(MSG_HITWALL, "You slam into a wall of rubble.");
		} else if (square_isrock(cave, end)) {
			msgt(MSG_HITWALL, "You slam into a wall.");
		} else {
			msgt(MSG_HITWALL, "You slam into a door.");
		}
    } else if (mon) {
		/* Monsters end the leap */
        char m_name[80];

        /* Get the monster name */
        monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);

        if (monster_is_visible(mon)) {
			msg("%s blocks your landing.", m_name);
        } else {
            msg("Some unseen foe blocks your landing.");
		}
	} else {
		/* Successful leap */
        /* We generously give you your free flanking attack... */
        player_flanking_or_retreat(player, end);

        /* Move player to the new position */
        monster_swap(player->grid, end);
    }

    /* Land on the ground */
    player_land(player);
}

/**
 * Move player in the given direction.
 *
 * This routine should only be called when energy has been expended.
 *
 * Note that this routine handles monsters in the destination grid,
 * and also handles attempting to move into walls/doors/rubble/etc.
 */
void move_player(int dir, bool disarm)
{
	struct loc grid = loc_sum(player->grid, ddgrid[dir]);

	int m_idx = square(cave, grid)->mon;
	struct monster *mon = cave_monster(cave, m_idx);
	bool trap = square_isdisarmabletrap(cave, grid);
	bool door = square_iscloseddoor(cave, grid) &&
		!square_issecretdoor(cave, grid);
	bool confused = player->timed[TMD_CONFUSED] > 0;

	/* Many things can happen on movement */
	if (mon && monster_is_visible(mon)) {
		/* Attack visible monsters */
		py_attack(player, grid, ATT_MAIN);
	} else if (((trap && disarm) || door) && square_isknown(cave, grid)) {
		/* Auto-repeat if not already repeating */
		if (cmd_get_nrepeats() == 0)
			cmd_set_repeat(99);
		if (door) {
			if (!do_cmd_open_aux(grid)) {
				disturb(player, false);
			}
		} else {
			do_cmd_alter_aux(dir);
		}
	} else if (trap && player->upkeep->running) {
		/* Stop running before known traps */
		disturb(player, false);

		/* Don't take a turn... */
		player->upkeep->energy_use = 0;
	} else if (!square_ispassable(cave, grid)) {
		disturb(player, false);

		/* Notice unknown obstacles, mention known obstacles */
		if (!square_isknown(cave, grid)) {
			if (square_isrubble(cave, grid)) {
				msgt(MSG_HITWALL,
					 "You feel a pile of rubble blocking your way.");
				square_mark(cave, grid);
				square_light_spot(cave, grid);
			} else if (door) {
				msgt(MSG_HITWALL, "You feel a door blocking your way.");
				square_mark(cave, grid);
				square_light_spot(cave, grid);
			} else {
				msgt(MSG_HITWALL, "You feel a wall blocking your way.");
				square_mark(cave, grid);
				square_light_spot(cave, grid);
			}
		} else {
			if (square_isrubble(cave, grid))
				msgt(MSG_HITWALL,
					 "There is a pile of rubble blocking your way.");
			else if (door)
				msgt(MSG_HITWALL, "There is a door blocking your way.");
			else
				msgt(MSG_HITWALL, "There is a wall blocking your way.");
		}

		/* Store the action type */
		player->previous_action[0] = ACTION_MISC;
	} else if (player->upkeep->total_weight >
			   (weight_limit(player->state) * 3) / 2) {
		/* Deal with overburdened characters */
		msg("You are too burdened to move.");

		/* Disturb the player */
		disturb(player, false);

		/* Don't take a turn... */
		player->upkeep->energy_use = 0;
	} else if (player_can_leap(player, grid, dir) && confirm_leap(grid, dir)) {
		/* At this point attack any invisible monster that may be there */
		if (mon) {
			msg("An unseen foe blocks your way.");

			/* Attack */
			py_attack(player, grid, ATT_MAIN);
		} else {
			/* Otherwise do the leap! */
			struct loc mid = loc_sum(player->grid, ddgrid[dir]);

			/* We generously give you your free flanking attack... */
			player_flanking_or_retreat(player, mid);

			/* Store the action type */
			player->previous_action[0] = dir;

			/* Move player to the new position */
			monster_swap(player->grid, mid);

			/* Remember that the player is in the air now */
			player->upkeep->leaping = true;
			cmdq_push(CMD_LEAP);
		}
	} else {
		/* Normal movement */
		bool pit = square_ispit(cave, player->grid);
		bool web = square_iswebbed(cave, player->grid);
		bool step = true;

		/* Check before walking on known traps/chasms on movement */
		if (!confused && square_ismark(cave, grid)) {
			/* If the player hasn't already leapt */
			if (square_ischasm(cave, grid)) {
				/* Disturb the player */
				disturb(player, false);

				/* Flush input */
				event_signal(EVENT_MESSAGE_FLUSH);

				if (!get_check("Step into the chasm? ")) {
					/* Don't take a turn... */
					player->upkeep->energy_use = 0;
					step = false;
				}
			}

			/* Traps */
            if (trap) {
                /* Disturb the player */
				disturb(player, false);

				/* Flush input */
				event_signal(EVENT_MESSAGE_FLUSH);

                if (!get_check("Are you sure you want to step on the trap? ")) {
                    /* Don't take a turn... */
                    player->upkeep->energy_use = 0;
					step = false;
                }
            }
		}

		/* At this point attack any invisible monster that may be there */
        if (mon) {
			msg("An unseen foe blocks your way.");

			/* Attack */
			py_attack(player, grid, ATT_MAIN);
			step = false;
		}

		/* It is hard to get out of a pit */
		if (pit && !player_escape_pit(player)) {
			step = false;
		}

		/* It is hard to get out of a web */
		if (web && !player_break_web(player)) {
			step = false;
		}

		/* We can move */
		if (step) {
			/* Do flanking or controlled retreat attack if any */
			player_flanking_or_retreat(player, grid);

			/* Move player */
			monster_swap(player->grid, grid);

			/* New location */
			grid = player->grid;

			/* Spontaneous Searching */
			perceive(player);

			/* Remember this direction of movement */
			player->previous_action[0] = dir;

			/* Discover stairs if blind */
			if (square_isstairs(cave, grid)) {
				square_mark(cave, grid);
				square_light_spot(cave, grid);
			}

			/* Remark on Forge and discover it if blind */
			if (square_isforge(cave, grid)) {
				struct feature *feat = square_feat(cave, grid);
				if ((feat->fidx == FEAT_FORGE_UNIQUE) &&
					!player->unique_forge_seen) {
					msg("You enter the forge 'Orodruth' - the Mountain's Anger - where Grond was made in days of old.");
					msg("The fires burn still.");
					player->unique_forge_seen = true;
					history_add(player, "Entered the forge 'Orodruth'",
								HIST_FORGE_FOUND);
				} else {
					const char *article;
					char name[50];
					square_apparent_name(cave, grid, name, sizeof(name));
					if (feat->fidx == FEAT_FORGE_UNIQUE) {
						article = "the";
					} else if (feat->fidx == FEAT_FORGE_GOOD) {
						article = "an";
					} else {
						article = "a";
					}
					msg("You enter %s %s.", article, name);
				}
				square_mark(cave, grid);
				square_light_spot(cave, grid);
			}

			/* Discover invisible traps, set off visible ones */
			if (square_isplayertrap(cave, grid)) {
				disturb(player, false);
				square_reveal_trap(cave, grid, true);
				hit_trap(grid);
			} else if (square_ischasm(cave, grid)) {
				player_fall_in_chasm(player);
			}

			/* Update view */
			update_view(cave, player);
			cmdq_push(CMD_AUTOPICKUP);
		}
	}

	player->upkeep->running_firststep = false;
}

/**
 * Stay still.  Search.  Enter stores.
 * Pick up treasure if "pickup" is true.
 */
static void do_cmd_hold_aux(void)
{
	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_STAND;

	/* Store the 'focus' attribute */
	player->focused = true;

	/* Look at the floor */
	event_signal(EVENT_SEEFLOOR);
	square_know_pile(cave, player->grid);

	/* Make less noise if you did nothing at all
	 * (+7 in total whether or not stealth mode is used) */
	if (player->stealth_mode) {
		player->stealth_score += 2;
	} else {
		player->stealth_score += 7;
	}

    /* Passing in stealth mode removes the speed penalty
	 * (as there was no bonus either) */
    player->upkeep->update |= (PU_BONUS);
    player->upkeep->redraw |= (PR_STATE | PR_SPEED);

	/* Searching */
	search(player);
}

/**
 * Stay still.  Search.  Enter stores.
 * Pick up treasure if "pickup" is true.
 */
void do_cmd_hold(struct command *cmd)
{
	do_cmd_hold_aux();
}

/**
 * Determine if a given grid may be "walked"
 */
static bool do_cmd_walk_test(struct loc grid)
{
	int m_idx = square(cave, grid)->mon;
	struct monster *mon = cave_monster(cave, m_idx);

	/* If we don't know the grid, allow attempts to walk into it */
	if (!square_isknown(cave, grid))
		return true;

	/* Allow attack on visible monsters if unafraid */
	if (m_idx > 0 && monster_is_visible(mon)) {
		return true;
	}

	/* Require open space */
	if (!square_ispassable(cave, grid)) {
		if (square_isrubble(cave, grid)) {
			/* Rubble */
			msgt(MSG_HITWALL, "There is a pile of rubble in the way!");

			/* Store the action type */
			player->previous_action[0] = ACTION_MISC;
		} else if (square_isrock(cave, grid)) {
			/* Wall */
			msgt(MSG_HITWALL, "There is a wall in the way!");

			/* Store the action type */
			player->previous_action[0] = ACTION_MISC;
		} else {
			/* Door */
			return true;
		}

		/* Cancel repeat */
		disturb(player, false);

		/* Nope */
		return false;
	}

	/* Okay */
	return true;
}


/**
 * Walk in the given direction.
 */
void do_cmd_walk(struct command *cmd)
{
	struct loc grid;
	int dir;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

    /* Convert walking in place to 'hold' */
    if (dir == DIR_NONE) {
        do_cmd_hold_aux();
        return;
    }

	/* Apply confusion if necessary */
	if (player_confuse_dir(player, &dir, false))
		/* Confused movements use energy no matter what */
		player->upkeep->energy_use = z_info->move_energy;
	
	/* Verify walkability, first checking for if the player is escaping */
	grid = loc_sum(player->grid, ddgrid[dir]);
	if (!square_in_bounds_fully(cave, grid)
			&& square_ispassable(cave, grid)
			&& !square_monster(cave, grid)) {
		/* Deal with leaving the map */
		do_cmd_escape();
		return;
	} else if (!do_cmd_walk_test(grid)) {
		return;
	}

	player->upkeep->energy_use = z_info->move_energy;

	/* Attempt to disarm unless it's a trap and we're trapsafe */
	move_player(dir, !square_isdisarmabletrap(cave, grid));
}


/**
 * Walk into a trap.
 */
void do_cmd_jump(struct command *cmd)
{
	struct loc grid;
	int dir;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	/* Apply confusion if necessary */
	if (player_confuse_dir(player, &dir, false))
		player->upkeep->energy_use = z_info->move_energy;

	/* Verify walkability */
	grid = loc_sum(player->grid, ddgrid[dir]);
	if (!do_cmd_walk_test(grid))
		return;

	player->upkeep->energy_use = z_info->move_energy;

	move_player(dir, false);
}


/**
 * Start running.
 *
 * Note that running while confused is not allowed.
 */
void do_cmd_run(struct command *cmd)
{
	struct loc grid;
	int dir;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	if (player_confuse_dir(player, &dir, true))
		return;

	/* Get location */
	if (dir) {
		grid = loc_sum(player->grid, ddgrid[dir]);
		if (!do_cmd_walk_test(grid))
			return;
			
		/* Hack: convert repeat count to running count */
		if (cmd->nrepeats > 0) {
			player->upkeep->running = cmd->nrepeats;
			cmd->nrepeats = 0;
		}
		else {
			player->upkeep->running = 0;
		}
	}

	/* Start run */
	run_step(dir);
}


/**
 * Start running with pathfinder.
 *
 * Note that running while confused is not allowed.
 */
void do_cmd_pathfind(struct command *cmd)
{
	struct loc grid;

	/* XXX-AS Add better arg checking */
	cmd_get_arg_point(cmd, "point", &grid);

	if (player->timed[TMD_CONFUSED])
		return;

	if (find_path(grid)) {
		player->upkeep->running = 1000;
		/* Calculate torch radius */
		player->upkeep->update |= (PU_TORCH);
		player->upkeep->running_withpathfind = true;
		run_step(0);
	}
}

/**
 * Stop, start or change a song
 */
void do_cmd_change_song(struct command *cmd)
{
	change_song();
}


 
/**
 * Rest (restores hit points and mana and such)
 */
void do_cmd_rest(struct command *cmd)
{
	int n;

	/* XXX-AS need to insert UI here */
	if (cmd_get_arg_choice(cmd, "choice", &n) != CMD_OK)
		return;

	/* Typically resting ends your current song */
	if (OPT(player, stop_singing_on_rest)) {
		player_change_song(player, NULL, false);
	}

	/* 
	 * A little sanity checking on the input - only the specified negative 
	 * values are valid. 
	 */
	if (n < 0 && !player_resting_is_special(n))
		return;

	/* Do some upkeep on the first turn of rest */
	if (!player_is_resting(player)) {
		player->upkeep->update |= (PU_BONUS);

		/* If a number of turns was entered, remember it */
		if (n > 1)
			player_set_resting_repeat_count(player, n);
		else if (n == 1)
			/* If we're repeating the command, use the same count */
			n = player_get_resting_repeat_count(player);
	}

	/* Set the counter, and stop if told to */
	player_resting_set_count(player, n);
	if (!player_is_resting(player))
		return;

	/* Take a turn */
	player_resting_step_turn(player);

	/* Redraw the state if requested */
	handle_stuff(player);

	/* Prepare to continue, or cancel and clean up */
	if (player_resting_count(player) > 0) {
		cmdq_push(CMD_REST);
		cmd_set_arg_choice(cmdq_peek(), "choice", n - 1);
	} else if (player_resting_is_special(n)) {
		cmdq_push(CMD_REST);
		cmd_set_arg_choice(cmdq_peek(), "choice", n);
		player_set_resting_repeat_count(player, 0);
	} else {
		player_resting_cancel(player, false);
	}
}


/**
 * Spend a turn doing nothing
 */
void do_cmd_sleep(struct command *cmd)
{
	/* Stop singing */
	player_change_song(player, NULL, false);

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;
}

/**
 * Skip a turn after being knocked back
 */
void do_cmd_skip(struct command *cmd)
{
	/* Let the player know */
	msg("You recover your footing.");

	/* Flush input */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Reset flag */
	player->upkeep->knocked_back = false;

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;
}


