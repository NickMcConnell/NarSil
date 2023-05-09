/**
 * \file game-world.c
 * \brief Game core management of the game world
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
#include "cmds.h"
#include "effects.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-util.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "songs.h"
#include "source.h"
#include "target.h"
#include "trap.h"
#include "tutorial.h"
#include "z-queue.h"

uint16_t daycount = 0;
uint32_t seed_randart;		/* Hack -- consistent random artifacts */
uint32_t seed_flavor;		/* Hack -- consistent object colors */
int32_t turn;			/* Current game turn */
bool character_generated;	/* The character exists */
bool character_dungeon;		/* The character has a dungeon */
struct level *world;

/**
 * This table allows quick conversion from "speed" to "energy"
 * It used to be complex, but in Sil it is basically linear.
 * It is set up so that there are 10 game turns per player turn at normal speed
 *
 * Note that creatures should never have speed 0 in the first place
 */
const uint8_t extract_energy[8] =
{
	/* Impossible */  5,
	/* Slow */        5,
	/* Normal */     10,
	/* Fast */       15,
	/* V Fast */     20,
	/* X Fast */     25,
	/* I Fast */     30,
	/* A Fast */     35,
};

/**
 * Find a level by its name
 */
struct level *level_by_name(const char *name)
{
	struct level *lev = world;
	while (lev) {
		if (streq(lev->name, name)) {
			break;
		}
		lev = lev->next;
	}
	return lev;
}

/**
 * Find a level by its depth
 */
struct level *level_by_depth(int depth)
{
	struct level *lev = world;
	while (lev) {
		if (lev->depth == depth) {
			break;
		}
		lev = lev->next;
	}
	return lev;
}

/**
 * Say whether it's daytime or not
 */
bool is_daytime(void)
{
	if ((turn % (10L * z_info->day_length)) < ((10L * z_info->day_length) / 2)) 
		return true;

	return false;
}

/**
 * The amount of energy gained in a turn by a player or monster
 */
int turn_energy(int speed)
{
	return extract_energy[speed] * z_info->move_energy / 100;
}

/**
 * Determines how many points of health/song is regenerated next round
 * assuming it increases by 'max' points every 'period'.
 * Note that players use 'player->turn' and monsters use 'turn'.
 * This lets hasted players regenerate at the right speed.
 */
int regen_amount(int turn_number, int max, int period)
{
	int regen_so_far, regen_next;

	if (turn_number == 0) {
		/* Do nothing on the first turn of the game */
		return 0;
	}
	if ((turn_number % period) > 0) {
		regen_so_far = (max * ((turn_number - 1) % period)) / period;
		regen_next = (max * (turn_number % period)) / period;
	} else {
		regen_so_far = (max * ((turn_number - 1) % period)) / period;
		regen_next = (max * period) / period;
	}

	return regen_next - regen_so_far;
}

/**
 * Represents the different levels of health.
 * Note that it is a bit odd with fewer health levels in the SOMEWHAT_WOUNDED
 * category. This is due to a rounding off tension between the natural way to
 * do the colours (perfect having its own) and the natural way to do the stars
 * for the health bar (zero having its own).
 * It should be unnoticeable to the player.
 */
int health_level(int current, int max)
{
	int level;

	if (current == max) { 
		level = HEALTH_UNHURT;
	} else {
		switch ((4 * current + max - 1 ) / max) {
			case 4: level = HEALTH_SOMEWHAT_WOUNDED; break; /* 76% - 99% */
			case 3: level = HEALTH_WOUNDED         ; break; /* 51% - 75% */
			case 2: level = HEALTH_BADLY_WOUNDED   ; break; /* 26% - 50% */
			case 1:	level = HEALTH_ALMOST_DEAD     ; break; /* 1% - 25% */
			default:level = HEALTH_DEAD            ; break; /* 0% */
		}
	}

	return (level);
}

/**
 * Play an ambient sound dependent on dungeon level, and day or night in town
 */
void play_ambient_sound(void)
{
	if (player->depth == 0) {
		if (is_daytime())
			sound(MSG_AMBIENT_DAY);
		else 
			sound(MSG_AMBIENT_NITE);
	} else if (player->depth <= 20) {
		sound(MSG_AMBIENT_DNG1);
	} else if (player->depth <= 40) {
		sound(MSG_AMBIENT_DNG2);
	} else if (player->depth <= 60) {
		sound(MSG_AMBIENT_DNG3);
	} else if (player->depth <= 80) {
		sound(MSG_AMBIENT_DNG4);
	} else {
		sound(MSG_AMBIENT_DNG5);
	}
}

/**
 * Helper for process_world -- decrement player->timed[] and curse effect fields
 */
static void decrease_timeouts(void)
{
	int i;

	/* Most timed effects decrement by 1 */
	for (i = 0; i < TMD_MAX; i++) {
		int decr = player_timed_decrement_amount(player, i);
		/* Food is handled separately */
		if (!player->timed[i] || (i == TMD_FOOD))
			continue;

		/* Decrement the effect */
		player_dec_timed(player, i, decr, false);
	}
}


/**
 * Get the additional cost to monster or noise flow due to terrain
 */
static int square_flow_cost(struct chunk *c, struct loc grid,
							struct monster *mon)
{
	int cost = 0;

	/* Deal with monster pathfinding */
	if (mon) {
		bool bash = false;
		/* Get the percentage chance of the monster being able
		 * to move onto that square */
		int chance = monster_entry_chance(c, mon, grid, &bash);

		/* If there is any chance, then convert it to a number of turns */
		if (chance > 0) {
			cost += (100 / chance) - 1;

			/* Add an extra turn for unlocking/opening doors as
			 * this action doesn't move the monster */
			if (square_iscloseddoor(c, grid) && !bash) {
				if (!(rf_has(mon->race->flags, RF_PASS_DOOR) ||
					  rf_has(mon->race->flags, RF_PASS_WALL))) {
					cost += 1;
				}
			} else if (!square_isprojectable(c, grid) &&
					   rf_has(mon->race->flags, RF_TUNNEL_WALL)) {
				/* Add extra turn(s) for tunneling through rubble/walls as
				 * this action doesn't move the monster */
				if (square_isrubble(c, grid)) {
					/* An extra turn to dig through */
					cost += 1;
				} else {
					/* Two extra turns to dig through granite/quartz */
					cost += 2;
				}
			} else if (!square_isprojectable(c, grid) &&
					   rf_has(mon->race->flags, RF_KILL_WALL)) {
				/* Pretend it would take an extra turn (to prefer routes
				 * with less wall destruction */
				cost += 1; 
			}
		} else {
			/* If there is no chance, just skip this square */
			return -1;
		}
	} else {
		/* Deal with noise flows */
		/* Ignore walls */
		if (square_iswall(c, grid)) {
			return -1;
		}
 
		/* Penalize doors by 5 when calculating the real noise*/
		if (square_iscloseddoor(c, grid) || square_issecretdoor(c, grid)){
			cost += 5;
		}
	}
	return cost;
}

/**
 * Used to convert grid into an array index (i) in a chunk of width w.
 * \param grid location
 * \param w area width
 * \return index
 */
static int grid_to_i(struct loc grid, int w)
{
	return grid.y * w + grid.x;
}

/**
 * Used to convert an array index (i) into grid in a chunk of width w.
 * \param i grid index
 * \param w area width
 * \param grid location
 */
static void i_to_grid(int i, int w, struct loc *grid)
{
	grid->y = i / w;
	grid->x = i % w;
}

/**
 * Sil needs various 'flows', which are arrays of the same size as the map,
 * with a number for each map square.
 *
 * One of these flows is used to represent the from the player noise at each
 * location.
 * Another is used to represent the noise from a particular monster.
 *
 * Each monster has a flow which it uses for alert pathfinding, representing the
 * shortest route each monster could take to get to the player.
 *
 * Flows are also used for the pathfinding of unwary monsters who move in their
 * initial groups to various locations around the map.
 *
 * Note that the noise is generated around the centre.
 * This is often the player, but can be a monster (for FLOW_MONSTER_NOISE)
 */
void update_flow(struct chunk *c, struct flow *flow, struct monster *mon)
{
	struct loc next = flow->centre;
	int y, x, d;
	int value = 0;
	struct queue *queue = q_new(c->height * c->width);

	/* Set all the grids to maximum */
	for (y = 1; y < c->height - 1; y++) {
		for (x = 1; x < c->width - 1; x++) {
			flow->grids[y][x] = z_info->flow_max;
		}
	}

	if (loc_eq(next, loc(0, 0))) {
		quit("Flow has no centre!");
	}

	/* Set the centre value to zero, push it onto the queue */
	flow->grids[next.y][next.x] = 0;
	q_push_int(queue, grid_to_i(next, c->width));
	value++;

	/* Propagate outwards */
	while ((q_len(queue) > 0) && (value < z_info->flow_max)) {
		/* Process only the grids currently on the queue */
		int count = q_len(queue);
		while (count) {
			/* Get the next grid, count it */
			i_to_grid(q_pop_int(queue), c->width, &next);
			count--;

			/* If it costs more the current value, put it back on the queue */
			if (flow->grids[next.y][next.x] > value) {
				q_push_int(queue, grid_to_i(next, c->width));
				continue;
			}

			/* Iterate over the current grid's children */
			for (d = 0; d < 8; d++)	{
				/* Child location */
				struct loc grid = loc_sum(next, ddgrid_ddd[d]);

				/* Extra cost of the grid */
				int cost = square_flow_cost(c, grid, mon);

				/* Monster on this grid */
				struct monster *grid_mon = square_monster(c, grid);

				/* Legal grids only */
				if (!square_in_bounds(c, grid)) continue;

				/* Skip grids that have already been processed */
				if (flow->grids[grid.y][grid.x] < z_info->flow_max) continue;

				/* Ignore features that block flow */
				if (cost < 0) continue;

				/* Save the flow value */
				flow->grids[grid.y][grid.x] = value + cost;

				/* Enqueue that child */
				q_push_int(queue, grid_to_i(grid, c->width));

				/* Monsters at this site need to re-consider their targets */
				if (grid_mon) {
					grid_mon->target.grid = loc(0, 0);
				}
			}
		}
		value++;
	}

	q_free(queue);
}

/**
 * Determines how far a grid is from the source using the given flow.
 */
int flow_dist(struct flow flow, struct loc grid)
{
	return flow.grids[grid.y][grid.x];
}

/**
 * Characters leave scent trails for perceptive monsters to track.
 *
 * Scent is rather more limited than sound.  Many creatures cannot use
 * it at all, it doesn't extend very far outwards from the character's
 * current position, and monsters can use it to home in the character,
 * but not to run away.
 *
 * Scent is valued according to age.  When a character takes his turn,
 * scent is aged by one, and new scent of the current age is laid down.
 * Speedy characters leave more scent, true, but it also ages faster,
 * which makes it harder to hunt them down.
 *
 * Whenever the age count loops, most of the scent trail is erased and
 * the age of the remainder is recalculated.
 */
static void update_scent(void)
{
	int y, x;
	int scent_strength[5][5] = {
		{250, 2, 2, 2, 250},
		{  2, 1, 1, 1,   2},
		{  2, 1, 0, 1,   2},
		{  2, 1, 1, 1,   2},
		{250, 2, 2, 2, 250},
	};

	/* Scent becomes "younger" */
	cave->scent_age--;

	/* Loop the age and adjust scent values when necessary */
	if (cave->scent_age <= 0) {
		/* Update scent for all grids */
		for (y = 1; y < cave->height - 1; y++) {
			for (x = 1; x < cave->width - 1; x++) {
				/* Ignore non-existent scent */
				if (cave->scent.grids[y][x] == 0) continue;

				/* Erase the earlier part of the previous cycle */
				if (cave->scent.grids[y][x] > SMELL_STRENGTH) {
					cave->scent.grids[y][x] = 0;
				} else {
					/* Reset the ages of the most recent scent */
					cave->scent.grids[y][x] += 250 - SMELL_STRENGTH;
				}
			}
		}

		/* Reset the age value */
		cave->scent_age = 250 - SMELL_STRENGTH;
	}

	/* Lay down new scent around the player */
	for (y = 0; y < 5; y++) {
		for (x = 0; x < 5; x++) {
			struct loc scent;
			int new_scent = scent_strength[y][x];

			/* Initialize */
			scent.y = y + player->grid.y - 2;
			scent.x = x + player->grid.x - 2;

			/* Ignore invalid or non-scent-carrying grids */
			if (!square_in_bounds(cave, scent)) continue;
			if (square_isnoscent(cave, scent)) continue;

			/* Grid must not be blocked by walls from the character */
			if (!los(cave, player->grid, loc(x, y))) continue;

			/* Note grids that are too far away */
			if (scent_strength[y][x] == 250) continue;

			/* Mark the scent */
			cave->scent.grids[scent.y][scent.x] = cave->scent_age + new_scent;
		}
	}
}

/**
 * Get and return the strength (age) of scent in a given grid.
 *
 * Return "-1" if no scent exists in the grid.
 */
int get_scent(struct chunk *c, struct loc grid)
{
	int age;
	int scent;

	/* Check Bounds */
	if (!square_in_bounds(c, grid)) return -1;

	/* Sent trace? */
	scent = c->scent.grids[grid.y][grid.x];

	/* No scent at all */
	if (!c->scent.grids[grid.y][grid.x]) return -1;

	/* Get age of scent */
	age = scent - c->scent_age;

	if (age > SMELL_STRENGTH) return -1;

	/* Return the age of the scent */
	return age;
}

/**
 * Handle things that need updating once every 10 game turns
 */
void process_world(struct chunk *c)
{
	/* Compact the monster list if we're approaching the limit */
	if (cave_monster_count(c) + 32 > z_info->level_monster_max)
		compact_monsters(c, 64);

	/* Too many holes in the monster list - compress */
	if (cave_monster_count(c) + 32 < cave_monster_max(c))
		compact_monsters(c, 0);

	/*** Check the Time ***/

	/* Play an ambient sound at regular intervals. */
	if (!(turn % ((10L * z_info->day_length) / 4)))
		play_ambient_sound();

	/* Handle  the "surface" */
	if (!player->depth) {
		if (percent_chance(10)) {
			/* Make a new monster */
			(void)pick_and_place_monster_on_stairs(c, player, true, 0, false);
		}
	}

	/* Check for creature generation */
	if (silmarils_possessed(player) >= 2) {
		/* Vastly more wandering monsters during the endgame when you have
		 * 2 or 3 Silmarils */
		int percent = (c->height * c->width)
			/ (z_info->block_hgt * z_info->block_wid);
		if (percent_chance(percent)) {
			(void)pick_and_place_monster_on_stairs(c, player, true, c->depth,
												   false);
		}
	} else if (one_in_(z_info->alloc_monster_chance)) {
		/* Normal wandering monster generation */
		(void)pick_and_place_monster_on_stairs(c, player, true, c->depth,
											   false);
	}

	/* Players with the haunted curse attract wraiths */
	if (percent_chance(player->state.flags[OF_HAUNTED])) {
		/* Make a new wraith */
		(void)pick_and_place_monster_on_stairs(c, player, true, c->depth, true);
	}

	/* Process light */
	player_update_light(player);
}


/**
 * Housekeeping after the processing monsters but before processing the player
 */
void pre_process_player(void)
{
	int i;

	/* Reset the riposte flag */
	player->upkeep->riposte = false;

	/* Reset the was_entranced flag */
	player->upkeep->was_entranced = false;

	/* Update the player's light radius */
	calc_light(player);

	/* Make the stealth-modified noise (has to occur after monsters have
	 * had a chance to move) */
	monsters_hear(true, true, player->stealth_score);

	/* Stop stealth mode if something happened */
	if (player->stealth_mode == STEALTH_MODE_STOPPING) {
		/* Cancel */
		player->stealth_mode = STEALTH_MODE_OFF;

		/* Recalculate bonuses */
		player->upkeep->update |= (PU_BONUS);

		/* Redraw the state */
		player->upkeep->redraw |= (PR_STATE);
	}

	/* Morgoth will announce a challenge if adjacent */
	if (player->truce && (player->depth == z_info->dun_depth)) {
		check_truce(player);
	}

	/* List all challenge options at the start of the game */
	if (player->turn == 1) {
		options_list_challenge();
	}

	/* Shuffle along the array of previous actions */
	for (i = MAX_ACTION - 1; i > 0; i--) {
		player->previous_action[i] = player->previous_action[i - 1];
	}

	/* Put in a default for this turn */
	player->previous_action[0] = ACTION_NOTHING;

	/* Redraw stuff (if needed) */
	redraw_stuff(player);

	/* Have to update the player bonuses at every turn with sprinting, dodging
	 * etc. This might cause annoying slowdowns, I'm not sure */
	player->upkeep->update |= (PU_BONUS);

}

/**
 * Housekeeping after the processing of any player command
 */
static void process_player_cleanup(void)
{
	int i;

	/* Check for greater vault squares */
	if (square_isgreatervault(cave, player->grid) && cave->vault_name) {
		char note[120];

		strnfmt(note, sizeof(note), "Entered %s", cave->vault_name);
		history_add(player, note, HIST_VAULT_ENTERED);

		/* Give a message unless it is the Gates or the Throne Room */
		if (player->depth > 0 && player->depth < 20) {
			msg("You have entered %s.", cave->vault_name);
		}
		string_free(cave->vault_name);
		cave->vault_name = NULL;
	}

	/* Significant */
	if (player->upkeep->energy_use) {
		/* Use some energy */
		player->energy -= player->upkeep->energy_use;

		/* Increment the total energy counter */
		player->total_energy += player->upkeep->energy_use;

		/* Do nothing else if player has auto-dropped stuff */
		if (!player->upkeep->dropping) {
			/* Hack -- constant hallucination */
			if (player->timed[TMD_IMAGE])
				player->upkeep->redraw |= (PR_MAP);

			/* Shimmer multi-hued monsters */
			for (i = 1; i < cave_monster_max(cave); i++) {
				struct monster *mon = cave_monster(cave, i);
				if (!mon->race)
					continue;
				if (!rf_has(mon->race->flags, RF_ATTR_MULTI))
					continue;
				square_light_spot(cave, mon->grid);
			}

			/* Show marked monsters */
			for (i = 1; i < cave_monster_max(cave); i++) {
				struct monster *mon = cave_monster(cave, i);
				if (mflag_has(mon->mflag, MFLAG_MARK)) {
					if (!mflag_has(mon->mflag, MFLAG_SHOW)) {
						mflag_off(mon->mflag, MFLAG_MARK);
						update_mon(mon, cave, false);
					}
				}
			}
		}
	}

	/* Clear SHOW flag and player drop status */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);
		mflag_off(mon->mflag, MFLAG_SHOW);
	}
	player->upkeep->dropping = false;

	/* Hack - update needed first because inventory may have changed */
	update_stuff(player);
	redraw_stuff(player);
}

/**
 * Housekeeping after the processing of a player game command (ie energy use)
 */
static void process_player_post_energy_use_cleanup(void)
{

    /* If the player is exiting the the game in some manner, stop processing */
    if (player->is_dead || player->upkeep->generate_level) return;

	/* Do song effects */
	player_sing(player);

	/* Make less noise if you did nothing at all (+7 in total whether or
	 * not stealth mode is used) */
	if (player_is_resting(player)) {
		player->stealth_score += (player->stealth_mode) ? 2 : 7;
	}

	/* Make much more noise when smithing */
	if (player->upkeep->smithing) {
		/* Make a lot of noise */
		monsters_hear(true, false, -10);
	}

	/* Update noise and scent */
	cave->player_noise.centre = player->grid;
	update_flow(cave, &cave->player_noise, NULL);
	update_scent();

	/* Possibly identify passive abilities every so often*/
	if (one_in_(500)) {
		ident_passive(player);
	}

	/*** Damage over Time ***/

	/* Take damage from poison */
	if (player->timed[TMD_POISONED]) {
		/* Amount is one fifth of the poison, rounding up */
		take_hit(player, (player->timed[TMD_POISONED] + 4) / 5, "poison");
	}

	/* Take damage from cuts, worse from serious cuts */
	if (player->timed[TMD_CUT]) {
		/* Take damage */
		take_hit(player, (player->timed[TMD_CUT] + 4) / 5, "a fatal wound");
	}

	/* Reduce the wrath counter */
	if (player->wrath) {
		int amount = (player->wrath / 100) * (player->wrath / 100);

		/* Half as fast if still singing the song */
		if (player_is_singing(player, lookup_song("Slaying"))) {
			player->wrath -= MAX(amount / 2, 1);
		} else {
			player->wrath -= MAX(amount, 1);
		}
		player->upkeep->update |= (PU_BONUS);
		player->upkeep->redraw |= (PR_SONG);
	}

	/*** Check the Food, and Regenerate ***/

	/* Digest */
	player_digest(player);

	/* Regenerate Hit Points if needed */
	if (player->chp < player->mhp) {
		player_regen_hp(player);
	}

	/* Regenerate voice if needed */
	if (player->csp < player->msp) {
		player_regen_mana(player);
	}

	/* Timeout various things */
	decrease_timeouts();

	/* Lower the staircasiness */
	if (player->staircasiness > 0) {
		int amount;

        /* Decreases much faster on the escape */
        if (player->on_the_run) {
            /* Amount is one hundredth of the current value, rounding up */
            amount = (player->staircasiness + 99) / 100;
        } else {
            /* Amount is one thousandth of the current value, rounding up */
            amount = (player->staircasiness + 999) / 1000;
        }

		player->staircasiness -= amount;
	}

	/* Increase the time since the last forge */
	player->forge_drought++;

	/* Reset the focus flag if the player didn't 'pass' this turn */
	if (player->previous_action[0] != ACTION_STAND) {
		player->focused = false;
	}

	/* Reset the consecutive attacks if the player didn't attack or 'pass' */
	if (!player->attacked && (player->previous_action[0] != ACTION_STAND)) {
		player->consecutive_attacks = 0;
		player->last_attack_m_idx = 0;
	}

	/* Check for radiance */
	if (player_radiates(player)) {
		sqinfo_on(square(cave, player->grid)->info, SQUARE_GLOW);
	}

	player->turn++;
}

/**
 * Process player commands from the command queue, finishing when there is a
 * command using energy (any regular game command), or we run out of commands
 * and need another from the user, or the character changes level or dies, or
 * the game is stopped.
 *
 * Notice the annoying code to handle "pack overflow", which
 * must come first just in case somebody manages to corrupt
 * the savefiles by clever use of menu commands or something. (Can go? NRM)
 *
 * Notice the annoying code to handle "monster memory" changes,
 * which allows us to avoid having to update the window flags
 * every time we change any internal monster memory field, and
 * also reduces the number of times that the recall window must
 * be redrawn.
 */
void process_player(void)
{
	/* Check for interrupts */
	player_resting_complete_special(player);
	event_signal(EVENT_CHECK_INTERRUPT);

	/* Repeat until energy is reduced */
	do {
		/* Refresh */
		notice_stuff(player);
		handle_stuff(player);
		event_signal(EVENT_REFRESH);

		/* Hack -- Pack Overflow */
		pack_overflow(NULL);

		/* Assume free turn */
		player->upkeep->energy_use = 0;

		/* Reset number of attacks this turn */
		event_signal(EVENT_COMBAT_RESET);

		/* Get base stealth score for the round; this will get modified by
		 * the type of action */
		player->stealth_score = player->state.skill_use[SKILL_STEALTH];

		/* Paralyzed or Knocked Out player gets no turn */
		if (player->timed[TMD_ENTRANCED] ||
			player_timed_grade_eq(player, TMD_STUN, "Knocked Out")) {
			cmdq_push(CMD_SLEEP);
		} else if (player->upkeep->knocked_back) {
			/* Knocked back player needs to recover footing */
			cmdq_push(CMD_SKIP);
		}

		/* Prepare for the next command */
		if (cmd_get_nrepeats() > 0) {
			event_signal(EVENT_COMMAND_REPEAT);
		} else {
			/* Check monster recall */
			if (player->upkeep->monster_race)
				player->upkeep->redraw |= (PR_MONSTER);

			/* Place cursor on player/target */
			event_signal(EVENT_REFRESH);
		}
		//TODO handle autopickup

		/* Get a command from the queue if there is one */
		if (!cmdq_pop(CTX_GAME))
			break;

		if (!player->upkeep->playing)
			break;

		process_player_cleanup();
	} while (!player->upkeep->energy_use &&
			 !player->is_dead &&
			 !player->upkeep->generate_level);

	if (player->upkeep->energy_use)
		process_player_post_energy_use_cleanup();

	/* Notice stuff (if needed) */
	notice_stuff(player);
}

/**
 * Housekeeping on arriving on a new level
 */
void on_new_level(void)
{
	int i;

	/* Update noise and scent */
	cave->player_noise.centre = player->grid;
	update_flow(cave, &cave->player_noise, NULL);
	update_scent();

	/* Disturb */
	disturb(player, false);

	/* Display the entry poetry, prepare for guaranteed forge */
	if (player->turn == 0) {
		event_signal_poem(EVENT_POEM, player->sex->poetry_name, 5, 15);
		player->forge_count = 0;
		player->forge_drought = 5000;
	}

	/* Flush messages */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Update display */
	event_signal(EVENT_NEW_LEVEL_DISPLAY);

	/* Track maximum dungeon level */
	if (player->max_depth < player->depth) {
		for (i = player->max_depth + 1; i <= player->depth; i++) {
			if (i > 1) {
				int new_exp = i * 50;
				player_exp_gain(player, new_exp);
				player->descent_exp += new_exp;
			}
		}
		player->max_depth = player->depth;
	}

	/* Update player */
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS | PU_INVEN);
	player->upkeep->notice |= (PN_COMBINE);
	notice_stuff(player);
	update_stuff(player);
	redraw_stuff(player);

	/* Refresh */
	event_signal(EVENT_REFRESH);

	/* Explain the truce for the final level */
	if ((player->depth == z_info->dun_depth) && player->truce) {
		msg("There is a strange tension in the air.");
		if (player->state.skill_use[SKILL_PERCEPTION] >= 15) {
			msg("You feel that Morgoth's servants are reluctant to attack before he delivers judgment.");	
		}
	}
}

/**
 * Housekeeping on leaving a level
 */
static void on_leave_level(void) {
	/* Don't allow command repeat if moved away from item used. */
	cmd_disable_repeat_floor_item();

	/* Any pending processing */
	notice_stuff(player);
	update_stuff(player);
	redraw_stuff(player);

	/* Flush messages */
	event_signal(EVENT_MESSAGE_FLUSH);
}


/**
 * The main game loop.
 *
 * This function will run until the player needs to enter a command, or closes
 * the game, or the character dies.
 */
void run_game_loop(void)
{
	/* Process the character until energy use or another command is needed */
	while (player->upkeep->playing) {
		process_player();
		if (player->upkeep->energy_use) {
			break;
		} else {
			return;
		}
	}

	/* Now that the player's turn is fully complete, we run the main loop 
	 * until player input is needed again */
	while (true) {
		notice_stuff(player);
		handle_stuff(player);
		event_signal(EVENT_REFRESH);

		/* Process the rest of the world, give the character energy and
		 * increment the turn counter unless we need to stop playing or
		 * generate a new level */
		if (player->is_dead || !player->upkeep->playing) {
			return;
		} else if (!player->upkeep->generate_level) {
			/* Process the rest of the monsters */
			process_monsters(0);

			/* Mark all monsters as ready to act when they have the energy */
			reset_monsters();

			/* Refresh */
			notice_stuff(player);
			handle_stuff(player);
			event_signal(EVENT_REFRESH);
			if (player->is_dead || !player->upkeep->playing)
				return;

			/* Process the world every ten turns */
			if (!(turn % 10) && !player->upkeep->generate_level) {
				process_world(cave);

				/* Refresh */
				notice_stuff(player);
				handle_stuff(player);
				event_signal(EVENT_REFRESH);
				if (player->is_dead || !player->upkeep->playing)
					return;
			}

			/* Give the character some energy */
			player->energy += turn_energy(player->state.speed);

			/* Count game turns */
			turn++;
		} else {
			/* Make a new level if requested */
			if (character_dungeon) {
				on_leave_level();
			}
			if (!in_tutorial()) {
				prepare_next_level(player);
			} else {
				tutorial_prepare_section(
					tutorial_get_next_section(player),
					player);
			}
			on_new_level();
			player->upkeep->generate_level = false;
		}

		/* If the character has enough energy to move they now do so, after
		 * any monsters with more energy take their turns */
		while (player->energy >= z_info->move_energy) {
			/* Do any necessary animations */
			event_signal(EVENT_ANIMATE);

			/* Process monster with even more energy first */
			process_monsters(player->energy + 1);
			if (player->is_dead || !player->upkeep->playing ||
				player->upkeep->generate_level)
				break;

			/* Process the player until they use some energy */
			while (player->upkeep->playing) {
				pre_process_player();
				process_player();
				if (player->upkeep->energy_use) {
					break;
				} else {
					return;
				}
			}
		}
	}
}
