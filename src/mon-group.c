/**
 * \file mon-group.c
 * \brief Monster group behaviours
 *
 * Copyright (c) 2018 Nick McConnell
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
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-group.h"
#include "mon-make.h"
#include "mon-util.h"
#include "monster.h"
#include "tutorial.h"

/**
 * Allocate a new monster group
 */
static struct monster_group *monster_group_new(struct chunk *c)
{
	struct monster_group *group = mem_zalloc(sizeof(struct monster_group));
	flow_new(c, &group->flow);
	return group;
}

/**
 * Free a monster group
 */
void monster_group_free(struct chunk *c, struct monster_group *group)
{
	/* Free the member list */
	while (group->member_list) {
		struct mon_group_list_entry *next = group->member_list->next;
		mem_free(group->member_list);
		group->member_list = next;
	}

	flow_free(c, &group->flow);
	mem_free(group);
}

/**
 * Handle the leader of a group being removed
 *
 * In Sil, we just grab the next monster in the list
 */
static void monster_group_remove_leader(struct chunk *c, struct monster *leader,
										struct monster_group *group)
{
	struct mon_group_list_entry *list_entry = group->member_list;

	/* Look for another leader */
	while (list_entry) {
		struct monster *mon = cave_monster(c, list_entry->midx);

		if (!mon) {
			list_entry = list_entry->next;
			continue;
		} else {
			/* Appoint the new leader */
			group->leader = mon->midx;
			mon->group_info.role = MON_GROUP_LEADER;
			break;
		}
	}

	monster_groups_verify(c);
}

/**
 * Remove a monster from a monster group, deleting the group if it's empty.
 * Deal with removal of the leader.
 */
void monster_remove_from_group(struct chunk *c, struct monster *mon)
{
	struct monster_group *group = c->monster_groups[mon->group_info.index];
	struct mon_group_list_entry *list_entry = group->member_list;

	/* Check if the first entry is the one we want */
	if (list_entry->midx == mon->midx) {
		if (!list_entry->next) {
			/* If it's the only monster, remove the group */
			monster_group_free(c, group);
			c->monster_groups[mon->group_info.index] = NULL;
		} else {
			/* Otherwise remove the first entry */
			group->member_list = list_entry->next;
			mem_free(list_entry);
			if (group->leader == mon->midx) {
				monster_group_remove_leader(c, mon, group);
			}
		}
		return;
	}

	/* Check - necessary? */
	if (list_entry->next == NULL) {
		quit_fmt("Bad group: index=%d, monster=%d", mon->group_info.index,
				 mon->midx);
	}

	/* We have to look further down the member list */
	while (list_entry->next) {
		if (list_entry->next->midx == mon->midx) {
			struct mon_group_list_entry *remove = list_entry->next;
			list_entry->next = list_entry->next->next;
			mem_free(remove);
			if (group->leader == mon->midx) {
				monster_group_remove_leader(c, mon, group);
			}
			break;
		}
		list_entry = list_entry->next;
	}
	group->size--;
	monster_groups_verify(c);
}

/**
 * Get the next available monster group index
 */
int monster_group_index_new(struct chunk *c)
{
	int index;

	for (index = 1; index < z_info->level_monster_max; index++) {
		if (!(c->monster_groups[index])) return index;
	}

	/* Fail, very unlikely */
	return 0;
}

/**
 * Add a monster to an existing monster group
 */
void monster_add_to_group(struct chunk *c, struct monster *mon,
						  struct monster_group *group)
{
	struct mon_group_list_entry *list_entry;

	/* Confirm we're adding to the right group */
	assert(mon->group_info.index == group->index);

	/* Make a new list entry and add it to the start of the list */
	list_entry = mem_zalloc(sizeof(struct mon_group_list_entry));
	list_entry->midx = mon->midx;
	list_entry->next = group->member_list;
	group->member_list = list_entry;
	group->size++;
	mon->group_info.role = MON_GROUP_MEMBER;
}


/**
 * Make a monster group for a single monster
 */
static void monster_group_start(struct chunk *c, struct monster *mon)
{
	/* Get a group and a group index */
	struct monster_group *group = monster_group_new(c);
	int index = monster_group_index_new(c);
	assert(index);

	/* Put the group in the group list */
	c->monster_groups[index] = group;

	/* Fill out the group */
	group->index = index;
	group->leader = mon->midx;
	group->member_list = mem_zalloc(sizeof(struct mon_group_list_entry));
	group->member_list->midx = mon->midx;
	group->size = 1;

	/* Write the index to the monster's group info, make it leader */
	mon->group_info.index = index;
	mon->group_info.role = MON_GROUP_LEADER;
}

/**
 * Assign a monster to a monster group
 */
void monster_group_assign(struct chunk *c, struct monster *mon,
						  struct monster_group_info info, bool loading)
{
	int index = info.index;
	struct monster_group *group = monster_group_by_index(c, index);

	if (!loading) {
		/* For newly created monsters, use the group start and add functions */
		if (group) {
			monster_add_to_group(c, mon, group);
		} else {
			monster_group_start(c, mon);
		}
	} else {
		/* For loading from a savefile, build by hand */
		struct mon_group_list_entry *entry = mem_zalloc(sizeof(*entry));

		/* Check the index */
		index = info.index;
		if (!index) {
			/* Everything should have a group */
			quit_fmt("Monster %d has no group", mon->midx);
		}

		/* Fill out the group, creating if necessary */
		group = monster_group_by_index(c, index);
		if (!group) {
			group = monster_group_new(c);
			group->index = index;
			c->monster_groups[index] = group;
		}
		if (info.role == MON_GROUP_LEADER) {
			group->leader = mon->midx;
		}

		/* Add this monster */
		entry->midx = mon->midx;
		entry->next = group->member_list;
		group->member_list = entry;
		group->size++;
	}
}

/**
 * Get the index of a monster group
 */
int monster_group_index(struct monster_group *group)
{
	return group->index;
}

/**
 * Get a monster group from its index
 */
struct monster_group *monster_group_by_index(struct chunk *c, int index)
{
	return index ? c->monster_groups[index] : NULL;
}

/**
 * Change the group record of the index of a monster
 */
bool monster_group_change_index(struct chunk *c, int new, int old)
{
	int index = cave_monster(c, old)->group_info.index;
	struct monster_group *group = monster_group_by_index(c, index);
	struct mon_group_list_entry *entry = group->member_list;

	if (group->leader == old) {
		group->leader = new;
	}
	while (entry) {
		if (entry->midx == old) {
			entry->midx = new;
			return true;
		}
		entry = entry->next;
	}

	return false;
}

/**
 * Get the size of a monster's group
 */
int monster_group_size(struct chunk *c, const struct monster *mon)
{
	int index = mon->group_info.index;
	struct monster_group *group = c->monster_groups[index];
	return group->size;
}

/**
 * Get the index of the leader of a monster group
 */
int monster_group_leader_idx(struct monster_group *group)
{
	return group->leader;
}

/**
 * Get the leader of a monster's group
 */
struct monster *monster_group_leader(struct chunk *c, struct monster *mon)
{
	int index = mon->group_info.index;
	struct monster_group *group = c->monster_groups[index];
	return cave_monster(c, group->leader);
}

/**
 * Set the centre of a new flow for a monster group, and update the flow
 */
void monster_group_new_wandering_flow(struct chunk *c, struct monster *mon,
									  struct loc tgrid)
{	
	int i;
	struct monster_group *group = monster_group_by_index(c,
														 mon->group_info.index);
	struct monster *leader = cave_monster(c, group->leader);
	struct monster_race *race = !!leader ? leader->race : NULL;
	struct loc grid;

	/* On loading, the leader may not be loaded yet, so set this when it is */
	if (!leader) return;

	/* Territorial monsters target their creation location; same with
	 * the tutorial */
	if (rf_has(race->flags, RF_TERRITORIAL) || in_tutorial()) {
		/* They only pick a new location on creation.  Detect this using the
		 * fact that speed hasn't been determined yet on creation */
		if (mon->mspeed == 0) {
			update_flow(c, &group->flow, leader);
		}
	} else if (square_in_bounds_fully(c, tgrid)) {
		/* If a location was requested, use that */
		group->flow.centre = tgrid;
		update_flow(c, &group->flow, leader);
	} else {
		/* Otherwise choose a location */
		if (rf_has(race->flags, RF_SMART) &&
			!rf_has(race->flags, RF_TERRITORIAL) &&
			(player->depth != z_info->dun_depth) && one_in_(5) &&
			cave_find(c, &grid, square_isstairs) &&
			square_monster(c, grid) && !square_isvault(c, grid)) {
			/* Sometimes intelligent monsters want to pick a staircase and leave
			 * the level */
			group->flow.centre = grid;
			update_flow(c, &group->flow, leader);
		} else {
			/* Otherwise pick a random location (on a floor, in a room, and not
			 * in a vault) */
			for (i = 0; i < 100; i++) {
				grid.y = randint0(c->height);
				grid.x = randint0(c->width);
				if (square_in_bounds_fully(c, grid) &&
					square_isfloor(c, grid) && 
					square_isroom(c, grid) &&
					!square_isvault(c, grid)) {
					group->flow.centre = grid;
					update_flow(c, &group->flow, leader);
					break;
				}
			}
		}
	}

	/* Reset the pause (if any) */
	group->wandering_pause = 0;
}

/**
 * Verify the integrity of all the monster groups
 */
void monster_groups_verify(struct chunk *c)
{
	int i;

	for (i = 0; i < z_info->level_monster_max; i++) {
		if (c->monster_groups[i]) {
			struct monster_group *group = c->monster_groups[i];
			struct mon_group_list_entry *entry = group->member_list;
			while (entry) {
				struct monster *mon = cave_monster(c, entry->midx);
				struct monster_group_info info = mon->group_info;
				if (info.index != i) {
					quit_fmt("Bad group index: group: %d, monster: %d", i,
							 info.index);
				}
				entry = entry->next;
			}
		}
	}
}

