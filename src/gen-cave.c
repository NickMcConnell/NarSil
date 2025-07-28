/**
 * \file gen-cave.c
 * \brief Generation of dungeon levels
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
 * In this file, we use the SQUARE_WALL flags to the info field in
 * cave->squares.  Those are applied and tested on granite.  SQUARE_WALL_SOLID
 * fills the areas between rooms and can be carved out by tunneling.
 * SQUARE_WALL_INNER is used in rooms, either for exterior walls that can not
 * be carved out by tunneling or for interior walls.  SQUARE_WALL_OUTER is
 * used in rooms for exterior walls that can be carved out by tunneling.
 *
 * Note that a tunnel which attempts to leave a room near the edge of the
 * dungeon in a direction toward that edge will cause "silly" wall piercings,
 * but will have no permanently incorrect effects, as long as the tunnel can
 * eventually exit from another side. And note that the wall may not come back
 * into the room by the hole it left through, so it must bend to the left or
 * right and then optionally re-enter the room (at least 2 grids away). This is
 * not a problem since every room that is large enough to block the passage of
 * tunnels is also large enough to allow the tunnel to pierce the room itself
 * several times.
 *
 * Note that no two corridors may enter a room through adjacent grids, they
 * must either share an entryway or else use entryways at least two grids
 * apart. This prevents large (or "silly") doorways.
 *
 * Traditionally, to create rooms in the dungeon, it was divided up into
 * "blocks" of 11x11 grids each, and all rooms were required to occupy a
 * rectangular group of blocks.  As long as each room type reserved a
 * sufficient number of blocks, the room building routines would not need to
 * check bounds. Note that in classic generation most of the normal rooms
 * actually only use 23x11 grids, and so reserve 33x11 grids.
 *
 * Note that a lot of the original motivation for the block system was the
 * fact that there was only one size of map available, 22x66 grids, and the
 * dungeon level was divided up into nine of these in three rows of three.
 * Now that the map can be resized and enlarged, and dungeon levels themselves
 * can be different sizes, much of this original motivation has gone.  Blocks
 * can still be used, but different cave profiles can set their own block
 * sizes.  The classic generation method still uses the traditional blocks; the
 * main motivation for using blocks now is for the aesthetic effect of placing
 * rooms on a grid.
 */

#include "angband.h"
#include "cave.h"
#include "datafile.h"
#include "game-event.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-group.h"
#include "mon-make.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "player-util.h"
#include "trap.h"
#include "z-queue.h"
#include "z-type.h"

/* ------------------ UTILITIES ---------------- */
//static char dumpname[30];
//strnfmt(dumpname, sizeof(dumpname), "%s", whatevs);
//dump_level_simple(dumpname, "Test Level", c);

/**
 * Determines whether the player can pass through a given feature
 * icky locations (inside vaults) are all considered passable.
 */
static bool player_pass(struct chunk *c, struct loc grid, bool ignore_rubble)
{
	bool vault_interior = square_isvault(c, grid) &&
		square_isvault(c, loc(grid.x - 1, grid.y)) &&
		square_isvault(c, loc(grid.x + 1, grid.y)) &&
		square_isvault(c, loc(grid.x, grid.y - 1)) &&
		square_isvault(c, loc(grid.x, grid.y + 1));
	return square_ispassable(c, grid) || square_issecretdoor(c, grid) ||
		square_iscloseddoor(c, grid) ||
		(square_isrubble(c, grid) && ignore_rubble) || vault_interior;
}

/**
 * Floodfills access through the dungeon, marking all accessible squares true
 */
static void flood_access(struct chunk *c, struct loc grid, bool **access,
						 bool ignore_rubble)
{
	int i;

	/* First check the map bounds */
	if (!square_in_bounds(c, grid)) return;
	
	access[grid.y][grid.x] = true;
	for (i = 0; i < 8; i++) {
		struct loc check = loc_sum(grid, ddgrid_ddd[i]);
		if (player_pass(c, check, ignore_rubble) && !access[check.y][check.x]) {
			flood_access(c, check, access, ignore_rubble);
		}
	}
	return;
}

/**
 * Make sure that the level is sufficiently connected.
 *
 * Currently a failure here results in a new level being generated, which is OK
 * as long as it's not happening too often.  Ideally connect_rooms_stairs()
 * should be rewritten so it doesn't happen at all - NRM.
 */
static bool check_connectivity(struct chunk *c)
{
	struct loc grid;
	bool result = false;

	/* Set the array used for checking connectivity */
	bool **access = mem_zalloc(c->height * sizeof(bool*));
	for (grid.y = 0; grid.y < c->height; grid.y++) {
		access[grid.y] = mem_zalloc(c->width  * sizeof(bool));
	}

	/* Make sure entire dungeon is connected (ignoring rubble) */
	flood_access(c, player->grid, access, true);
	for (grid.y = 0; grid.y < c->height; grid.y++) {
		for (grid.x = 0; grid.x < c->width; grid.x++) {
			if (player_pass(c, grid, true) && !access[grid.y][grid.x]) {
				goto CLEANUP;
			}
		}
	}

	/* Reset the array used for checking connectivity */
	for (grid.y = 0; grid.y < c->height; grid.y++) {
		for (grid.x = 0; grid.x < c->width; grid.x++) {
			access[grid.y][grid.x] = false;
		}
	}
	
	/* Make sure player can reach stairs without going through rubble */
	flood_access(c, player->grid, access, false);
	for (grid.y = 0; grid.y < c->height; grid.y++) {
		for (grid.x = 0; grid.x < c->width; grid.x++) {
			if (access[grid.y][grid.x] && square_isstairs(c, grid)) {
				result = true;
				goto CLEANUP;
			}
		}
	}

CLEANUP:
	for (grid.y = 0; grid.y < c->height; grid.y++) {
		mem_free(access[grid.y]);
	}
	mem_free(access);

	return result;
}

/**
 * Floodfills access through the *graph* of the dungeon
 */
static void flood_piece(int n, int piece_num)
{
	int i;

	dun->piece[n] = piece_num;

	for (i = 0; i < dun->cent_n; i++) {
		if (dun->connection[n][i] && (dun->piece[i] == 0)) {
			flood_piece(i, piece_num);
		}
	}
	return;
}

static int dungeon_pieces(void)
{
	int piece_num;
	int i;

	/* First reset the pieces */
	for (i = 0; i < dun->cent_n; i++) {
		dun->piece[i] = 0;
	}

	for (piece_num = 1; piece_num <= dun->cent_n; piece_num++) {
		/* Find the next room that doesn't belong to a piece */
		for (i = 0; (i < dun->cent_n) && (dun->piece[i] != 0); i++);

		if (i == dun->cent_n) {
			break;
		} else {
			flood_piece(i, piece_num);
		}
	}

	return (piece_num - 1);
}

/**
 * Places a streamer of rock through dungeon.
 *
 * \param c is the current chunk
 * \param feat is the base feature (FEAT_MAGMA or FEAT_QUARTZ)
 *
 * Note that their are actually six different terrain features used to
 * represent streamers. Three each of magma and quartz, one for basic vein, one
 * with hidden gold, and one with known gold. The hidden gold types are
 * currently unused.
 */
static bool build_streamer(struct chunk *c, int feat)
{
	/* Hack -- Choose starting point */
	struct loc grid = rand_loc(loc(c->width / 2, c->height / 2), 15, 10);

	/* Choose a random direction */
	int dir = ddd[randint0(8)];

	int tries1 = 0;

	/* Place streamer into dungeon */
	while (true) {
		int i;
		struct loc change;

		tries1++;
		if (tries1 > 2500) return false;

		/* One grid per density */
		for (i = 0; i < dun->profile->str.den; i++) {
			int d = dun->profile->str.rng;

			/* Pick a nearby grid */
			find_nearby_grid(c, &change, grid, d, d);

			/* Only convert walls */
			if (square_isgranitewall(c, change)) {
				/* Turn the rock into the vein type */
				square_set_feat(c, change, feat);
			}
		}

		/* Advance the streamer */
		grid = loc_sum(grid, ddgrid[dir]);

		/* Stop at dungeon edge */
		if (!square_in_bounds(c, grid)) break;
	}

	return true;
}

/**
 * Places a single chasm.
 *
 * \param c is the current chunk
 */
static void build_chasm(struct chunk *c)
{
	bool chasm_ok = false;
	struct loc grid1;

	/* Try to mark squares to be a chasm */
	while (!chasm_ok) {
		int i;

        /* Choose starting point */
        struct loc grid = loc(rand_range(10, c->width - 10),
							  rand_range(10, c->height - 10));

		/* Choose a random cardinal direction for it to run in */
		int main_dir = ddd[randint0(4)];

		/* Choose a random length for it */
        int length = damroll(4, 8);

		/* Count floor squares that will be turned to chasm */
        int floor_to_chasm = 0;

		/* Determine its shape */
        for (i = 0; i < length; i++) {
            /* Go in a random direction half the time */
            if (one_in_(2)) {
                /* Choose the random cardinal direction */
                grid = loc_sum(grid, ddgrid_ddd[randint0(4)]);
            } else {
				/* Go straight ahead the other half */
				grid = loc_sum(grid, ddgrid[main_dir]);
			}

			/* Stop near dungeon edge */
            if ((grid.y < 3) || (grid.y > c->height - 3) ||
				(grid.x < 3) || (grid.x > c->width - 3))
				break;

            /* Mark that we want to put a chasm here */
            sqinfo_on(square(c, grid)->info, SQUARE_CHASM);
        }

		/* Start by assuming it will be OK */
        chasm_ok = true;

		/* Check it doesn't wreck the dungeon */
        for (grid.y = 1; grid.y < c->height - 1; grid.y++) {
            for (grid.x = 1; grid.x < c->width - 1; grid.x++) {
				/* Adjacent grids in the cardinal directions */
				struct loc gride = loc_sum(grid, loc(1, 0));
				struct loc grids = loc_sum(grid, loc(0, 1));
				struct loc gridw = loc_sum(grid, loc(-1, 0));
				struct loc gridn = loc_sum(grid, loc(0, -1));

                /* Only inspect squares currently destined to be chasms */
                if (square_tobechasm(c, grid)) {
                    /* Avoid chasms in interesting rooms / vaults */
                    if (square_isvault(c, grid)) {
                        chasm_ok = false;
                    }

                    /* Avoid two chasm square in a row in corridors */
                    if (square_tobechasm(c, grids) && !square_isroom(c, grid) &&
						!square_isroom(c, grids) && square_isfloor(c, grid) &&
						square_isfloor(c, grids)) {
                        chasm_ok = false;
                    }
                    if (square_tobechasm(c, gride) && !square_isroom(c, grid) &&
						!square_isroom(c, gride) && square_isfloor(c, grid) &&
						square_isfloor(c, gride)) {
                        chasm_ok = false;
                    }
                    
                    /* Avoid a chasm taking out the rock next to a door */
                    if (square_iscloseddoor(c, gride) ||
						square_iscloseddoor(c, grids) ||
						square_iscloseddoor(c, gridw) ||
						square_iscloseddoor(c, gridn)) {
                        chasm_ok = false;
                    }
                    
                    /* Avoid a chasm just hitting the wall of a lit room (would
					 * look odd that the light doesn't hit the wall behind) */
                    if (square_isrock(c, grid) && square_isglow(c, grid)) {
                        if ((square_isrock(c, gride) && !square_isglow(c, gride)
							 && !square_tobechasm(c, gride)) ||
							(square_isrock(c, grids) && !square_isglow(c, grids)
							 && !square_tobechasm(c, grids)) ||
							(square_isrock(c, gridw) && !square_isglow(c, gridw)
							 && !square_tobechasm(c, gridw)) ||
							(square_isrock(c, gridn) && !square_isglow(c, gridn)
							 && !square_tobechasm(c, gridn))) {
                            chasm_ok = false;
                        }
                    }
                        
                    /* Avoid a chasm having no squares in a room/corridor */
                    if (square_ispassable(c, grid)) {
                        floor_to_chasm++;
                    }
                }
            }
        }

		/* The chasm must affect at least one floor square */
        if (floor_to_chasm < 1) {
			chasm_ok = false;
		}

		/* Clear the flag for failed chasm placement */
        if (!chasm_ok) {
			for (grid.y = 0; grid.y < c->height; grid.y++) {
				for (grid.x = 0; grid.x < c->width; grid.x++) {
					sqinfo_off(square(c, grid)->info, SQUARE_CHASM);
                }
            }
        }
	}

	/* Actually place the chasm and clear the flag */
	for (grid1.y = 0; grid1.y < c->height; grid1.y++) {
		for (grid1.x = 0; grid1.x < c->width; grid1.x++) {
			if (sqinfo_has(square(c, grid1)->info, SQUARE_CHASM)) {
				square_set_feat(c, grid1, FEAT_CHASM);
				sqinfo_off(square(c, grid1)->info, SQUARE_CHASM);
			}
		}
	}
}

/**
 * Places chasms through dungeon
 */
static void build_chasms(struct chunk *c)
{
    int i;
    int chasms = 0;
    int blocks = (c->height / z_info->block_hgt) * (c->width / z_info->block_wid);

    /* Determine whether to add chasms, and how many */
    if ((c->depth > 2) && (c->depth < z_info->dun_depth - 1) &&
		percent_chance(c->depth + 30)) {
        /* Add some chasms */
        chasms += damroll(1, blocks / 3);

        /* Flip a coin, and if it is heads... */
        while (one_in_(2)) {
            /* Add some more chasms and flip again... */
            chasms += damroll(1, blocks / 3);
        }
    }

    /* Build them */
    for (i = 0; i < chasms; i++) {
        build_chasm(c);
    }

    if (OPT(player, cheat_room) && (chasms > 0)) {
		msg("%d chasms.", chasms);
	}
}

/**
 * Check the validity of a horizontal or vertical tunnel
 */
static bool tunnel_ok(struct chunk *c, struct loc grid1, struct loc grid2,
						bool tentative, int desired_changes)
{
	int x, y;
	struct loc grid_lo, grid_hi;
	bool vert = true;
	int changes = 0;

	/* Get the direction */
	if (grid1.y == grid2.y) {
		/* Horizontal */
		vert = false;
		if (grid1.x < grid2.x) {
			grid_lo = grid1;
			grid_hi = grid2;
		} else {
			grid_lo = grid2;
			grid_hi = grid1;
		}
	} else {
		/* Vertical */
		if (grid1.y < grid2.y) {
			grid_lo = grid1;
			grid_hi = grid2;
		} else {
			grid_lo = grid2;
			grid_hi = grid1;
		}
	}

	/* Don't dig corridors ending at a room's outer wall (can happen at corners
	 * of L-corridors) */
	if (square_iswall_outer(c, grid_lo) || square_iswall_outer(c, grid_hi)) {
		return false;
	}

	/* Don't dig L-corridors when the corner is too close to empty space */
	if (!square_isroom(c, grid_lo)) {
		struct loc check1 = loc_sum(grid_lo, loc(-1, -1));
		struct loc check2 = vert ? loc_sum(grid_lo, loc(1, -1)) :
			loc_sum(grid_lo, loc(-1, 1));
		if (square_isfloor(c, check1) || square_isfloor(c, check2)) {
			return false;
		}
	}
	if (!square_isroom(c, grid_hi)) {
		struct loc check1 = vert ? loc_sum(grid_hi, loc(-1, 1)) :
			loc_sum(grid_hi, loc(1, -1));
		struct loc check2 = loc_sum(grid_hi, loc(1, 1));
		if (square_isfloor(c, check1) || square_isfloor(c, check2)) {
			return false;
		}
	}

	/* Test each location in the corridor */
	for (x = grid_lo.x, y = grid_lo.y; x <= grid_hi.x && y <= grid_hi.y;
		 vert ? y++ : x++) {
		/* Grid we're testing */
		struct loc grid = loc(x, y);
		/* Grid above or left */
		struct loc perp0 = vert ? loc(x - 1, y) : loc(x, y - 1);
		/* Grid below or right */
		struct loc perp1 = vert ? loc(x + 1, y) : loc(x, y + 1);
		/* Previous grid along the tunnel */
		struct loc prev = vert ? loc(x, y - 1) : loc(x - 1, y);

		/* Abort if the tunnel would go through or adjacent to an existing door
		 * (except in vaults) */
		if (square_iscloseddoor(c, perp0) && !square_isvault(c, perp0)) {
			return false;
		}
		if (square_iscloseddoor(c, grid) &&	!square_isvault(c, grid)) {
			return false;
		}
		if (square_iscloseddoor(c, perp1) && !square_isvault(c, perp1)) {
			return false;
		}
		
		/* Abort if the tunnel would have floor beside it at some point
		 * outside a room */
		if ((square_isfloor(c, perp0) || square_isfloor(c, perp1)) &&
			!square_isroom(c, grid)) {
			return false;
		}

		/* The remaining checks compare successive grids along the tunnel,
		 * so we skip the first tunnel grid */
		if ((x == grid_lo.x) && (y == grid_lo.y)) continue;

		/*
		 * Count the number of times it enters or leaves a room.
		 * This matches Sil 1.3's logic but won't count cases
		 * where the interior grid is quartz or rubble (the former
		 * does not seem to happen in Sil 1.3's vaults; that latter
		 * can happen for Sil 1.3's Collapsed Cross, Collapsed Keep,
		 * Cave in, and perhaps others).  Note that a tunnel that
		 * grazes a room's boundary could also contribute to the
		 * count (for instance, a horizontal tunnel hitting a vault
		 * with a horizontal boundary of "$7$").
		 */
		if (square_iswall_outer(c, grid) &&
			(square_ispassable(c, prev) || square_iswall_inner(c, prev))) {
			/* To outside from inside */
			changes++;
		}
		if (square_iswall_outer(c, prev) &&
			(square_ispassable(c, grid) || square_iswall_inner(c, grid))) {
			/* From outside to inside */
			changes++;
		}
		
		/*
		 * Abort if the tunnel would go through two adjacent squares
		 * of the outside wall of a room.  This matches Sil 1.3's
		 * logic, but Sil 1.3's vaults have grids on the boundaries
		 * that are not SQUARE_WALL_OUTER:  SQUARE_WALL_INNER,
		 * quartz, rubble, and chasms, for instance.  Most problematic
		 * tunnels with those vaults will be screened out by this
		 * test, the test for going through an adjacent
		 * SQUARE_WALL_OUTER and SQUARE_WALL_INNER, the test for
		 * directly accessing the internals of a room without passing
		 * through a SQUARE_WALL_OUTER, or the constraint on the
		 * number of inside/outside crossings.  However, the Glittering
		 * Caves vaults in Sil 1.3 (where there are possible tunnels
		 * that access the internals and only pass through '%') may
		 * be a problem.  It is possible to construct vaults (say
		 * with a horizontal boundary that looks like "$7$", "$:$:$",
		 * or "$%%$%%$") which would allow grazing tunnels that
		 * wouldn't be rejected by the tests here or in Sil 1.3.
		 */
		if (square_iswall_outer(c, grid) && square_iswall_outer(c, prev)) {
			return false;
		}

		/* Abort if the tunnel would go between a door and an outside wall */
		if (square_iswall_outer(c, prev) &&	square_iscloseddoor(c, grid)) {
			return false;
		}
		if (square_iswall_outer(c, grid) &&	square_iscloseddoor(c, prev)) {
			return false;
		}

		/* Abort if the tunnel would go between an outside and an inside wall */
		if (square_iswall_outer(c, prev) &&	square_iswall_inner(c, grid)) {
			return false;
		}
		if (square_iswall_outer(c, grid) && square_iswall_inner(c, prev)) {
			return false;
		}

		/*
		 * Abort if the tunnel would directly enter a vault without
		 * going through a designated square.  This matches Sil 1.3
		 * but does not prevent a tunnel from entering a vault through
		 * quartz or rubble on the boundary.  Converting the test on
		 * second grid to (!square_iswall_outer() && square_is_room))
		 * would do that.
		 */
		if (square_iswall_solid(c, prev) && 
			(square_ispassable(c, grid) || square_iswall_inner(c, grid))) {
			return false;
		}
		if (square_iswall_solid(c, grid) && 
			(square_ispassable(c, prev) || square_iswall_inner(c, prev))) {
			return false;
		}
	}

	/* Reject if we were checking changes and failed, otherwise accept */
	if (tentative && (changes != desired_changes)) {
		return false;
	} else {
		return true;
	}
}

/**
 * Lay the tunnel grids of a straight tunnel between two rooms
 *
 * \return the number of walls converted to floors or doors.
 */
static int lay_tunnel(struct chunk *c, struct loc grid1, struct loc grid2,
					   int r1, int r2)
{
        int ncnvt = 0;
	int x, y;
	struct loc grid_lo, grid_hi;
	bool vert = true;

	/* Get the direction */
	if (grid1.y == grid2.y) {
		/* Horizontal */
		vert = false;
		if (grid1.x < grid2.x) {
			grid_lo = grid1;
			grid_hi = grid2;
		} else {
			grid_lo = grid2;
			grid_hi = grid1;
		}
	} else {
		/* Vertical */
		if (grid1.y < grid2.y) {
			grid_lo = grid1;
			grid_hi = grid2;
		} else {
			grid_lo = grid2;
			grid_hi = grid1;
		}
	}

	/* Set floors and doors */
	for (x = grid_lo.x, y = grid_lo.y; x <= grid_hi.x && y <= grid_hi.y;
		 vert ? y++ : x++) {
		struct loc grid = loc(x, y);
		if (square_iswall_outer(c, grid)) {
			/* All doors get randomised later */
			square_set_feat(c, grid, FEAT_CLOSED);	
			++ncnvt;
		} else if (square_iswall_solid(c, grid)) {
			square_set_feat(c, grid, FEAT_FLOOR);
			dun->tunn1[y][x] = r1;
			dun->tunn2[y][x] = r2;
			++ncnvt;
		}
	}

	return ncnvt;
}

/**
 * Build a tunnel between two grids in nominated rooms
 *
 * Build horizontally or vertically if possible, otherwise build an L-shaped
 * tunnel, randomly horizontal or vertical first
 */
static bool build_tunnel(struct chunk *c, int r1, int r2, struct loc grid1,
		struct loc grid2, bool tentative, enum tunnel_type t)
{
	struct loc grid_mid;
	tunnel_direction_type dir;
	int nver, nhor;

	/* Horizontal or vertical */
	if ((grid1.y == grid2.y) || (grid1.x == grid2.x)) {
		/*
		 * Check validity (room to corridor tunnels have already been
		 * checked), tunnel and we're done
		 */
		if (t == TUNNEL_ROOM_TO_CORRIDOR
				|| tunnel_ok(c, grid1, grid2, tentative, 2)) {
			nver = lay_tunnel(c, grid1, grid2, r1, r2);
			nhor = 0;
			if (grid1.y == grid2.y) {
				int tmp = nver;

				nver = nhor;
				nhor = tmp;
				dir = TUNNEL_HOR;
			} else {
				dir = TUNNEL_VER;
			}
			event_signal_tunnel(EVENT_GEN_TUNNEL_FINISHED, t, dir,
				nver, nhor);
			return true;
		} else {
			return false;
		}
	} else if (one_in_(2)) {
		/* Horizontal, then vertical */
		grid_mid = loc(grid2.x, grid1.y);
		dir = TUNNEL_BENT;
	} else {
		/* Vertical, then horizontal */
		grid_mid = loc(grid1.x, grid2.y);
		dir = TUNNEL_BENT;
	}

	/* Check validity */
	if (!tunnel_ok(c, grid1, grid_mid, tentative, 1)) return false;
	if (!tunnel_ok(c, grid_mid, grid2, tentative, 1)) return false;

	/* Lay tunnel */
	nver = lay_tunnel(c, grid1, grid_mid, r1, r2);
	nhor = lay_tunnel(c, grid_mid, grid2, r1, r2);
	if (grid_mid.y == grid1.y) {
		int tmp = nver;

		nver = nhor;
		nhor = tmp;
	}
	event_signal_tunnel(EVENT_GEN_TUNNEL_FINISHED, t, dir, nver, nhor);

	return true;
}

static bool connect_two_rooms(struct chunk *c, int r1, int r2, bool tentative,
							  bool desperate)
{
	struct loc cent1 = dun->cent[r1];
	struct loc cent2 = dun->cent[r2];
	struct loc top_left1 = dun->corner[r1].top_left;
	struct loc top_left2 = dun->corner[r2].top_left;
	struct loc bottom_right1 = dun->corner[r1].bottom_right;
	struct loc bottom_right2 = dun->corner[r2].bottom_right;
	bool success;
	
	int distance_limitx = desperate ? 22 : 15;
	int distance_limity = desperate ? 16 : 10;
	
	/* If the rooms are too far apart, then just give up immediately */

	/* Look at total distance of room centres */
	if ((ABS(cent1.y - cent2.y) > distance_limity * 3) ||
		(ABS(cent1.x - cent2.x) > distance_limitx * 3)) {
		return false;
	}
	/* Then look at distance of relevant room edges */
	if ((cent1.x < cent2.x) &&
		(top_left2.x - bottom_right1.x > distance_limitx)) {
		return false;
	}
	if ((cent2.x < cent1.x) &&
		(top_left1.x - bottom_right2.x > distance_limitx)) {
		return false;
	}
	if ((cent1.y < cent2.y) &&
		(top_left2.y - bottom_right1.y > distance_limity)) {
		return false;
	}
	if ((cent2.y < cent1.y) &&
		(top_left1.y - bottom_right2.y > distance_limity)) {
		return false;
	}
			
	/* If we have vertical or horizontal overlap, connect a straight tunnel
	 * at a random point where they overlap */
	if ((top_left1.x <= bottom_right2.x) && (top_left2.x <= bottom_right1.x)) {
		/* If horizontal overlap */
		int x = rand_range(MAX(top_left1.x, top_left2.x),
						   MIN(bottom_right1.x, bottom_right2.x));
		struct loc grid1 = loc(x, cent1.y);
		struct loc grid2 = loc(x, cent2.y);

		/* Unless careful, there will be too many vertical tunnels
		 * since rooms are wider than they are tall */
		if (tentative && one_in_(2)) {
			return false;
		}

		success = build_tunnel(c, r1, r2, grid1, grid2, tentative,
			(desperate) ? TUNNEL_DESPERATE : TUNNEL_ROOM_TO_ROOM);
	} else if ((top_left1.y <= bottom_right2.y) &&
			   (top_left2.y <= bottom_right1.y)) {
		/* If vertical overlap */
		int y = rand_range(MAX(top_left1.y, top_left2.y),
						   MIN(bottom_right1.y, bottom_right2.y));
		struct loc grid1 = loc(cent1.x, y);
		struct loc grid2 = loc(cent2.x, y);

		success = build_tunnel(c, r1, r2, grid1, grid2, tentative,
			(desperate) ? TUNNEL_DESPERATE : TUNNEL_ROOM_TO_ROOM);
	} else {
		/* Otherwise, make an L shaped corridor between their centres;
		 * this must fail if any of the tunnels would be too long */
		if (MIN(ABS(cent2.x - top_left1.x), ABS(cent2.x - bottom_right1.x))
			> distance_limitx - 2) {
			return false;
		}
		if (MIN(ABS(cent1.x - top_left2.x), ABS(cent1.x - bottom_right2.x))
			> distance_limitx - 2) {
			return false;
		}
		if (MIN(ABS(cent2.y - top_left1.y), ABS(cent2.y - bottom_right1.y))
			> distance_limity - 2) {
			return false;
		}
		if (MIN(ABS(cent1.y - top_left2.y), ABS(cent1.y - bottom_right2.y))
			> distance_limity - 2) {
			return false;
		}

		success = build_tunnel(c, r1, r2, cent1, cent2, tentative,
			(desperate) ? TUNNEL_DESPERATE : TUNNEL_ROOM_TO_ROOM);
	}
	
	if (success) {
		dun->connection[r1][r2] = true;
		dun->connection[r2][r1] = true;	
	}
	
	return success;
}

static bool connect_room_to_corridor(struct chunk *c, int r)
{
	int length = 10;
	struct loc grid, cent = dun->cent[r];
	int r1, r2;
	bool success = false;
	bool done = false;

	/* Go down/right half the time, up/left the other half */
	int delta = one_in_(2) ? 1 : -1;
	/* Go horizontal half the time, vertical the other half */
	bool vert = one_in_(2);
		
	/* Start at the centre and look for a tunnel */
	grid = cent;
	while (!done) {
		if (vert) {
			grid.y += delta;
		} else {
			grid.x += delta;
		}

		/* Abort if the tunnel leaves the map or passes through a door */
		if (!square_in_bounds(c, grid) || (ABS(grid.x - cent.x) > length)
			|| (ABS(grid.y - cent.y) > length)
			|| square_iscloseddoor(c, grid)) {
			success = false;
			done = true;
		} else if (square_isfloor(c, grid) && !square_isroom(c, grid)) {
			/* It has intercepted a tunnel! */
			r1 = dun->tunn1[grid.y][grid.x];
			r2 = dun->tunn2[grid.y][grid.x];

			/* Make sure that the tunnel intercepts only connects rooms
			 * that aren't connected to this room */
			if ((r1 < 0) || (r2 < 0) ||
				(!(dun->connection[r][r1]) && !(dun->connection[r][r2]))) {
				struct loc grid1 = vert ? loc(grid.x, cent.y)
					: loc(cent.x, grid.y);
				struct loc grid2 = vert ? loc(grid.x, grid.y - (delta * 2))
					: loc(grid.x - (delta * 2), grid.y);
				if (tunnel_ok(c, grid1, grid2, true, 1)) {
					(void) build_tunnel(c, r, r1, grid1,
						grid, false,
						TUNNEL_ROOM_TO_CORRIDOR);

					/* Mark the new room connections */
					dun->connection[r][r1] = true;
					dun->connection[r1][r] = true;
					dun->connection[r][r2] = true;
					dun->connection[r2][r] = true;
					success = true;
				}
			}
			done = true;
		}
	}

	return success;
}

static bool connect_rooms_stairs(struct chunk *c)
{
	int i;
	int corridor_attempts = dun->cent_n * dun->cent_n * 10;
	int r1, r2;
	int pieces = 0;
    int width;
	int stairs = 0;

	/* Phase 1:
	 * Connect each room to the closest room (if not already connected) */
	for (r1 = 0; r1 < dun->cent_n; r1++) {
		int r_closest = 0;
		int d_closest = 1000;

		/* Find closest room */
		for (r2 = 0; r2 < dun->cent_n; r2++) {
			if (r2 != r1) {
				int d = distance(dun->cent[r1], dun->cent[r2]); 
				if (d < d_closest) {
					d_closest = d;
					r_closest = r2;
				}
			}
		}
		
		/* Connect the rooms, if not already connected */
		if (!(dun->connection[r1][r_closest])) {
			(void) connect_two_rooms(c, r1, r_closest, true, false);
		}
	}

	/* Phase 2: */
	/* Make some random connections between rooms so long as they don't
	 * intersect things */
	for (i = 0; i < corridor_attempts; i++) {
		r1 = randint0(dun->cent_n);
		r2 = randint0(dun->cent_n);
		if ((r1 != r2) && !(dun->connection[r1][r2])) {
			(void) connect_two_rooms(c, r1, r2, true, false);
		}
	}

	/* Add some T-intersections in the corridors */
	for (i = 0; i < corridor_attempts; i++) {
		r1 = randint0(dun->cent_n);
		(void) connect_room_to_corridor(c, r1);
	}
	
	/* Phase 3: */
	/* Cut the dungeon up into connected pieces and try hard to make
	 * corridors that connect them */
	pieces = dungeon_pieces();
	while (pieces > 1) {
		bool joined = false;
		for (r1 = 0; r1 < dun->cent_n; r1++) { 
			for (r2 = 0; r2 < dun->cent_n; r2++) {
				if (!joined && (dun->piece[r1] != dun->piece[r2])) {
					for (i = 0; i < 10; i++) {
						if (!(dun->connection[r1][r2])) {
							joined = connect_two_rooms(c, r1, r2, true, true);
						}
					}
				}
			}
		}

		/* This is terrible, and needs fixing - NRM */
		if (!joined) {
			break;
		}

		/* Cut the dungeon up into connected pieces and count them */
		pieces = dungeon_pieces();
	}

	/* Place down stairs */
    width = c->width / z_info->block_wid;
	if (width <= 3) {
		stairs = 1;
    } else if (width == 4) {
		stairs = 2;
    } else {
		stairs = 4;
    }
	if ((player->upkeep->create_stair == FEAT_MORE) ||
		(player->upkeep->create_stair == FEAT_MORE_SHAFT)) {
		stairs--;
	}
	alloc_stairs(c, FEAT_MORE, stairs);

	/* Place up stairs */
    width = c->width / z_info->block_wid;
	if (width <= 3) {
		stairs = 1;
    } else if (width == 4) {
		stairs = 2;
    } else {
		stairs = 4;
    }
    if ((player->upkeep->create_stair == FEAT_LESS) ||
		(player->upkeep->create_stair == FEAT_LESS_SHAFT))
		stairs--;
	alloc_stairs(c, FEAT_LESS, stairs);

	/* Add some quartz streamers */
	for (i = 0; i < dun->profile->str.qua; i++) {
		/* If we can't build streamers, something is wrong with level */
		if (!build_streamer(c, FEAT_QUARTZ)) return false;
	}

    /* Add any chasms if needed */
    build_chasms(c);

	return true;
}

/* ------------------ LEVEL GENERATORS ---------------- */

/**
 * Generate a new dungeon level
 */
struct chunk *cave_gen(struct player *p)
{
	int i, y, x;
	int blocks;
	int room_attempts;
	struct chunk *c;

	/* Hack - variables for allocations */
	int rubble_gen, mon_gen, obj_room_gen;

	/* Sil - determine the dungeon size
	 * note: Block height and width is 1/6 of max height/width */

	/* Between 3 x 3 and 5 x 5 */
	blocks = 3 + ((p->depth + randint1(5)) / 10);

	c = cave_new(blocks * z_info->block_hgt, blocks * z_info->block_wid);
	room_attempts = blocks * blocks * blocks * blocks;
	c->depth = p->depth;

	/* Fill cave area with basic granite */
	fill_rectangle(c, 0, 0, c->height - 1, c->width - 1, FEAT_GRANITE,
				   SQUARE_WALL_SOLID);

	/* Initialize the room tunnel arrays */
	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			dun->tunn1[y][x] = -1;
			dun->tunn2[y][x] = -1;
		}
	}

	/* Guarantee a forge if one hasn't been generated in a while */
	if (p->forge_drought >= rand_range(2000, 5000)) {
		struct room_profile profile = lookup_room_profile("Interesting room");
		if (OPT(p, cheat_room)) msg("Trying to force a forge:");
		p->upkeep->force_forge = true;

		/* Failure (not clear why this would happen) */
		if (!room_build(c, profile)) {
			p->upkeep->force_forge = false;
			if (OPT(p, cheat_room)) msg("failed.");
			uncreate_artifacts(c);
			uncreate_greater_vaults(c, p);
			wipe_mon_list(c, p);
			cave_free(c);
			return NULL;
		}

		if (OPT(p, cheat_room)) msg("succeeded.");
		p->upkeep->force_forge = false;
	}

	/* Build some rooms */
	for (i = 0; i < room_attempts; i++) {
		int j;
		struct room_profile profile = dun->profile->room_profiles[0];
		int hardness = randint1(c->depth + 5);
        if (one_in_(5)) hardness += randint1(5);

		/* Once we have a hardness, we iterate through out list of room profiles
		 * looking for a match (whose hardness >= this hardness, or which meets
		 * depth or random conditions). We then try building this room. */
		for (j = 0; j < dun->profile->n_room_profiles; j++) {
			profile = dun->profile->room_profiles[j];
			if ((profile.hardness > hardness) || !profile.hardness) break;
			if (profile.level && (profile.level == c->depth)) break;
			if (profile.random && one_in_(profile.random)) break;
		}

		/* Try again if failed */
		if (!profile.builder) continue;

		/* Build it */
		room_build(c, profile);

		/* Stop if there are too many rooms */
		if (dun->cent_n == z_info->level_room_max - 1) break;
	}

	/* Generate permanent walls around the edge of the generated area */
	draw_rectangle(c, 0, 0, c->height - 1, c->width - 1, FEAT_PERM, SQUARE_NONE,
				   true);

	/* Start over on levels with less than two rooms due to inevitable crash */
	if (dun->cent_n < z_info->level_room_min) {
		if (OPT(p, cheat_room)) msg("Not enough rooms.");
		uncreate_artifacts(c);
		uncreate_greater_vaults(c, p);
		wipe_mon_list(c, p);
		cave_free(c);
		return NULL;
	}

	/* Make the tunnels
	 * Sil - This has been changed considerably */
	if (!connect_rooms_stairs(c)) {
		if (OPT(p, cheat_room)) msg("Couldn't connect the rooms.");
		uncreate_artifacts(c);
		uncreate_greater_vaults(c, p);
		wipe_mon_list(c, p);
		cave_free(c);
		return NULL;
	}
	
	/* Randomise the doors (except those in vaults) */
	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			struct loc grid = loc(x, y);
			if (square_iscloseddoor(c, grid) && !square_isvault(c, grid)) {
				if (one_in_(4)) {
					square_set_feat(c, grid, FEAT_FLOOR);			
				} else {
					place_random_door(c, grid);
				}
			}
		}
	}
	
	/* Place some rubble, occasionally much more on deep levels */
	rubble_gen = randint1((blocks * blocks) / 3);
	if ((c->depth >= 10) && one_in_(10)) {
		rubble_gen += blocks * blocks * 2;
	}
	alloc_object(c, SET_BOTH, TYP_RUBBLE, rubble_gen, c->depth,
		ORIGIN_FLOOR);

	/* Place the player */
	new_player_spot(c, p);

	/* Check dungeon connectivity */
	if (!check_connectivity(c)) {
		if (OPT(p, cheat_room)) msg("Failed connectivity.");
		uncreate_artifacts(c);
		uncreate_greater_vaults(c, p);
		wipe_mon_list(c, p);
		cave_free(c);
		return NULL;
	}

	if (c->depth == 1) {
		/* Smaller number of monsters at 50ft */
		mon_gen = dun->cent_n / 2;
	} else {
		/* Pick some number of monsters (between 0.5 per room and 1 per room) */
		mon_gen = (dun->cent_n + randint1(dun->cent_n)) / 2;
	}

	/* Put some objects in rooms */
	obj_room_gen = 3 * mon_gen / 4;
	if (obj_room_gen > 0) {
		alloc_object(c, SET_ROOM, TYP_OBJECT, obj_room_gen, c->depth,
			ORIGIN_FLOOR);
	}
	
    /* Place the traps */
    place_traps(c);

	/* Put some monsters in the dungeon */
	for (i = mon_gen; i > 0; i--) {
		(void) pick_and_place_distant_monster(c, p, true, c->depth);
	}

	/* Place Morgoth if on the run */
	if (p->on_the_run && !p->morgoth_slain) {
		/* Place Morgoth */
		struct loc grid;
		int count = 100;

		/* Find a suitable place */
		while (cave_find(c, &grid, square_suits_start) && count--) {
			/* Out of sight of the player */
			if (!los(c, p->grid, grid)) {
				struct monster_group_info info = {0, 0};
				place_new_monster_one(c, grid, lookup_monster("Morgoth"), true,
									  true, info, ORIGIN_DROP);
				break;
			}
		}
	}

	/* Add a curved sword near the player if this is the start of the game */
	if (p->turn == 0) {
		place_item_near_player(c, p, TV_SWORD, "Curved Sword");
	}

	return c;
}

/**
 * Create the level containing Morgoth's throne room
 */
struct chunk *throne_gen(struct player *p)
{
	int y, x;
	struct chunk *c;
	struct room_profile profile = lookup_room_profile("Throne room");
	struct loc pgrid = loc(0, 0);

	/* Display the throne poetry */
	event_signal_poem(EVENT_POEM, "throne_poetry", 5, 15);

	/* Set the 'truce' in action */
	p->truce = true;

	/* Restrict to single-screen size */
	c = cave_new(3 * z_info->block_hgt, 3 * z_info->block_wid);
	c->depth = p->depth;

	/* Fill cave area with basic granite */
	fill_rectangle(c, 0, 0, c->height - 1, c->width - 1, FEAT_GRANITE,
				   SQUARE_WALL_SOLID);

	/* Generate permanent walls around the edge of the generated area */
	draw_rectangle(c, 0, 0, c->height - 1, c->width - 1, FEAT_PERM, SQUARE_NONE,
				   true);

	/* Build it */
	room_build(c, profile);

	/* Find an up staircase */
	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			struct loc grid = loc(x, y);
			/* Assumes the important staircase is at the centre of the level */
			if (square_isupstairs(c, grid) && (x >= 40) && (x <= 55)) {
				pgrid = grid;
				break;
			}
		}
		if (!loc_eq(pgrid, loc(0, 0))) break;
	}

	if (loc_eq(pgrid, loc(0, 0))) {
		msg("Failed to find an up staircase in the throne-room");
	}

	/* Delete any monster on the starting square */
	delete_monster(c, pgrid);
	
	/* Place the player */
	player_place(c, p, pgrid);

	return c;
}


/**
 * Create the gates to Angband level
 */
struct chunk *gates_gen(struct player *p)
{
	int y, x;
	struct chunk *c;
	struct room_profile profile = lookup_room_profile("Gates of Angband");
	const struct vault *gates_room =
		random_vault(p->depth, "Gates of Angband", false);
	struct loc pgrid = loc(0, 0);

	/* Make it just big enough for the room and permanent boundary. */
	c = cave_new(2 + gates_room->hgt, 2 + gates_room->wid);
	c->depth = p->depth;

	/* Fill cave area with basic granite */
	fill_rectangle(c, 0, 0, c->height - 1, c->width - 1, FEAT_GRANITE,
				   SQUARE_WALL_SOLID);

	/* Generate permanent walls around the edge of the generated area */
	draw_rectangle(c, 0, 0, c->height - 1, c->width - 1, FEAT_PERM, SQUARE_NONE,
				   true);

	/* Remove the bottom wall */
	for (x = 1; x < c->width - 1; x++) {
		square_set_feat(c, loc(x, c->height - 1), FEAT_FLOOR);
	}

	/* Build it */
	room_build(c, profile);

	/* Find a down staircase */
	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			struct loc grid = loc(x, y);
			/* Assumes the important staircase is at the centre of the level */
			if (square_isdownstairs(c, grid)) {
				pgrid = grid;
				break;
			}
		}
		if (!loc_eq(pgrid, loc(0, 0))) break;
	}

	if (loc_eq(pgrid, loc(0, 0))) {
		msg("Failed to find a down staircase in the gates level");
	}

	/* Delete any monster on the starting square */
	delete_monster(c, pgrid);
	
	/* Place the player */
	player_place(c, p, pgrid);

	return c;
}

