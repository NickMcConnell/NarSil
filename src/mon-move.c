/**
 * \file mon-move.c
 * \brief Monster movement
 *
 * Monster AI affecting movement and spells, process a monster 
 * (with spells and actions of all kinds, reproduction, effects of any 
 * terrain on monster movement, picking up and destroying objects), 
 * process all monsters.
 *
 * Copyright (c) 1997 Ben Harrison, David Reeve Sward, Keldon Jones.
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
#include "combat.h"
#include "effects.h"
#include "game-world.h"
#include "init.h"
#include "monster.h"
#include "mon-attack.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-group.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "mon-timed.h"
#include "obj-desc.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "songs.h"
#include "trap.h"
#include "tutorial.h"


/**
 * ------------------------------------------------------------------------
 * Routines to enable decisions on monster behaviour
 * ------------------------------------------------------------------------ */
/**
 * Check if the monster can smell anything
 */
static bool monster_can_smell(struct monster *mon)
{
	int age = get_scent(cave, mon->grid);
	if (age == -1) return false;

	/* Wolves are amazing trackers */
	if (streq("wolf", mon->race->base->name)) {
		/* I smell a character! */
		return true;
	} else if (streq("cat", mon->race->base->name)) {
		/* Felines are also quite good */
		if (age <= SMELL_STRENGTH / 2) {
			/* Something's in the air... */
			return true;
		}
	}

	return false;
}

/**
 * Check if the monster normally occurs with other monsters
 */
static bool monster_talks_to_friends(struct monster *mon)
{
	struct monster_race *race = mon->race;
	return rf_has(race->flags, RF_FRIENDS) ||
		rf_has(race->flags, RF_FRIEND) ||
		rf_has(race->flags, RF_UNIQUE_FRIEND) ||
		rf_has(race->flags, RF_ESCORT) ||
		rf_has(race->flags, RF_ESCORTS) ||
		rsf_has(race->spell_flags, RSF_SHRIEK);
}

/**
 * Check if the monster can occupy a grid safely
 */
static bool monster_can_exist(struct chunk *c, struct monster *mon,
							  struct loc grid, bool occupied_ok, bool can_dig)
{
	struct monster_race *race = mon->race;

	/* Check Bounds */
	if (!square_in_bounds(c, grid)) return false;

	/* The grid is already occupied. */
	if (square_monster(c, grid) && !occupied_ok) return false;

	/* Glyphs -- must break first */
	if (square_iswarded(c, grid)) return false;

    /* Only flying creatures can pass chasms */
    if (square_ischasm(c, grid)) {
        if (rf_has(race->flags, RF_FLYING)) {
			return true;
        } else {
			return false;
		}
	}

	/* Anything else passable is fine */
	if (square_ispassable(c, grid)) return true;

	/* Permanent walls are never OK */
	if (square_isperm(c, grid)) return false;

	/* Monster can pass through walls */
	if (rf_has(race->flags, RF_PASS_WALL)) return true;

	/* Monster can bore through walls, and is allowed to. */
	if (rf_has(race->flags, RF_KILL_WALL) && (can_dig)) return true;

	/* Monster can dig through walls, and is allowed to. */
	if (rf_has(race->flags, RF_TUNNEL_WALL) && (can_dig)) return true;

	/* Some monsters can pass under doors */
	if (square_isdoor(c, grid) && rf_has(race->flags, RF_PASS_DOOR))
		return true;

	/* Not passable, and the monster can do nothing about it */
	return false;
}

/**
 * Determines the chance of a skill or hit roll succeeding.
 * (1 d sides + skill) - (1 d sides + difficulty)
 * Results <= 0 count as fails.
 * Results > 0 are successes.
 *
 * returns the number of ways you could succeed
 * (i.e. number of chances out of sides*sides
 *
 * note that this will be a percentage for normal skills (10 sides)
 * but will be out of 400 for hit rolls
 */
static int success_chance(int sides, int skill, int difficulty)
{
	int i, j;
	int ways = 0;
		
	for (i = 1; i <= sides; i++)
		for (j = 1; j <= sides; j++)
			if (i + skill > j + difficulty)
				ways++;

	return ways;
}

/**
 * Can the monster enter this grid?  How easy is it for them to do so?
 *
 * Returns the percentage chance of success.
 *
 * The code that uses this function sometimes assumes that it will never
 * return a value greater than 100.
 *
 * The usage of level to determine whether one monster can push past
 * another is a tad iffy, but ensures that orc soldiers can always
 * push past other orc soldiers.
 */
int monster_entry_chance(struct chunk *c, struct monster *mon, struct loc grid,
						 bool *bash)
{
	struct monster *mon1 = square_monster(c, grid);

	/* Assume nothing in the grid other than the terrain hinders movement */
	int move_chance = 100;

	/* Check Bounds */
	if (!square_in_bounds(c, grid)) return 0;

	/* Permanent walls are never passable */
	if (square_isperm(c, grid)) return 0;

	/* The grid is occupied by the player or a monster. */
	if (square_isplayer(c, grid)) {
		return 100;
	} else if (mon1) {
		/* All monsters can attempt to push past monsters that can move */
		if (!rf_has(mon1->race->flags, RF_NEVER_MOVE) &&
			!rf_has(mon1->race->flags, RF_HIDDEN_MOVE)) {
			/* It is easy to push past unwary or sleeping monsters */
			if ((mon1->alertness < ALERTNESS_ALERT) &&
				(monster_group_leader(c, mon) !=
				 monster_group_leader(c, mon1))) {
				move_chance = 80;
			} else if ((mon1->stance == STANCE_FLEEING) &&
					   (mon->stance != STANCE_FLEEING)) {
				/* Easy for non-fleeing monsters to push past fleeing ones */
				move_chance = 80;
			} else if ((mon1->stance != STANCE_FLEEING) &&
					   (mon->stance == STANCE_FLEEING)) {
				/* Easy for fleeing monsters to push past non-fleeing ones */
				move_chance = 80;
			} else if (mon->race->level > mon1->race->level) {
				/* It is easy to push past weaker monsters */
				move_chance = 80;
			} else if (mon->race->level == mon1->race->level) {
				/* It is quite hard to push past monsters of equal strength */
				move_chance = 20;
			} else {
				/* It is very difficult to move past alert, unafraid,
				 * stronger monsters */
				move_chance = 10;
			}
		} else {
			/* Cannot do anything to clear away the other monster */
			return 0;
		}
	}

	/* Glyphs */
	if (square_iswarded(c, grid)) {
		/* A simulated Will check */
		int chance = success_chance(10, monster_skill(mon, SKILL_WILL), 20);

		/* Unwary monsters won't break glyphs */
		if (mon->alertness < ALERTNESS_ALERT) {
			chance = 0;
		}

		/* Glyphs are hard to break */
		move_chance = MIN(move_chance, chance);
	}

    /* Only flying creatures can pass chasms */
    if (square_ischasm(c, grid)) {
        if (!rf_has(mon->race->flags, RF_FLYING)) {
			return 0;
		}
	}

	/* Feature is passable */
	if (square_ispassable(c, grid)) {
		/* Glyphs and chasms are handled above, everything else is fine */
		return move_chance;
	} else {
		/* Granite, Quartz, Rubble */
		if (square_iswall(c, grid) && !square_isdoor(c, grid)) {
			/* Impassible except for monsters that move through walls */
			if (rf_has(mon->race->flags, RF_PASS_WALL) ||
				rf_has(mon->race->flags, RF_KILL_WALL)) {
				return move_chance;
			} else if (rf_has(mon->race->flags, RF_TUNNEL_WALL) &&
					(mon->alertness >= ALERTNESS_ALERT)) {
				/* Alert monsters can slowly tunnel through walls */
				return move_chance;
			}
			return 0;
		}

		/* Doors */
		if (square_isdoor(c, grid)) {
			int unlock_chance = 0;
			int bash_chance = 0;

			/* Some monsters can simply pass through doors */
			if (rf_has(mon->race->flags, RF_PASS_DOOR) ||
				rf_has(mon->race->flags, RF_PASS_WALL)) {
				return move_chance;
			}

			/* Unwary monsters won't open doors in vaults or interesting rooms*/
			if ((mon->alertness < ALERTNESS_ALERT) &&
				square_isvault(c, grid)) {
				return 0;
			}

			/* No monsters open secret doors in vaults or interesting rooms */
			if (square_issecretdoor(c, grid) && square_isvault(c, grid)) {
				return 0;
			}

			/* Monster can open doors that are not jammed */
			if (rf_has(mon->race->flags, RF_OPEN_DOOR) &&
				!square_isjammeddoor(c, grid)) {
				/* Closed doors and secret doors
				 * Note:  This section will have to be rewritten if
				 * secret doors can be jammed or locked as well. */
				if (!square_islockeddoor(c, grid)) {
					/* It usually takes two turns to open a door
					 * and move into the doorway. */
					return move_chance;
				} else if (rf_has(mon->race->flags, RF_UNLOCK_DOOR)) {
					/* Lock difficulty (power + 5) */
					int difficulty = square_door_lock_power(c, grid) + 5;

					/* Unlocking skill equals monster perception */
					int skill = monster_skill(mon, SKILL_PERCEPTION);

					/* We ignore the fact that it takes extra time to
					 * open the door and walk into the entranceway. */
					unlock_chance = success_chance(10, skill, difficulty);
				}
			}

			/* Monster can bash doors */
			if (rf_has(mon->race->flags, RF_BASH_DOOR)) {
				/* Door difficulty (power + 2)
				 * Just because a door is difficult to unlock
				 * shouldn't mean that it's hard to bash.  Until the
				 * character door bashing code is changed, however,
				 * we'll stick with this.
				 */
				int difficulty = square_islockeddoor(c, grid) ?
					square_door_lock_power(c, grid) :
					square_door_jam_power(c, grid);

				/* Calculate bashing ability (ie effective strength) */
				int skill = monster_stat(mon, STAT_STR) * 2;

				/* Note that monsters "fall" into the entranceway in the same
				 * turn that they bash the door down. */
				bash_chance = success_chance(10, skill, difficulty);
			}

			/* A monster cannot both bash and unlock a door in the same
			 * turn.  It needs to pick one of the two methods to use. */
			if ((unlock_chance > bash_chance) || (bash_chance == 0)) {
				*bash = false;
			} else {
				*bash = true;
			}
			return MIN(move_chance, MAX(unlock_chance, bash_chance));
		}

		/* Any grid that isn't explicitly made passable is impassable. */
		return 0;
	}

}

/**
 * Counts the number of monsters adjacent to a given square
 */
int adj_mon_count(struct loc grid)
{
	int d, count = 0;

	for (d = 0; d < 8; d++) {
		if (square_monster(cave, loc_sum(grid, ddgrid_ddd[d]))) {
			count++;
		}
	}

	return count;
}
/**
 * The square of the distance between two points.
 *
 * Used when we need a fine-grained ordering of euclidean distance.
 * e.g. helps an archer who is stuck against a wall to find his way out.
 */
static int distance_squared(struct loc grid1, struct loc grid2)
{
	int y_diff = grid1.y - grid2.y;
	int x_diff = grid1.x - grid2.x;
	return y_diff * y_diff + x_diff * x_diff;
}

/**
 * ------------------------------------------------------------------------
 * Monster wandering and alertness
 * Also other diverse things, need to re-org
 * ------------------------------------------------------------------------ */
/**
 * Calculate minimum and desired combat ranges.  -BR-
 *
 * Afraid monsters will set this to their maximum flight distance.
 * Currently this is recalculated every turn - if it becomes a significant
 * overhead it could be calculated only when something has changed (monster HP,
 * chance of escaping, etc)
 */
static void monster_find_range(struct monster *mon)
{
	/* Monsters will run up to z_info->flee_range grids out of sight */
	int flee_range = z_info->max_sight + z_info->flee_range;

	/* All "afraid" monsters will run away */
	if (mon->stance == STANCE_FLEEING) {
		mon->min_range = flee_range;
	} else {
		/* Other monsters default to range 1 */
		mon->min_range = 1;

		/* Creatures that don't move never like to get too close */
		if (rf_has(mon->race->flags, RF_NEVER_MOVE)) {
			mon->min_range += 3;
		}

		/* Spies have a high minimum range */
		if (rf_has(mon->race->flags, RF_SMART) &&
			rsf_has(mon->race->spell_flags, RSF_SHRIEK) &&
			(mon->stance != STANCE_AGGRESSIVE)) {
			mon->min_range = 10;
		}
	}

	/* Nearby monsters won't run away */
	if ((mon->cdis < z_info->turn_range) &&
		(mon->mspeed < player->state.speed)) {
		mon->min_range = 1;
	}

	/* Now find preferred range */
	mon->best_range = mon->min_range;

	/* Breathers like range 2 */
	if ((mon->race->freq_ranged > 15) && !rf_has(mon->race->flags, RF_QUESTOR)){
		if (monster_breathes(mon) && (mon->best_range < 6)) {
			mon->best_range = 2;
		} else if (mon->mana >= z_info->mana_max / 5) {
			/* Specialized ranged attackers will sit back */
			mon->best_range += (mon->race->freq_ranged - 15) / 5;
			mon->best_range = MIN(mon->best_range, 8);
			mon->min_range = mon->best_range - 1;
		}
	}
	
	/* Deal with the 'truce' on Morgoth's level (overrides everything else) */
	if (player->truce && (mon->min_range < 5)) { 
		mon->min_range = 5;
		mon->best_range = 5;
	}
}

/**
 * Determine whether a monster is active or passive
 */
static bool monster_check_active(struct monster *mon)
{
	/* Monsters with targets are all active */
	if (!loc_eq(mon->target.grid, loc(0, 0))) {
		return true;
	} else if (mon->stance == STANCE_FLEEING) {
		/* Monsters that are fleeing are active, so as to get far enough away */
		return true;
	} else if (rf_has(mon->race->flags, RF_QUESTOR) && player->on_the_run) {
		/* Morgoth is always active during the escape */
		return true;
	} else if ((mon->race->level > 17) && (player->depth == 0)) {
		/* Pursuing creatures are always active at the Gates */
		return true;
	} else if (rf_has(mon->race->flags, RF_SHORT_SIGHTED)) {
		/* Short sighted monsters are active when the player is *very* close */
		if (mon->cdis <= 2) return true;
	} else {
		/* Monsters that can see the player are active */
		if (los(cave, mon->grid, player->grid)) return true;
		
		/* Monsters that can hear the player are active */
		if (flow_dist(cave->player_noise, mon->grid) < 20) return true;

		/* Monsters that can smell the player are active */
		if (monster_can_smell(mon)) return true;
	}

	return false;
}


/**
 * Determine the next move for an unwary wandering monster
 */
static bool get_move_wander(struct monster *mon, struct loc *tgrid)
{
	struct loc grid1 = mon->grid, grid;
	struct monster_group *group = monster_group_by_index(cave,
														 mon->group_info.index);
	struct monster_race *race = mon->race;
	bool random_move = false;
	bool no_move = false;
	int d;
    int dist;
	int closest = z_info->flow_max - 1;
					
	/* Deal with monsters that don't have a destination */
	if (loc_eq(group->flow.centre, loc(0, 0))) {
		/* Some monsters cannot move at all */
		if (rf_has(race->flags, RF_NEVER_MOVE)) {
			return false;
		} else if (rf_has(race->flags, RF_SHORT_SIGHTED) ||
				   rf_has(race->flags, RF_HIDDEN_MOVE)) {
			/* Some just never wander */
			return false;
		} else {
			/* Many monsters can only make random moves */
			random_move = true;
		}
	} else {
		/* Deal with some special cases for monsters that have a destination */
		int group_size = monster_group_size(cave, mon);
		struct mon_group_list_entry *list_entry = group->member_list;
		int group_furthest = 0;
		int group_sleepers = 0;
		struct loc sleeper_grid = loc(0, 0);
		bitflag mask[RF_SIZE];
		bool hoarder = false;

        /* How far is the monster from its wandering destination? */
        dist = flow_dist(group->flow, grid1);

		/* Check out monsters in the same group */
		while (list_entry) {
			struct monster *mon1 = cave_monster(cave, list_entry->midx);
			if (mon1->alertness < ALERTNESS_UNWARY) {
				group_sleepers++;
				if (group_sleepers == 1) {
					sleeper_grid = mon1->grid;
				}
				if (mon1->wandering_dist > group_furthest) {
					group_furthest = mon1->wandering_dist;
				}
			}
			list_entry = list_entry->next;
		}

		/* No wandering on the Gates level */
		if (player->depth == 0) {
			return false;
		}

		/* No wandering in the throne room during the truce */
		if (player->truce) {
			return false;
		}

		/* Determine if the monster has a hoard */
		create_mon_flag_mask(mask, RFT_DROP, RFT_MAX);
		if (rf_is_inter(mon->race->flags, mask)) {
			hoarder = true;
		}

		/* Treasure-hoarding territorial monsters stay still at their hoard...*/
		if (rf_has(race->flags, RF_TERRITORIAL) && hoarder && (dist == 0)) {
			/* Very occasionally fall asleep */
			if (one_in_(100) && !in_tutorial() &&
					!rf_has(race->flags, RF_NO_SLEEP)) {
				set_alertness(mon, rand_range(ALERTNESS_MIN,
											  ALERTNESS_UNWARY - 1));
			}
			return false;
		}

		/* If the destination is too far away, pick a new one */
		if (dist > z_info->wander_range) {
			monster_group_new_wandering_flow(cave, mon, loc(0, 0));
		}

		/* If the group is at the destination and not pausing, then start */
		if ((group->wandering_pause == 0) && (dist <= 0)) {
			group->wandering_pause = randint1(50) * group_size;
		} else if (group->wandering_pause > 1) {
			/* If the group is pausing, then decrease the pause counter */
			random_move = true;
			group->wandering_pause--;
		} else if (group->wandering_pause == 1) {
			/* If the group has finished pausing at an old destination,
			 * choose a new destination */
			monster_group_new_wandering_flow(cave, mon, loc(0, 0));
			group->wandering_pause--;
		}

		/* If the monster is not making progress */
		if (dist >= mon->wandering_dist) {
			/* Possibly pick a new destination */
			if (one_in_(20 * group_size)) {
				monster_group_new_wandering_flow(cave, mon, loc(0, 0));
			}
		}

		/* Sometimes delay to let others catch up */
		if (dist < group_furthest - group_size) {
			if (one_in_(2)) no_move = true;
		}

		/* Unwary monsters won't wander off while others are sleeping */
		if ((mon->alertness < ALERTNESS_ALERT) && (group_sleepers > 0)) {
			/* Only set the new flow if needed */
			if (!loc_eq(group->flow.centre, sleeper_grid)) {
				monster_group_new_wandering_flow(cave, mon, sleeper_grid);
			}
			if (one_in_(2)) random_move = true;
		}

		/* Non-territorial monsters in vaults move randomly */
		if (!rf_has(race->flags, RF_TERRITORIAL) &&
			square_isvault(cave, mon->grid)) {
			random_move = true;
		}

		/* Update the wandering_dist */
		group->dist = dist;
	}

	if (no_move) return false;

	/* Do a random move if needed */
	if (random_move) {        
		/* Mostly stay still */
		if (!one_in_(4)) {
			return false;
		} else {
			/* Random direction */
			grid = loc_sum(grid1, ddgrid_ddd[randint0(8)]);

			/* Check Bounds */
			if (!square_in_bounds(cave, grid)) return false;

			/* Monsters in vaults shouldn't leave them */
			if (square_isvault(cave, mon->grid) &&
				!square_isvault(cave, grid)) return false;

			/* Save the location */
			*tgrid = grid;
		}
	} else {
		/* Smart monsters who are at the stairs they are aiming for
		 * leave the level */
		if (rf_has(race->flags, RF_SMART) &&
			!rf_has(race->flags, RF_TERRITORIAL) &&
			(player->depth != z_info->dun_depth) && 
			square_isstairs(cave, mon->grid) && (mon->wandering_dist == 0)) {
			if (monster_is_visible(mon)) {
				if (square_isdownstairs(cave, mon->grid)) {
					add_monster_message(mon, MON_MSG_GO_DOWN_STAIRS, true);
				} else {
					add_monster_message(mon, MON_MSG_GO_UP_STAIRS, true);
				}
			}

			/* Stop pausing to allow others to use the stairs */
			group->wandering_pause = 0;
			
			delete_monster(cave, mon->grid);
			return false;
		}

		/* Using flow information, check nearby grids, diagonals first. */
		for (d = 7; d >= 0; d--) {
			/* Get the location */
			grid = loc_sum(grid1, ddgrid_ddd[d]);

			/* Check Bounds */
			if (!square_in_bounds(cave, grid)) continue;

			dist = flow_dist(group->flow, grid);

			/* Ignore grids that are further than the current favourite */
			if (closest < dist) continue;

			/* Save the location */
			closest = dist;
			*tgrid = grid;
		}
		
		/* If no useful square to wander into was found, then abort */
		if (closest == z_info->flow_max - 1) {
			return false;
		}
	}

	/* Success */
	return true;
}
/**
 * ------------------------------------------------------------------------
 * Monster movement routines
 * These routines, culminating in get_move(), choose if and where a monster
 * will move on its turn
 * ------------------------------------------------------------------------ */
/**
 * "Do not be seen."
 *
 * Monsters in LOS that want to retreat are primarily interested in
 * finding a nearby place that the character can't see into.
 * Search for such a place with the lowest cost to get to up to 15
 * grids away.
 *
 * Look outward from the monster's current position in a square-
 * shaped search pattern.  Calculate the approximate cost in monster
 * turns to get to each passable grid, using a crude route finder.  Penal-
 * ize grids close to or approaching the character.  Ignore hiding places
 * with no safe exit.  Once a passable grid is found that the character
 * can't see, the code will continue to search a little while longer,
 * depending on how pricey the first option seemed to be.
 *
 * If the search is successful, the monster will target that grid,
 * and (barring various special cases) run for it until it gets there.
 *
 * We use a limited waypoint system (see function "get_route_to_target()"
 * to reduce the likelihood that monsters will get stuck at a wall between
 * them and their target (which is kinda embarrassing...).
 *
 * This function does not yield perfect results; it is known to fail
 * in cases where the previous code worked just fine.  The reason why
 * it is used is because its failures are less common and (usually)
 * less embarrassing than was the case before.  In particular, it makes
 * monsters great at not being seen.
 *
 * This function is fairly expensive.  Call it only when necessary.
 */
static bool get_move_find_safety(struct monster *mon, struct loc *tgrid)
{
	int i, j, d, x, y;
	int range = z_info->hide_range;
	int countdown = range;
	int least_cost = 100;
	struct loc least_cost_grid = loc(0, 0);
	int chance, cost, parent_cost;
	bool dummy;
	bool stair;

	/* Origin of the table as an actual dungeon grid */
	struct loc origin = loc_diff(loc(range, range), mon->grid);

	/* Allocate and initialize a table of movement costs.
	 * Both axes must be (2 * range + 1). */
	uint8_t **safe_cost;

	safe_cost = mem_zalloc((range * 2 + 1) * sizeof(uint8_t*));
	for (i = 0; i < range * 2 + 1; i++) {
		safe_cost[i] = mem_zalloc((range * 2 + 1) *
								  sizeof(uint8_t));
	}

	/* Mark the origin */
	safe_cost[range][range] = 1;

	/* If the character's grid is in range, mark it as being off-limits */
	if ((ABS(mon->grid.y - player->grid.y) <= range) &&
	    (ABS(mon->grid.x - player->grid.x) <= range)) {
		safe_cost[player->grid.y + origin.y][player->grid.x + origin.x] = 100;
	}

	/* Work outward from the monster's current position */
	for (d = 0; d < range; d++) {
		for (y = range - d; y <= range + d; y++) {
			for (x = range - d; x <= range + d;) {
				struct loc grid = loc(x, y);
				int x_tmp;

				/* Scan grids of top and bottom rows, just outline other rows */
				if ((y != range - d) && (y != range + d)) {
					if (x == range + d) {
						x_tmp = 999;
					} else {
						x_tmp = range + d;
					}
				} else {
					x_tmp = x + 1;
				}

				/* Grid and adjacent grids must be legal */
				if (!square_in_bounds_fully(cave, loc_diff(grid, origin))) {
					x = x_tmp;
					continue;
				}

				/* Grid is inaccessible (or at least very difficult to enter) */
				if ((safe_cost[y][x] == 0) || (safe_cost[y][x] >= 100)) {
					x = x_tmp;
					continue;
				}

				/* Get the accumulated cost to enter this grid */
				parent_cost = safe_cost[y][x];

				/* Scan all adjacent grids */
				for (i = 0; i < 8; i++) {
					struct loc grid1 = loc_sum(grid, ddgrid_ddd[i]);
					struct loc actual = loc_diff(grid1, origin);

					/* Check bounds */
					if ((grid1.y < 0) || (grid1.y > range*2) ||
						(grid1.x < 0) || (grid1.x > range*2)) continue;

					/* Handle grids with empty cost and passable grids
					 * with costs we have a chance of beating. */
					if ((safe_cost[grid1.y][grid1.x] == 0) ||
						((safe_cost[grid1.y][grid1.x] > parent_cost + 1) &&
						 (safe_cost[grid1.y][grid1.x] < 100))) {
						/* Get the cost to enter this grid */
						chance = monster_entry_chance(cave, mon, actual,&dummy);

						/* Impassable */
						if (!chance) {
							/* Cannot enter this grid */
							safe_cost[grid1.y][grid1.x] = 100;
							continue;
						}

						/* Calculate approximate cost (in monster turns) */
						cost = 100 / chance;

						/* Next to character */
						if (distance(loc_diff(grid1, origin), player->grid)
							<= 1) {
							/* Don't want to maneuver next to the character */
							cost += 3;
						}

						/* Mark this grid with a cost value */
						safe_cost[grid1.y][grid1.x] = parent_cost + cost;

						/* Check if it is a stair and the monster can use it */
						stair = square_isstairs(cave, actual) &&
							rf_has(mon->race->flags, RF_SMART) &&
							!rf_has(mon->race->flags, RF_TERRITORIAL);

						/* Character can't see this grid, or it is a stair... */
						if (!square_isview(cave, actual) || stair) {
							int this_cost = safe_cost[grid1.y][grid1.x];

							/* Penalize grids that approach character */
							if (ABS(player->grid.y - actual.y) <
							    ABS(mon->grid.y - actual.y)) {
								this_cost *= 2;
							}
							if (ABS(player->grid.x - actual.x) <
							    ABS(mon->grid.x - actual.x)) {
								this_cost *= 2;
							}

							/* Value stairs very highly */
							if (stair) {
								this_cost /= 2;
							}

							/* Accept lower-cost, sometimes accept same-cost
							 * options */
							if ((least_cost > this_cost) ||
							    ((least_cost == this_cost) && one_in_(2))) {
								bool has_escape = false;

								/* Scan all adjacent grids for escape routes */
								for (j = 0; j < 8; j++) {
									/* Calculate real adjacent grids */
									struct loc grid2 = loc_sum(grid1,
															   ddgrid_ddd[i]);

									/* Check bounds */
									if (!square_in_bounds(cave, grid2))
										continue;

									/* Take any passable grid not in LOS */
									if (!square_isview(cave, grid2) &&
									    monster_entry_chance(cave, mon, grid2,
															 &dummy)) {
										/* Not a one-grid cul-de-sac */
										has_escape = true;
										break;
									}
								}

								/* Ignore cul-de-sacs other than stairs */
								if ((has_escape == false) && !stair) continue;

								least_cost = this_cost;
								least_cost_grid = grid1;

								/* Look hard for alternative hiding places if
								 * this one seems pricey. */
								countdown = 1 + least_cost - d;
							}
						}
					}
				}

				/* Adjust x as instructed */
				x = x_tmp;
			}
		}

		/* We found a good place a while ago, and haven't done better
		 * since, so we're probably done. */
		if (countdown-- <= 0) break;
	}

	/* Free memory */
	for (i = 0; i < range * 2 + 1; i++) {
		mem_free(safe_cost[i]);
	}
	mem_free(safe_cost);

	/* We found a place that can be reached in reasonable time */
	if (least_cost < 50) {
		/* Convert to actual dungeon grid. */
		struct loc grid = loc_diff(least_cost_grid, origin);

		/* Move towards the hiding place */
		*tgrid = grid;

		/* Target the hiding place */
		mon->target.grid = grid;

		return true;
	}

	/* No good place found */
	return false;
}

/**
 * Helper function for monsters that want to retreat from the character.
 * Used for any monster that is terrified, frightened, is looking for a
 * temporary hiding spot, or just wants to open up some space between it
 * and the character.
 *
 * If the monster is well away from danger, let it relax.
 * If the monster's current target is not in LOS, use it (+).
 * If the monster is not in LOS, and cannot pass through walls, try to
 * use flow (noise) information.
 * If the monster is in LOS, even if it can pass through walls,
 * search for a hiding place (helper function "find_safety()").
 * If no hiding place is found, and there seems no way out, go down
 * fighting.
 *
 * If none of the above solves the problem, run away blindly.
 *
 * (+) There is one exception to the automatic usage of a target.  If the
 * target is only out of LOS because of "knight's move" rules (distance
 * along one axis is 2, and along the other, 1), then the monster will try
 * to find another adjacent grid that is out of sight.  What all this boils
 * down to is that monsters can now run around corners properly!
 *
 * Return true if the monster did actually want to do anything.
 */
static bool get_move_retreat(struct monster *mon, struct loc *tgrid)
{
	struct monster_race *race = mon->race;
	int i;
	struct loc grid;
	bool dummy;

	/* If it can call for help, then it might */
	if (rf_has(race->spell_flags, RSF_SHRIEK) &&
		(randint0(100) < race->freq_ranged)) {
		do_mon_spell(RSF_SHRIEK, mon, square_isview(cave, mon->grid));
		return false;
	}

	/* If the monster is well away from danger, let it relax. */
	if (mon->cdis >= z_info->flee_range) {
		return false;
	}

	/* Intelligent monsters that are fleeing can try to use stairs */
	if (rf_has(race->flags, RF_SMART) && !rf_has(race->flags, RF_TERRITORIAL) &&
		(mon->stance == STANCE_FLEEING)) {
		if (square_isstairs(cave, mon->grid)) {
			*tgrid = mon->grid;
			return true;
		}

		/* Check for adjacent stairs and move towards one */
		for (i = 0; i < 8; i++) {
			grid = loc_sum(mon->grid, ddgrid_ddd[i]);

			/* Check for (accessible) stairs */
			if (square_isstairs(cave, grid) &&
				(monster_entry_chance(cave, mon, grid, &dummy) > 0) &&
				!square_isplayer(cave, grid)) {
				*tgrid = grid;
				return true;
			}
		}
	}

	/* Monsters that like ranged attacks a lot (e.g. archers) try to stay
	 * in good shooting locations */
	if (race->freq_ranged >= 50) {
		int start = randint0(8);
		bool acceptable = false;
		int best_score = 0;
		struct loc best_grid = mon->grid;
		int dist;
				
		/* The 'score to beat' as the score for the monster's current square */
		dist = distance_squared(mon->grid, player->grid);
		best_score += dist;
		if (projectable(cave, mon->grid, player->grid, PROJECT_STOP) &&
			(mon->cdis > 1)) {
			best_score += 100;
		}

		/* The position is only acceptable if it isn't adjacent to the player */
		if (mon->cdis > 1) acceptable = true;
	
		/* Set the hacky path ignore variable so that the project_path()
		 * function doesn't consider the monster's current location to
		 * block line of fire. */
		cave->project_path_ignore = mon->grid;
	
		/* Look for adjacent shooting places */
		for (i = start; i < 8 + start; i++) {
			int score = 0;

			grid = loc_sum(mon->grid, ddgrid_ddd[i % 8]);
			dist = distance_squared(grid, player->grid);

			/* Check Bounds */
			if (!square_in_bounds(cave, grid)) continue;
						
			/* Skip the player's square */
			if (square_isplayer(cave, grid)) continue;
			
			/* Grid must be pretty easy to enter */
			if (monster_entry_chance(cave, mon, grid, &dummy) < 50) continue;
			
			/* Skip adjacent squares */
			if (distance(grid, player->grid) == 1) continue;

			/* Any position non-adjacent to the player will be acceptable */
			acceptable = true;
			
			/* Reward distance from player */
			score += dist;
			
			/* Reward having a shot at the player */
			if (projectable(cave, grid, player->grid, PROJECT_STOP)) {
				score += 100;
			}

			if (score > best_score) {
				best_score = score;
				best_grid = grid;
			}
		}

		/* Unset the hacky path ignore variable so that the project_path()
		 * function didn't consider the monster's current location to
		 * block line of fire. */
		cave->project_path_ignore = loc(0, 0);
		
		if (acceptable) {
			*tgrid = best_grid;

			/* Success */
			return true;
		} else if ((mon->stance != STANCE_FLEEING) &&
				   !rf_has(race->flags, RF_UNIQUE) &&
				   monster_is_visible(mon)) {
			/* This step is artificial stupidity for archers and other serious
			 * ranged weapon users.  They only evade you properly near walls
			 * if they are: afraid or uniques or invisible.
			 * Otherwise things are a bit too annoying */
			return false;
		}
	}

	/* Monster has a target */
	if (!loc_eq(mon->target.grid, loc(0, 0))) {
		/* It's out of LOS; keep using it, except in "knight's move" cases */
		if (!square_isview(cave, mon->target.grid)) {
			/* Get axis distance from character to current target */
			int dist_y = ABS(player->grid.y - mon->target.grid.y);
			int dist_x = ABS(player->grid.x - mon->target.grid.x);

			/* It's only out of LOS because of "knight's move" rules */
			if (((dist_y == 2) && (dist_x == 1)) ||
				((dist_y == 1) && (dist_x == 2))) {
				/* If there is another grid adjacent to the monster that
				 * the character cannot see into, and it isn't any harder
				 * to enter, use it instead.  Prefer diagonals. */
				for (i = 7; i >= 0; i--) {
					grid = loc_sum(mon->grid, ddgrid_ddd[i]);

					/* Check Bounds */
					if (!square_in_bounds(cave, grid)) continue;
					if (square_isview(cave, grid)) continue;
					if (loc_eq(grid, mon->target.grid)) continue;
					if (monster_entry_chance(cave, mon, mon->target.grid,
											 &dummy) >
					    monster_entry_chance(cave, mon, grid, &dummy)) continue;

					mon->target.grid = grid;
					break;
				}
			}

			/* Move towards the target */
			*tgrid = mon->target.grid;
			return true;
		} else if (!square_isstairs(cave, mon->target.grid)) {
			/* It's in LOS, but not a stair; cancel it. */
			mon->target.grid = loc(0, 0);
		}
	}

	/* The monster is not in LOS, but thinks it's still too close. */
	if (!square_isview(cave, mon->grid)) {
        /* Run away from noise */
        if (flow_dist(mon->flow, mon->grid) < z_info->flow_max) {
			bool done = false;

            /* Look at adjacent grids, diagonals first */
            for (i = 7; i >= 0; i--) {
				grid = loc_sum(mon->grid, ddgrid_ddd[i]);
 
                /* Check bounds */
                if (!square_in_bounds(cave, grid)) continue;

                /* Accept the first non-visible grid with a higher cost */
                if (flow_dist(mon->flow, grid) >
					flow_dist(mon->flow, mon->grid)) {
                    if (!square_isview(cave, grid)) {
                        *tgrid = grid;
                        done = true;
                        break;
                    }
                }
            }

            /* Return if successful */
            if (done) return true;
        }
		/* No flow info, or don't need it -- see bottom of function */
	} else {
		/* The monster is in line of sight. */
		int prev_dist = flow_dist(mon->flow, mon->grid);
		int start = randint0(8);

		/* Look for adjacent hiding places */
		for (i = start; i < 8 + start; i++) {
			grid = loc_sum(mon->grid, ddgrid_ddd[i % 8]);

			/* Check Bounds */
			if (!square_in_bounds(cave, grid)) continue;

			/* No grids in LOS */
			if (square_isview(cave, grid)) continue;

			/* Grid must be pretty easy to enter */
			if (monster_entry_chance(cave, mon, grid, &dummy) < 50) continue;

			/* Accept any grid that doesn't have a lower flow (noise) cost. */
			if (flow_dist(mon->flow, grid) >= prev_dist) {
				*tgrid = grid;

				/* Success */
				return true;
			}
		}

		/* Find a nearby grid not in LOS of the character. */
		if (get_move_find_safety(mon, tgrid) == true) return true;

		/* No safe place found; a monster in LOS and close will turn to fight */
		if (square_isview(cave, mon->grid) &&
		    ((mon->cdis < z_info->turn_range) ||
			 (mon->mspeed < player->state.speed)) &&
			!player->truce && (race->freq_ranged < 50)) {			
			/* Message if visible */
			if (monster_is_visible(mon)) {
				/* Dump a message */
				add_monster_message(mon, MON_MSG_PANIC, true);
			}

            /* Boost morale and make the monster aggressive */
            mon->tmp_morale = MAX(mon->tmp_morale + 60, 60);
            calc_morale(mon);
            calc_stance(mon);
            mflag_on(mon->mflag, MFLAG_AGGRESSIVE);

			return true;
		}
	}

	/* Move directly away from character. */
	*tgrid = loc_diff(mon->grid, loc_diff(player->grid, mon->grid));

	/* We want to run away */
	return true;
}

/**
 * Helper function for monsters that want to advance toward the character.
 * Assumes that the monster isn't frightened, and is not in LOS of the
 * character.
 *
 * Ghosts and rock-eaters do not use flow information, because they
 * can - in general - move directly towards the character.  We could make
 * them look for a grid at their preferred range, but the character
 * would then be able to avoid them better (it might also be a little
 * hard on those poor warriors...).
 *
 * Other monsters will use target information, then their ears, then their
 * noses (if they can), and advance blindly if nothing else works.
 *
 * When flowing, monsters prefer non-diagonal directions.
 *
 * XXX - At present, this function does not handle difficult terrain
 * intelligently.  Monsters using flow may bang right into a door that
 * they can't handle.  Fixing this may require code to set monster
 * paths.
 */
static void get_move_advance(struct monster *mon, struct loc *tgrid)
{
	int i;
 	int closest = z_info->flow_max;
	bool can_use_scent = false;
	struct monster_lore *lore = get_lore(mon->race);
    
	/* Some monsters don't try to pursue when out of sight */
	if (rf_has(mon->race->flags, RF_TERRITORIAL) &&
		!los(cave, player->grid, mon->grid)) {
		*tgrid = mon->grid;

        /* Remember that the monster behaves like this */
		rf_on(lore->flags, RF_TERRITORIAL);

		/* Sometimes become unwary and wander back to its lair */
		if (one_in_(10) && (mon->alertness >= ALERTNESS_ALERT)) {
			set_alertness(mon, mon->alertness - 1);
		}

		return;
	}

	/* Use target information if available */
	if (!loc_eq(mon->target.grid, loc(0, 0))) {
		*tgrid = mon->target.grid;
		return;
	}

	/* If we can't hear noises */
	if (flow_dist(mon->flow, mon->grid) >= z_info->flow_max) {
		/* Otherwise, try to follow a scent trail */
		if (monster_can_smell(mon)) {
			can_use_scent = true;
		} else {
			/* Sight but no sound means blocked by a chasm, get out of there! */
			if (los(cave, mon->grid, player->grid)) {
				get_move_retreat(mon, tgrid);
				return;
			} else {
				/* No sound, no scent, no sight: advance blindly */
				*tgrid = player->grid;
				return;
			}
		}
	}

	/* Using flow information.  Check nearby grids, diagonals first. */
	for (i = 7; i >= 0; i--) {
		/* Get the location */
		struct loc grid = loc_sum(mon->grid, ddgrid_ddd[i]);

		/* Check Bounds */
		if (!square_in_bounds(cave, grid)) continue;

		/* We're following a scent trail */
		if (can_use_scent) {
			int age = get_scent(cave, grid);
			if (age == -1) continue;

			/* Accept younger scent */
			if (closest < age) continue;
			closest = age;
		} else {
			/* We're using sound */
			int dist = flow_dist(mon->flow, grid);

			/* Accept louder sounds */
			if (closest < dist) continue;
			closest = dist;
		}

		/* Save the location */
		*tgrid = grid;
	}
}

/**
 * This determines how vulnerable the player is to monster attacks
 * It combines elements for available spaces to attack from and for
 * the player's condition and other monsters attacking
 *
 * I'm sure it could be further improved
 */
static int get_move_calc_vulnerability(struct loc mgrid)
{
	struct loc pgrid = player->grid;
	int vulnerability = 0;

    /* Determine the main direction from the player to the monster */
    int dir = rough_direction(pgrid, mgrid);
    
    /* Extract the deltas from the direction */
    int dy = ddy[dir];
    int dx = ddx[dir];

	/* If monster in an orthogonal direction   753
	 *                                         8@1 m
	 *                                         642 */
	if (dy * dx == 0) {
		int i;
		/* Note array indices are one less than the diagram directions */
		struct loc grid[8] = { loc_sum(pgrid, loc(dx, dy)),
							   loc_sum(pgrid, loc(dx - dy, dx + dy)),
							   loc_sum(pgrid, loc(dx + dy, dy - dx)),
							   loc_sum(pgrid, loc(-dy, dx)),
							   loc_sum(pgrid, loc(dy, -dx)),
							   loc_sum(pgrid, loc(-dx - dy, dx - dy)),
							   loc_sum(pgrid, loc(dy - dx, -dx - dy)),
							   loc_sum(pgrid, loc(-dx, -dy))};

		/* Increase vulnerability for each open square towards the monster */
		for (i = 0; i < 5; i++) {
			if (square_isprojectable(cave, grid[i])) vulnerability++;
		}
		
		/* Increase for monsters already engaged with the player
		 * (but not the one directly between)... */
		for (i = 1; i < 5; i++) {
			if (square_monster(cave, grid[i])) vulnerability++;
		}

		/* ...especially if they are behind the player */
		for (i = 5; i < 8; i++) {
			if (square_monster(cave, grid[i])) vulnerability += 2;
		}
	} else {
		/* If monster in a diagonal direction   875 
		 *                                      6@3 
		 *                                      421 
		 *                                          m */
		int i;

		/* Note array indices are one less than the diagram directions */
		struct loc grid[8] = { loc_sum(pgrid, loc(dx, dy)),
							   loc_sum(pgrid, loc(0, dy)),
							   loc_sum(pgrid, loc(dx, 0)),
							   loc_sum(pgrid, loc(-dy, dx)),
							   loc_sum(pgrid, loc(dy, -dx)),
							   loc_sum(pgrid, loc(0, -dy)),
							   loc_sum(pgrid, loc(-dx, 0)),
							   loc_sum(pgrid, loc(-dx, -dy))};

		/* Increase vulnerability for each open square towards the monster */
		for (i = 0; i < 5; i++) {
			if (square_isprojectable(cave, grid[i])) vulnerability++;
		}
		
		/* Increase for monsters already engaged with the player
		 * (but not the one directly between)... */
		for (i = 1; i < 5; i++) {
			if (square_monster(cave, grid[i])) vulnerability++;
		}

		/* ...especially if they are behind the player */
		for (i = 5; i < 8; i++) {
			if (square_monster(cave, grid[i])) vulnerability += 2;
		}
	}
	
	/* Take player's health into account */
	switch (health_level(player->chp, player->mhp)) {
		case  HEALTH_WOUNDED: vulnerability += 1; break;  /* <= 75% health */
		case  HEALTH_BADLY_WOUNDED:vulnerability += 1; break;/* <= 50% health */
		case  HEALTH_ALMOST_DEAD: vulnerability += 2; break; /* <= 25% health */
	}

	/* Take player's conditions into account */
	if (player->timed[TMD_BLIND] || player->timed[TMD_IMAGE] ||
		player->timed[TMD_CONFUSED] || player->timed[TMD_AFRAID] ||
		player->timed[TMD_ENTRANCED] || (player->timed[TMD_STUN] > 50) ||
		player->timed[TMD_SLOW]) {
		vulnerability += 2;
	}

	return vulnerability;
}


/**
 * This determines how hesitant the monster is to attack.
 * If the hesitance is lower than the player's vulnerability, it will attack
 *
 * The main way to gain hesitance is to have similar smart monsters who could
 * gang up if they waited for the player to get into the open.
 */

static int get_move_calc_hesitance(struct monster *mon)
{
	int x, y;
	int hesitance = 1;

	/* Gain hesitance for up to one nearby similar monster
	 * who isn't yet engaged in combat */
	for (y = -5; y <= +5; y++) {
		for (x = -5; x <= +5; x++) {
			struct loc grid = loc_sum(mon->grid, loc(x, y)); 
			if (!loc_eq(grid, mon->grid) && square_in_bounds(cave, grid)) {
				struct monster *mon1 = square_monster(cave, grid);
				if (mon1 && similar_monsters(mon, mon1) &&
					(distance(grid, player->grid) > 1) && (hesitance < 2)) {
					hesitance++;
				}
			}
		}
	}

	/* Archers should be slightly more hesitant as they are
	 * in an excellent situation */
	if ((mon->race->freq_ranged > 30) && (hesitance == 2)) {
		hesitance++;
	}

	return hesitance;
}

/**
 * Choose the probable best direction for a monster to move in.  This
 * is done by choosing a target grid and then finding the direction that
 * best approaches it.
 *
 * Monsters that cannot move always attack if possible.
 * Frightened monsters retreat.
 * Monsters adjacent to the character attack if possible.
 *
 * Monster packs lure the character into open ground and then leap
 * upon him.  Monster groups try to surround the character.  -KJ-
 *
 * Monsters not in LOS always advance (this avoids player frustration).
 * Monsters in LOS will advance to the character, up to their standard
 * combat range, to a grid that allows them to target the character, or
 * just stay still if they are happy where they are, depending on the
 * tactical situation and the monster's preferred and minimum combat
 * ranges.
 * NOTE:  Here is an area that would benefit from more development work.
 *
 * Non-trivial movement calculations are performed by the helper
 * functions get_move_advance() and get_move_retreat(), which keeps
 * this function relatively simple.
 *
 * The variable "must_use_target" is used for monsters that can't
 * currently perceive the character, but have a known target to move
 * towards.  With a bit more work, this will lead to semi-realistic
 * "hunting" behavior.
 *
 * Return false if monster doesn't want to move or can't.
 */
static bool get_move(struct monster *mon, struct loc *tgrid, bool *fear,
					 bool must_use_target)
{
	struct monster_race *race = mon->race;
	struct monster_lore *lore = get_lore(race);
	int i, start;
	struct loc grid;

	/* Assume no movement */
	*tgrid = mon->grid;

	/* Some monsters will not move into sight of the player. */
	if (rf_has(race->flags, RF_HIDDEN_MOVE) &&
		 (square_isseen(cave, mon->grid) ||
		  square_seen_by_keen_senses(cave, mon->grid))) {
		/* Memorize lack of moves after a while. */
		if (monster_is_visible(mon) && one_in_(50)) {
			rf_on(lore->flags, RF_HIDDEN_MOVE);
		}

		/* If we are in sight, do not move */
		return false;
	}

	/* Morgoth will not move during the 'truce' */
	if (rf_has(race->flags, RF_QUESTOR) && player->truce) {
		return false;
	}

    /* Worm masses, nameless things and the like won't deliberately move
	 * towards the player if she is too far away */
	if (rf_has(race->flags, RF_MINDLESS) &&
		rf_has(race->flags, RF_TERRITORIAL) && (mon->cdis > 5)) {
		return false;
	}

	/* Monsters that cannot move will attack the character if he is
	 * adjacent.  Otherwise, they cannot move. */
	if (rf_has(race->flags, RF_NEVER_MOVE)) {
		/* Hack -- memorize lack of moves after a while. */
		if (monster_is_visible(mon) && one_in_(20)) {
			rf_on(lore->flags, RF_NEVER_MOVE);
		}

		/* Is character in range? */
		if (mon->cdis <= 1) {
			/* Kill. */
			*fear = false;
			*tgrid = player->grid;
			return true;
		}

		/* If we can't hit anything, do not move */
		return false;
	}

	/* Monster is only allowed to use targeting information. */
	if (must_use_target) {
		*tgrid = mon->target.grid;
		return true;
	}

	/* Is the monster scared? */
	*fear = ((mon->min_range >= z_info->flee_range) || (mon->stance == STANCE_FLEEING));

	/* Monster is frightened or terrified. */
	if (*fear) {
		/* The character is too close to avoid, and faster than we are */
		if ((mon->stance != STANCE_FLEEING) && (mon->cdis < z_info->turn_range)
			&& (player->state.speed > mon->mspeed)) {
			/* Recalculate range */
			monster_find_range(mon);

			/* Note changes in monster attitude */
			if (mon->min_range < mon->cdis) {
				/* Cancel fear */
				*fear = false;

				/* Charge! */
				*tgrid = player->grid;

				return true;
			}
		} else if (mon->cdis < z_info->flee_range) {
			/* Find and move towards a hidey-hole */
			get_move_retreat(mon, tgrid);
			return true;
		} else {
			/* Monster is well away from danger, no need to move */
			return false;
		}
	}

	/* If far too close, step back towards the monster's minimum range */
	if (!*fear && (mon->cdis < mon->min_range - 2)) {
		if (get_move_retreat(mon, tgrid)) {
			*fear = true;
			return true;
		} else {
			/* No safe spot -- charge */
			*tgrid = player->grid;
		}			
	}

	/* If the character is adjacent, back off, surround the player, or attack */
	if (!*fear && (mon->cdis <= 1)) {
		/* Smart monsters try harder to surround the player */
		if (rf_has(race->flags, RF_SMART)) {
			struct loc mgrid = mon->grid;
			int count = adj_mon_count(mgrid);
			int dy = player->grid.y - mon->grid.y;
			int dx = player->grid.x - mon->grid.x;

			start = randint0(8);

			/* Maybe move to a less crowded square near the player if we can */
			for (i = start; i < 8 + start; i++) {
				/* Pick squares near player */
				grid = loc_sum(player->grid, ddgrid_ddd[i % 8]);

				/* If also adjacent to monster */
				if (distance(mon->grid, grid) == 1) {
					/* if it is free... */
					if (square_ispassable(cave, grid) &&
						!square_monster(cave, grid)) {
						/* and has a lower count... */
						if ((adj_mon_count(grid) <= count) &&
							(rf_has(race->flags, RF_FLANKING) || one_in_(2))) {
							/* then maybe set it as a new target */
							*tgrid = grid;
							return true;
						}
					}
				}
			}
				
			/* If the monster didn't do that, check for end-corridor cases
			 * if player is in an orthogonal direction, eg:
			 *  X#A
			 *  Xo@
			 *  X#B */
			if (dy * dx == 0) {
				/* If walls on either side of monster (#) */
				struct loc wall1 = loc(mgrid.x + dy, mgrid.y + dx);
				struct loc wall2 = loc(mgrid.x - dy, mgrid.y - dx);
				if (!square_isprojectable(cave, wall1) &&
					!square_isprojectable(cave, wall2)) {
					/* If there is a monster in 1 of the 3 squares behind (X) */
					struct loc grid1 = loc_diff(wall1, loc(dx, dy));
					struct loc grid2 = loc_diff(mgrid, loc(dx, dy));
					struct loc grid3 = loc_diff(wall2, loc(dx, dy));
					struct loc grida = loc_sum(wall1, loc(dx, dy));
					struct loc gridb = loc_sum(wall2, loc(dx, dy));
					struct monster *mon1 = square_monster(cave, grid1);
					struct monster *mon2 = square_monster(cave, grid2);
					struct monster *mon3 = square_monster(cave, grid3);
					struct monster *mona = square_monster(cave, grida);
					struct monster *monb = square_monster(cave, gridb);
					if (mon1 || mon2 || mon3) {
						/* if 'A' and 'B' are free, go to one at random */
						if (!mona && !monb) {
							*tgrid = one_in_(2) ? grida : gridb;
							return true;
						} else if (!mona) {
							/* if 'A' is free, go there */
							*tgrid = grida;
							return true;
						} else if (!monb) {
							/* if 'B' is free, go there */
							*tgrid = gridb;
							return true;
						}
					}
				}
			} else {
				/* If player is in a diagonal direction, eg: 
				*  X#       XXX
				*  XoA  or  #o#
				*  X#@       A@ */
				struct loc gridn = loc_sum(mgrid, loc(0, -1));
				struct loc grids = loc_sum(mgrid, loc(0, 1));
				struct loc gride = loc_sum(mgrid, loc(1, 0));
				struct loc gridw = loc_sum(mgrid, loc(-1, 0));

				/* If walls north and south of monster ('#') */
				if (!square_isprojectable(cave, gridn) &&
					!square_isprojectable(cave, grids)) {
					/* If there is a monster in 1 of the 3 squares behind (X) */
					struct loc grid1 = loc_diff(mgrid, loc(dx, -1));
					struct loc grid2 = loc_diff(mgrid, loc(dx, 0));
					struct loc grid3 = loc_diff(mgrid, loc(dx, 1));
					struct loc grida = loc_sum(mgrid, loc(dx, 0));
					struct monster *mon1 = square_monster(cave, grid1);
					struct monster *mon2 = square_monster(cave, grid2);
					struct monster *mon3 = square_monster(cave, grid3);
					struct monster *mona = square_monster(cave, grida);
					if (mon1 ||	mon2 || mon3) {
						/* If 'A' is free, go there */
						if (!mona) {
							*tgrid = grida;
							return true;
						}
					}
				} else if (!square_isprojectable(cave, gride) &&
						   !square_isprojectable(cave, gridw)) {
					/* If walls east and west of monster (#) and there is
					 * a monster in one of the three squares behind (X) */
					struct loc grid1 = loc_diff(mgrid, loc(-1, dx));
					struct loc grid2 = loc_diff(mgrid, loc(0, dx));
					struct loc grid3 = loc_diff(mgrid, loc(1, dx));
					struct loc grida = loc_sum(mgrid, loc(0, dy));
					struct monster *mon1 = square_monster(cave, grid1);
					struct monster *mon2 = square_monster(cave, grid2);
					struct monster *mon3 = square_monster(cave, grid3);
					struct monster *mona = square_monster(cave, grida);
					if (mon1 ||	mon2 || mon3) {
						/* If 'A' is free, go there */
						if (!mona) {
							*tgrid = grida;
							return true;
						}
					}
				}
			}
		}

		/* All other monsters attack. */
		*tgrid = player->grid;
		return true;
	}

	/* Smart monsters try to lure the character into the open. */
	if (!*fear && rf_has(race->flags, RF_SMART) &&
		!rf_has(race->flags, RF_PASS_WALL) &&
		!rf_has(race->flags, RF_KILL_WALL) &&
		(mon->stance == STANCE_CONFIDENT)) {
		/* Determine how vulnerable the player is */
		int vulnerability = get_move_calc_vulnerability(mon->grid);
		
		/* determine how hesitant the monster is */
		int hesitance = get_move_calc_hesitance(mon);
		
		/* Character is insufficiently vulnerable */
		if (vulnerability < hesitance) {
			/* Monster has to be willing to melee */
			if (mon->min_range == 1) {
				/* If we're in sight, find a hiding place */
				if (square_isseen(cave, mon->grid) &&
					square_isfire(cave, mon->grid)) {
					/* Find a safe spot to lurk in */
					if (get_move_retreat(mon, tgrid)) {
						*fear = true;
					} else {
						/* No safe spot -- charge */
						*tgrid = player->grid;
					}
				} else {
					/* Otherwise, we advance cautiously ... */
					get_move_advance(mon, tgrid);

					/* ... but make sure we stay hidden. */
					if (mon->cdis > 1) *fear = true;
				}

				/* Done */
				return true;
			} else if (square_isseen(cave, mon->grid) &&
					   square_isfire(cave, mon->grid)) {
				/* Find a safe spot to lurk in */
				if (get_move_retreat(mon, tgrid)) {
					*fear = true;
				} else {
					/* No safe spot -- charge */
					*tgrid = player->grid;
				}
			}
		}
	}

	/* Monster groups try to surround the character */
	if ((!*fear) && (mon->cdis <= 3) && square_isview(cave, mon->grid) &&
		(rf_has(race->flags, RF_FRIENDS) || rf_has(race->flags, RF_FRIEND))) {
		/* Only if we do not have a clean path to player */
		if (projectable(cave, mon->grid, player->grid, PROJECT_CHCK)
			!= PROJECT_PATH_CLEAR) {
			start = randint0(8);

			/* Find a random empty square next to the player to head for */
			for (i = start; i < 8 + start; i++) {			
				/* Pick squares near player */
				grid = loc_sum(player->grid, ddgrid_ddd[i % 8]);

				/* Check Bounds */
				if (!square_in_bounds(cave, grid)) continue;

				/* Ignore occupied grids */
				if (square_monster(cave, grid)) continue;

				/* Ignore grids that monster can't enter immediately */
				if (!monster_can_exist(cave, mon, grid, false, true)) continue;

				/* Accept */
				*tgrid = grid;
				return true;
			}
		}
	}

	/* No special moves made -- use standard movement */
	if (!*fear) {
		/*
		 * XXX XXX -- The monster cannot see the character.  Make it
		 * advance, so the player can have fun ambushing it.
		 */
		if (!square_isview(cave, mon->grid)) {
			/* Advance */
			get_move_advance(mon, tgrid);
		} else {
			/* Always reset the monster's target */
			mon->target.grid = player->grid;

			/* Monsters too far away will advance. */
			if (mon->cdis > mon->best_range) {
				*tgrid = player->grid;
			} else if ((mon->cdis > mon->min_range) && one_in_(2)) {
				/* Monsters not too close will often advance */
				*tgrid = player->grid;
			} else if (!square_isfire(cave, mon->grid)) {
				/* Monsters that can't target the character will advance. */
				*tgrid = player->grid;
			} else {
				/* Otherwise they will stay still or move randomly. */
				if (rf_has(race->flags, RF_RAND_50) ||
					rf_has(race->flags, RF_RAND_25)) {
					/* Pick a random grid next to the monster */
					i = randint0(8);

					*tgrid = loc_sum(mon->grid, ddgrid_ddd[i]);
				}

				/* Monsters could look for better terrain... */
			}
            
            /* In most cases where the monster is targetting the player,
			 * use the clever pathfinding instead */
            if (loc_eq(*tgrid, player->grid)) {
                mon->target.grid = loc(0, 0);

                /* Advance */
                get_move_advance(mon, tgrid);
            }
		}
	} else {
		/* Back away -- try to be smart about it */
		get_move_retreat(mon, tgrid);
	}

	/* We do not want to move */
	if (loc_eq(*tgrid, mon->grid)) return false;

	/* We want to move */
	return true;
}

/**
 * ------------------------------------------------------------------------
 * make_move*()
 * ------------------------------------------------------------------------ */
/**
 * Choose the basic direction of movement, and whether to bias left or right
 * if the main direction is blocked.
 *
 * Note that the input is an offset to the monster's current position, and
 * the output direction is intended as an index into the side_dirs array.
 */
static int make_move_choose_direction(struct loc offset)
{
	int dir = 0;
	int dx = offset.x, dy = offset.y;

	/* Extract the "absolute distances" */
	int ay = ABS(dy);
	int ax = ABS(dx);

	/* We mostly want to move vertically */
	if (ay > (ax * 2)) {
		/* Choose between directions '8' and '2' */
		if (dy > 0) {
			/* We're heading down */
			dir = 2;
			if ((dx > 0) || (dx == 0 && turn % 2 == 0))
				dir += 10;
		} else {
			/* We're heading up */
			dir = 8;
			if ((dx < 0) || (dx == 0 && turn % 2 == 0))
				dir += 10;
		}
	}

	/* We mostly want to move horizontally */
	else if (ax > (ay * 2)) {
		/* Choose between directions '4' and '6' */
		if (dx > 0) {
			/* We're heading right */
			dir = 6;
			if ((dy < 0) || (dy == 0 && turn % 2 == 0))
				dir += 10;
		} else {
			/* We're heading left */
			dir = 4;
			if ((dy > 0) || (dy == 0 && turn % 2 == 0))
				dir += 10;
		}
	}

	/* We want to move down and sideways */
	else if (dy > 0) {
		/* Choose between directions '1' and '3' */
		if (dx > 0) {
			/* We're heading down and right */
			dir = 3;
			if ((ay < ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		} else {
			/* We're heading down and left */
			dir = 1;
			if ((ay > ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		}
	}

	/* We want to move up and sideways */
	else {
		/* Choose between directions '7' and '9' */
		if (dx > 0) {
			/* We're heading up and right */
			dir = 9;
			if ((ay > ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		} else {
			/* We're heading up and left */
			dir = 7;
			if ((ay < ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		}
	}

	return dir;
}

/**
 * A simple method to help fleeing monsters who are having trouble getting
 * to their target.  It's very stupid, but works fairly well in the
 * situations it is called upon to resolve.  XXX XXX
 *
 * If this function claims success, ty and tx must be set to a grid
 * adjacent to the monster.
 *
 * Return true if this function actually did any good.
 */
static bool make_move_get_route_to_target(struct monster *mon,
										  struct loc *tgrid)
{
	int i, j;
	int dist_y, dist_x;
	struct loc grid, grid1, tar_grid = loc(0, 0);

	bool dummy;
	bool below = false;
	bool right = false;

	/* Is the target further away vertically or horizontally? */
	dist_y = ABS(mon->target.grid.y - mon->grid.y);
	dist_x = ABS(mon->target.grid.x - mon->grid.x);

	/* Target is further away vertically than horizontally */
	if (dist_y > dist_x) {
		/* Find out if the target is below the monster */
		if (mon->target.grid.y - mon->grid.y > 0) below = true;

		/* Search adjacent grids */
		for (i = 0; i < 8; i++) {
			grid = loc_sum(mon->grid, ddgrid_ddd[i]);

			/* Check bounds */
			if (!square_in_bounds_fully(cave, grid)) continue;

			/* Grid is not passable */
			if (!monster_entry_chance(cave, mon, grid, &dummy)) continue;

			/* Grid will take me further away */
			if ((below && (grid.y < mon->grid.y)) ||
				(!below && (grid.y > mon->grid.y))) {
				continue;
			} else if (grid.y == mon->grid.y) {
				/* Grid will not take me closer or further, see if it
				 * leads to better things */
				for (j = 0; j < 8; j++) {
					grid1 = loc_sum(grid, ddgrid_ddd[j]);

					/* Grid does lead to better things */
					if ((below && (grid1.y > mon->grid.y)) ||
						(!below && (grid1.y < mon->grid.y))) {
						/* But it is not passable */
						if (!monster_entry_chance(cave, mon, grid1, &dummy))
							continue;

						/* Accept (original) grid, but don't immediately claim
						 * success */
						tar_grid = grid;
					}
				}
			} else {
				/* Grid will take me closer, don't look this gift horse
				 * in the mouth. */
				*tgrid = grid;
				return true;
			}
		}
	} else if (dist_x > dist_y) {
		/* Target is further away horizontally than vertically,
		 * find out if the target is right of the monster */
		if (mon->target.grid.x - mon->grid.x > 0) right = true;

		/* Search adjacent grids */
		for (i = 0; i < 8; i++) {
			grid = loc_sum(mon->grid, ddgrid_ddd[i]);

			/* Check bounds */
			if (!square_in_bounds_fully(cave, grid)) continue;

			/* Grid is not passable */
			if (!monster_entry_chance(cave, mon, grid1, &dummy)) continue;

			/* Grid will take me further away */
			if ((right && (grid.x < mon->grid.x)) ||
			    (!right && (grid.x > mon->grid.x))) {
				continue;
			} else if (grid.x == mon->grid.x) {
				/* Grid will not take me closer or further, see if it
				 * leads to better things */
				for (j = 0; j < 8; j++) {
					grid1 = loc_sum(grid, ddgrid_ddd[j]);

					/* Grid does lead to better things */
					if ((right && (grid1.x > mon->grid.x)) ||
						(!right && (grid1.x < mon->grid.x))) {
						/* But it is not passable */
						if (!monster_entry_chance(cave, mon, grid1, &dummy))
							continue;

						/* Accept (original) grid, but don't immediately claim
						 * success */
						tar_grid = grid;
					}
				}
			} else {
				/* Grid will take me closer, don't look this gift horse
				 * in the mouth. */
				*tgrid = grid;
				return true;
			}
		}
	} else {
		/* Target is the same distance away along both axes. */
		/* XXX XXX - code something later to fill this hole. */
		return false;
	}

	/* If we found a solution, claim success */
	if (!loc_eq(tar_grid, loc(0, 0))) {
		*tgrid = tar_grid;
		return true;
	}

	/* No luck */
	return false;
}

/**
 * Confused monsters bang into walls and doors.  This function assumes that
 * the monster does not belong in this grid, and therefore should suffer for
 * trying to enter it.
 */
static void make_confused_move(struct monster *mon, struct loc grid)
{
	bool seen = monster_is_visible(mon) && square_isseen(cave, grid);

	/* Check Bounds (fully) */
	if (!square_in_bounds_fully(cave, grid)) return;

    /* Feature is a chasm */
    if (square_ischasm(cave, grid)) {
		/* The creature can't fly and the grid is empty */
        if (!rf_has(mon->race->flags, RF_FLYING) &&
			!square_monster(cave, grid)) {
            monster_swap(mon->grid, grid);
        }
    } else if (square_iswall(cave, grid)) {
		if (square_isdoor(cave, grid)) {
			if (seen)
				add_monster_message(mon, MON_MSG_STAGGER_DOOR, true);
		} else if (square_isrubble(cave, grid)) {
			if (seen)
				add_monster_message(mon, MON_MSG_STAGGER_RUBBLE, true);
		} else {
			if (seen)
				add_monster_message(mon, MON_MSG_STAGGER_WALL, true);
		}

		/* Possibly update the monster health bar */
		if (player->upkeep->health_who == mon)
			player->upkeep->redraw |= (PR_HEALTH);
	} else {
		/* No changes */
	}
}

/**
 * Given a target grid, calculate the grid the monster will actually
 * attempt to move into.
 *
 * The simplest case is when the target grid is adjacent to us and
 * able to be entered easily.  Usually, however, one or both of these
 * conditions don't hold, and we must pick an initial direction, than
 * look at several directions to find that most likely to be the best
 * choice.  If so, the monster needs to know the order in which to try
 * other directions on either side.  If there is no good logical reason
 * to prioritize one side over the other, the monster will act on the
 * "spur of the moment", using current turn as a randomizer.
 *
 * The monster then attempts to move into the grid.  If it fails, this
 * function returns false and the monster ends its turn.
 *
 * The variable "fear" is used to invoke any special rules for monsters
 * wanting to retreat rather than advance.  For example, such monsters
 * will not leave an non-viewable grid for a viewable one and will try
 * to avoid the character.
 *
 * The variable "bash" remembers whether a monster had to bash a door
 * or not.  This has to be remembered because the choice to bash is
 * made in a different function than the actual bash move.  XXX XXX  If
 * the number of such variables becomes greater, a structure to hold them
 * would look better than passing them around from function to function.
 */
static bool make_move(struct monster *mon, struct loc *tgrid, bool fear,
					  bool *bash)
{
	int i, j;

	/* Start direction, current direction */
	int dir0, dir;

	/* Existing monster location, proposed new location */
	struct loc current = mon->grid, next;

	bool avoid = false;
	bool passable = false;
	bool look_again = false;

	int chance;

	/* Build a structure to hold movement data */
	static struct move_data {
		int move_chance;
		bool move_bash;
	} moves_data[8];

	/* Get the direction needed to get to the target */
	dir0 = make_move_choose_direction(loc_diff(*tgrid, current));

	/* If the monster wants to stay still... */
	if (loc_eq(*tgrid, mon->grid)) {
		/* If it is adjacent to the player, then just attack */
		if (mon->cdis == 1) {
			*tgrid = player->grid;
		} else {
			/* Otherwise just do nothing */
			return false;
		}
	}

	/* Apply monster confusion */
	if ((mon->m_timed[MON_TMD_CONF]) &&
		!rf_has(mon->race->flags, RF_NEVER_MOVE)) {
		/* Undo +10 modifiers */
		if (dir0 > 10) dir0 -= 10;

		/* Gives 3 chances to be turned left and 3 chances to be turned right
		 * leads to a binomial distribution of direction around the
		 * intended one:
		 * 15 20 15
		 *  6     6   (chances are all out of 64)
		 *  1  0  1 */
		i = damroll(3, 2) - damroll(3, 2);
		dir0 = cycle[chome[dir0] + i];
	}
		
	/* Is the target grid adjacent to the current monster's position? */
	if ((distance(*tgrid, current) <= 1) && !mon->m_timed[MON_TMD_CONF]) {
		/* If it is, try the shortcut of simply moving into the grid */
		chance = monster_entry_chance(cave, mon, *tgrid, bash);

		/* Grid must be pretty easy to enter */
		if (chance >= 50) {
			/* We can enter this grid */
			if ((chance >= 100) || (randint0(100) < chance)) {
				return true;
			} else {
				/* Failure to enter grid.  Cancel move */
				return false;
			}
		}
	}
		
	/* Now that we have an initial direction, we must determine which
	 * grid to actually move into.
	 * Scan each of the eight possible directions, in the order of
	 * priority given by the table "side_dirs", choosing the one that
	 * looks like it will get the monster to the character - or away
	 * from them - most effectively. */
	for (i = 0; i <= 8; i++) {
		/* Out of options */
		if (i == 8) break;

		/* Get the actual direction */
		dir = side_dirs[dir0][i];

		/* Get the grid in our chosen direction */
		next = loc_sum(current, ddgrid[dir]);

		/* Check Bounds */
		if (!square_in_bounds(cave, next)) continue;

		/* Store this grid's movement data. */
		moves_data[i].move_chance =	monster_entry_chance(cave, mon, next, bash);
		moves_data[i].move_bash = *bash;

		/* Confused monsters must choose the first grid */
		if (mon->m_timed[MON_TMD_CONF]) break;

		/* If this grid is totally impassable, skip it */
		if (moves_data[i].move_chance == 0) continue;

		/* Frightened monsters work hard not to be seen. */
		if (fear) {
			/* Monster is having trouble navigating to its target. */
			if (!loc_eq(mon->target.grid, loc(0, 0)) && (i >= 2) &&
				(distance(mon->grid, mon->target.grid) > 1)) {
				/* Look for an adjacent grid leading to the target */
				if (make_move_get_route_to_target(mon, tgrid)) {
					/* Calculate the chance to enter the grid */
					chance = monster_entry_chance(cave, mon, *tgrid, bash);

					/* Try to move into the grid */
					if (randint0(100) < chance) {
						/* Can move */
						return true;
					}

					/* Can't move */
					return (false);
				} else if (i >= 3) {
					/* We can't get to our hiding place.  We're in line of fire.
					 * The only thing left to do is go down fighting. */
					if (monster_is_visible(mon) &&
						(square_isfire(cave, current)) &&
						!player->truce && (mon->race->freq_ranged < 50)) {
						/* Dump a message */
						add_monster_message(mon, MON_MSG_PANIC, true);

						/* Boost morale and make the monster aggressive */
						mon->tmp_morale = MAX(mon->tmp_morale + 60, 60);
						calc_morale(mon);
						calc_stance(mon);
						mflag_on(mon->mflag, MFLAG_AGGRESSIVE);
					}
				}
			}

			/* Attacking the character as a first choice? */
			if ((i == 0) && loc_eq(next, player->grid)) {
				/* Need to rethink some plans XXX XXX XXX */
				mon->target.grid = loc(0, 0);
			}

			/* Monster is visible */
			if (monster_is_visible(mon)) {
				/* And is in LOS */
				if (square_isview(cave, current)) {
					/* Accept any easily passable grid out of LOS */
					if ((!square_isview(cave, next)) &&
						(moves_data[i].move_chance > 40)) {
						break;
					}
				} else {
					/* Do not enter a grid in LOS */
					if (square_isview(cave, next)) {
						moves_data[i].move_chance = 0;
						continue;
					}
				}
			} else {
				/* Monster can't be seen, and is not in a "seen" grid. */
				if (!square_isview(cave, current)) {
					/* Do not enter a grid in LOS */
					if (square_isview(cave, next)) {
						moves_data[i].move_chance = 0;
						continue;
					}
				}
			}
		}

		/* XXX XXX -- Sometimes attempt to break glyphs. */
		if (square_iswarded(cave, next) && (!fear) && one_in_(5)) {
			break;
		}

		/* Initial direction is almost certainly the best one */
		if ((i == 0) && (moves_data[i].move_chance >= 80)) {
			/* If backing away and close, try not to walk next
			 * to the character, or get stuck fighting them. */
			if ((fear) && (mon->cdis <= 2) &&
				(distance(player->grid, next) <= 1)) {
				avoid = true;
			} else {
				break;
			}
		} else if (((i == 1) || (i == 2)) &&
				   (moves_data[i].move_chance >= 50)) {
			/* Either of the first two side directions looks good */
			if ((moves_data[0].move_chance >= moves_data[i].move_chance)) {
				/* Accept the central direction if at least as good */
				if (avoid) {
					/* Frightened monsters try to avoid the character */
					if (distance(player->grid, next) == 0) {
						i = 0;
					}
				} else {
					i = 0;
				}
			}

			/* Accept this direction */
			break;
		}

		/* This is the first passable direction */
		if (!passable) {
			/* Note passable */
			passable = true;

			/* All the best directions are blocked. */
			if (i >= 3) {
				/* Settle for "good enough" */
				break;
			}
		}

		/* We haven't made a decision yet; look again. */
		if (i == 7) look_again = true;
	}

	/* We've exhausted all the easy answers. */
	if (look_again) {
		/* There are no passable directions. */
		if (!passable) {
			return false;
		}

		/* We can move. */
		for (j = 0; j < 8; j++) {
			/* Accept the first option, however poor.  XXX */
			if (moves_data[j].move_chance) {
				i = j;
				break;
			}
		}
	}

	/* If no direction was acceptable, end turn */
	if (i >= 8) {
		return false;
	}

	/* Get movement information (again) */
	dir = side_dirs[dir0][i];
	*bash = moves_data[i].move_bash;

	/* No good moves, so we just sit still and wait. */
	if ((dir == DIR_NONE) || (dir == DIR_UNKNOWN)) {
		return false;
	}

	/* Get grid to move into */
	*tgrid = loc_sum(current, ddgrid[dir]);

	/* Amusing messages and effects for confused monsters trying
	 * to enter terrain forbidden to them. */
	if (mon->m_timed[MON_TMD_CONF] && (moves_data[i].move_chance <= 25)) {
		/* Sometimes hurt the poor little critter */
		make_confused_move(mon, *tgrid);

		/* Do not actually move */
		if (!moves_data[i].move_chance) return false;
	}

	/* Try to move in the chosen direction.  If we fail, end turn. */
	if ((moves_data[i].move_chance < 100) &&
		(randint0(100) > moves_data[i].move_chance)) {
		return false;
	}

	/* Monster is frightened, and is obliged to fight. */
	if ((fear) && square_isplayer(cave, *tgrid) && !player->truce) {
		/* Message if visible */
		if (monster_is_visible(mon)) {
			/* Dump a message */
			add_monster_message(mon, MON_MSG_PANIC, true);
		}

		/* Boost morale and make the monster aggressive */
		mon->tmp_morale = MAX(mon->tmp_morale + 60, 60);
		calc_morale(mon);
		calc_stance(mon);
		mflag_on(mon->mflag, MFLAG_AGGRESSIVE);
	}

	/* We can move. */
	return true;
}

/**
 * ------------------------------------------------------------------------
 * process_move*()
 * ------------------------------------------------------------------------ */
/**
 * Deal with the monster Ability: exchange places
 */
static void process_move_exchange_places(struct monster *mon)
{
    struct monster_lore *lore = get_lore(mon->race);
    char m_name1[80];
    char m_name2[80];
    struct loc grid = mon->grid;

	monster_desc(m_name1, sizeof(m_name1), mon, (MDESC_PRO_VIS | MDESC_OBJE));
    monster_desc(m_name2, sizeof(m_name2), mon, MDESC_PRO_VIS);

    /* Message */
	add_monster_message(mon, MON_MSG_EXCHANGE, true);

    /* Swap positions with the player */
    monster_swap(mon->grid, player->grid);

    /* Update some things */
    update_view(cave, player);

    /* Attack of opportunity */
    if (!player->timed[TMD_AFRAID] && !player->timed[TMD_ENTRANCED] &&
		(player->timed[TMD_STUN] <= 100)) {
        /* This might be the most complicated auto-grammatical message
		 * in the game... */
        msg("You attack %s as %s slips past.", m_name1, m_name2);
        py_attack_real(player, mon->grid, ATT_OPPORTUNITY);
    }

    /* Remember that the monster can do this */
    if (monster_is_visible(mon)) {
		rf_on(lore->flags, RF_EXCHANGE);
	}

    /* Set off traps */
    if (square_isplayertrap(cave, grid) || square_ischasm(cave, grid)) {
		/* Reveal the trap */
		square_reveal_trap(cave, grid, true);

        /* Hit the trap */
        hit_trap(grid);
    }
}

/**
 * If one monster moves into another monster's grid, they will
 * normally swap places.  If the second monster cannot exist in the
 * grid the first monster left, this can't happen.  In such cases,
 * the first monster tries to push the second out of the way.
 */
static bool process_move_push_aside(struct monster *mon, struct monster *mon1)
{
	int i, dir = 0;
	struct loc grid;

	/* Translate the difference between the locations of the two monsters
	 * into a direction of travel. */
	for (i = 0; i < 10; i++) {
		/* Require correct difference */
		if (!loc_eq(loc_diff(mon1->grid, mon->grid), ddgrid[i])) continue;

		/* Found the direction */
		dir = i;
		break;
	}

	/* Favor either the left or right side on the "spur of the moment". */
	if (one_in_(2)) dir += 10;

	/* Check all directions radiating out from the initial direction. */
	for (i = 0; i < 7; i++) {
		int side_dir = side_dirs[dir][i];
		grid = loc_sum(mon1->grid, ddgrid[side_dir]);

		/* Illegal grid */
		if (!square_in_bounds_fully(cave, grid)) continue;

		/* Grid is not occupied, and the 2nd monster can exist in it. */
		if (monster_can_exist(cave, mon1, grid, false, true)) {
			/* Push the 2nd monster into the empty grid. */
			monster_swap(mon1->grid, grid);
			return true;
		}
	}

	/* We didn't find any empty, legal grids */
	return false;
}

/**
 * Grab all objects from the grid.
 */
static void process_move_grab_objects(struct monster *mon, struct loc new)
{
	struct monster_lore *lore = get_lore(mon->race);
	struct object *obj = square_object(cave, new);
	bool visible = monster_is_visible(mon);
	char m_name[80];

	/*
	 * Don't allow item pickup or smashing in the tutorial:  items on the
	 * floor are likely specially placed and having them disappear
	 * because a monster happened by is inconvenient.
	 */
	if (in_tutorial()) {
		return;
	}

	/* Abort if can't pickup */
	if (!rf_has(mon->race->flags, RF_TAKE_ITEM)) return;

	/* Get the monster name/poss */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);

	/* Take objects on the floor */
	obj = square_object(cave, new);
	while (obj) {
		char o_name[80];
		bool useless = object_is_cursed(obj) || object_is_broken(obj);
		struct object *next = obj->next;

		/* Get the object name */
		object_desc(o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL,
					player);

		/* Try to pick up */
		if (useless && visible && square_isview(cave, new)) {
			/* Dump a message */
			msg("%s looks at %s, but moves on.", m_name, o_name);
		} else {
			/* Make a copy so the original can remain as a placeholder if
			 * the player remembers seeing the object. */
			struct object *taken = object_new();

			/* Learn about item pickup behavior */
			rf_on(lore->flags, RF_TAKE_ITEM);

			object_copy(taken, obj);
			taken->oidx = 0;

			/* Describe observable situations */
			if (square_isseen(cave, new)) {
				msg("%s picks up %s.", m_name, o_name);
			}

			/* Delete the object */
			square_delete_object(cave, new, obj, true, true);

			/* Try to carry the copy */
			(void) monster_carry(cave, mon, taken);
		}

		/* Next object */
		obj = next;
	}
}

/**
 * Process a monster's move.
 *
 * All the plotting and planning has been done, and all this function
 * has to do is move the monster into the chosen grid.
 *
 * This may involve attacking the character, breaking a glyph of
 * warding, bashing down a door, etc..  Once in the grid, monsters may
 * stumble into monster traps, hit a scent trail, pick up or destroy
 * objects, and so forth.
 *
 * A monster's move may disturb the character, depending on which
 * disturbance options are set.
 */
static void process_move(struct monster *mon, struct loc tgrid, bool bash)
{
	struct monster_race *race = mon->race;
	struct monster_lore *lore = get_lore(race);

	/* Existing monster location, proposed new location */
	struct loc current = mon->grid, next = tgrid;

	/* Default move, default lack of view */
	bool do_move = true;
	bool do_view = false;

	/* Assume nothing */
    bool did_swap = false;
	bool did_pass_door = false;
	bool did_open_door = false;
	bool did_unlock_door = false;
	bool did_bash_door = false;
	bool did_pass_wall = false;
	bool did_kill_wall = false;
	bool did_tunnel_wall = false;

	/* Check Bounds */
	if (!square_in_bounds(cave, next)) return;

	/* Some monsters will not move into sight of the player. */
	if (rf_has(race->flags, RF_HIDDEN_MOVE) && 
	    (square_isseen(cave, tgrid) ||
		 square_seen_by_keen_senses(cave, tgrid))) {
		/* Hack -- memorize lack of moves after a while. */
		if (!rf_has(lore->flags, RF_HIDDEN_MOVE)) {
			if (monster_is_visible(mon) && (one_in_(50))) {
				rf_on(lore->flags, RF_HIDDEN_MOVE);
			}
		}

		/* If we are in sight, do not move */
		return;		
	}

	/* The grid is occupied by the player. */
	if (square_isplayer(cave, next)) {
		/* Unalert monsters notice the player instead of attacking */
		if (mon->alertness < ALERTNESS_ALERT) {
			set_alertness(mon,rand_range(ALERTNESS_ALERT, ALERTNESS_ALERT + 5));

			/* We must unset this flag to avoid the monster missing *2* turns */
			mon->skip_next_turn = false;
		} else if (!player->truce) {
			/* Otherwise attack if possible */
            if (rf_has(race->flags, RF_EXCHANGE) && one_in_(4) &&
                (adj_mon_count(mon->grid) >= adj_mon_count(player->grid))) {
                process_move_exchange_places(mon);
            } else {
                (void)make_attack_normal(mon, player);
            }
		}

		/* End move */
		do_move = false;
	}

	/* Can still move */
	if (do_move) {
		/* Entering a wall */
		if (square_iswall(cave, next)) {
			/* Monster passes through walls (and doors) */
			if (rf_has(race->flags, RF_PASS_WALL)) {
				/* Monster went through a wall */
				did_pass_wall = true;
			} else if (rf_has(race->flags, RF_KILL_WALL)) {
				/* Monster destroys walls (and doors) */
				bool note = false;

				/* Noise distance depends on monster "dangerousness"  XXX */
				int noise_dist = 10;

				/* Note that the monster killed the wall */
				if (square_isview(cave, next)) {
					do_view = true;
					did_kill_wall = true;
				} else if (mon->cdis <= noise_dist) {
					/* Output warning messages if the racket gets too loud */
					note = true;
				}

				/* Grid is currently a door */
				if (square_isdoor(cave, next)) {
					square_set_feat(cave, next, FEAT_BROKEN);

					if (note) {
						/* Disturb the player */
						disturb(player, false);
						msg("You hear a door being smashed open.");
					}

					/* Monster noise */
					mon->noise += 10;
				} else {
					/* Grid is anything else */
					square_set_feat(cave, next, FEAT_FLOOR);

					if (note) {
						/* Disturb the player */
						disturb(player, false);
						msg("You hear grinding noises.");
					}

					/* Monster noise */
					mon->noise += 15;
				}
			} else if (rf_has(race->flags, RF_TUNNEL_WALL) &&
					   !square_iswall(cave, next)) {
				bool note = false;
                
				/* Noise distance depends on monster "dangerousness"  XXX */
				int noise_dist = 10;
                
                /* Do not move */
                do_move = false;
                
				/* Note that the monster killed the wall */
				if (square_isview(cave, next)) {
					do_view = true;
					did_tunnel_wall = true;
				} else if (mon->cdis <= noise_dist) {
					/* Output warning messages if the racket gets too loud */
					note = true;
				}
               
				/* Grid is currently rubble */
				if (square_isrubble(cave, next)) {
					square_set_feat(cave, next, FEAT_FLOOR);

					if (note) {
						/* Disturb the player */
						disturb(player, false);
						msg("You hear grinding noises.");
					}

					/* Monster noise */
					mon->noise += 15;
				} else {
					/* Grid is granite or quartz */
					square_set_feat(cave, next, FEAT_RUBBLE);

					if (note) {
						/* Disturb the player */
						disturb(player, false);
						msg("You hear grinding noises.");
					}

					/* Monster noise */
					mon->noise += 15;
				}
			} else if (square_iscloseddoor(cave, next)) {
				/* Monster passes through doors */
				if (rf_has(race->flags, RF_PASS_DOOR)) {
					/* Monster went through a door */
					did_pass_door = true;
				} else if (bash) {
					/* Monster bashes the door down */
					if (square_isview(cave, next)) {
						/* Handle doors in sight */
						disturb(player, false);
						msg("The door bursts open!");
						do_view = true;
					} else if (mon->cdis < 20) {
						/* Character is not too far away */
						disturb(player, false);
						msg("You hear a door burst open!");
					}

					/* Note that the monster bashed the door (if visible) */
					did_bash_door = true;

					/* Monster noise */
					mon->noise += 10;

					/* Break down the door */
					if (one_in_(2)) {
						square_set_feat(cave, next, FEAT_BROKEN);
					} else {
						square_set_feat(cave, next, FEAT_OPEN);
					}
				} else {
					/* Monster opens the door */
					if (square_islockeddoor(cave, next)) {
						/* Note that the monster unlocked the door */
						did_unlock_door = true;

						/* Unlock the door */
						square_set_feat(cave, next, FEAT_CLOSED);

						/* Do not move */
						do_move = false;

						/* Handle doors in sight */
						if (square_isview(cave, next)) {
							msg("You hear a 'click'.");
						}
					} else {
						/* Note that the monster opened the door */
						did_open_door = true;

						/* Open the door */
						square_set_feat(cave, next, FEAT_OPEN);

						/* Step into doorway sometimes */
						if (!one_in_(5)) do_move = false;
					}

					/* Handle doors in sight */
					if (square_isview(cave, next)) {
						/* Do not disturb automatically */
						do_view = true;
					}
				}
			} else {
				/* Paranoia -- Ignore all features not added to this code */
				return;
			}
		} else if (square_iswarded(cave, next)) {
			/* Describe observable breakage */
			if (square_isseen(cave, next)) {
				msg("The glyph of warding is broken!");

				/* Forget the rune */
				square_unmark(cave, next);
			}

			/* Break the rune */
			square_destroy_trap(cave, next);
		}
	}

	/* Monster is still allowed to move */
	if (do_move) {
		/* The grid is occupied by a monster. */
		struct monster *mon1 = square_monster(cave, next);
		if (mon1) {
			/* Swap with or push aside the other monster */
			did_swap = true;

			/* The other monster cannot switch places */
			if (!monster_can_exist(cave, mon1, mon->grid, true, true)) {
				/* Try to push it aside */
				if (!process_move_push_aside(mon, mon1)) {
					/* Cancel move on failure */
					do_move = false;
				}
			}
		}
	}

	/* Monster can (still) move */
	if (do_move) {
		struct monster *mon1;

		/* Deal with possible flanking attack */
        if (rf_has(race->flags, RF_FLANKING) &&
            (distance(current, player->grid) == 1) &&
			(distance(next, player->grid) == 1) &&
            (mon->alertness >= ALERTNESS_ALERT) &&
			(mon->stance != STANCE_FLEEING) && !mon->m_timed[MON_TMD_CONF] &&
			!did_swap) {
			add_monster_message(mon, MON_MSG_FLANK, false);
            make_attack_normal(mon, player);

            /* Remember that the monster can do this */
            if (monster_is_visible(mon)) {
				rf_on(lore->flags, RF_FLANKING);
			}
        }

		/* Move the monster */
		monster_swap(current, next);

		/* Monster may have been killed in the swap */
		if (!mon->race) return;

		/* Cancel target when reached */
		if (loc_eq(mon->target.grid, next)) {
			mon->target.grid = loc(0, 0);
		}

		/* Did a new monster get pushed into the old space? */
		mon1 = square_monster(cave, current);
		if (mon1) {
			mflag_on(mon1->mflag, MFLAG_PUSHED);
            /* Exchanging places doesn't count as movement in a direction
			 * for abilities */
        } else {
			/* If it moved into a free space, record the direction of travel
			 * (for charge attacks) */
            mon->previous_action[0] = rough_direction(current, next);
        }

		/* If a member of a monster group capable of smelling hits a
		 * scent trail while out of LOS of the character, it will
		 * communicate this to similar monsters. */
		if (!square_isview(cave, next) && rf_has(race->flags, RF_FRIENDS) &&
		    monster_can_smell(mon) && (get_scent(cave, current) == -1) &&
		    loc_eq(mon->target.grid, loc(0, 0))) {
			int i;
			bool alerted_others = false;

			/* Scan all other monsters */
			for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
				/* Access the monster */
				mon1 = cave_monster(cave, i);

				/* Ignore dead monsters */
				if (!mon1->race) continue;

				/* Ignore monsters with the wrong base */
				if (race->base != mon1->race->base) continue;

				/* Ignore monsters with specific orders */
				if (!loc_eq(mon->target.grid, loc(0, 0))) continue;

				/* Ignore monsters picking up a good scent */
				if (get_scent(cave, mon1->grid) < SMELL_STRENGTH - 10)
					continue;

				/* Ignore monsters not in LOS */
				if (!los(cave, mon->grid, mon1->grid)) continue;

				/* Activate all other monsters and give directions */
				make_alert(mon, 0);
				mflag_on(mon1->mflag, MFLAG_ACTIVE);
				mon1->target.grid = next;
				alerted_others = true;
			}
			
			if (alerted_others) { 
				message_pursuit(mon);
			}
		}

		/* Monster is visible and not cloaked */
		if (monster_is_visible(mon)) {
			/* Report passing through doors */
			if (square_isdoor(cave, mon->grid)) {
				if (rf_has(race->flags, RF_PASS_DOOR)) {
					add_monster_message(mon, MON_MSG_PASS_DOOR, true);
				}
			}

			/* Player will always be disturbed */
			disturb(player, false);
		}

		/* Take objects on the floor */
		process_move_grab_objects(mon, next);
	}


	/* Notice changes in view */
	if (do_view) {
		/* Update the visuals */
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	}

	/* Learn things from observable monster */
	if (monster_is_visible(mon)) {
		/* Monster passed through a door */
		if (did_pass_door) rf_on(lore->flags, RF_PASS_DOOR);
		
		/* Monster opened a door */
		if (did_open_door) rf_on(lore->flags, RF_OPEN_DOOR);

		/* Monster unlocked a door */
		if (did_unlock_door) rf_on(lore->flags, RF_UNLOCK_DOOR);

		/* Monster bashed a door */
		if (did_bash_door) rf_on(lore->flags, RF_BASH_DOOR);

		/* Monster passed through a wall */
		if (did_pass_wall) rf_on(lore->flags, RF_PASS_WALL);

		/* Monster killed a wall */
		if (did_kill_wall) rf_on(lore->flags, RF_KILL_WALL);
        
		/* Monster tunneled through a wall */
		if (did_tunnel_wall) rf_on(lore->flags, RF_TUNNEL_WALL);
	}
}

/**
 * ------------------------------------------------------------------------
 * Monster turn routines
 * These routines, culminating in monster_turn(), decide how a monster uses
 * its turn
 * ------------------------------------------------------------------------ */
/**
 * Deal with monsters trying to wander around the dungeon
 *
 * This function will finish the monster's turn
 */
static void monster_turn_wander(struct monster *mon)
{
	struct monster_group *group = monster_group_by_index(cave,
														 mon->group_info.index);
	struct loc tgrid;
	bool fear = false;
	bool bash = false;
	const struct artifact *crown = lookup_artifact_name("of Morgoth");
    
    /* Begin a song of piercing if possible; Morgoth must be uncrowned */
    if (rsf_has(mon->race->spell_flags, RSF_SNG_PIERCE) &&
		(mon->song != lookup_song("Piercing")) &&
		(mon->alertness < ALERTNESS_ALERT) &&
		(mon->mana >= z_info->mana_cost) &&
		is_artifact_created(crown)) {
		do_mon_spell(RSF_SNG_PIERCE, mon, square_isview(cave, mon->grid));
    }

    /* Occasionally update the flow to take account of changes in the dungeon
	 * (new glyphs of warding, doors closed etc) */
    if (one_in_(10) && !loc_eq(group->flow.centre, loc(0, 0))) {
		update_flow(cave, &group->flow, mon);
     }

	/* Choose a target grid, or cancel the move. */
	if (!get_move_wander(mon, &tgrid)) return;

	/* Calculate the actual move.  Cancel move on failure to enter grid. */
	if (!make_move(mon, &tgrid, fear, &bash)) return;
	
	/* Change terrain, move the monster, handle secondary effects. */
	process_move(mon, tgrid, bash);
}

/**
 * Check whether there are nearby kin who are asleep/unwary
 */
static bool monster_has_sleeping_kin(struct monster *mon)
{
	int i;
	bool has_kin = false;

	/* Scan all other monsters */
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		/* Access the monster */
		struct monster *mon1 = cave_monster(cave, i);

		/* Ignore dead monsters */
		if (!mon1->race) continue;

		/* Ignore monsters with the wrong base */
		if (mon->race->base != mon1->race->base) continue;

		/* Determine line of sight between the monsters */
		if (!los(cave, mon->grid, mon1->grid)) continue;

		/* Ignore monsters that are awake */
		if (mon1->alertness >= ALERTNESS_ALERT) continue;

		/* Activate all other monsters and communicate to them */
		has_kin = true;
	}

	return has_kin;
}

/**
 * Alert others in pack about something using the m_flag, and wake them up
 */
void tell_allies(struct monster *mon, int flag)
{
	int i;
	bool warned = false;

	/* Scan all other monsters */
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		/* Access the monster */
		struct monster *mon1 = cave_monster(cave, i);
		int dist;

		/* Ignore dead monsters */
		if (!mon1->race) continue;

		/* Ignore monsters with the wrong base */
		if (mon->race->base != mon1->race->base) continue;

		/* Ignore monsters that already know */
		if ((mon1->alertness >= ALERTNESS_ALERT) &&
			mflag_has(mon1->mflag, flag)) {
			continue;
		}

		/* Determine the distance between the monsters */
		dist = distance(mon->grid, mon1->grid);
		
		/* Penalize this for not being in line of sight */
		if (!los(cave, mon->grid, mon1->grid)) {
			dist *= 2;
		}

		/* Ignore monsters that are too far away */
		if (dist > 15) continue;

		/* When the first monster in need of warning is found,
		 * make the warning shout */
		if (!warned) {
			message_warning(mon);
			warned = true;
		}

		/* If an eligible monster is now alert, then set the flag */
		if (mon1->alertness >= ALERTNESS_ALERT) {
			mflag_on(mon1->mflag, (MFLAG_ACTIVE | flag));
		}
	}
}

/**
 * Deal with a monster hit by a ranged attack from the player
 */
static void monster_turn_hit_by_ranged(struct monster *mon)
{
	/* Monster will be very upset if it can't see the player or if it is
	 * in a corridor and can't fire back */
	if (((mon->best_range == 1) && !square_isroom(cave, mon->grid)) ||
		!square_isview(cave, mon->grid)) {
		mflag_on(mon->mflag, MFLAG_AGGRESSIVE);

		/* If smart and has allies, let them know */
		if (rf_has(mon->race->flags, RF_SMART)
			&& monster_talks_to_friends(mon)) {
			tell_allies(mon, MFLAG_AGGRESSIVE);
		}

		/* Monsters with ranged attacks will try to cast a spell*/
		if (mon->race->freq_ranged) mflag_on(mon->mflag, MFLAG_ALWAYS_CAST);

		calc_monster_speed(mon);
	}

	/* Clear the flag */
	mflag_off(mon->mflag, MFLAG_HIT_BY_RANGED);
}

/**
 * Deal with a monster hit by a melee attack from the player
 */
static void monster_turn_hit_by_melee(struct monster *mon)
{
	/* Monster will be very upset if it isn't next to the player on its turn
	 * (pillar dance, hack-n-back, etc) */
	if ((mon->cdis > 1) && !mflag_has(mon->mflag, MFLAG_PUSHED)) {
		mflag_on(mon->mflag, MFLAG_AGGRESSIVE);

		/* If smart and has allies, let them know */
		if (rf_has(mon->race->flags, RF_SMART) &&
			monster_talks_to_friends(mon)) {
			tell_allies(mon, MFLAG_AGGRESSIVE);
		}

		/* Monsters with ranged attacks will try to cast a spell*/
		if (mon->race->freq_ranged) mflag_on(mon->mflag, MFLAG_ALWAYS_CAST);

		calc_monster_speed(mon);
	}

	/* Clear the flag */
	mflag_off(mon->mflag, MFLAG_HIT_BY_MELEE);
}

/**
 * Lets the given monster attempt to reproduce.
 *
 * Note that "reproduction" REQUIRES empty space.
 *
 * Returns true if the monster successfully reproduced.
 */
bool multiply_monster(const struct monster *mon)
{
	struct loc grid;
	struct monster_group_info info = { 0, 0 };

	/* Pick an empty location. */
	if (scatter_ext(cave, &grid, 1, mon->grid, 1, true,	square_isempty) > 0) {
		/* Create a new monster (awake, no groups) */
		return place_new_monster(cave, grid, mon->race, false, false,
								 info, ORIGIN_DROP_BREED);
	}

	/* Fail */
	return false;
}

/**
 * Attempt to reproduce, if possible.  All monsters are checked here for
 * lore purposes, the unfit fail.
 */
static bool monster_turn_multiply(struct monster *mon)
{
	int k = 0, y, x;
	struct monster_lore *lore = get_lore(mon->race);

	/* Leave now if not a breeder */
	if (!rf_has(mon->race->flags, RF_MULTIPLY)) return false;

	/* Too many monsters on the level already */
	if (cave_monster_count(cave) > z_info->level_monster_max - 50) return false;

	/* Count the adjacent monsters */
	for (y = mon->grid.y - 1; y <= mon->grid.y + 1; y++) {
		for (x = mon->grid.x - 1; x <= mon->grid.x + 1; x++) {
			if (!square_in_bounds(cave, loc(x, y))) continue;
			if (square(cave, loc(x, y))->mon > 0) k++;
		}
	}

	/* Multiply slower in crowded areas */
	if ((k < 4) && one_in_(k * z_info->repro_monster_rate)) {
		/* Try to multiply */
		if (multiply_monster(mon)) {
			if (monster_is_visible(mon)) {
				/* Make a sound */
				sound(MSG_MULTIPLY);

				/* Successful breeding attempt, learn about that now */
				rf_on(lore->flags, RF_MULTIPLY);
			}

			/* Multiplying takes energy */
			return true;
		}
	}

	return false;
}

/**
 * Check if a monster should step at random or not.
 */
static bool monster_turn_random_move(struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);
	int chance = 0;

	/* RAND_25 and RAND_50 are cumulative */
	if (rf_has(mon->race->flags, RF_RAND_25)) {
		chance += 25;
		if (monster_is_visible(mon))
			rf_on(lore->flags, RF_RAND_25);
	}
	if (rf_has(mon->race->flags, RF_RAND_50)) {
		chance += 50;
		if (monster_is_visible(mon))
			rf_on(lore->flags, RF_RAND_50);
	}

	/* Adjacent to the character means more chance of just attacking normally */
	if (mon->cdis == 1) {
		chance /= 2;
	}

	return randint0(100) < chance;
}


/**
 * Monster takes its turn.
 */
static void monster_turn(struct monster *mon)
{
	struct song *mastery = lookup_song("Mastery");
	int i;
	struct loc tgrid = loc(0, 0), grid;
	bool random_move = false;
	bool fear = false;
	bool bash = false;

	/* Assume the monster doesn't have a target */
	bool must_use_target = false;

    /* Assume we are not under the influence of the Song of Mastery */
    mon->skip_this_turn = false;

	/* First work out if the song of mastery stops the monster's turn */
	if (player_is_singing(player, mastery)) {
		int pskill = song_bonus(player, player->state.skill_use[SKILL_SONG],
								mastery);
		int mskill = monster_skill(mon, SKILL_WILL) + 5
			+ flow_dist(cave->player_noise, mon->grid);
		if (skill_check(source_player(), pskill, mskill,
						source_monster(mon->midx)) > 0) {
            /* Make sure the monster doesn't do any free attacks before its
			 * next turn */
            mon->skip_this_turn = true;

            /* End the monster's turn */
            return;
		}
	}

    /* Deal with monster songs */
    if (mon->song) {
        int dist = flow_dist(cave->player_noise, mon->grid);

        if ((mon->mana == 0) ||	((mon->song == lookup_song("Piercing")) &&
								 (mon->alertness >= ALERTNESS_ALERT))) {
            if (monster_is_visible(mon)) {
				add_monster_message(mon, MON_MSG_END_SONG, false);
			} else if (dist <= 30) {
				msg("The song ends.");
			}
            mon->song = NULL;
        } else {
            mon->mana--;
			effect_simple(mon->song->effect->index, source_monster(mon->midx),
						  "0", 0, 0, 0, NULL);
        }
    }

	/* Update view if the monster affects light and is close enough */
	if ((mon->race->light != 0) &&
		(mon->cdis < z_info->max_sight + ABS(mon->race->light))) {
		player->upkeep->update |= (PU_UPDATE_VIEW);
	}

    /* Shuffle along the array of previous actions, enter this turn's default */
    for (i = MAX_ACTION - 1; i > 0; i--) {
        mon->previous_action[i] = mon->previous_action[i - 1];
    }
	mon->previous_action[0] = ACTION_MISC;

	/* Unwary but awake monsters can wander around the dungeon */
	if (mon->alertness < ALERTNESS_ALERT) {
		monster_turn_wander(mon);
		return;
	}

    /* Update monster flow information */
	mon->flow.centre = player->grid;
    update_flow(cave, &mon->flow, mon);

	/* Calculate the monster's preferred combat range when needed */
	if (mon->min_range == 0) {
		monster_find_range(mon);
	}

	/* Determine if the monster should be active */
	if (monster_check_active(mon)) {
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else {
		mflag_off(mon->mflag, MFLAG_ACTIVE);
	}

	/* Special handling if the first turn a monster has after
	 * being attacked by the player, but the player is out of sight */
	if (mflag_has(mon->mflag, MFLAG_HIT_BY_RANGED)) {
		monster_turn_hit_by_ranged(mon);
	}

	/* First turn a monster has after being attacked by the player*/
	if (mflag_has(mon->mflag, MFLAG_HIT_BY_MELEE)) {
		monster_turn_hit_by_melee(mon);
	}

	/* If a smart monster has sleeping friends and sees the player,
	 * sometimes shout a warning */
	if (one_in_(2) && rf_has(mon->race->flags, RF_SMART) &&
	    square_isview(cave, mon->grid) && monster_has_sleeping_kin(mon) &&
		monster_talks_to_friends(mon)) {
		tell_allies(mon, MFLAG_ACTIVE);
		calc_monster_speed(mon);
	}

	/* Clear CHARGED and PUSHED flags */
	mflag_off(mon->mflag, MFLAG_CHARGED);
	mflag_off(mon->mflag, MFLAG_PUSHED);

	/* A monster in passive mode will end its turn at this point. */
	if (!mflag_has(mon->mflag, MFLAG_ACTIVE)) {
		monster_turn_wander(mon);
		return;
	}

	/* Redraw (later) if needed */
	if (player->upkeep->health_who == mon)
		player->upkeep->redraw |= (PR_HEALTH);

	/* Try to multiply - this can use up a turn */
	if (monster_turn_multiply(mon)) return;

	/* Attempt a ranged attack */
	if ((randint0(100) < monster_cast_chance(mon)) && make_ranged_attack(mon)) {
		return;
	}

	/* Innate semi-random movement. */
	random_move = monster_turn_random_move(mon);

	/* Monster isn't moving randomly, isn't running away, doesn't hear
	 * or smell the character */
	if (!random_move) {
		/* Monsters who can't cast, are aggressive, and are not afraid
		 * just want to charge */
		if ((mon->stance == STANCE_AGGRESSIVE) && (!mon->race->freq_ranged)) {
			mon->target.grid = loc(0, 0);
		}

		/* Player can see the monster, and it is not afraid */
		if (square_isview(cave, mon->grid) && (mon->stance != STANCE_FLEEING)) {
			mon->target.grid = loc(0, 0);
		}

		/* Monster still has a known target */
		if (!loc_eq(mon->target.grid, loc(0, 0))) {
			must_use_target = true;
		}
	}

	/* Find a target to move to */
	if (random_move) {
		/* Monster isn't confused, just moving semi-randomly */
		int start = randint0(8);
		bool dummy, no_move = rf_has(mon->race->flags, RF_NEVER_MOVE);

		/* Is the monster scared? */
		if (((mon->min_range >= z_info->flee_range) ||
			 (mon->stance == STANCE_FLEEING))
			&& !no_move) {
			fear = true;
		}

		/* Look at adjacent grids, starting at random. */
		for (i = start; i < 8 + start; i++) {
			grid = loc_sum(mon->grid, ddgrid_ddd[i % 8]);

			/* Accept first passable grid. */
			if (monster_entry_chance(cave, mon, grid, &dummy) != 0) {
				tgrid = grid;
				break;
			}
		}

		/* No passable grids found */
		if (loc_eq(tgrid, loc(0, 0))) return;

		/* Cannot move, target grid does not contain the character */
		if (no_move && !square_isplayer(cave, tgrid)) return;
	} else {
        /* Extremely frightened monsters next to chasms may jump into the void*/
        if ((mon->stance == STANCE_FLEEING) && (mon->morale < -200) &&
			!rf_has(mon->race->flags, RF_FLYING) && one_in_(2)) {
			struct loc chasm = loc(0, 0);

			/* Look at adjacent grids */
            for (i = 0; i < 8; i++) {
                grid = loc_sum(mon->grid, ddgrid_ddd[i]);

                /* Check bounds */
                if (!square_in_bounds(cave, grid)) continue;

                /* Accept a chasm square */
                if (square_ischasm(cave, grid)) {
                    chasm = grid;
                    break;
                }
            }

            if (!loc_eq(chasm, loc(0, 0))) {
                monster_swap(mon->grid, chasm);
                return;
            }
		}

		/* Choose a pair of target grids, or cancel the move. */
		if (!get_move(mon, &tgrid, &fear, must_use_target)) {
			return;
		}
	}

	/* If the monster thinks its location is optimal... */
	if (loc_eq(tgrid, mon->grid)) {
		/* Intelligent monsters (but not territorial ones) that are fleeing
		 * can try to use stairs */
		if (rf_has(mon->race->flags, RF_SMART) &&
			!rf_has(mon->race->flags, RF_TERRITORIAL) &&
			(mon->stance == STANCE_FLEEING) &&
			square_isstairs(cave, tgrid)) {
			if (monster_is_visible(mon)) {
				if (square_isdownstairs(cave, tgrid)) {
					add_monster_message(mon, MON_MSG_FLEE_DOWN_STAIRS, true);
				} else {
					add_monster_message(mon, MON_MSG_FLEE_UP_STAIRS, true);
				}
			}

			/* If adjacent, player gets a chance for an opportunist attack,
			 * which might kill them (skip_next_turn is there to stop the
			 * player getting opportunist attacks afer knocking back) */
			if (player_active_ability(player, "Opportunist") &&
				monster_is_visible(mon) &&
				!mon->skip_next_turn &&
				(mon->alertness >= ALERTNESS_ALERT) && !player->truce &&
				!player->timed[TMD_CONFUSED] &&
				!player->timed[TMD_AFRAID] &&
				!player->timed[TMD_ENTRANCED] &&
				(player->timed[TMD_STUN] <= 100) &&
				(distance(tgrid, player->grid) == 1)) {
				py_attack_real(player, tgrid, ATT_OPPORTUNIST);
			}

			/* Removes the monster if it is still alive */
			delete_monster(cave, tgrid);
			return;
		}

		/* If the square is non-adjacent to the player,
		 * then allow a ranged attack instead of a move */
		if ((mon->cdis > 1) && mon->race->freq_ranged) {
			make_ranged_attack(mon);
		}

		/* Nothing left to do */
		return;
	}

	/* Calculate the actual move.  Cancel move on failure to enter grid. */
	if (!make_move(mon, &tgrid, fear, &bash)) return;

	/* Change terrain, move the monster, handle secondary effects. */
	process_move(mon, tgrid, bash);
}

/**
 * ------------------------------------------------------------------------
 * Processing routines that happen to a monster regardless of whether it
 * gets a turn, and/or to decide whether it gets a turn
 * ------------------------------------------------------------------------ */
/**
 * Monster regeneration of HPs.
 */
static void regen_monster(struct monster *mon, int num)
{
	int regen_period = z_info->mon_regen_hp_period;

	/* Regenerate (if needed) */
	if (mon->hp < mon->maxhp) {
		/* Some monsters regenerate quickly */
		if (rf_has(mon->race->flags, RF_REGENERATE)) regen_period /= 5;

		/* Regenerate */
		mon->hp += regen_amount(turn / 10, mon->maxhp, regen_period);

		/* Do not over-regenerate */
		if (mon->hp > mon->maxhp) mon->hp = mon->maxhp;
			
		/* Fully healed -> flag minimum range for recalculation */
		if (mon->hp == mon->maxhp) mon->min_range = 0;

		/* Redraw (later) if needed */
		if (player->upkeep->health_who == mon)
			player->upkeep->redraw |= (PR_HEALTH);
	}

	/* Allow mana regeneration, if needed. */
	if (mon->mana != z_info->mana_max) {
		/* Can only regenerate mana if not singing */
		if (!mon->song) {
			mon->mana += regen_amount(turn / 10, z_info->mana_max,
									  z_info->mon_regen_sp_period);

			/* Do not over-regenerate */
			if (mon->mana > z_info->mana_max) mon->mana = z_info->mana_max;

			/* Fully healed -> flag minimum range for recalculation */
			if (mon->mana == z_info->mana_max) mon->min_range = 0;
		}
	}
}

/**
 * Process a monster's timed effects and other things that need to be done
 * every turn
 */
static void process_monster_recover(struct monster *mon)
{
	int i;

    /* Summoned monsters have a half-life of one turn after the song stops */
	if (mflag_has(mon->mflag, MFLAG_SUMMONED)) {
        int still_singing = false;

        for (i = 1; i < cave_monster_max(cave); i++) {
            struct monster *mon1 = cave_monster(cave, i);

            /* Skip dead monsters */
            if (!mon1->race) continue;

            /* Note if any monster is singing the song of oaths */
            if (mon1->song == lookup_song("Oaths")) {
				still_singing = true;
				break;
			}
		}

        if (!still_singing && one_in_(2)) {
            /* Remove the monster */
            delete_monster(cave, mon->grid);
        }
    }

	/* Reduce timed effects */
	for (i = 0; i < MON_TMD_MAX; i++) {
		if (mon->m_timed[i]) {
			mon_dec_timed(mon, i, 1, MON_TMD_FLG_NOTIFY);
		}
	}

	/* Reduce temporary morale modifiers by 10% */
	if (mon->tmp_morale != 0) {
		mon->tmp_morale *= 9;
		mon->tmp_morale /= 10;
	}

	/* Hack -- Update the health and mana bar (always) */
	if (player->upkeep->health_who == mon)
		player->upkeep->redraw |= (PR_HEALTH);

	/* Monsters who are out of sight and fail their perception rolls by 25
	 * or more (15 with Vanish) start to lose track of the player */
	if (!los(cave, mon->grid, player->grid) &&
		(mon->alertness >= ALERTNESS_ALERT) && 
	    (mon->stance != STANCE_FLEEING) && (mon->race->sleep > 0)) {
		int bonus = player_active_ability(player, "Vanish") ? 15 : 25;
		int result = skill_check(source_monster(mon->midx), 
		                         monster_skill(mon, SKILL_PERCEPTION) + bonus,
		                         player->state.skill_use[SKILL_STEALTH] +
								 flow_dist(cave->player_noise, mon->grid),
								 source_player());

		if (result < 0) {
			set_alertness(mon, MAX(mon->alertness + result, ALERTNESS_UNWARY));
		}
	}

	/* Calculate the monster's morale and stance */
	calc_morale(mon);
	calc_stance(mon);
}


/**
 * ------------------------------------------------------------------------
 * Monster processing routines to be called by the main game loop
 * ------------------------------------------------------------------------ */
/**
 * Process all the "live" monsters, once per game turn.
 *
 * During each game turn, we scan through the list of all the "live" monsters,
 * (backwards, so we can excise any "freshly dead" monsters), energizing each
 * monster, and allowing fully energized monsters to move, attack, pass, etc.
 *
 * This function and its children are responsible for a considerable fraction
 * of the processor time in normal situations, greater if the character is
 * resting.
 */
void process_monsters(int minimum_energy)
{
	int i;

	/* Only process some things every so often */
	bool regen = false;

	/* If time is stopped, no monsters can move */
	if (OPT(player, cheat_timestop)) return;

	/* Regenerate hitpoints and mana every 100 game turns */
	if (turn % 10 == 0)
		regen = true;

	/* Process the monsters (backwards) */
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		struct monster *mon;
		bool moving;

		/* Handle "leaving" */
		if (player->is_dead || player->upkeep->generate_level) break;

		/* Get a 'live' monster */
		mon = cave_monster(cave, i);
		if (!mon->race) continue;

		/* Ignore monsters that have already been handled */
		if (mflag_has(mon->mflag, MFLAG_HANDLED))
			continue;

		/* Not enough energy to move yet */
		if (mon->energy < minimum_energy) continue;

		/* Does this monster have enough energy to move? */
		moving = mon->energy >= z_info->move_energy ? true : false;

		/* Prevent reprocessing */
		mflag_on(mon->mflag, MFLAG_HANDLED);

		/* Handle monster regeneration if requested */
		if (regen)
			regen_monster(mon, 1);

		/* Give this monster some energy */
		mon->energy += turn_energy(mon->mspeed);

		/* End the turn of monsters without enough energy to move */
		if (!moving) continue;

		/* Process timed effects and other every-turn things */
		process_monster_recover(mon);

		/* Use up "some" energy */
		mon->energy -= z_info->move_energy;

		/* Sleeping monsters don't get a move */
		if (mon->alertness < ALERTNESS_UNWARY) continue;

		/* Monsters who have just noticed you miss their turns (as do those
		 * who have been knocked back...) */
		if (mon->skip_next_turn) {
            /* Reset its previous movement to stop it charging etc. */
			mon->previous_action[0] = ACTION_MISC;
			mon->skip_next_turn = false;
			continue;
		}

		/* Set this monster to be the current actor */
		cave->mon_current = i;

		/* The monster takes its turn */
		monster_turn(mon);

		/* Monster is no longer current */
		cave->mon_current = -1;
	}

	/* Update monster visibility after this */
	/* XXX This may not be necessary */
	player->upkeep->update |= PU_MONSTERS;
}

/**
 * Clear 'moved' status from all monsters.
 *
 * Clear noise if appropriate.
 */
void reset_monsters(void)
{
	int i;
	struct monster *mon;

	/* Process the monsters (backwards) */
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		/* Access the monster */
		mon = cave_monster(cave, i);

		/* Monster is ready to go again */
		mflag_off(mon->mflag, MFLAG_HANDLED);
	}
}
