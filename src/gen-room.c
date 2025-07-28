/**
 * \file gen-room.c
 * \brief Dungeon room generation.
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2013 Erik Osheim, Nick McConnell
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
 *
 * This file covers everything to do with generation of individual rooms in
 * the dungeon.  It consists of room generating helper functions plus the 
 * actual room builders (which are referred to in the room profiles in
 * generate.c).
 *
 * The room builders all take as arguments the chunk they are being generated
 * in, and the co-ordinates of the room centre in that chunk.  Each room
 * builder is also able to find space for itself in the chunk using the 
 * find_space() function; the chunk generating functions can ask it to do that
 * by passing too large centre co-ordinates.
 */

#include "angband.h"
#include "cave.h"
#include "datafile.h"
#include "game-event.h"
#include "generate.h"
#include "init.h"
#include "mon-group.h"
#include "mon-make.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "trap.h"
#include "z-queue.h"
#include "z-type.h"

/**
 * ------------------------------------------------------------------------
 * Selection of random templates
 * ------------------------------------------------------------------------ */
/**
 * Chooses a vault of a particular kind at random.
 * \param depth the current depth, for vault bound checking
 * \param typ vault type
 * \param forge whether we are forcing a forge
 * \return a pointer to the vault template
 */
struct vault *random_vault(int depth, const char *typ, bool forge)
{
	struct vault *v = vaults;
	struct vault *r = NULL;
	uint32_t rarity_sum = 0;
	do {
		if (streq(v->typ, typ) && (v->depth <= depth)) {
			bool valid = v->rarity > 0;
			/* Check if we need a forge and don't have one */
			if (forge && !v->forge) {
				valid = false;
			}

			/* Check if it's a greater vault we've already seen */
			if (streq(v->typ, "Greater vault") && player->vaults[v->index]) {
				valid = false;
			}

			if (valid) {
				rarity_sum += v->rarity;
				if (Rand_div(rarity_sum) < v->rarity) r = v;
			}
		}
		v = v->next;
	} while(v);
	return r;
}



/**
 * ------------------------------------------------------------------------
 * Room build helper functions
 * ------------------------------------------------------------------------ */
/**
 * Test a rectangle to see if it is all rock (i.e. not floor and not vault)
 * \param c the current chunk
 * \param y1 inclusive room boundaries
 * \param x1 inclusive room boundaries
 * \param y2 inclusive room boundaries
 * \param x2 inclusive room boundaries
 */
static bool solid_rock(struct chunk *c, int y1, int x1, int y2, int x2)
{
	int y, x;

	for (y = y1; y <= y2; y++) {
		for (x = x1; x <= x2; x++) {
			if (square_isfloor(c, loc(x, y)) || square_isvault(c, loc(x, y))) {
				return false;
			}
		}
	}
	return true;
}

/**
 * Test around a rectangle to see if there would be a doubled wall
 *
 * eg:
 *       ######
 * #######....#
 * #....##....#
 * #....#######
 * ######
 * \param c the current chunk
 * \param y1 inclusive room boundaries
 * \param x1 inclusive room boundaries
 * \param y2 inclusive room boundaries
 * \param x2 inclusive room boundaries
 */
static bool doubled_wall(struct chunk *c, int y1, int x1, int y2, int x2)
{
	int y, x;

	/* Check top wall */
	for (x = x1; x < x2; x++) {
		if (square_iswall_outer(c, loc(x, y1 - 2)) &&
			square_iswall_outer(c, loc(x + 1, y1 - 2)))
			return true;
	}

	/* Check bottom wall */
	for (x = x1; x < x2; x++) {
		if (square_iswall_outer(c, loc(x, y2 + 2)) &&
			square_iswall_outer(c, loc(x + 1, y2 + 2)))
			return true;
	}

	/* Check left wall */
	for (y = y1; y < y2; y++) {
		if (square_iswall_outer(c, loc(x1 - 2, y)) &&
			square_iswall_outer(c, loc(x1 - 2, y + 1)))
			return true;
	}

	/* Check right wall */
	for (y = y1; y < y2; y++) {
		if (square_iswall_outer(c, loc(x2 + 2, y)) &&
			square_iswall_outer(c, loc(x2 + 2, y + 1)))
			return true;
	}

	return false;
}

/**
 * Mark squares as being in a room, and optionally light them.
 * \param c the current chunk
 * \param y1 inclusive room boundaries
 * \param x1 inclusive room boundaries
 * \param y2 inclusive room boundaries
 * \param x2 inclusive room boundaries
 * \param light whether or not to light the room
 */
static void generate_room(struct chunk *c, int y1, int x1, int y2, int x2,
						  int light)
{
	struct loc grid;
	for (grid.y = y1; grid.y <= y2; grid.y++)
		for (grid.x = x1; grid.x <= x2; grid.x++) {
			sqinfo_on(square(c, grid)->info, SQUARE_ROOM);
			if (light)
				sqinfo_on(square(c, grid)->info, SQUARE_GLOW);
		}
}

/**
 * Mark a rectangle with a sqinfo flag
 * \param c the current chunk
 * \param y1 inclusive room boundaries
 * \param x1 inclusive room boundaries
 * \param y2 inclusive room boundaries
 * \param x2 inclusive room boundaries
 * \param flag the SQUARE_* flag we are marking with
 */
void generate_mark(struct chunk *c, int y1, int x1, int y2, int x2, int flag)
{
	struct loc grid;
	for (grid.y = y1; grid.y <= y2; grid.y++) {
		for (grid.x = x1; grid.x <= x2; grid.x++) {
			sqinfo_on(square(c, grid)->info, flag);
		}
	}
}

/**
 * Fill a rectangle with a feature.
 * \param c the current chunk
 * \param y1 inclusive room boundaries
 * \param x1 inclusive room boundaries
 * \param y2 inclusive room boundaries
 * \param x2 inclusive room boundaries
 * \param feat the terrain feature
 * \param flag the SQUARE_* flag we are marking with
 */
void fill_rectangle(struct chunk *c, int y1, int x1, int y2, int x2, int feat,
					int flag)
{
	int y, x;
	for (y = y1; y <= y2; y++)
		for (x = x1; x <= x2; x++)
			square_set_feat(c, loc(x, y), feat);
	if (flag) generate_mark(c, y1, x1, y2, x2, flag);
}

/**
 * Fill the edges of a rectangle with a feature.
 * \param c the current chunk
 * \param y1 inclusive room boundaries
 * \param x1 inclusive room boundaries
 * \param y2 inclusive room boundaries
 * \param x2 inclusive room boundaries
 * \param feat the terrain feature
 * \param flag the SQUARE_* flag we are marking with
 * \param overwrite_perm whether to overwrite features already marked as
 * permanent
 */
void draw_rectangle(struct chunk *c, int y1, int x1, int y2, int x2, int feat,
					int flag, bool overwrite_perm)
{
	int y, x;

	for (y = y1; y <= y2; y++) {
		if (overwrite_perm || !square_isperm(c, loc(x1, y))) {
			square_set_feat(c, loc(x1, y), feat);
		}
		if (overwrite_perm || !square_isperm(c, loc(x2, y))) {
			square_set_feat(c, loc(x2, y), feat);
		}
	}
	if (flag) {
		generate_mark(c, y1, x1, y2, x1, flag);
		generate_mark(c, y1, x2, y2, x2, flag);
	}
	for (x = x1; x <= x2; x++) {
		if (overwrite_perm || !square_isperm(c, loc(x, y1))) {
			square_set_feat(c, loc(x, y1), feat);
		}
		if (overwrite_perm || !square_isperm(c, loc(x, y2))) {
			square_set_feat(c, loc(x, y2), feat);
		}
	}
	if (flag) {
		generate_mark(c, y1, x1, y1, x2, flag);
		generate_mark(c, y2, x1, y2, x2, flag);
	}
}

/**
 * Fill the lines of a cross/plus with a feature.
 *
 * \param c the current chunk
 * \param y1 inclusive room boundaries
 * \param x1 inclusive room boundaries
 * \param y2 inclusive room boundaries
 * \param x2 inclusive room boundaries
 * \param feat the terrain feature
 * \param flag the SQUARE_* flag we are marking with
 * When combined with draw_rectangle() this will generate a large rectangular 
 * room which is split into four sub-rooms.
 */
static void generate_plus(struct chunk *c, int y1, int x1, int y2, int x2, 
						  int feat, int flag)
{
	int y, x;

	/* Find the center */
	int y0 = (y1 + y2) / 2;
	int x0 = (x1 + x2) / 2;

	assert(c);

	for (y = y1; y <= y2; y++) square_set_feat(c, loc(x0, y), feat);
	if (flag) generate_mark(c, y1, x0, y2, x0, flag);
	for (x = x1; x <= x2; x++) square_set_feat(c, loc(x, y0), feat);
	if (flag) generate_mark(c, y0, x1, y0, x2, flag);
}

/**
 * Place a square of granite with a flag
 * \param c the current chunk
 * \param grid the square co-ordinates
 * \param flag the SQUARE_* flag we are marking with
 */
void set_marked_granite(struct chunk *c, struct loc grid, int flag)
{
	square_set_feat(c, grid, FEAT_GRANITE);
	if (flag) generate_mark(c, grid.y, grid.x, grid.y, grid.x, flag);
}

/**
 * Build a vault from its string representation.
 * \param c the chunk the room is being built in
 * \param centre the room centre; out of chunk centre invokes find_space()
 * \param v pointer to the vault template
 * \param flip whether or not to diagonally flip (interchange x and y) the
 * vault template
 * \return success
 */
bool build_vault(struct chunk *c, struct loc centre, struct vault *v, bool flip)
{
	const char *data = v->text;
	int x, y;
	const char *t;
	bool flip_v = false;
	bool flip_h = false;

	assert(c);

	/* Check that the vault doesn't contain invalid things for its depth */
	for (t = data, y = 0; y < v->hgt; y++) {	
		for (x = 0; x < v->wid; x++, t++) {
			/* Barrow wights can't be deeper than level 12 */
			if ((*t == 'W') && (c->depth > 12)) {
				return false;
			}

			/* Chasms can't occur at 950 ft */
			if ((*t == '7') && (c->depth >= z_info->dun_depth - 1)) {
				return false;
			}
		}
	}

	/* Reflections */
	if ((c->depth > 0) && (c->depth < z_info->dun_depth)) {
		/* Reflect it vertically half the time */
		if (one_in_(2)) flip_v = true;
		/* Reflect it horizontally half the time */
		if (one_in_(2)) flip_h = true;
	}

	/* Place dungeon features and objects */
	for (t = data, y = 0; y < v->hgt && *t; y++) {
		int ay = flip_v ? v->hgt - 1 - y : y;
		for (x = 0; x < v->wid && *t; x++, t++) {
			int ax = flip_h ? v->wid - 1 - x : x;
			struct loc grid;

			/* Extract the location, flipping diagonally if requested */
			if (flip) {
				grid.x = centre.x - (v->hgt / 2) + ay;
				grid.y = centre.y - (v->wid / 2) + ax;
			} else {
				grid.x = centre.x - (v->wid / 2) + ax;
				grid.y = centre.y - (v->hgt / 2) + ay;
			}

			/* Skip non-grids */
			if (*t == ' ') continue;

			/* Lay down a floor */
			square_set_feat(c, grid, FEAT_FLOOR);

			/* Debugging assertion */
			assert(square_isempty(c, grid));

			/* Part of a vault */
			sqinfo_on(square(c, grid)->info, SQUARE_ROOM);
			sqinfo_on(square(c, grid)->info, SQUARE_VAULT);

			/* Analyze the grid */
			switch (*t) {
				/* Outer outside granite wall */
			case '$': set_marked_granite(c, grid, SQUARE_WALL_OUTER); break;
				/* Inner or non-tunnelable outside granite wall */
			case '#': set_marked_granite(c, grid, SQUARE_WALL_INNER); break;
				/* Quartz vein */
			case '%': square_set_feat(c, grid, FEAT_QUARTZ); break;
				/* Rubble */
			case ':': square_set_feat(c, grid, FEAT_RUBBLE); break;
				/* Glyph of warding */
			case ';': square_add_glyph(c, grid, GLYPH_WARDING); break;
				/* Stairs */
			case '<': square_set_feat(c, grid, FEAT_LESS); break;
			case '>': square_set_feat(c, grid, FEAT_MORE); break;
				/* Visible door */
			case '+': place_closed_door(c, grid); break;
				/* Secret door */
			case 's': place_secret_door(c, grid); break;
				/* Trap */
			case '^': if (one_in_(2)) square_add_trap(c, grid); break;
				/* Forge */
			case '0': place_forge(c, grid); break;
				/* Chasm */
			case '7': square_set_feat(c, grid, FEAT_CHASM); break;

			}
		}
	}


	/* Place regular dungeon monsters and objects */
	for (t = data, y = 0; y < v->hgt && *t; y++) {
		int ay = flip_v ? v->hgt - 1 - y : y;
		for (x = 0; x < v->wid && *t; x++, t++) {
			int ax = flip_h ? v->wid - 1 - x : x;
			struct loc grid;
			struct monster_group_info info = { 0, 0 };

			/* Extract the location, flipping diagonally if requested */
			if (flip) {
				grid.x = centre.x - (v->hgt / 2) + ay;
				grid.y = centre.y - (v->wid / 2) + ax;
			} else {
				grid.x = centre.x - (v->wid / 2) + ax;
				grid.y = centre.y - (v->hgt / 2) + ay;
			}

			/* Hack -- skip "non-grids" */
			if (*t == ' ') continue;

			/* Analyze the symbol */
			switch (*t)
			{
				/* A monster from 1 level deeper */
				case '1': {
					pick_and_place_monster(c, grid, c->depth + 1, true, true,
											   ORIGIN_DROP_VAULT);
					break;
				}

				/* A monster from 2 levels deeper */
				case '2': {
					pick_and_place_monster(c, grid, c->depth + 2, true, true,
											   ORIGIN_DROP_VAULT);
					break;
				}

				/* A monster from 3 levels deeper */
				case '3': {
					pick_and_place_monster(c, grid, c->depth + 3, true, true,
											   ORIGIN_DROP_VAULT);
					break;
				}

				/* A monster from 4 levels deeper */
				case '4': {
					pick_and_place_monster(c, grid, c->depth + 4, true, true,
											   ORIGIN_DROP_VAULT);
					break;
				}

				/* An object from 1-4 levels deeper */
				case '*': {
					place_object(c, grid, c->depth + randint1(4), false, false,
								 ORIGIN_VAULT, lookup_drop("not useless"));
					break;
				}

				/* A good object from 1-4 levels deeper */
				case '&': {
					place_object(c, grid, c->depth + randint1(4), true, false,
								 ORIGIN_VAULT, lookup_drop("not useless"));
					break;
				}

				/* A chest from 4 levels deeper */
				case '~': {
					int depth = c->depth ? c->depth + 4 : z_info->dun_depth;
					place_object(c, grid, depth, false, false,
								 ORIGIN_VAULT, lookup_drop("chest"));
					break;
				}

				/* A skeleton */
				case 'S': {
					/* Make a skeleton 1/2 of the time */
					if (one_in_(2)) {
						struct object *obj = object_new();
						int sval;
						struct object_kind *kind;

						if (one_in_(3)) {
							sval = lookup_sval(TV_USELESS, "Human Skeleton");
						} else {
							sval = lookup_sval(TV_USELESS, "Elf Skeleton");
						}
						kind = lookup_kind(TV_USELESS, sval);

						/* Prepare the item */
						object_prep(obj, kind, c->depth, RANDOMISE);

						/* Drop it in the dungeon */
						drop_near(c, &obj, 0, grid, false, false);
					}
					break;
				}

				/* Monster and/or object from 1 level deeper */
				case '?': {
					int r = randint1(3);
					
					if (r <= 2) {
						pick_and_place_monster(c, grid, c->depth + 1, true,
											   true, ORIGIN_DROP_VAULT);
					}
					if (r >= 2) {
						place_object(c, grid, c->depth + 1, false, false,
									 ORIGIN_VAULT, NULL);
					}
					break;
				}


				/* Carcharoth */
				case 'C': {
					place_new_monster_one(c, grid, lookup_monster("Carcharoth"),
										  true, true, info,
										  ORIGIN_DROP_VAULT);
					break;
				}
				
				/* silent watcher */
				case 'H': {
					place_new_monster_one(c, grid,
										  lookup_monster("Silent watcher"),
										  true, false, info,
										  ORIGIN_DROP_VAULT);
					break;
				}

				/* easterling spy */
				case '@': {
					place_new_monster_one(c, grid,
										  lookup_monster("Easterling spy"),
										  true, false, info,
										  ORIGIN_DROP_VAULT);
					break;
				}
					
				/* orc champion */
				case 'o': {
					place_new_monster_one(c, grid,
										  lookup_monster("Orc champion"), true,
										  false, info, ORIGIN_DROP_VAULT);
					break;
				}

				/* orc captain */
				case 'O': {
					place_new_monster_one(c, grid,
										  lookup_monster("Orc captain"), true,
										  false, info, ORIGIN_DROP_VAULT);
					break;
				}

				/* cat warrior */
				case 'f': {
					place_new_monster_one(c, grid,
										  lookup_monster("Cat warrior"), true,
										  false, info, ORIGIN_DROP_VAULT);
					break;
				}

				/* cat assassin */
				case 'F': {
					place_new_monster_one(c, grid,
										  lookup_monster("Cat assassin"), true,
										  false, info, ORIGIN_DROP_VAULT);
					break;
				}
					
				/* troll guard */
				case 'T': {
					place_new_monster_one(c, grid,
										  lookup_monster("Troll guard"), true,
										  false, info, ORIGIN_DROP_VAULT);
					break;
				}

				/* barrow wight */
				case 'W': {
					place_new_monster_one(c, grid,
										  lookup_monster("Barrow wight"), true,
										  false, info, ORIGIN_DROP_VAULT);
					break;
				}
				
				/* dragon */
				case 'd': {
					place_monster_by_flag(c, grid, RF_DRAGON, -1, true,
										  c->depth + 4, false);
					break;
				}

				/* young cold drake */
				case 'y': {
					place_new_monster_one(c, grid,
										  lookup_monster("Young cold-drake"),
										  true, false, info,
										  ORIGIN_DROP_VAULT);
					break;
				}
					
				/* young fire drake */
				case 'Y': {
					place_new_monster_one(c, grid,
									  lookup_monster("Young fire-drake"),
									  true, false, info, ORIGIN_DROP_VAULT);
					break;
				}
					
				/* Spider */
				case 'M': {
					place_monster_by_flag(c, grid, RF_SPIDER, -1, true,
										  c->depth + rand_range(1, 4), false);
					break;
				}
				
				/* Vampire */
				case 'v': {
					place_monster_by_letter(c, grid, 'v', true,
											c->depth + rand_range(1, 4));
					break;
				}

                /* Archer */
				case 'a': {
					place_monster_by_flag(c, grid, RSF_ARROW1, RSF_ARROW2, true,
										  c->depth + 1, true);
					break;
				}

                /* Flier */
				case 'b': {
					place_monster_by_flag(c, grid, RF_FLYING, -1, true,
										  c->depth + 1, false);
					break;
				}

				/* Wolf */
				case 'c': {
					place_monster_by_flag(c, grid, RF_WOLF, -1, true,
										  c->depth + rand_range(1, 4), false);
					break;
				}
					
				/* Rauko */
				case 'r': {
					place_monster_by_flag(c, grid, RF_RAUKO, -1, true,
										  c->depth + rand_range(1, 4), false);
					break;
				}
					
                /* Aldor */
				case 'A': {
					place_new_monster_one(c, grid, lookup_monster("Aldor"),
										  true, true, info,
										  ORIGIN_DROP_VAULT);
					break;
				}
                    
				/* Glaurung */
				case 'D': {
					place_new_monster_one(c, grid, lookup_monster("Glaurung"),
										  true, true, info,
										  ORIGIN_DROP_VAULT);
					break;
				}

				/* Gothmog */
				case 'R': {
					place_new_monster_one(c, grid, lookup_monster("Gothmog"),
										  true, true, info,
										  ORIGIN_DROP_VAULT);
					break;
				}
					
				/* Ungoliant */
				case 'U': {
					place_new_monster_one(c, grid, lookup_monster("Ungoliant"),
										  true, true, info,
										  ORIGIN_DROP_VAULT);
					break;
				}

				/* Gorthaur */
				case 'G': {
					place_new_monster_one(c, grid, lookup_monster("Gorthaur"),
										  true, true, info,
										  ORIGIN_DROP_VAULT);
					break;
				}
					
				/* Morgoth */
				case 'V': {
					place_new_monster_one(c, grid, lookup_monster("Morgoth, Lord of Darkness"),
										  true, true, info,
										  ORIGIN_DROP_VAULT);
					break;
				}
			}

		}
	}

	for (t = data, y = 0; y < v->hgt && *t; y++) {
		int ay = flip_v ? v->hgt - 1 - y : y;
		for (x = 0; x < v->wid && *t; x++, t++) {
			int ax = flip_h ? v->wid - 1 - x : x;
			struct loc grid;
			int mult;

			/* Extract the location, flipping diagonally if requested */
			if (flip) {
				grid.x = centre.x - (v->hgt / 2) + ay;
				grid.y = centre.y - (v->wid / 2) + ax;
			} else {
				grid.x = centre.x - (v->wid / 2) + ax;
				grid.y = centre.y - (v->hgt / 2) + ay;
			}

			/* Hack -- skip "non-grids" */
			if (*t == ' ') continue;

			/* Some vaults are always lit */
			if (roomf_has(v->flags, ROOMF_LIGHT)) {
				sqinfo_on(square(c, grid)->info, SQUARE_GLOW);
			}

			/* Traps are usually 5 times as likely in vaults,
			 * but are 10 times as likely if the TRAPS flag is set */
			mult = roomf_has(v->flags, ROOMF_TRAPS) ? 10 : 5;

			/* Another chance to place traps, with 4 times the normal chance
			 * so traps in interesting rooms and vaults are a total of 5 times
			 * more likely */
			if (randint1(1000) <= trap_placement_chance(c, grid) * (mult - 1)) {
				square_add_trap(c, grid);
			} else if (roomf_has(v->flags, ROOMF_WEBS) && one_in_(20)) {
				/* Webbed vaults also have a large chance of receiving webs */
				square_add_web(c, grid);

				/* Hide it half the time */
				if (one_in_(2)) {
					struct trap *trap = square_trap(c, grid);
					trf_on(trap->flags, TRF_INVISIBLE);
				}
			}
		}
	}

	return true;
}

/**
 * Helper function for building vaults.
 * \param c the chunk the room is being built in
 * \param centre the room centre; out of chunk centre invokes find_space()
 * \param typ the vault type
 * \param forge whether we are forcing a forge
 * \return success
 */
static bool build_vault_type(struct chunk *c, struct loc centre,
							 const char *typ, bool forge)
{
	bool flip_d;
	int y1, x1, y2, x2;
	struct vault *v = random_vault(c->depth, typ, forge);
	if (v == NULL) {
		return false;
	}

	/* Choose whether to rotate (flip diagonally) if allowed */
	flip_d = one_in_(3) && !roomf_has(v->flags, ROOMF_NO_ROTATION);

	if (flip_d) {
		/* Determine the coordinates with height/width flipped */
		y1 = centre.y - (v->wid / 2);
		x1 = centre.x - (v->hgt / 2);
		y2 = y1 + v->wid - 1;
		x2 = x1 + v->hgt - 1;
	} else {
		/* Determine the coordinates */
		y1 = centre.y - (v->hgt / 2);
		x1 = centre.x - (v->wid / 2);
		y2 = y1 + v->hgt - 1;
		x2 = x1 + v->wid - 1;
	}

	/* Make sure that the location is within the map bounds */
	if ((y1 <= 3) || (x1 <= 3) || (y2 >= c->height - 3) ||(x2 >= c->width - 3)){
		return false;
	}

	/* Make sure that the location is empty */
	if (!solid_rock(c, y1 - 2, x1 - 2, y2 + 2, x2 + 2)) {
		return false;
	}

	/* Build the vault */
	if (!build_vault(c, centre, v, flip_d)) {
		return false;
	}

	/* Save the corner locations */
	dun->corner[dun->cent_n].top_left = loc(x1 + 1, y1 + 1);
	dun->corner[dun->cent_n].bottom_right = loc(x2 - 1, y2 - 1);

	/* Save the room location */
	dun->cent[dun->cent_n] = centre;
	dun->cent_n++;

	ROOM_LOG("%s (%s)", typ, v->name);

	/* Memorise and mark greater vaults */
	if (streq(typ, "Greater vault")) {
		player->vaults[v->index] = true;
		generate_mark(c, y1, x1, y2, x2, SQUARE_G_VAULT);
		assert(!c->vault_name);
		c->vault_name = string_make(v->name);
	}

	return true;
}

/**
 * ------------------------------------------------------------------------
 * Room builders
 * ------------------------------------------------------------------------ */
/**
 * Builds a normal rectangular room.
 * \param c the chunk the room is being built in
 * \param centre the room centre
 * \return success
 */
bool build_simple(struct chunk *c, struct loc centre)
{
	int y, x, y1, x1, y2, x2;
	bool light = false;

	/* Occasional light - chance of darkness starts very small and
	 * increases quadratically until always dark at 950 ft */
	if ((c->depth < randint1(z_info->dun_depth - 1)) ||
		(c->depth < randint1(z_info->dun_depth - 1))) {
		light = true;
	}

	/* Pick a room size */
	y1 = centre.y - randint1(3);
	x1 = centre.x - randint1(5);
	y2 = centre.y + randint1(3);
	x2 = centre.x + randint1(4) + 1;

	/* Sil: bounds checking */
	if (y1 <= 3 || (x1 <= 3) || (y2 >= c->height - 3) || (x2 >= c->width - 3)) {
		return false;
	}

	/* Check to see if the location is all plain rock */
	if (!solid_rock(c, y1 - 1, x1 - 1, y2 + 1, x2 + 1)) {
		return false;
	}

	if (doubled_wall(c, y1, x1, y2, x2)) {
		return false;
	}

	/* Save the corner locations */
	dun->corner[dun->cent_n].top_left = loc(x1, y1);
	dun->corner[dun->cent_n].bottom_right = loc(x2, y2);

	/* Save the room location */
	dun->cent[dun->cent_n] = centre;
	dun->cent_n++;

	/* Generate new room */
	generate_room(c, y1 - 1, x1 - 1, y2 + 1, x2 + 1, light);

	/* Generate outer walls and inner floors */
	draw_rectangle(c, y1 - 1, x1 - 1, y2 + 1, x2 + 1, FEAT_GRANITE,
				   SQUARE_WALL_OUTER, false);
	fill_rectangle(c, y1, x1, y2, x2, FEAT_FLOOR, SQUARE_NONE);

	/* Sometimes make a pillar room */
	if (one_in_(20) && ((x2 - x1) % 2 == 0) && ((y2 - y1) % 2 == 0)) {
		event_signal_string(EVENT_GEN_ROOM_CHOOSE_SUBTYPE, "pillared");

		for (y = y1 + 1; y <= y2; y += 2) {
			for (x = x1 + 1; x <= x2; x += 2) {
				set_marked_granite(c, loc(x, y), SQUARE_WALL_INNER);
			}
		}
	} else if (one_in_(10) && ((x2 - x1) % 2 == 0) && ((y2 - y1) % 2 == 0)) {
		/* Sometimes make a pillar-lined room */
		event_signal_string(EVENT_GEN_ROOM_CHOOSE_SUBTYPE, "ragged");

		for (y = y1 + 1; y <= y2; y += 2) {
			for (x = x1 + 1; x <= x2; x += 2) {
				if ((x == x1 + 1) || (x == x2 - 1) || (y == y1 + 1) ||
					(y == y2 - 1)) {
					set_marked_granite(c, loc(x, y), SQUARE_WALL_INNER);
				}
			}
		}
	}

	return true;
}


/**
 * Builds a cross-shaped room.
 * \param c the chunk the room is being built in
 * \param centre the room centre
 * \return success
 *
 * Room "v" runs north/south, and Room "h" runs east/west 
 */
bool build_crossed(struct chunk *c, struct loc centre)
{
	int y, x;

	int y1h, x1h, y2h, x2h;
	int y1v, x1v, y2v, x2v;

	int h_hgt, h_wid, v_hgt, v_wid;

	int light = false;

	/* Occasional light - always at level 1 down to never at Morgoth's level */
	if (c->depth <= randint1(z_info->dun_depth - 1)) light = true;

	/* Pick a room size */
	h_hgt = 1;                /* 3 */
	h_wid = rand_range(5, 7); /* 11, 13, 15 */

	y1h = centre.y - h_hgt;
	x1h = centre.x - h_wid;
	y2h = centre.y + h_hgt;
	x2h = centre.x + h_wid;

	v_hgt = rand_range(3, 6); /* 7, 9, 11, 13 */
	v_wid = rand_range(1, 2); /* 3, 5 */

	y1v = centre.y - v_hgt;
	x1v = centre.x - v_wid;
	y2v = centre.y + v_hgt;
	x2v = centre.x + v_wid;

	/* Sil: bounds checking */
	if ((y1v <= 3) || (x1h <= 3) || (y2v >= c->height - 3) ||
		(x2h >= c->width - 3)) {
		return false;
	}

	/* Check to see if the location is all plain rock */
	if (!solid_rock(c, y1v - 1, x1h - 1, y2v + 1, x2h + 1)) {
		return false;
	}

	if (doubled_wall(c, y1v, x1h, y2v, x2h)) {
		return false;
	}

	/* Save the corner locations */
	dun->corner[dun->cent_n].top_left = loc(x1h, y1v);
	dun->corner[dun->cent_n].bottom_right = loc(x2h, y2v);


	/* Save the room location */
	dun->cent[dun->cent_n] = centre;
	dun->cent_n++;

	/* Generate new rooms */
	generate_room(c, y1h - 1, x1h - 1, y2h + 1, x2h + 1, light);
	generate_room(c, y1v - 1, x1v - 1, y2v + 1, x2v + 1, light);

	/* Generate outer walls */
	draw_rectangle(c, y1h - 1, x1h - 1, y2h + 1, x2h + 1, 
				   FEAT_GRANITE, SQUARE_WALL_OUTER, false);
	draw_rectangle(c, y1v - 1, x1v - 1, y2v + 1, x2v + 1,
				   FEAT_GRANITE, SQUARE_WALL_OUTER, false);

	/* Generate inner floors */
	fill_rectangle(c, y1h, x1h, y2h, x2h, FEAT_FLOOR, SQUARE_NONE);
	fill_rectangle(c, y1v, x1v, y2v, x2v, FEAT_FLOOR, SQUARE_NONE);

	/* Special features */
	switch (randint1(7)) {
		case 1: {
			event_signal_string(EVENT_GEN_ROOM_CHOOSE_SUBTYPE, "chest");
			if ((v_wid == 2) && (v_hgt == 6)) {
				for (y = y1v + 1; y <= y2v; y += 2) {
					for (x = x1v + 1; x <= x2v; x += 2) {
						set_marked_granite(c, loc(x, y), SQUARE_WALL_INNER);
					}
				}
				place_object(c, centre, c->depth, false, false, ORIGIN_SPECIAL,
							 lookup_drop("chest"));
			}
			break;
		}

		case 2: {
			if ((v_wid == 1) && (h_hgt == 1)) {
				event_signal_string(EVENT_GEN_ROOM_CHOOSE_SUBTYPE, "plus");
				generate_plus(c, centre.y - 1, centre.x - 1, centre.y + 1,
							  centre.x + 1, FEAT_GRANITE, SQUARE_WALL_INNER);
			}
			break;
		}

		case 3: {
			if ((v_wid == 1) && (h_hgt == 1)) {
				event_signal_string(EVENT_GEN_ROOM_CHOOSE_SUBTYPE, "pinched");

				set_marked_granite(c, loc(centre.x - 1, centre.y - 1),
								   SQUARE_WALL_INNER);
				set_marked_granite(c, loc(centre.x - 1, centre.y + 1),
								   SQUARE_WALL_INNER);
				set_marked_granite(c, loc(centre.x + 1, centre.y - 1),
								   SQUARE_WALL_INNER);
				set_marked_granite(c, loc(centre.x + 1, centre.y + 1),
								   SQUARE_WALL_INNER);
			}
			break;
		}

		case 4: {
			if ((v_wid == 1) && (h_hgt == 1)) {
				event_signal_string(EVENT_GEN_ROOM_CHOOSE_SUBTYPE, "hollow plus");

				set_marked_granite(c, loc(centre.x - 1, centre.y),
								   SQUARE_WALL_INNER);
				set_marked_granite(c, loc(centre.x + 1, centre.y),
								   SQUARE_WALL_INNER);
				set_marked_granite(c, loc(centre.x, centre.y - 1),
								   SQUARE_WALL_INNER);
				set_marked_granite(c, loc(centre.x, centre.y + 1),
								   SQUARE_WALL_INNER);
			}
			break;
		}
		default: {
			break;
		}
	}

	return true;
}


/**
 * Build an interesting room.
 * \param c the chunk the room is being built in
 * \param centre the room centre
 * \return success
 */
bool build_interesting(struct chunk *c, struct loc centre)
{
	return build_vault_type(c, centre, "Interesting room",
							player->upkeep->force_forge);
}


/**
 * Build a lesser vault.
 * \param c the chunk the room is being built in
 * \param centre the room centre
 * \return success
 */
bool build_lesser_vault(struct chunk *c, struct loc centre)
{
	return build_vault_type(c, centre, "Lesser vault", false);
}


/**
 * Build a greater vault.
 * \param c the chunk the room is being built in
 * \param centre the room centre
 * \return success
 */

bool build_greater_vault(struct chunk *c, struct loc centre)
{
	/* Can only have one greater vault per level */
	if (c->vault_name) {
		return false;
	}
	return build_vault_type(c, centre, "Greater vault", false);
}


/**
 * Build Morgoth's throne room.
 * \param c the chunk the room is being built in
 * \param centre the room centre
 * \return success
 */
bool build_throne(struct chunk *c, struct loc centre)
{
	int y1, x1, y2, x2;
	struct vault *v = random_vault(c->depth, "Throne room", false);
	if (v == NULL) {
		return false;
	}

	/* Determine the coordinates */
	centre = loc(c->width / 2, c->height / 2);
	y1 = centre.y - (v->hgt / 2);
	x1 = centre.x - (v->wid / 2);
	y2 = y1 + v->hgt - 1;
	x2 = x1 + v->wid - 1;

	/* Build the vault */
	if (!build_vault(c, centre, v, false)) {
		return false;
	}

	/* Memorise and mark */
	generate_mark(c, y1, x1, y2, x2, SQUARE_G_VAULT);
	assert(!c->vault_name);
	c->vault_name = string_make(v->name);

	return true;
}

/**
 * Build the Gates of Angband.
 * \param c the chunk the room is being built in
 * \param centre the room centre
 * \return success
 */
bool build_gates(struct chunk *c, struct loc centre)
{
	int y1, x1, y2, x2;
	struct vault *v = random_vault(c->depth, "Gates of Angband", false);
	if (v == NULL) {
		return false;
	}

	/* Determine the coordinates */
	centre = loc(c->width / 2, c->height / 2);
	y1 = centre.y - (v->hgt / 2);
	x1 = centre.x - (v->wid / 2);
	y2 = y1 + v->hgt - 1;
	x2 = x1 + v->wid - 1;

	/* Build the vault */
	if (!build_vault(c, centre, v, false)) {
		return false;
	}

	/* Memorise and mark */
	generate_mark(c, y1, x1, y2, x2, SQUARE_G_VAULT);
	assert(!c->vault_name);
	c->vault_name = string_make(v->name);

	return true;
}


/**
 * Attempt to build a room of the given type at the given block
 *
 * \param c the chunk the room is being built in
 * \param profile the profile of the rooom we're trying to build
 * \return success
 */
bool room_build(struct chunk *c, struct room_profile profile)
{
	struct loc centre = loc(rand_range(5, c->width - 5),
							rand_range(5, c->height - 5));

	if (dun->cent_n >= z_info->level_room_max) {
		return false;
	}

	event_signal_string(EVENT_GEN_ROOM_START, profile.name);

	/* Try to build a room */
	while (!profile.builder(c, centre)) {
		/* Keep trying if we're forcing a forge, but reset the centre
		 * This is a bit dangerous, and may need more modification - NRM */
		centre = loc(rand_range(5, c->width - 5), rand_range(5, c->height - 5));
		if (!player->upkeep->force_forge) {
			event_signal_flag(EVENT_GEN_ROOM_END, false);
			return false;
		}
	}

	/* Success */
	event_signal_flag(EVENT_GEN_ROOM_END, true);
	return true;
}
