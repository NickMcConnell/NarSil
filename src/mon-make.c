/**
 * \file mon-make.c
 * \brief Monster creation / placement code.
 *
 * Copyright (c) 1997-2007 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "alloc.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-group.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-predicate.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "target.h"
#include "tutorial.h"

/**
 * ------------------------------------------------------------------------
 * Monster race allocation
 *
 * Monster race allocation is done using an allocation table (see alloc.h).
 * This table is sorted by depth.  Each line of the table contains the
 * monster race index, the monster race level, and three probabilities:
 * - prob1 is the base probability of the race, calculated from monster.txt.
 * - prob2 is calculated by get_mon_num_prep(), which decides whether a
 *         monster is appropriate based on a secondary function; prob2 is
 *         always either prob1 or 0.
 * - prob3 is calculated by get_mon_num(), which checks whether universal
 *         restrictions apply (for example, unique monsters can only appear
 *         once on a given level); prob3 is always either prob2 or 0.
 * ------------------------------------------------------------------------ */
static int16_t alloc_race_size;
static struct alloc_entry *alloc_race_table;

/**
 * Initialize monster allocation info
 */
static void init_race_allocs(void) {
	int i;
	struct monster_race *race;
	struct alloc_entry *table;
	int16_t *num = mem_zalloc(z_info->max_depth * sizeof(int16_t));
	int16_t *already_counted =
		mem_zalloc(z_info->max_depth * sizeof(int16_t));

	/* Size of "alloc_race_table" */
	alloc_race_size = 0;

	/* Scan the monsters (not the ghost) */
	for (i = 1; i < z_info->r_max - 1; i++) {
		/* Get the i'th race */
		race = &r_info[i];

		/* Legal monsters */
		if (race->rarity) {
			/* Count the entries */
			alloc_race_size++;

			/* Group by level */
			num[race->level]++;
		}
	}

	/* Calculate the cumultive level totals */
	for (i = 1; i < z_info->max_depth; i++) {
		/* Group by level */
		num[i] += num[i - 1];
	}

	/* Allocate the alloc_race_table */
	alloc_race_table = mem_zalloc(alloc_race_size * sizeof(struct alloc_entry));

	/* Get the table entry */
	table = alloc_race_table;

	/* Scan the monsters (not the ghost) */
	for (i = 1; i < z_info->r_max - 1; i++) {
		/* Get the i'th race */
		race = &r_info[i];

		/* Count valid races */
		if (race->rarity) {
			int p, lev, prev_lev_count, race_index;

			/* Extract this race's level */
			lev = race->level;

			/* Extract the base probability */
			p = (100 / race->rarity);

			/* Skip entries preceding this monster's level */
			prev_lev_count = (lev > 0) ? num[lev - 1] : 0;

			/* Skip entries already counted for this level */
			race_index = prev_lev_count + already_counted[lev];

			/* Load the entry */
			table[race_index].index = i;
			table[race_index].level = lev;
			table[race_index].prob1 = p;
			table[race_index].prob2 = p;
			table[race_index].prob3 = p;

			/* Another entry complete for this locale */
			already_counted[lev]++;
		}
	}
	mem_free(already_counted);
	mem_free(num);
}

static void cleanup_race_allocs(void) {
	mem_free(alloc_race_table);
}


/**
 * Apply a monster restriction function to the monster allocation table.
 * This way, we can use get_mon_num() to get a level-appropriate monster that
 * satisfies certain conditions (such as belonging to a particular monster
 * family).
 */
void get_mon_num_prep(bool (*get_mon_num_hook)(struct monster_race *race))
{
	int i;

	/* Scan the allocation table */
	for (i = 0; i < alloc_race_size; i++) {
		struct alloc_entry *entry = &alloc_race_table[i];

		/* Check the restriction, if any */
		if (!get_mon_num_hook || (*get_mon_num_hook)(&r_info[entry->index])) {
			/* Accept this monster */
			entry->prob2 = entry->prob1;
		} else {
			/* Do not use this monster */
			entry->prob2 = 0;
		}
	}
}

/**
 * Helper function for get_mon_num(). Scans the prepared monster allocation
 * table and picks a random monster. Returns the race of a monster in
 * `table`.
 */
static struct monster_race *get_mon_race_aux(long total,
											 const struct alloc_entry *table)
{
	int i;

	/* Pick a monster */
	long value = randint0(total);

	/* Find the monster */
	for (i = 0; i < alloc_race_size; i++) {
		/* Found the entry */
		if (value < table[i].prob3) break;

		/* Decrement */
		value -= table[i].prob3;
	}

	return &r_info[table[i].index];
}

/**
 * Chooses a monster race that seems appropriate to the given level
 *
 * \param level is the starting point for the level to use when choosing the
 * race.  If special is false, that starting point may be modified by
 * context-specific factors.
 * \param special will, if true, cause level to be used as is, cause vault
 * to be ignored, and allow for monsters whose native level is less than
 * level and more than level / 2 to appear.  One use for that is when
 * choosing escorts for a leader.  In that case, level would customarily be
 * the native level for the leader.
 * \param allow_non_smart will, if true, allow selection of monster races
 * that lack both the SMART and TERRITORIAL flags.  Generally, allow_non_smart
 * is true when choosing monsters to populate a newly created level or for
 * handling a specific summons and is false when choosing monsters that arrive
 * on a level while the player is there.
 * \param vault will, if true, disable most of the modifications to level
 * that are possible when special is false.  That is intended for use when
 * selecting the denizens of a vault.  vault is ignored when special is true.
 *
 * This function uses the "prob2" field of the monster allocation table,
 * and various local information, to calculate the "prob3" field of the
 * same table, which is then used to choose an appropriate monster, in
 * a relatively efficient manner.
 *
 * Note that if no monsters are appropriate, then this function will
 * fail, and return NULL, but this should *almost* never happen.
 */
struct monster_race *get_mon_num(int level, bool special, bool allow_non_smart,
								 bool vault)
{
	int i;
	long total = 0L;
	struct monster_race *race;
	struct alloc_entry *table = alloc_race_table;
	bool pursuing_monster = false;

	/* Level 24 monsters can only be generated if especially asked for */
	bool allow_24 = (level == z_info->dun_depth + 4);

	/* Default level */
	int generation_level = level;

	/* If generating escorts or similar, just use the level (which will be the
	 * captain's level); this will function as the *maximum* level
	 * for generation.  Otherwise, modify the level. */
	if (!special) {
		/* Deal with 'danger' items */
		generation_level += player->state.flags[OF_DANGER];

		/* Various additional modifications when not created in a vault */
		if (!vault) {
			/* If on the run from Morgoth, then levels 17--23 used for all
			 * forced smart monsters and half of others */
			if (player->on_the_run && (one_in_(2) || !allow_non_smart)) {
				pursuing_monster = true;
				generation_level = rand_range(17, 23);
			}

			/* The surface generates monsters as levels 17--23 */
			if (level == 0) {
				pursuing_monster = true;
				generation_level = rand_range(17, 23);
			}

			/* Most of the time use a small distribution */
			if (pursuing_monster) {
				/* Leave as is */
			} else if (level == player->depth) {
				/* Modify the effective level by a small random amount:
				 * [1, 4, 6, 4, 1] */
				generation_level += damroll(2, 2) - damroll(2, 2);
			} else {
				/* Modify the effective level by a tiny random amount:
				 * [1, 2, 1] */
				generation_level += damroll(1, 2) - damroll(1, 2);
			}
		}
	}

	/* Final bounds checking */
	if (generation_level < 1) generation_level = 1;
	if (allow_24) {
		generation_level = MIN(generation_level, z_info->dun_depth + 4);
	} else {
		generation_level = MIN(generation_level, z_info->dun_depth + 3);
	}

	/* Process probabilities */
	for (i = 0; i < alloc_race_size; i++) {
		/* Monsters are sorted by depth */
		if (table[i].level > generation_level) break;

		/* Default */
		table[i].prob3 = 0;

		/* Get the chosen monster */
		race = &r_info[table[i].index];

		/* Ignore monsters before the set level unless in special generation */
		if (!special && (table[i].level < generation_level)) continue;

		/* Even in special generation ignore monsters before 1/2 the level */
		if (special && (table[i].level <= generation_level / 2)) continue;

		/* Only one copy of a unique must be around at the same time */
		if (rf_has(race->flags, RF_UNIQUE) && (race->cur_num >= race->max_num))
			continue;

		/* Some monsters never appear out of depth */
		if (rf_has(race->flags, RF_FORCE_DEPTH) && race->level > player->depth)
			continue;

		/* Non-moving monsters can't appear as out-of-depth pursuing monsters */
		if (rf_has(race->flags, RF_NEVER_MOVE) && pursuing_monster) continue;

		/* Territorial monsters can't appear as out-of-depth pursuing monsters*/
		if (rf_has(race->flags, RF_TERRITORIAL) && pursuing_monster) continue;

		/* Forbid the generation of non-smart monsters except at level-creation
		 * or specific summons */
		if (!allow_non_smart && !rf_has(race->flags, RF_SMART) &&
			!rf_has(race->flags, RF_TERRITORIAL)) continue;

		/* Accept */
		table[i].prob3 = table[i].prob2;

		/* Total */
		total += table[i].prob3;
	}

	/* No legal monsters */
	if (total <= 0) return NULL;

	/* Pick a monster */
	race = get_mon_race_aux(total, table);

	/* Result */
	return race;
}

/**
 * ------------------------------------------------------------------------
 * Deleting of monsters and monster list handling
 * ------------------------------------------------------------------------ */
/**
 * Deletes a monster by index.
 *
 * When a monster is deleted, all of its objects are deleted.
 */
void delete_monster_idx(struct chunk *c, int m_idx)
{
	struct monster *mon = cave_monster(c, m_idx);
	struct loc grid;

	assert(m_idx > 0);
	assert(square_in_bounds(c, mon->grid));
	grid = mon->grid;

	/* Hack -- Reduce the racial counter */
	mon->race->cur_num--;

	/* Affect light? */
	if (mon->race->light != 0)
		player->upkeep->update |= PU_UPDATE_VIEW | PU_MONSTERS;

	/* Hack -- remove target monster */
	if (target_get_monster() == mon)
		target_set_monster(NULL);

	/* Hack -- remove tracked monster */
	if (player->upkeep->health_who == mon)
		health_track(player->upkeep, NULL);

	/* Monster is gone from square and group */
	square_set_mon(c, grid, 0);
	monster_remove_from_group(c, mon);

	/* Delete objects */
	struct object *obj = mon->held_obj;
	while (obj && (c == cave)) {
		struct object *next = obj->next;

		/* Delete the object.  Since it's in the cave's list do
		 * some additional bookkeeping. */
		if (obj->known) {
			/* It's not in a floor pile so remove it completely.
			 * Once compatibility with old savefiles isn't needed
			 * can skip the test and simply delist and delete
			 * since any obj->known from a monster's inventory
			 * will not be in a floor pile. */
			if (loc_is_zero(obj->known->grid) && (c == cave)) {
				delist_object(player->cave, obj->known);
				object_delete(player->cave, NULL, &obj->known);
			}
		}
		delist_object(c, obj);
		if (c == cave) {
			object_delete(cave, player->cave, &obj);
		}
		obj = next;
	}

	/* Free flow */
	flow_free(c, &mon->flow);

	/* Wipe the Monster */
	memset(mon, 0, sizeof(struct monster));

	/* Count monsters */
	c->mon_cnt--;

	/* Visual update */
	square_light_spot(c, grid);
}


/**
 * Deletes the monster, if any, at the given location.
 */
void delete_monster(struct chunk *c, struct loc grid)
{
	assert(square_in_bounds(c, grid));

	/* Delete the monster (if any) */
	if (square(c, grid)->mon > 0) {
		delete_monster_idx(c, square(c, grid)->mon);
	}
}


/**
 * Move a monster from index i1 to index i2 in the monster list.
 *
 * This should only be called when there is an actual monster at i1
 */
void monster_index_move(int i1, int i2)
{
	struct monster *mon;
	struct object *obj;

	/* Do nothing */
	if (i1 == i2) return;

	/* Old monster */
	mon = cave_monster(cave, i1);
	if (!mon) return;

	/* Update the cave */
	square_set_mon(cave, mon->grid, i2);

	/* Update midx */
	mon->midx = i2;

	/* Update group */
	if (!monster_group_change_index(cave, i2, i1)) {
		quit("Bad monster group info!") ;
		monster_groups_verify(cave);
	}

	/* Repair objects being carried by monster */
	for (obj = mon->held_obj; obj; obj = obj->next)
		obj->held_m_idx = i2;

	/* Update the target */
	if (target_get_monster() == mon)
		target_set_monster(cave_monster(cave, i2));

	/* Update the health bar */
	if (player->upkeep->health_who == mon)
		player->upkeep->health_who = cave_monster(cave, i2);

	/* Move monster */
	memcpy(cave_monster(cave, i2),
			cave_monster(cave, i1),
			sizeof(struct monster));

	/* Wipe hole */
	memset(cave_monster(cave, i1), 0, sizeof(struct monster));
}


/**
 * Compacts and reorders the monster list.
 *
 * This function can be very dangerous, use with caution!
 *
 * When `num_to_compact` is 0, we just reorder the monsters into a more compact
 * order, eliminating any "holes" left by dead monsters. If `num_to_compact` is
 * positive, then we delete at least that many monsters and then reorder.
 * We try not to delete monsters that are high level or close to the player.
 * Each time we make a full pass through the monster list, if we haven't
 * deleted enough monsters, we relax our bounds a little to accept
 * monsters of a slightly higher level, and monsters slightly closer to
 * the player.
 */
void compact_monsters(struct chunk *c, int num_to_compact)
{
	int m_idx, num_compacted, iter;

	int max_lev, min_dis, chance;


	/* Message (only if compacting) */
	if (num_to_compact)
		msg("Compacting monsters...");


	/* Compact at least 'num_to_compact' objects */
	for (num_compacted = 0, iter = 1; num_compacted < num_to_compact; iter++) {
		/* Get more vicious each iteration */
		max_lev = 5 * iter;

		/* Get closer each iteration */
		min_dis = 5 * (20 - iter);

		/* Check all the monsters */
		for (m_idx = 1; m_idx < cave_monster_max(c); m_idx++) {
			struct monster *mon = cave_monster(c, m_idx);

			/* Skip "dead" monsters */
			if (!mon->race) continue;

			/* High level monsters start out "immune" */
			if (mon->race->level > max_lev) continue;

			/* Ignore nearby monsters */
			if ((min_dis > 0) && (mon->cdis < min_dis)) continue;

			/* Saving throw chance */
			chance = 90;

			/* Only compact "Quest" Monsters in emergencies */
			if (rf_has(mon->race->flags, RF_QUESTOR) && (iter < 1000))
				chance = 100;

			/* Try not to compact Unique Monsters */
			if (rf_has(mon->race->flags, RF_UNIQUE)) chance = 99;

			/* All monsters get a saving throw */
			if (randint0(100) < chance) continue;

			/* Delete the monster */
			delete_monster(c, mon->grid);

			/* Count the monster */
			num_compacted++;
		}
	}


	/* Excise dead monsters (backwards!) */
	for (m_idx = cave_monster_max(c) - 1; m_idx >= 1; m_idx--) {
		struct monster *mon = cave_monster(c, m_idx);

		/* Skip real monsters */
		if (mon->race) continue;

		/* Move last monster into open hole */
		monster_index_move(cave_monster_max(c) - 1, m_idx);

		/* Compress "c->mon_max" */
		c->mon_max--;
	}
}


/**
 * Deletes all the monsters when the player leaves the level.
 *
 * This is an efficient method of simulating multiple calls to the
 * "delete_monster()" function, with no visual effects.
 *
 * Note that we must delete the objects the monsters are carrying, but we
 * do nothing with mimicked objects.
 */
void wipe_mon_list(struct chunk *c, struct player *p)
{
	int m_idx, i;

	/* Delete all the monsters */
	for (m_idx = cave_monster_max(c) - 1; m_idx >= 1; m_idx--) {
		struct monster *mon = cave_monster(c, m_idx);
		struct object *held_obj = mon ? mon->held_obj : NULL;

		/* Skip dead monsters */
		if (!mon->race) continue;

		/* Delete all the objects */
		if (held_obj) {
			/* Go through all held objects and remove from the cave's object
			 * list.  That way, the scan for orphaned objects in cave_free()
			 * doesn't attempt to access freed memory or free memory twice. */
			struct object *obj = held_obj;
			while (obj) {
				if (obj->oidx) {
					c->objects[obj->oidx] = NULL;
				}
				obj = obj->next;
			}
			object_pile_free(c, (p && c == cave) ? p->cave : NULL,
				held_obj);
		}

		/* Reduce the racial counter */
		mon->race->cur_num--;

		/* Monster is gone from square */
		square_set_mon(c, mon->grid, 0);

		/* Free flow */
		flow_free(c, &mon->flow);

		/* Wipe the Monster */
		memset(mon, 0, sizeof(struct monster));
	}

	/* Delete all the monster groups */
	for (i = 1; i < z_info->level_monster_max; i++) {
		if (c->monster_groups[i]) {
			monster_group_free(c, c->monster_groups[i]);
		}
	}

	/* Reset "cave->mon_max" */
	c->mon_max = 1;

	/* Reset "mon_cnt" */
	c->mon_cnt = 0;

	/* Hack -- no more target */
	target_set_monster(0);

	/* Hack -- no more tracking */
	health_track(p->upkeep, 0);
}

/**
 * ------------------------------------------------------------------------
 * Monster creation utilities:
 *  Getting a new monster index
 *  Creating objects for monsters to carry or mimic
 *  Calculating hitpoints
 * ------------------------------------------------------------------------ */
/**
 * Returns the index of a "free" monster, or 0 if no slot is available.
 *
 * This routine should almost never fail, but it *can* happen.
 * The calling code must check for and handle a 0 return.
 */
int16_t mon_pop(struct chunk *c)
{
	int m_idx;

	/* Normal allocation */
	if (cave_monster_max(c) < z_info->level_monster_max) {
		/* Get the next hole */
		m_idx = cave_monster_max(c);

		/* Expand the array */
		c->mon_max++;

		/* Count monsters */
		c->mon_cnt++;

		return m_idx;
	}

	/* Recycle dead monsters if we've run out of room */
	for (m_idx = 1; m_idx < cave_monster_max(c); m_idx++) {
		struct monster *mon = cave_monster(c, m_idx);

		/* Skip live monsters */
		if (!mon->race) {
			/* Count monsters */
			c->mon_cnt++;

			/* Use this monster */
			return m_idx;
		}
	}

	/* Warn the player if no index is available */
	if (character_dungeon)
		msg("Too many monsters!");

	/* Try not to crash */
	return 0;
}


/**
 * Set hallucinatory monster race
 */
static void set_hallucinatory_race(struct monster *mon)
{
	/* Try hard to find a random race */
	int tries = 1000;
	while (tries) {
		int race_idx = randint0(z_info->r_max);
		struct monster_race *race = &r_info[race_idx];
		if ((race->rarity != 0) && one_in_(race->rarity)) {
			mon->image_race = race;
			return;
		}
		tries--;
	}

	/* No hallucination this time */
	mon->image_race = mon->race;
}


/**
 * Determines a wandering destination for a monster.
 */
static void new_wandering_destination(struct chunk *c, struct monster *mon)
{
	struct monster_race *race = mon->race;

	/* Many monsters don't get wandering destinations: */
	if (rf_has(race->flags, RF_NEVER_MOVE) ||
		rf_has(race->flags, RF_HIDDEN_MOVE)	||
		!(rf_has(race->flags, RF_SMART) ||
		  rf_has(race->spell_flags, RSF_SHRIEK))) {
		return;
	}

	mon->wandering_dist = z_info->wander_range;
	monster_group_new_wandering_flow(c, mon, loc(0, 0));
}

/**
 * ------------------------------------------------------------------------
 * Placement of a single monster
 * These are the functions that actually put the monster into the world
 * ------------------------------------------------------------------------ */
/**
 * Attempts to place a copy of the given monster at the given position in
 * the dungeon.
 *
 * All of the monster placement routines eventually call this function. This
 * is what actually puts the monster in the dungeon (i.e., it notifies the cave
 * and sets the monster's position). The dungeon loading code also calls this
 * function directly.
 *
 * `origin` is the item origin to use for any monster drops (e.g. ORIGIN_DROP,
 * ORIGIN_DROP_PIT, etc.) The dungeon loading code calls this with origin = 0,
 * which prevents the monster's drops from being generated again.
 *
 * Returns the m_idx of the newly copied monster, or 0 if the placement fails.
 */
int16_t place_monster(struct chunk *c, struct loc grid, struct monster *mon,
		uint8_t origin)
{
	int16_t m_idx;
	struct monster *new_mon;
	struct monster_group_info info = mon->group_info;
	bool loading = mon->midx > 0;

	assert(square_in_bounds(c, grid));
	assert(!square_monster(c, grid));

	/* Get a new record, or recycle the old one */
	if (loading) {
		m_idx = mon->midx;
		c->mon_max++;
		c->mon_cnt++;
	} else {
		m_idx = mon_pop(c);
		if (!m_idx) return 0;
	}

	/* Copy the monster */
	new_mon = cave_monster(c, m_idx);
	memcpy(new_mon, mon, sizeof(struct monster));

	/* Set the ID */
	new_mon->midx = m_idx;

	/* Set the location and origin */
	square_set_mon(c, grid, new_mon->midx);
	new_mon->grid = grid;
	assert(square_monster(c, grid) == new_mon);
	mon->origin = origin;

	/* Assign monster to its monster group */
	monster_group_assign(c, new_mon, info, loading);

	/* Give the monster a place to wander towards */
	new_wandering_destination(c, mon);

	/* Update the monster */
	update_mon(new_mon, c, true);

	/* Count racial occurrences */
	new_mon->race->cur_num++;

	/* Result */
	return m_idx;
}

/**
 * Attempts to place a monster of the given race at the given location.
 *
 * If `sleep` is true, the monster is placed with its default sleep value,
 * which is given in monster.txt.
 *
 * `origin` is the item origin to use for any monster drops (e.g. ORIGIN_DROP,
 * ORIGIN_DROP_PIT, etc.)
 *
 * This routine refuses to place out-of-depth "FORCE_DEPTH" monsters.
 *
 * This is the only function which may place a monster in the dungeon,
 * except for the savefile loading code, which calls place_monster()
 * directly.
 */
bool place_new_monster_one(struct chunk *c, struct loc grid,
						   struct monster_race *race, bool sleep,
						   bool ignore_depth, 
						   struct monster_group_info group_info, uint8_t origin)
{
	struct monster *mon, *leader;
	struct monster monster_body;
	int index = group_info.index;
	struct monster_group *group;

	assert(square_in_bounds(c, grid));
	assert(race && race->name);

	/* Not where monsters already are */
	if (square_monster(c, grid)) return false;

	/* Not where the player already is */
	if (loc_eq(player->grid, grid)) return false;

	/* Prevent monsters from being placed where they cannot walk, but allow
	 * other feature types */
	if (!square_is_monster_walkable(c, grid)) return false;

	/* No creation on glyphs */
	if (square_iswarded(c, grid)) return false;

	/* "unique" monsters must be "unique" */
	if (rf_has(race->flags, RF_UNIQUE) && (race->cur_num >= race->max_num))
		return false;

	/* Check for depth issues except where we're ignoring that */
	if (!ignore_depth) {
		/* Force depth monsters may NOT normally be created out of depth */
		if (rf_has(race->flags, RF_FORCE_DEPTH) && c->depth < race->level)
			return false;

		/* Special generation may NOT normally be created */
		if (rf_has(race->flags, RF_SPECIAL_GEN))
			return false;
	}

	/* Check out-of-depth-ness */
	if (OPT(player, cheat_hear)) {
		if (race->level > c->depth) {
			if (rf_has(race->flags, RF_UNIQUE)) {
				/* OOD unique */
				msg("Deep unique (%s).", race->name);
			} else {
				/* Normal monsters but OOD */
				msg("Deep monster (%s).", race->name);
			}
		} else if (rf_has(race->flags, RF_UNIQUE)) {
			msg("Unique (%s).", race->name);
		}
	}

	/* Get local monster */
	mon = &monster_body;

	/* Clean out the monster */
	memset(mon, 0, sizeof(struct monster));

	/* Save the race */
	mon->race = race;

	/* Determine group leader, if any */
	group = monster_group_by_index(c, index);
	if (group) {
		leader = cave_monster(c, group->leader);
	} else {
		leader = NULL;
	}

	/* Save the hallucinatory race */
	if (race == lookup_monster("Morgoth, Lord of Darkness")) {
		mon->image_race = lookup_monster("Melkor, Rightful Lord of Arda");
	} else if (leader) {
		mon->image_race = leader->image_race;
	} else {
		set_hallucinatory_race(mon);
	}

	/* Set alertness */
	if (sleep) {
		int amount = 0;

		/* Enforce sleeping if needed */
		if (race->sleep) {
			amount = randint1(race->sleep);
		}

		/* If there is a lead monster, copy its value */
		if (leader) {
			amount = ALERTNESS_ALERT - leader->alertness;
		} else if (player->on_the_run) {
			/* Many monsters are more alert during the player's escape */
			if ((player->depth == 0) && (amount > 0)) {
				/* including all monsters on the Gates level */
				amount = damroll(1, 3);
			} else if ((race->level > player->depth + 2) &&
					   !square_isvault(c, grid) && (amount > 0)) { 
				/* and dangerous monsters out of vaults (which are assumed
				 * to be in direct pursuit) */
				amount = damroll(1, 3);
			}
		}

		mon->alertness = ALERTNESS_ALERT - amount;
	}

	/* Uniques get a fixed amount of HP */
	if (rf_has(race->flags, RF_UNIQUE)) {
		mon->maxhp = race->hdice * (1 + race->hside) / 2;
	} else {
		mon->maxhp = damroll(race->hdice, race->hside);
	}

	/* Initialize mana */
	mon->mana = z_info->mana_max;

	/* Initialize song */
	mon->song = NULL;

	/* And start out fully healthy */
	mon->hp = mon->maxhp;

	/* Extract the monster base speed */
	calc_monster_speed(mon);

	/* Mark minimum range for recalculation */
	mon->min_range = 0;

	/* Initialize flow */
	flow_new(c, &mon->flow);

	/* Give almost no starting energy (avoids clumped movement) -
	 * same as old FORCE_SLEEP flag, which is now the default behaviour */
	mon->energy = (uint8_t)randint0(10);

	/* Affect light? */
	if (mon->race->light != 0)
		player->upkeep->update |= PU_UPDATE_VIEW | PU_MONSTERS;

	/* Set the group info */
	mon->group_info.index = index ? index : monster_group_index_new(c);
	mon->group_info.role = group_info.role;

	/* Place the monster in the dungeon */
	if (!place_monster(c, grid, mon, origin))
		return (false);

	/* Monsters that don't pursue you drop their treasure upon being created */
	if (rf_has(mon->race->flags, RF_TERRITORIAL)) {
		drop_loot(c, mon, grid, false);
	}

	/* Success */
	return (true);
}


/**
 * ------------------------------------------------------------------------
 * More complex monster placement routines
 * ------------------------------------------------------------------------ */
/**
 * Race for escort type
 */
static struct monster_race *place_escort_race = NULL;

/**
 * Help pick an escort type
 */
static bool place_escort_okay(struct monster_race *race)
{
	/* Require similar "race" */
	if (race->base != place_escort_race->base) return false;

	/* Skip more advanced monsters */
	if (race->level > place_escort_race->level) return false;

	/* Skip unique monsters */
	if (rf_has(race->flags, RF_UNIQUE)) return false;

	/* Paranoia -- Skip identical monsters */
	if (place_escort_race == race) return false;

	/* Okay */
	return true;
}


/**
 * Attempt to place a unique's unique ally at a given location
 */
static void place_monster_unique_friend(struct chunk *c, struct loc grid,
										struct monster_race *race, bool sleep,
										struct monster_group_info group_info,
										uint8_t origin)
{
	int i, r;
	
	/* Find the unique friend */
	for (r = 1; r < z_info->r_max; r++) {
		struct monster_race *race1 = &r_info[r];

		if ((race->base == race1->base) &&
			rf_has(race1->flags, RF_UNIQUE_FRIEND)) {
			/* Random direction */
			int start = randint0(8);

			/* Check each direction */
			for (i = start; i < 8 + start; i++) {
				struct loc try = loc_sum(grid, ddgrid_ddd[i % 8]);
				if (place_new_monster_one(c, try, race1, sleep, true, group_info,
										  origin)) {
					/* Success */
					break;
				}
			}
		}
	}
}

/**
 * Attempts to place a group of monsters of race `r_idx` around
 * the given location. The number of monsters to place is `total`.
 *
 * If `sleep` is true, the monster is placed with its default sleep value,
 * which is given in monster.txt.
 *
 * `origin` is the item origin to use for any monster drops (e.g. ORIGIN_DROP,
 * ORIGIN_DROP_PIT, etc.)
 */
static bool place_new_monster_group(struct chunk *c, struct loc grid,
		struct monster_race *race, bool sleep,
		struct monster_group_info group_info,
		int total, uint8_t origin)
{
	int n, i;

	int loc_num;

	/* Locations of the placed monsters */
	struct loc *loc_list = mem_zalloc(sizeof(struct loc) *
									  z_info->monster_group_max);

	/* Sanity and bounds check */
	assert(race);
	total = MIN(total, z_info->monster_group_max);

	/* Start on the monster */
	loc_num = 1;
	loc_list[0] = grid;

	/* Puddle monsters, breadth first, up to total */
	for (n = 0; (n < loc_num) && (loc_num < total); n++) {
		int start = randint0(8);

		/* Check each direction, up to total */
		for (i = start; (i < 8) && (loc_num < total); i++) {
			struct loc try = loc_sum(loc_list[n], ddgrid_ddd[i % 8]);

			/* Walls and Monsters block flow */
			if (!square_isempty(c, try)) continue;

			/* Attempt to place another monster */
			if (place_new_monster_one(c, try, race, sleep, false, group_info,
									  origin)) {
				/* Add it to the "hack" set */
				loc_list[loc_num] = try;
				loc_num++;
			}
		}
	}

	mem_free(loc_list);

	/* Return true if it places >= 1 monster (even if fewer than desired) */
	return (loc_num > 1);
}

/**
 * Helper function to place monsters that appear as friends or escorts
 */
static void place_monster_escort(struct chunk *c, struct loc grid,
								 struct monster_race *race, bool sleep,
								 struct monster_group_info group_info,
								 uint8_t origin)
{
	int n, i;
	int loc_num;
	int escort_size;
	int extras = 0;

	/* Locations of the placed monsters */
	struct loc *loc_list = mem_zalloc(sizeof(struct loc) *
									  z_info->monster_group_max);

	/* Monster races of the placed monsters */
	struct monster_race **escort_races =
		mem_zalloc(sizeof(struct monster_race*) * z_info->monster_group_max);
	struct monster_race *escort_race;

	assert(race);

	/* Calculate the number of escorts we want. */
	if (rf_has(race->flags, RF_ESCORTS)) {
		escort_size = rand_range(8, 16);
	} else {
		escort_size = rand_range(4, 7);
	}
	escort_size = MIN(escort_size, z_info->monster_group_max);

	/* Use the leader's monster type to restrict the escorts. */
	place_escort_race = race;

	/* Prepare allocation table */
	get_mon_num_prep(place_escort_okay);

	/* Build monster table, get indices of all escorts */
	for (i = 0; i < escort_size; i++) {
		if (extras > 0) {
			escort_races[i] = escort_races[i - 1];
			extras--;
		} else {
			escort_races[i] = get_mon_num(race->level, true, false, false);

			/* Skip this creature if get_mon_num failed (paranoia) */
			if (escort_races[i] == NULL) continue;

			if (rf_has(escort_races[i]->flags, RF_FRIENDS)) {
				extras = rand_range(2, 3);
			} else if (rf_has(escort_races[i]->flags, RF_FRIEND)) {
				extras = rand_range(1, 2);
			} else {
				extras = 0;
			}
		}
	}

	escort_race = escort_races[0];

	/* Start on the monster */
	loc_num = 1;
	loc_list[0] = grid;

	/* Puddle monsters, breadth first, up to escort size */
	for (n = 0; (n < loc_num) && (loc_num < escort_size); n++) {
		int start = randint0(8);

		/* Check each direction, up to escort size */
		for (i = start; (i < 8) && (loc_num < escort_size); i++) {
			struct loc try = loc_sum(loc_list[n], ddgrid_ddd[i % 8]);

			/* Walls and Monsters block flow */
			if (!square_isempty(c, try)) continue;

			/* Attempt to place another monster */
			if (place_new_monster_one(c, try, escort_race, sleep, false, group_info,
									  origin)) {
				/* Get index of the next escort */
				escort_race = escort_races[loc_num];

				/* Add it to the "hack" set */
				loc_list[loc_num] = try;
				loc_num++;
			}
		}
	}

	/* Prepare allocation table */
	get_mon_num_prep(NULL);

	/* Success */
	mem_free(loc_list);
	mem_free(escort_races);
}

/**
 * Attempts to place a monster of the given race at the given location.
 *
 * Note that certain monsters are placed with a large group of
 * identical or similar monsters. However, if `group_okay` is false,
 * then such monsters are placed by themselves.
 *
 * If `sleep` is true, the monster is placed with its default sleep value,
 * which is given in monster.txt.
 *
 * `origin` is the item origin to use for any monster drops (e.g. ORIGIN_DROP,
 * ORIGIN_DROP_PIT, etc.)
 */
bool place_new_monster(struct chunk *c, struct loc grid,
					   struct monster_race *race, bool sleep, bool group_ok,
					   struct monster_group_info group_info, uint8_t origin)
{
	assert(c);
	assert(race);

	/* If we don't have a group index already, make one; our first monster
	 * will be the leader */
	if (!group_info.index) {
		group_info.index = monster_group_index_new(c);
	}

	/* Place one monster, or fail */
	if (!place_new_monster_one(c, grid, race, sleep, false, group_info,
							   origin)) {
		return (false);
	}

	/* We're done unless the group flag is set */
	if (!group_ok) return (true);

	/* Go through friends/escorts flags */
	if (rf_has(race->flags, RF_UNIQUE_FRIEND)) {
		place_monster_unique_friend(c, grid, race, sleep, group_info, origin);
	} else if (rf_has(race->flags, RF_FRIENDS)) {
		/* relative depth  |  number in group  (FRIENDS) */
		/*             -2  |    2                        */
		/*             -1  |  2 / 3                      */
		/*              0  |    3                        */
		/*             +1  |  3 / 4                      */
		/*             +2  |    4                        */
		int amount = (rand_range(6,7) + (c->depth - race->level)) / 2;
		amount = MIN(MAX(amount, 2), 4);
		group_info.role = MON_GROUP_MEMBER;
		return place_new_monster_group(c, grid, race, sleep, group_info,
									   amount, origin);
	} else if (rf_has(race->flags, RF_FRIEND)) {
		/* relative depth  |  chance of having a companion  (FRIEND) */
		/*             -2  |    0%                                   */
		/*             -1  |   25%                                   */
		/*              0  |   50%                                   */
		/*             +1  |   75%                                   */
		/*             +2  |  100%                                   */
		int amount = 1;
		if (randint1(4) <= c->depth - race->level + 2) amount++;
		group_info.role = MON_GROUP_MEMBER;
		return place_new_monster_group(c, grid, race, sleep, group_info,
									   amount, origin);
	} else if (rf_has(race->flags, RF_ESCORT) ||
			   rf_has(race->flags, RF_ESCORTS)) {
		group_info.role = MON_GROUP_SERVANT;
		place_monster_escort(c, grid, race, sleep, group_info, origin);
	}

	/* Success */
	return true;
}


/**
 * Picks a monster race, makes a new monster of that race, then attempts to
 * place it in the dungeon. The monster race chosen will be appropriate for
 * dungeon level equal to `depth`.
 *
 * If `sleep` is true, the monster is placed with its default sleep value,
 * which is given in monster.txt.
 *
 * If `group_okay` is true, we allow the placing of a group, if the chosen
 * monster appears with friends or an escort.
 *
 * `origin` is the item origin to use for any monster drops (e.g. ORIGIN_DROP,
 * ORIGIN_DROP_PIT, etc.)
 *
 * Returns true if we successfully place a monster.
 */
bool pick_and_place_monster(struct chunk *c, struct loc grid, int depth,
		bool sleep, bool group_okay, uint8_t origin)
{
	/* Pick a monster race, no specified group */
	struct monster_race *race = get_mon_num(depth, false, sleep,
											origin == ORIGIN_DROP_VAULT);
	struct monster_group_info info = { 0, 0 };

	if (race) {
		return place_new_monster(c, grid, race, sleep, group_okay, info,
								 origin);
	} else {
		return false;
	}
}


/**
 * Has a very good go at placing a monster of kind represented by a flag
 * (eg RF_DRAGON) at grid. It is governed by a maximum depth and tries
 * 100 times at this depth and each shallower depth.
 */
void place_monster_by_flag(struct chunk *c, struct loc grid, int flg1, int flg2,
						   bool allow_unique, int max_depth, bool spell)
{
	bool got_race = false;
	int tries = 0;
	struct monster_race *race = NULL;
	int depth = max_depth;
	struct monster_group_info info = { 0, 0 };
		
	while (!got_race && (depth > 0)) {		
		race = get_mon_num(depth, false, true, true);
        if (race && (allow_unique || !rf_has(race->flags, RF_UNIQUE))) {
            if (rf_has(race->flags, flg1)) {
                got_race = true;
                break;
            } else if ((flg2 > 0) && rf_has(race->flags, flg2)) {
                got_race = true;
                break;
			}
		}

		tries++;
		if (tries >= 100) {
			tries = 0;
			depth--;
		}
	}

	/* Place a monster of that type if you could find one */
	if (got_race) {
		place_new_monster_one(c, grid, race, true, false, info,
							  ORIGIN_DROP_VAULT);
	}
}


/**
 * Has a very good go at placing a monster of kind represented by its base
 * glyph (eg 'v' for vampire) at grid. It is governed by a maximum depth and
 * tries 100 times at this depth and each shallower depth.
 */
void place_monster_by_letter(struct chunk *c, struct loc grid, char ch,
							 bool allow_unique, int max_depth)
{
	bool got_race = false;
	int tries = 0;
	struct monster_race *race = NULL;
	int depth = max_depth;
	char stmp[2] = { '\0', '\0' };
	wchar_t wtmp[2];
	struct monster_group_info info = { 0, 0 };
	
	/* Try to convert the character to one wide character, return on failure */
	stmp[0] = ch;
	if (text_mbstowcs(wtmp, stmp, N_ELEMENTS(wtmp)) != 1) {
		return;
	}
	while (!got_race && (depth > 0)) {		
		race = get_mon_num(depth, false, true, true);
		if (race->d_char == wtmp[0] &&
			(allow_unique || !rf_has(race->flags, RF_UNIQUE))) {
			got_race = true;
			break;
		}

		tries++;
		if (tries >= 100) {
			tries = 0;
			depth--;
		}
	}

	/* Place a monster of that type if you could find one */
	if (got_race) {
		place_new_monster_one(c, grid, race, true, false, info,
							  ORIGIN_DROP_VAULT);
	}
}

/**
 * Picks a monster race, makes a new monster of that race, then attempts to
 * place it in the dungeon at least `dis` away from the player. The monster
 * race chosen will be appropriate for dungeon level equal to `depth`.
 *
 * If `sleep` is true, the monster is placed with its default sleep value,
 * which is given in monster.txt.
 *
 * Returns true if we successfully place a monster.
 */
bool pick_and_place_monster_on_stairs(struct chunk *c, struct player *p,
									  bool sleep, int depth, bool force_undead)
{
	struct loc stair, grid;
	struct monster *mon;
	bool displaced = false;
	bool placed = false;
	char dir[5];
	int tries = 0;

	/* No monsters come through the stairs on tutorial/challenge levels */
	if (in_tutorial() || p->game_type > 0)	return false;

	/* Get a stair location */
	if (!cave_find(c, &stair, square_isstairs))	return false;

	/* Default the new location to this location */
	grid = stair;

	/* If there is something on the stairs, try adjacent squares */
	mon = square_monster(c, stair);
	if (mon || loc_eq(p->grid, stair)) {
		int d, start;

		/* If the monster on the squares cannot move, then simply give up */
		if (mon && (rf_has(mon->race->flags, RF_NEVER_MOVE) ||
					rf_has(mon->race->flags, RF_HIDDEN_MOVE))) {
			return false;
		}

		/* Look through the eligible squares and choose an empty one randomly */
		start = randint0(8);
		for (d = start; d < 8 + start; d++) {
			struct loc grid1 = loc_sum(grid, ddgrid_ddd[d % 8]);

			/* Check Bounds */
			if (!square_in_bounds(c, grid1)) continue;

			/* Check Empty Square */
			if (!(square_isempty(c, grid1) ||
				  square_isplayer(c, grid1))) {
				continue;
			}

			/* Displace the existing monster */
			grid = grid1;
			displaced = true;
			break;
		}

		/* Give up */
		if (!displaced) return false;
	}

	/* First, displace the existing monster to the safe square */
	if (displaced) {
		monster_swap(stair, grid);

		/* Need to update the player's field of view if she is moved */
		if (loc_eq(p->grid, grid)) {
				update_view(c, p);
		}
	}

	/* Try hard to put a monster on the stairs */
	while (!placed && (tries < 50)) {
		/* Modify the monster generation level based on the stair type */
		int monster_level = depth;
		int feat = square_feat(c, stair)->fidx;
		if (feat == FEAT_LESS_SHAFT) {
			monster_level -= 2;
			my_strcpy(dir, "down", sizeof(dir));
		} else if (feat == FEAT_LESS) {
			monster_level -= 1;
			my_strcpy(dir, "down", sizeof(dir));
		} else if (feat ==  FEAT_MORE) {
			monster_level += 1;
			my_strcpy(dir, "up", sizeof(dir));
		} else if (feat == FEAT_MORE_SHAFT) {
			monster_level += 2;
			my_strcpy(dir, "up", sizeof(dir));
		}

		/* Correct deviant monster levels */
		if (monster_level < 1)	monster_level = 1;	

		/* Sometimes only wraiths are allowed */
		if (force_undead) {
			place_monster_by_flag(c, stair, RF_UNDEAD, -1, true,
								  MAX(monster_level + 3, 13), false);
			placed = true;
		} else {
			/* But usually allow most monsters */
			placed = pick_and_place_monster(c, stair, monster_level, false,
											true, ORIGIN_DROP);
		}

		tries++;
	}

	/* Print messages etc */
	if (placed) {
		struct monster *mon1 = square_monster(c, stair);

		/* Display a message if seen */
		if (monster_is_visible(mon1)) {
			char message[240];

			if (monster_has_friends(mon1)) {
				strnfmt(message, sizeof(message),
					"A group of enemies come %s the stair",
					dir);
			} else {
				char m_name[80];
				monster_desc(m_name, sizeof(m_name), mon1,
					MDESC_STANDARD);
				strnfmt(message, sizeof(message),
					"%s comes %s the stair", m_name, dir);
			}

			if (displaced) {
				char who[80];

				if (loc_eq(p->grid, grid)) {
					my_strcpy(who, "you", 80);
				} else {
					monster_desc(who, sizeof(who),
						square_monster(c, grid),
						MDESC_DIED_FROM);
				}
				msg("%s, forcing %s out of the way!", message,
					who);
			} else {
				msg("%s!", message);
			}
			return true;
		}
	}

	/* Didn't happen or not seen */
	return false;
}


/**
 * Picks a monster race, makes a new monster of that race, then attempts to
 * place it in the dungeon at least `dis` away from the player. The monster
 * race chosen will be appropriate for dungeon level equal to `depth`.
 *
 * If `sleep` is true, the monster is placed with its default sleep value,
 * which is given in monster.txt.
 *
 * Returns true if we successfully place a monster.
 */
bool pick_and_place_distant_monster(struct chunk *c, struct player *p,
									bool sleep, int depth)
{
	struct loc grid;
	int	attempts_left = 1000;

	assert(c);

	/* Find a legal, distant, unoccupied, space */
	while (--attempts_left) {
		/* Pick a location */
		grid = loc(randint0(c->width), randint0(c->height));

		/* Require "naked" floor grid */
		if (!square_isempty(c, grid)) continue;

		/* Accept grids out of view */
		if (!los(c, grid, p->grid)) break;
	}

	if (!attempts_left) {
		if (OPT(p, cheat_xtra) || OPT(p, cheat_hear))
			msg("Warning! Could not allocate a new monster.");

		return false;
	}

	/* Attempt to place the monster, allow groups */
	if (pick_and_place_monster(c, grid, depth, sleep, true, ORIGIN_DROP))
		return true;

	/* Nope */
	return false;
}

struct init_module mon_make_module = {
	.name = "monster/mon-make",
	.init = init_race_allocs,
	.cleanup = cleanup_race_allocs
};
