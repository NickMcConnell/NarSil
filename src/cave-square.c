 /**
 * \file cave-square.c
 * \brief functions for dealing with individual squares
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
#include "game-world.h"
#include "init.h"
#include "monster.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "object.h"
#include "player-abilities.h"
#include "player-quest.h"
#include "player-timed.h"
#include "trap.h"


/**
 * FEATURE PREDICATES
 *
 * These functions test a terrain feature index for the obviously described
 * type.  They are used in the square feature predicates below, and
 * occasionally on their own
 */

/**
 * True if the square is rock.
 */
bool feat_is_rock(int feat)
{
	return tf_has(f_info[feat].flags, TF_ROCK);
}

/**
 * True if the square is a quartz wall.
 */
bool feat_is_quartz(int feat)
{
	return tf_has(f_info[feat].flags, TF_QUARTZ);
}

/**
 * True if the square is a granite wall.
 */
bool feat_is_granite(int feat)
{
	return tf_has(f_info[feat].flags, TF_GRANITE);
}

/**
 * True if the feature is a solid wall (not rubble).
 */
bool feat_is_wall(int feat)
{
	return tf_has(f_info[feat].flags, TF_WALL);
}

/**
 * True if the feature is a floor.
 */
bool feat_is_floor(int feat)
{
	return tf_has(f_info[feat].flags, TF_FLOOR);
}

/**
 * True if the feature can hold a trap.
 */
bool feat_is_trap_holding(int feat)
{
	return tf_has(f_info[feat].flags, TF_TRAP);
}

/**
 * True if the feature can hold an object.
 */
bool feat_is_object_holding(int feat)
{
	return tf_has(f_info[feat].flags, TF_OBJECT);
}

/**
 * True if a monster can walk through the feature.
 */
bool feat_is_monster_walkable(int feat)
{
	return tf_has(f_info[feat].flags, TF_PASSABLE);
}

/**
 * True if the feature is a forge.
 */
bool feat_is_forge(int feat)
{
	return tf_has(f_info[feat].flags, TF_FORGE);
}

/**
 * True if the feature allows line-of-sight.
 */
bool feat_is_los(int feat)
{
	return tf_has(f_info[feat].flags, TF_LOS);
}

/**
 * True if the feature is passable by the player.
 */
bool feat_is_passable(int feat)
{
	return tf_has(f_info[feat].flags, TF_PASSABLE);
}

/**
 * True if any projectable can pass through the feature.
 */
bool feat_is_projectable(int feat)
{
	return tf_has(f_info[feat].flags, TF_PROJECT);
}

/**
 * True if the feature can be lit by light sources.
 */
bool feat_is_torch(int feat)
{
	return tf_has(f_info[feat].flags, TF_TORCH);
}

/**
 * True if the feature is a pit.
 */
bool feat_is_pit(int feat)
{
	return tf_has(f_info[feat].flags, TF_PIT);
}

/**
 * True if the feature doesn't carry monster flow information.
 */
bool feat_is_no_flow(int feat)
{
	return tf_has(f_info[feat].flags, TF_NO_FLOW);
}

/**
 * True if the feature doesn't carry player scent.
 */
bool feat_is_no_scent(int feat)
{
	return tf_has(f_info[feat].flags, TF_NO_SCENT);
}

/**
 * True if the feature is a chasm.
 */
bool feat_is_chasm(int feat)
{
	return tf_has(f_info[feat].flags, TF_CHASM);
}

/**
 * SQUARE FEATURE PREDICATES
 *
 * These functions are used to figure out what kind of square something is,
 * via c->squares[y][x].feat (preferably accessed via square(c, grid)).
 * All direct testing of square(c, grid)->feat should be rewritten
 * in terms of these functions.
 *
 * It's often better to use square behavior predicates (written in terms of
 * these functions) instead of these functions directly.
 */

/**
 * True if the square is normal open floor.
 */
bool square_isfloor(struct chunk *c, struct loc grid)
{
	return feat_is_floor(square(c, grid)->feat);
}

/**
 * True if the square can hold a trap.
 */
bool square_istrappable(struct chunk *c, struct loc grid)
{
	return feat_is_trap_holding(square(c, grid)->feat);
}

/**
 * True if the square can hold an object.
 */
bool square_isobjectholding(struct chunk *c, struct loc grid)
{
	return feat_is_object_holding(square(c, grid)->feat);
}

/**
 * True if the square is a normal granite rock wall.
 */
bool square_isgranitewall(struct chunk *c, struct loc grid)
{
	return (tf_has(f_info[square(c, grid)->feat].flags, TF_GRANITE) &&
			!tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_ANY));
}

/**
 * True if the square is granite.
 */
bool square_isgranite(struct chunk *c, struct loc grid)
{
	return feat_is_granite(square(c, grid)->feat);
}

/**
 * True if the square is a permanent wall.
 */
bool square_isperm(struct chunk *c, struct loc grid)
{
	return (tf_has(f_info[square(c, grid)->feat].flags, TF_PERMANENT) &&
			tf_has(f_info[square(c, grid)->feat].flags, TF_ROCK));
}

/**
 * True if the square is made of rock.
 */
bool square_isrock(struct chunk *c, struct loc grid)
{
	return feat_is_rock(square(c, grid)->feat);
}

/**
 * True if the square is a quartz wall.
 */
bool square_isquartz(struct chunk *c, struct loc grid)
{
	return feat_is_quartz(square(c, grid)->feat);
}

/**
 * True if the square is a mineral wall (quartz/granite).
 */
bool square_ismineral(struct chunk *c, struct loc grid)
{
	return square_isgranitewall(c, grid) || square_isquartz(c, grid);
}

/**
 * True if the square is rubble.
 */
bool square_isrubble(struct chunk *c, struct loc grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_ROCK) &&
			!tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_ANY) &&
			!tf_has(f_info[square(c, grid)->feat].flags, TF_WALL));
}

/**
 * True if the square is a hidden secret door.
 *
 * These squares appear as if they were granite--when detected a secret door
 * is replaced by a closed door.
 */
bool square_issecretdoor(struct chunk *c, struct loc grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_ANY) &&
			tf_has(f_info[square(c, grid)->feat].flags, TF_ROCK));
}

/**
 * True if the square is an open door.
 */
bool square_isopendoor(struct chunk *c, struct loc grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_CLOSABLE));
}

/**
 * True if the square is a closed door (possibly locked or jammed).
 */
bool square_iscloseddoor(struct chunk *c, struct loc grid)
{
	int feat = square(c, grid)->feat;
	return tf_has(f_info[feat].flags, TF_DOOR_CLOSED);
}

bool square_isbrokendoor(struct chunk *c, struct loc grid)
{
	int feat = square(c, grid)->feat;
    return (tf_has(f_info[feat].flags, TF_DOOR_ANY) &&
			tf_has(f_info[feat].flags, TF_PASSABLE) &&
			!tf_has(f_info[feat].flags, TF_CLOSABLE));
}

/**
 * True if the square is a door.
 *
 * This includes open, closed, and hidden doors.
 */
bool square_isdoor(struct chunk *c, struct loc grid)
{
	int feat = square(c, grid)->feat;
	return tf_has(f_info[feat].flags, TF_DOOR_ANY);
}

/**
 * True if the square is a wall.
 *
 * This includes closed doors and any type of blocking rock, including rubble.
 */
bool square_iswall(struct chunk *c, struct loc grid)
{
	int feat = square(c, grid)->feat;
	return tf_has(f_info[feat].flags, TF_DOOR_CLOSED) ||
		tf_has(f_info[feat].flags, TF_ROCK);
}

/**
 * True if square is any stair
 */
bool square_isstairs(struct chunk *c, struct loc grid)
{
	int feat = square(c, grid)->feat;
	return tf_has(f_info[feat].flags, TF_STAIR);
}

/**
 * True if square is an up stair.
 */
bool square_isupstairs(struct chunk*c, struct loc grid)
{
	int feat = square(c, grid)->feat;
	return tf_has(f_info[feat].flags, TF_UPSTAIR);
}

/**
 * True if square is a down stair.
 */
bool square_isdownstairs(struct chunk *c, struct loc grid)
{
	int feat = square(c, grid)->feat;
	return tf_has(f_info[feat].flags, TF_DOWNSTAIR);
}

/**
 * True if square is any shaft
 */
bool square_isshaft(struct chunk *c, struct loc grid)
{
	int feat = square(c, grid)->feat;
	return tf_has(f_info[feat].flags, TF_SHAFT);
}

/**
 * True if the square is a forge.
 */
bool square_isforge(struct chunk *c, struct loc grid)
{
	return feat_is_forge(square(c, grid)->feat);
}

/**
 * True if the square contains the player
 */
bool square_isplayer(struct chunk *c, struct loc grid) {
	return square(c, grid)->mon < 0 ? true : false;
}

/**
 * True if the square contains the player or a monster
 */
bool square_isoccupied(struct chunk *c, struct loc grid) {
	return square(c, grid)->mon != 0 ? true : false;
}

/**
 * True if the square is impassable (wall or chasm)
 */
bool square_isimpassable(struct chunk *c, struct loc grid) {
	return square_ischasm(c, grid) || square_isrock(c, grid);
}

/**
 * True if the player knows the terrain of the square
 */
bool square_isknown(struct chunk *c, struct loc grid) {
	if (c != cave && (!player || c != player->cave)) return false;
	if (!player->cave) return false;
	return square(player->cave, grid)->feat == FEAT_NONE ? false : true;
}

/**
 * True if the player's knowledge of the terrain of the square is wrong
 * or missing
 */
bool square_ismemorybad(struct chunk *c, struct loc grid) {
	return !square_isknown(c, grid)
		|| square(player->cave, grid)->feat != square(cave, grid)->feat;
}

/**
 * True if the square is a chasm.
 */
bool square_ischasm(struct chunk *c, struct loc grid)
{
	return feat_is_chasm(square(c, grid)->feat);
}

/**
 * SQUARE INFO PREDICATES
 *
 * These functions tell whether a square is marked with one of the SQUARE_*
 * flags.  These flags are mostly used to mark a square with some information
 * about its location or status.
 */

/**
 * True if the square is marked
 */
bool square_ismark(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_MARK);
}

/**
 * True if the square is lit
 */
bool square_isglow(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_GLOW);
}

/**
 * True if the square is part of a vault.
 *
 * This doesn't say what kind of square it is, just that it is part of a vault.
 */
bool square_isvault(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_VAULT);
}

/**
 * True if the square is part of a greater vault.
 */
bool square_isgreatervault(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_G_VAULT);
}

/**
 * True if the square is part of a room.
 */
bool square_isroom(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_ROOM);
}

/**
 * True if the square has been seen by the player
 */
bool square_isseen(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_SEEN);
}

/**
 * True if the cave square is currently viewable by the player
 */
bool square_isview(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_VIEW);
}

/**
 * True if the cave square is currently fireable by the player
 */
bool square_isfire(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_FIRE);
}

/**
 * True if the cave square was seen before the current update
 */
bool square_wasseen(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_WASSEEN);
}

/**
 * True if the square has a known trap
 */
bool square_istrap(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_TRAP);
}

/**
 * True if the square has an unknown trap
 */
bool square_isinvis(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_INVIS);
}

/**
 * True if cave square is an inner wall (generation)
 */
bool square_iswall_inner(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_WALL_INNER);
}

/**
 * True if cave square is an outer wall (generation)
 */
bool square_iswall_outer(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_WALL_OUTER);
}

/**
 * True if cave square is a solid wall (generation)
 */
bool square_iswall_solid(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_WALL_SOLID);
}

/**
 * True if cave square will become a chasm (generation)
 */
bool square_tobechasm(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_CHASM);
}

/**
 * True if cave square is marked for projection processing
 */
bool square_isproject(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return sqinfo_has(square(c, grid)->info, SQUARE_PROJECT);
}


/**
 * SQUARE BEHAVIOR PREDICATES
 *
 * These functions define how a given square behaves, e.g. whether it is
 * passable by the player, whether it is diggable, contains items, etc.
 *
 * These functions use the SQUARE FEATURE PREDICATES (among other info) to
 * make the determination.
 */

/**
 * True if the square is open (a floor square not occupied by a monster).
 */
bool square_isopen(struct chunk *c, struct loc grid) {
	return square_isfloor(c, grid) && !square(c, grid)->mon;
}

/**
 * True if the square is empty (an open square without any items).
 */
bool square_isempty(struct chunk *c, struct loc grid) {
	if (square_isplayertrap(c, grid)) return false;
	if (square_iswebbed(c, grid)) return false;
	return square_isopen(c, grid) && !square_object(c, grid);
}

/**
 * True if cave square is available to create a trap
 */
bool square_isunseen(struct chunk *c, struct loc grid) {
	if (square_isseen(cave, grid)) return false;
	return square_isempty(cave, grid);
}

/**
 * True if the square is usable to place a monster.
 */
bool square_isarrivable(struct chunk *c, struct loc grid) {
	if (square_iswall(c, grid)) return false;
	if (square_ischasm(c, grid)) return false;
	if (square(c, grid)->mon) return false;
	return true;
}

/**
 * True if the square is an untrapped floor square without items.
 */
bool square_canputitem(struct chunk *c, struct loc grid) {
	if (!square_isobjectholding(c, grid)) return false;
	if (square_istrap(c, grid)) return false;
	return !square_object(c, grid);
}

/**
 * True if the square can be dug: this includes rubble and non-permanent walls.
 */
bool square_isdiggable(struct chunk *c, struct loc grid) {
	return (square_ismineral(c, grid) ||
			square_issecretdoor(c, grid) || 
			square_isrubble(c, grid));
}

/**
 * True if the square is a floor with no traps.
 */
bool square_iswebbable(struct chunk *c, struct loc grid) {
	if (square_trap(c, grid)) return false;
	return square_isfloor(c, grid);
}

/**
 * True if the square is a chasm or a known pit or false floor
 */
bool square_isleapable(struct chunk *c, struct loc grid) {
	struct trap_kind *false_floor = lookup_trap("false floor");
	if (square_istrap(c, grid) &&
		square_trap_specific(c, grid, false_floor->tidx)) {
		return true;
	}
	if (square_ispit(c, grid)) return true;
	return square_ischasm(c, grid);
}

/**
 * True if a monster can walk through the tile.
 *
 * This is needed for polymorphing. A monster may be on a feature that isn't
 * an empty space, causing problems when it is replaced with a new monster.
 */
bool square_is_monster_walkable(struct chunk *c, struct loc grid)
{
	assert(square_in_bounds(c, grid));
	return feat_is_monster_walkable(square(c, grid)->feat);
}

/**
 * True if the square is passable by the player.
 */
bool square_ispassable(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return feat_is_passable(square(c, grid)->feat);
}

/**
 * True if any projectable can pass through the square.
 */
bool square_isprojectable(struct chunk *c, struct loc grid) {
	if (!square_in_bounds(c, grid)) return false;
	return feat_is_projectable(square(c, grid)->feat);
}

/**
 * True if the square allows line-of-sight.
 */
bool square_allowslos(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return feat_is_los(square(c, grid)->feat);
}

/**
 * True if the square is a permanent wall or one of the "stronger" walls.
 *
 * The stronger walls are granite, magma and quartz. This excludes things like
 * secret doors and rubble.
 */
bool square_isstrongwall(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return square_ismineral(c, grid) || square_isperm(c, grid);
}

/**
 * True if the cave square a pit.
 */
bool square_ispit(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return feat_is_pit(square(c, grid)->feat);
}

/**
 * True if the cave square is lit.
 */
bool square_islit(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return square_light(c, grid) > 0 ? true : false;
}

/**
 * True if the cave square doesn't allow monster flow information.
 */
bool square_isnoflow(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return feat_is_no_flow(square(c, grid)->feat);
}

/**
 * True if the cave square doesn't carry player scent.
 */
bool square_isnoscent(struct chunk *c, struct loc grid) {
	assert(square_in_bounds(c, grid));
	return feat_is_no_scent(square(c, grid)->feat);
}

bool square_iswarded(struct chunk *c, struct loc grid)
{
	struct trap_kind *rune = lookup_trap("glyph of warding");
	return square_trap_specific(c, grid, rune->tidx);
}

bool square_iswebbed(struct chunk *c, struct loc grid)
{
	struct trap_kind *web = lookup_trap("web");
	return square_trap_specific(c, grid, web->tidx);
}

bool square_seemslikewall(struct chunk *c, struct loc grid)
{
	return tf_has(f_info[square(c, grid)->feat].flags, TF_ROCK);
}

bool square_isinteresting(struct chunk *c, struct loc grid)
{
	int f = square(c, grid)->feat;
	return tf_has(f_info[f].flags, TF_INTERESTING);
}

/**
 * True if the square is a closed, locked door.
 */
bool square_islockeddoor(struct chunk *c, struct loc grid)
{
	return square_door_lock_power(c, grid) > 0;
}

/**
 * True if the square is a closed, jammed door.
 */
bool square_isjammeddoor(struct chunk *c, struct loc grid)
{
	return square_door_jam_power(c, grid) > 0;
}

/**
 * True if there is a player trap (known or unknown) in this square.
 */
bool square_isplayertrap(struct chunk *c, struct loc grid)
{
    return square_trap_flag(c, grid, TRF_TRAP);
}

/**
 * True if there is a visible trap in this square.
 */
bool square_isvisibletrap(struct chunk *c, struct loc grid)
{
    /* Look for a visible trap */
    return square_trap_flag(c, grid, TRF_VISIBLE);
}
/**
 * True if the square is an unknown player trap (it will appear as a floor tile)
 */
bool square_issecrettrap(struct chunk *c, struct loc grid)
{
    return !square_isvisibletrap(c, grid) && square_isplayertrap(c, grid);
}

/**
 * True if the square is a known, disarmable player trap.
 */
bool square_isdisarmabletrap(struct chunk *c, struct loc grid)
{
	return square_isvisibletrap(c, grid) && square_isplayertrap(c, grid);
}

/**
 * Determine if a given location may be "destroyed"
 *
 * Used by destruction spells, and for placing stairs, etc.
 */
bool square_changeable(struct chunk *c, struct loc grid)
{
	struct object *obj;

	/* Forbid perma-grids */
	if (square_isperm(c, grid) || square_isforge(c, grid) ||
		square_isstairs(c, grid)) {
		return false;
	}

	/* Check objects */
	for (obj = square_object(c, grid); obj; obj = obj->next) {
		/* Forbid grids with indestructible objects */
		if (of_has(obj->flags, OF_INDESTRUCTIBLE)) return false;
	}

	/* Accept */
	return true;
}


bool square_in_bounds(struct chunk *c, struct loc grid)
{
	assert(c);
	return grid.x >= 0 && grid.x < c->width &&
		grid.y >= 0 && grid.y < c->height;
}

bool square_in_bounds_fully(struct chunk *c, struct loc grid)
{
	assert(c);
	return grid.x > 0 && grid.x < c->width - 1 &&
		grid.y > 0 && grid.y < c->height - 1;
}

/**
 * Checks if a square is thought by the player to block projections
 */
bool square_isbelievedwall(struct chunk *c, struct loc grid)
{
	// the edge of the world is definitely gonna block things
	if (!square_in_bounds_fully(c, grid)) return true;
	// if we dont know assume its projectable
	if (!square_isknown(c, grid)) return false;
	// report what we think (we may be wrong)
	return !square_isprojectable(player->cave, grid);
}

/**
 * Checks if a square is empty and not in a vault
 */
bool square_suits_start(struct chunk *c, struct loc grid)
{
	return square_isempty(c, grid) && !square_isvault(c, grid);
}

/**
 * Checks if a square is in a room and not next to doors
 */
bool square_suits_stairs(struct chunk *c, struct loc grid)
{
	if (!square_isempty(c, grid) || !square_isroom(c, grid)) return false;
	return !square_num_doors_adjacent(c, grid);
}

/**
 * Checks if a square is appropriate for placing a summoned creature.
 */
bool square_allows_summon(struct chunk *c, struct loc grid)
{
	return square_isempty(c, grid) && !square_iswarded(c, grid);
}

/**
 * Determines whether a square is viewable (only) by the keen senses ability.
 *
 * Note that there is a slight flaw in the implementation.
 * Since light levels are not adjusted by monsters/items if out of view
 * this means that if a square adjacent to the monster is out of view but lit
 * and the monster's square is not lit, then sometimes it won't show when it
 * should show.
 * This is pretty rare though, and I doubt it is very noticeable.
 */
bool square_seen_by_keen_senses(struct chunk *c, struct loc grid)
{
	if (player_active_ability(player, "Keen Senses") &&
		square_isview(c, grid) && (square_light(c, grid) == 0)) {
		int d;
		for (d = 0; d < 8; d++) {
			/* Child location */
			struct loc grid1 = loc_sum(grid, ddgrid_ddd[d]);

			/* Check Bounds */
			if (!square_in_bounds(c, grid1)) continue;

			if ((square_light(c, grid1) > 0) && square_isfloor(c, grid1) &&
				square_isview(c, grid1)) {
				return true;
			}
		}
	}

	return false;
}


/**
 * OTHER SQUARE FUNCTIONS
 *
 * Below are various square-specific functions which are not predicates
 */

const struct square *square(struct chunk *c, struct loc grid)
{
	assert(square_in_bounds(c, grid));
	return &c->squares[grid.y][grid.x];
}

struct feature *square_feat(struct chunk *c, struct loc grid)
{
	assert(square_in_bounds(c, grid));
	return &f_info[square(c, grid)->feat];
}

int square_light(struct chunk *c, struct loc grid)
{
	assert(square_in_bounds(c, grid));
	return square(c, grid)->light;
}

/**
 * Get a monster on the current level by its position.
 */
struct monster *square_monster(struct chunk *c, struct loc grid)
{
	if (!square_in_bounds(c, grid)) return NULL;
	if (square(c, grid)->mon > 0) {
		struct monster *mon = cave_monster(c, square(c, grid)->mon);
		return mon && mon->race ? mon : NULL;
	}

	return NULL;
}

/**
 * Get the top object of a pile on the current level by its position.
 */
struct object *square_object(struct chunk *c, struct loc grid) {
	if (!square_in_bounds(c, grid)) return NULL;
	return square(c, grid)->obj;
}

/**
 * Get the first (and currently only) trap in a position on the current level.
 */
struct trap *square_trap(struct chunk *c, struct loc grid)
{
	if (!square_in_bounds(c, grid)) return NULL;
    return square(c, grid)->trap;
}

/**
 * Return true if the given object is on the floor at this grid
 */
bool square_holds_object(struct chunk *c, struct loc grid, struct object *obj) {
	assert(square_in_bounds(c, grid));
	return pile_contains(square_object(c, grid), obj);
}

/**
 * Excise an object from a floor pile, leaving it orphaned.
 */
void square_excise_object(struct chunk *c, struct loc grid, struct object *obj){
	assert(square_in_bounds(c, grid));
	pile_excise(&c->squares[grid.y][grid.x].obj, obj);
}

/**
 * Excise an entire floor pile.
 */
void square_excise_pile(struct chunk *c, struct loc grid) {
	struct chunk *p_c = (player && c == cave) ? player->cave : NULL;
	assert(square_in_bounds(c, grid));
	object_pile_free(c, p_c, square_object(c, grid));
	square_set_obj(c, grid, NULL);
}

/**
 * Remove all imagined objects from a floor pile.
 *
 * \param p_c is the chunk for a player's point of view which will be tested
 * for the imagined objects.
 * \param c is the chunk (typically cave) which holds the orphaned objects
 * corresponding to the imagined objects in p_c.
 * \param grid is the grid to check for imagined objects.
 *
 * If calling square_excise_pile() on p_c it will necessary to call this
 * function first to avoid leaving dangling references (via the known pointer
 * in orphaned objects within c's object list).
 */
void square_excise_all_imagined(struct chunk *p_c, struct chunk *c,
		struct loc grid)
{
	struct object *obj;

	assert(square_in_bounds(p_c, grid));
	obj = square_object(p_c, grid);
	while (obj) {
		struct object *next = obj->next;

		if (obj->notice & OBJ_NOTICE_IMAGINED) {
			struct object *original;

			assert(c->objects && c->objects[obj->oidx]);
			original = c->objects[obj->oidx];
			square_excise_object(p_c, grid, obj);
			delist_object(p_c, obj);
			object_delete(p_c, NULL, &obj);
			original->known = NULL;
			delist_object(c, original);
			object_delete(c, p_c, &original);
		}
		obj = next;
	}
}

/**
 * Excise an object from a floor pile and delete it while doing the other
 * necessary bookkeeping.  Normally, this is only called for the chunk
 * representing the true nature of the environment and not the one
 * representing the player's view of it.  If do_note is true, call
 * square_note_spot().  If do_light is true, call square_light_spot().
 * Unless calling this on the player's view, those both would be true
 * except as an optimization/simplification when the caller would call
 * square_note_spot()/square_light_spot() anyways or knows that those aren't
 * necessary.
 */
void square_delete_object(struct chunk *c, struct loc grid, struct object *obj,
						  bool do_note, bool do_light)
{
	struct chunk *p_c = (c == cave) ? player->cave : NULL;
	square_excise_object(c, grid, obj);
	delist_object(c, obj);
	object_delete(c, p_c, &obj);
	if (do_note) {
		square_note_spot(c, grid);
	}
	if (do_light) {
		square_light_spot(c, grid);
	}
}

/**
 * Helper for square_know_pile():  remove known
 * location for the requested items that are not on this grid.
 */
static void forget_remembered_objects(struct chunk *c, struct chunk *knownc,
		struct loc grid)
{
	struct object *obj = square_object(knownc, grid);

	while (obj) {
		struct object *next = obj->next;
		struct object *original = c->objects[obj->oidx];

		assert(original);
		if (!square_holds_object(c, grid, original)) {
			square_excise_object(knownc, grid, obj);
			obj->grid = loc(0, 0);

			/* Delete objects which no longer exist anywhere */
			if (obj->notice & OBJ_NOTICE_IMAGINED) {
				delist_object(knownc, obj);
				object_delete(knownc, NULL, &obj);
				original->known = NULL;
				delist_object(c, original);
				object_delete(c, knownc, &original);
			}
		}
		obj = next;
	}
}

/**
 * Update the player's knowledge of the objects on a grid in the current level
 */
void square_know_pile(struct chunk *c, struct loc grid)
{
	struct object *obj;

	if (c != cave) return;

	object_lists_check_integrity(c, player->cave);

	/* Know every item on this grid */
	for (obj = square_object(c, grid); obj; obj = obj->next) {
		object_see(player, obj);
		if (loc_eq(grid, player->grid)) {
			object_touch(player, obj);
		}
	}

	forget_remembered_objects(c, player->cave, grid);
}


/**
 * Return how many cardinal directions around (x, y) contain doors.
 * \param c current chunk
 * \param grid is the location in c to examine
 * \return the number of walls
 */
int square_num_doors_adjacent(struct chunk *c, struct loc grid)
{
    int k = 0;
    assert(square_in_bounds(c, grid));

    if (square_isdoor(c, next_grid(grid, DIR_S))) k++;
    if (square_isdoor(c, next_grid(grid, DIR_N))) k++;
    if (square_isdoor(c, next_grid(grid, DIR_E))) k++;
    if (square_isdoor(c, next_grid(grid, DIR_W))) k++;

    return k;
}

/**
 * Return how many diagonal directions around (x, y) contain walls.
 * \param c current chunk
 * \param grid is the location in c to examine
 * \return the number of walls
 */
int square_num_walls_diagonal(struct chunk *c, struct loc grid)
{
    int k = 0;
    assert(square_in_bounds(c, grid));

    if (feat_is_wall(square(c, next_grid(grid, DIR_SE))->feat)) k++;
    if (feat_is_wall(square(c, next_grid(grid, DIR_NW))->feat)) k++;
    if (feat_is_wall(square(c, next_grid(grid, DIR_NE))->feat)) k++;
    if (feat_is_wall(square(c, next_grid(grid, DIR_SW))->feat)) k++;

    return k;
}


/**
 * Set the terrain type for a square.
 *
 * This should be the only function that sets terrain, apart from the savefile
 * loading code.
 */
void square_set_feat(struct chunk *c, struct loc grid, int feat)
{
	int current_feat;

	assert(square_in_bounds(c, grid));
	current_feat = square(c, grid)->feat;

	/* Track changes */
	if (current_feat) c->feat_count[current_feat]--;
	if (feat) c->feat_count[feat]++;

	/* Make the change */
	c->squares[grid.y][grid.x].feat = feat;

	/* Make the new terrain feel at home */
	if (character_dungeon) {
		/* Remove traps if necessary */
		if (!square_istrappable(c, grid))
			square_destroy_trap(c, grid);

		square_note_spot(c, grid);
		square_light_spot(c, grid);
	} else {
		/* Make sure no incorrect wall flags set for dungeon generation */
		sqinfo_off(square(c, grid)->info, SQUARE_WALL_INNER);
		sqinfo_off(square(c, grid)->info, SQUARE_WALL_OUTER);
		sqinfo_off(square(c, grid)->info, SQUARE_WALL_SOLID);
	}
}

/**
 * Set the player-"known" terrain type for a square.
 */
static void square_set_known_feat(struct chunk *c, struct loc grid, int feat)
{
	if (c != cave) return;
	player->cave->squares[grid.y][grid.x].feat = feat;
}

/**
 * Set the occupying monster for a square.
 */
void square_set_mon(struct chunk *c, struct loc grid, int midx)
{
	c->squares[grid.y][grid.x].mon = midx;
}

/**
 * Set the (first) object for a square.
 */
void square_set_obj(struct chunk *c, struct loc grid, struct object *obj)
{
	c->squares[grid.y][grid.x].obj = obj;
}

/**
 * Set the (first) trap for a square.
 */
void square_set_trap(struct chunk *c, struct loc grid, struct trap *trap)
{
	c->squares[grid.y][grid.x].trap = trap;
}

void square_add_trap(struct chunk *c, struct loc grid)
{
	assert(square_in_bounds_fully(c, grid));
	if (!square_isempty(c, grid)) return;
	place_trap(c, grid, -1, c->depth);
}

void square_add_glyph(struct chunk *c, struct loc grid, int type)
{
	struct trap_kind *glyph = NULL;
	switch (type) {
		case GLYPH_WARDING: {
			glyph = lookup_trap("glyph of warding");
			break;
		}
		default: {
			msg("Non-existent glyph requested. Please report this bug.");
			return;
		}
	}
	place_trap(c, grid, glyph->tidx, 0);
}

void square_add_web(struct chunk *c, struct loc grid)
{
	struct trap_kind *web = lookup_trap("web");
	place_trap(c, grid, web->tidx, 0);
}

void square_add_stairs(struct chunk *c, struct loc grid, int depth) {
	int down = randint0(100) < 50;
	if (depth == 0)
		down = 1;
	else if (depth >= z_info->dun_depth)
		down = 0;

	square_set_feat(c, grid, down ? FEAT_MORE : FEAT_LESS);
}

void square_add_door(struct chunk *c, struct loc grid, bool closed) {
	square_set_feat(c, grid, closed ? FEAT_CLOSED : FEAT_OPEN);
}

void square_open_door(struct chunk *c, struct loc grid)
{
	square_remove_all_traps(c, grid);
	square_set_feat(c, grid, FEAT_OPEN);
}

void square_close_door(struct chunk *c, struct loc grid)
{
	square_set_feat(c, grid, FEAT_CLOSED);
}

void square_smash_door(struct chunk *c, struct loc grid)
{
	square_remove_all_traps(c, grid);
	square_set_feat(c, grid, FEAT_BROKEN);
}

void square_unlock_door(struct chunk *c, struct loc grid) {
	assert(square_islockeddoor(c, grid));
	square_set_door_lock(c, grid, 0);
}

void square_destroy_door(struct chunk *c, struct loc grid) {
	assert(square_isdoor(c, grid));
	square_remove_all_traps(c, grid);
	square_set_feat(c, grid, FEAT_FLOOR);
}

void square_destroy_trap(struct chunk *c, struct loc grid)
{
	square_remove_all_traps(c, grid);
}

void square_tunnel_wall(struct chunk *c, struct loc grid)
{
	if (square_isrubble(c, grid)) {
		square_set_feat(c, grid, FEAT_FLOOR);
	} else if (square_issecretdoor(c, grid)) {
		square_set_feat(c, grid, FEAT_CLOSED);
	} else {
		square_set_feat(c, grid, FEAT_RUBBLE);
	}
}

void square_destroy_wall(struct chunk *c, struct loc grid)
{
	square_set_feat(c, grid, FEAT_FLOOR);
}

void square_smash_wall(struct chunk *c, struct loc grid)
{
	int i;
	square_set_feat(c, grid, FEAT_FLOOR);

	for (i = 0; i < 8; i++) {
		/* Extract adjacent location */
		struct loc adj_grid = loc_sum(grid, ddgrid_ddd[i]);

		/* Check legality */
		if (!square_in_bounds_fully(c, adj_grid)) continue;

		/* Ignore permanent grids */
		if (square_isperm(c, adj_grid)) continue;

		/* Ignore floors */
		if (square_isfloor(c, adj_grid)) {
			continue;
		}

		/* Give this grid a chance to survive */
		if ((square_isgranite(c, adj_grid) && one_in_(4)) ||
			(square_isquartz(c, adj_grid) && one_in_(10))) {
			continue;
		}

		/* Remove it */
		square_set_feat(c, adj_grid, FEAT_FLOOR);
	}
}

void square_destroy_rubble(struct chunk *c, struct loc grid) {
	assert(square_isrubble(c, grid));
	square_set_feat(c, grid, FEAT_FLOOR);
}

void square_force_floor(struct chunk *c, struct loc grid) {
	square_set_feat(c, grid, FEAT_FLOOR);
}

int square_forge_bonus(struct chunk *c, struct loc grid) {
	if (square_isforge(c, grid))
		return f_info[square(c, grid)->feat].forge_bonus;
	return 0;
}

int square_digging(struct chunk *c, struct loc grid) {
	if (square_isdiggable(c, grid))
		return f_info[square(c, grid)->feat].dig;
	return 0;
}

int square_pit_difficulty(struct chunk *c, struct loc grid) {
	if (square_ispit(c, grid))
		return f_info[square(c, grid)->feat].pit_difficulty;
	return 0;
}

/**
 * Return the name for the terrain in a grid.  Accounts for the fact that
 * some terrain mimics another terrain.
 *
 * \param c Is the chunk to use.  Usually it is the player's version of the
 * chunk.
 * \param grid Is the grid to use.
 * \param name Is the buffer to hold the returned name.
 * \param size Is the maximum number of characters name can hold.
 */
void square_apparent_name(struct chunk *c, struct loc grid, char *name,
						  int size)
{
	int actual = square_isknown(c, grid) ? square(c, grid)->feat : 0;
	const struct feature *fp = f_info[actual].mimic ?
		f_info[actual].mimic : &f_info[actual];
	char forge_string[40];

	/* Handle forges */
	if (square_isforge(c, grid)) {
		/* Use actual forge uses */
		int uses = square_forge_uses(cave, grid);
		if (!uses) {
			my_strcpy(forge_string, " (exhausted)", sizeof(forge_string));
		} else if (uses == 1) {
			my_strcpy(forge_string, " (1 use remaining)", sizeof(forge_string));
		} else {
			strnfmt(forge_string, sizeof(forge_string), " (%d uses remaining)",
					uses);
		}
	} else {
		my_strcpy(forge_string, "", sizeof(forge_string));
	}

	strnfmt(name, size, "%s%s", fp->name, forge_string);
}

/**
 * Return the prefix, appropriate for describing looking at the grid in
 * question, for the name returned by square_name().
 *
 * \param c Is the chunk to use.  Usually it is the player's version of the
 * chunk.
 * \param grid Is the grid to use.
 *
 * The prefix is usually an indefinite article.  It may be an empty string.
 */
const char *square_apparent_look_prefix(struct chunk *c, struct loc grid) {
	int actual = square_isknown(c, grid) ? square(c, grid)->feat : 0;
	const struct feature *fp = f_info[actual].mimic ?
		f_info[actual].mimic : &f_info[actual];
	return (fp->look_prefix) ? fp->look_prefix :
		(is_a_vowel(fp->name[0]) ? "an " : "a ");
}

/**
 * Return a preposition, appropriate for describing the grid the viewer is on,
 * for the name returned by square_name().  May return an empty string when
 * the name doesn't require a preposition.
 *
 * \param c Is the chunk to use.  Usually it is the player's version of the
 * chunk.
 * \param grid Is the grid to use.
 */
const char *square_apparent_look_in_preposition(struct chunk *c, struct loc grid) {
	int actual = square(c, grid)->feat;
	const struct feature *fp = f_info[actual].mimic ?
		f_info[actual].mimic : &f_info[actual];
	return (fp->look_in_preposition) ?  fp->look_in_preposition : "on ";
}

/* Memorize the terrain */
void square_memorize(struct chunk *c, struct loc grid) {
	if (c != cave) return;
	square_set_known_feat(c, grid, square(c, grid)->feat);
}

/* Forget the terrain */
void square_forget(struct chunk *c, struct loc grid) {
	if (c != cave) return;
	square_set_known_feat(c, grid, FEAT_NONE);
}

void square_mark(struct chunk *c, struct loc grid) {
	sqinfo_on(square(c, grid)->info, SQUARE_MARK);
}

void square_unmark(struct chunk *c, struct loc grid) {
	sqinfo_off(square(c, grid)->info, SQUARE_MARK);
}
