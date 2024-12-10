/**
 * \file cave-map.c
 * \brief Lighting and map management functions
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
#include "init.h"
#include "monster.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "obj-ignore.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "trap.h"

/**
 * This function takes a grid location and extracts information the
 * player is allowed to know about it, filling in the grid_data structure
 * passed in 'g'.
 *
 * The information filled in is as follows:
 *  - g->f_idx is filled in with the terrain's feature type, or FEAT_NONE
 *    if the player doesn't know anything about the grid.  The function
 *    makes use of the "mimic" field in terrain in order to allow one
 *    feature to look like another (hiding secret doors, invisible traps,
 *    etc).  This will return the terrain type the player "Knows" about,
 *    not necessarily the real terrain.
 *  - g->m_idx is set to the monster index, or 0 if there is none (or the
 *    player doesn't know it).
 *  - g->first_kind is set to the object_kind of the first object in a grid
 *    that the player knows about, or NULL for no objects.
 *  - g->first_art is set to the artifact pointer of the first object in a grid
 *    that the player knows about, or NULL if there are no objects or the first
 *    known object is not an artifact.
 *  - g->muliple_objects is true if there is more than one object in the
 *    grid that the player knows and cares about (to facilitate any special
 *    floor stack symbol that might be used).
 *  - g->in_view is true if the player can currently see the grid - this can
 *    be used to indicate field-of-view, such as through the 
 *    OPT(player, view_bright_light) option.
 *  - g->lighting is set to indicate the lighting level for the grid:
 *    LIGHTING_LIT by default, LIGHTING_DARK for unlit but seen grids within the
 *    detection radius of a player with the UNLIGHT ability and a light source
 *    with an intensity of one or less, LIGHTING_TORCH for seen and lit grids
 *    within the radius of the player's light source when the view_yellow_light
 *    option is on, and LIGHTING_LOS for seen and lit grids that don't qualify
 *    for LIGHTING_TORCH.
 *  - g->is_player is true if the player is on the given grid.
 *  - g->hallucinate is true if the player is hallucinating something "strange"
 *    for this grid - this should pick a random monster to show if the m_idx
 *    is non-zero, and a random object if first_kind is non-zero.
 * 
 * NOTES:
 * This is called pretty frequently, whenever a grid on the map display
 * needs updating, so don't overcomplicate it.
 *
 * Terrain is remembered separately from objects and monsters, so can be
 * shown even when the player can't "see" it.  This leads to things like
 * doors out of the player's view still change from closed to open and so on.
 *
 * TODO:
 * Hallucination is a display-level hack (mostly in ui-map.c's
 * grid_data_as_text(); some here) and we need it to be a knowledge-level
 * hack.  The idea is that objects may turn into different objects, monsters
 * into different monsters, and terrain may be objects, monsters, or stay the
 * same.
 */
void map_info(struct loc grid, struct grid_data *g)
{
	struct object *obj;

	assert(grid.x < cave->width);
	assert(grid.y < cave->height);

	/* Default "clear" values, others will be set later where appropriate. */
	g->first_kind = NULL;
	g->first_art = NULL;
	g->trap = NULL;
	g->multiple_objects = false;
	g->glow = false;
	g->lighting = LIGHTING_LIT;

	/* Use real feature (remove later) */
	g->f_idx = square(cave, grid)->feat;
	if (f_info[g->f_idx].mimic)
		g->f_idx = (uint32_t) (f_info[g->f_idx].mimic - f_info);

	g->in_view = (square_isseen(cave, grid)) ? true : false;
	g->is_player = (square(cave, grid)->mon < 0) ? true : false;
	g->m_idx = (g->is_player) ? 0 : square(cave, grid)->mon;
	g->hallucinate = player->timed[TMD_IMAGE] ? true : false;
	g->rage = player->timed[TMD_RAGE] ? true : false;

	if (square_isglow(cave, grid)) {
		g->lighting = LIGHTING_LIT;
	}
	if (g->in_view) {
		bool lit = square_islit(cave, grid);

		if (lit) {
			g->lighting = LIGHTING_LOS;
		}

		/* Remember seen feature */
		square_memorize(cave, grid);
	} else if (g->rage) {
		/* Rage shows nothing out of view */
		g->f_idx = FEAT_NONE;
		g->m_idx = 0;
		return;
	} else if (!square_isknown(cave, grid)) {
		g->f_idx = FEAT_NONE;
	}

	/* Use known feature */
	g->f_idx = square(player->cave, grid)->feat;
	if (f_info[g->f_idx].mimic)
		g->f_idx = (uint32_t) (f_info[g->f_idx].mimic - f_info);

	/* There is a known trap in this square */
	if (square_trap(player->cave, grid) && square_isknown(cave, grid)) {
		struct trap *trap = square(player->cave, grid)->trap;

		/* Scan the square trap list */
		while (trap) {
			if (trf_has(trap->flags, TRF_TRAP) ||
				trf_has(trap->flags, TRF_GLYPH)) {
				/* Accept the trap */
				g->trap = trap;
				break;
			}
			trap = trap->next;
		}
    }

	/* Objects */
	for (obj = square_object(player->cave, grid); obj; obj = obj->next) {
		if (ignore_known_item_ok(player, obj)) {
			/* Item stays hidden */
		} else if (!g->first_kind) {
			/*
			 * For glowing, need to test the base object, not just
			 * what the player knows.
			 */
			struct object *base_obj = cave->objects[obj->oidx];

			g->first_kind = obj->kind;
			g->first_art = obj->artifact;
			assert(base_obj);
			g->glow = loc_eq(obj->grid, base_obj->grid)
				&& weapon_glows(base_obj, 0);
		} else {
			g->multiple_objects = true;
			break;
		}
	}

	/* Monsters */
	if (g->m_idx > 0) {
		/* If the monster isn't "visible", make sure we don't list it.*/
		struct monster *mon = cave_monster(cave, g->m_idx);
		if (!monster_is_visible(mon) && !monster_is_listened(mon)) g->m_idx = 0;
	}

	/* Rare random hallucination on non-outer walls */
	if (g->hallucinate && g->m_idx == 0 && g->first_kind == 0) {
		if (one_in_(128) && (int) g->f_idx != FEAT_PERM)
			g->m_idx = z_info->r_max + 1;
		else if (one_in_(128) && (int) g->f_idx != FEAT_PERM)
			/* if hallucinating, we just need first_kind to not be NULL */
			g->first_kind = k_info;
		else
			g->hallucinate = false;
	}

	assert((int) g->f_idx < FEAT_MAX);
	if (!g->hallucinate)
		assert((int)g->m_idx < cave->mon_max);
	/* All other g fields are 'flags', mostly booleans. */
}


/**
 * Memorize interesting viewable object/features in the given grid
 *
 * This function should only be called on "legal" grids.
 *
 * This function will memorize the object and/or feature in the given grid,
 * if they are (1) see-able and (2) interesting.  Note that all objects are
 * interesting, all terrain features except floors (and invisible traps) are
 * interesting, and floors (and invisible traps) are interesting sometimes
 * (depending on various options involving the illumination of floor grids).
 *
 * The automatic memorization of all objects and non-floor terrain features
 * as soon as they are displayed allows incredible amounts of optimization
 * in various places, especially "map_info()" and this function itself.
 *
 * Note that the memorization of objects is completely separate from the
 * memorization of terrain features, preventing annoying floor memorization
 * when a detected object is picked up from a dark floor, and object
 * memorization when an object is dropped into a floor grid which is
 * memorized but out-of-sight.
 *
 * This function should be called every time the "memorization" of a grid
 * (or the object in a grid) is called into question, such as when an object
 * is created in a grid, when a terrain feature "changes" from "floor" to
 * "non-floor", and when any grid becomes "see-able" for any reason.
 *
 * This function is called primarily from the "update_view()" function, for
 * each grid which becomes newly "see-able".
 */
void square_note_spot(struct chunk *c, struct loc grid)
{
	/* Require "seen" flag and the current level */
	if (c != cave) return;
	if (!square_isseen(c, grid) && !square_isplayer(c, grid)) return;

	/* Make the player know precisely what is on this grid */
	square_know_pile(c, grid);

	/* Notice traps, memorize those we can see */
	if (square_issecrettrap(c, grid)) {
		square_reveal_trap(c, grid, true);
	}
	square_memorize_traps(c, grid);

	if (!square_ismemorybad(c, grid))
		return;

	/* Memorize this grid */
	square_memorize(c, grid);
}



/**
 * Tell the UI that a given map location has been updated
 *
 * This function should only be called on "legal" grids.
 */
void square_light_spot(struct chunk *c, struct loc grid)
{
	if ((c == cave) && player->cave) {
		player->upkeep->redraw |= PR_ITEMLIST;
		event_signal_point(EVENT_MAP, grid.x, grid.y);
	}
}


/**
 * This routine will "darken" all grids in the set passed in.
 *
 * In addition, some of these grids will be "unmarked".
 *
 * This routine is used (only) by "light_room()"
 */
static void cave_unlight(struct point_set *ps)
{
	int i;

	/* Apply flag changes */
	for (i = 0; i < ps->n; i++)	{
		struct loc grid = ps->pts[i];

		/* Darken the grid... */
		sqinfo_off(square(cave, ps->pts[i])->info, SQUARE_GLOW);

		/* Hack -- Forget "boring" grids */
		if (square_isfloor(cave, grid))
			square_unmark(cave, grid);
	}

	/* Process the grids */
	for (i = 0; i < ps->n; i++)	{
		/* Redraw the grid */
		square_light_spot(cave, ps->pts[i]);
	}
}

/*
 * Aux function -- see below
 */
static void cave_room_aux(struct point_set *seen, struct loc grid)
{
	if (point_set_contains(seen, grid))
		return;

	if (!square_in_bounds(cave, grid))
		return;

	if (!square_isroom(cave, grid))
		return;

	/* Add it to the "seen" set */
	add_to_point_set(seen, grid);
}

/**
 * Illuminate or darken any room containing the given location.
 */
void light_room(struct loc grid, bool light)
{
	int i, d;
	struct point_set *ps;

	ps = point_set_new(200);

	/* Add the initial grid */
	cave_room_aux(ps, grid);

	/* While grids are in the queue, add their neighbors */
	for (i = 0; i < ps->n; i++) {
		/* Walls get lit, but stop light */
		if (!square_isprojectable(cave, ps->pts[i])) continue;

		/* Spread to the adjacent grids */
		for (d = 0; d < 8; d++) {
			cave_room_aux(ps, loc_sum(ps->pts[i], ddgrid_ddd[d]));
		}
	}

	/* Now, lighten or darken them all at once */
	if (!light) {
		cave_unlight(ps);
	}
	point_set_dispose(ps);

	/* Fully update the visuals */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Update stuff */
	update_stuff(player);
}



/**
 * Light up the dungeon using "claravoyance"
 *
 * This function "illuminates" every grid in the dungeon, memorizes all
 * "objects" (or notes the existence of an object "if" full is true),
 * and memorizes all grids as with magic mapping.
 */
void wiz_light(struct chunk *c, struct player *p)
{
	int i, y, x;

	/* Scan all grids */
	for (y = 1; y < c->height - 1; y++) {
		for (x = 1; x < c->width - 1; x++) {
			struct loc grid = loc(x, y);

			/* Process all non-walls */
			if (!square_seemslikewall(c, grid)) {
				if (!square_in_bounds_fully(c, grid)) continue;

				/* Scan all neighbors */
				for (i = 0; i < 9; i++) {
					struct loc a_grid = loc_sum(grid, ddgrid_ddd[i]);

					/* Perma-light the grid */
					sqinfo_on(square(c, a_grid)->info, SQUARE_GLOW);

					/* Memorize normal features */
					if (!square_isfloor(c, a_grid) || 
						square_isvisibletrap(c, a_grid)) {
						square_memorize(c, a_grid);
						square_mark(c, a_grid);
					}
				}
			}

			/* Memorize objects */
			square_know_pile(c, grid);

			/* Forget unprocessed, unknown grids in the mapping area */
			if (!square_ismark(c, grid) && square_ismemorybad(c, grid))
				square_forget(c, grid);
		}
	}

	/* Unmark grids */
	for (y = 1; y < c->height - 1; y++) {
		for (x = 1; x < c->width - 1; x++) {
			struct loc grid = loc(x, y);
			if (!square_in_bounds(c, grid)) continue;
			square_unmark(c, grid);
		}
	}

	/* Fully update the visuals */
	p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw whole map, monster list */
	p->upkeep->redraw |= (PR_MAP | PR_MONLIST | PR_ITEMLIST);
}


/**
 * Compeletly darken the level, forgetting everything
 */
void wiz_dark(struct chunk *c, struct player *p)
{
	int y, x;

	/* Scan all grids */
	assert(c == cave);
	for (y = 1; y < c->height - 1; y++) {
		for (x = 1; x < c->width - 1; x++) {
			struct loc grid = loc(x, y);
			struct object *obj = square_object(p->cave, grid);

			/* Forget all grids */
			square_forget(c, grid);

			/*
			 * Mark all grids as unseen so view calculations start
			 * from scratch.
			 */
			sqinfo_off(square(c, grid)->info, SQUARE_SEEN);

			/* Forget all objects */
			while (obj) {
				struct object *base = cave->objects[obj->oidx];
				struct object *next = obj->next;
				assert(base && base->known == obj);
				square_excise_object(p->cave, grid, obj);
				delist_object(p->cave, obj);
				object_delete(p->cave, NULL, &obj);
				base->known = NULL;
				obj = next;
			}

			/* Forget unmoving mindless monsters - TODO */
		}
	}
	/* Fully update the visuals */
	p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw whole map, monster list */
	p->upkeep->redraw |= (PR_MAP | PR_MONLIST | PR_ITEMLIST);
}
