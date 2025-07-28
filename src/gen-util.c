/**
 * \file gen-util.c
 * \brief Dungeon generation utilities
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
 * This file contains various utility functions for dungeon generation - mostly
 * for finding appropriate grids for some purposes, or placing things. 
 */

#include "angband.h"
#include "cave.h"
#include "datafile.h"
#include "effects.h"
#include "game-event.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-spell.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-util.h"
#include "trap.h"
#include "z-queue.h"
#include "z-type.h"


/**
 * Accept values for y and x (considered as the endpoints of lines) between
 * 0 and 40, and return an angle in degrees (divided by two).  -LM-
 *
 * This table's input and output need some processing:
 *
 * Because this table gives degrees for a whole circle, up to radius 20, its
 * origin is at (x,y) = (20, 20).  Therefore, the input code needs to find
 * the origin grid (where the lines being compared come from), and then map
 * it to table grid 20,20.  Do not, however, actually try to compare the
 * angle of a line that begins and ends at the origin with any other line -
 * it is impossible mathematically, and the table will return the value "255".
 *
 * The output of this table also needs to be massaged, in order to avoid the
 * discontinuity at 0/180 degrees.  This can be done by:
 *   rotate = 90 - first value
 *   this rotates the first input to the 90 degree line)
 *   tmp = ABS(second value + rotate) % 180
 *   diff = ABS(90 - tmp) = the angular difference (divided by two) between
 *   the first and second values.
 *
 * Note that grids diagonal to the origin have unique angles.
 */
uint8_t get_angle_to_grid[41][41] =
{
  {  68,  67,  66,  65,  64,  63,  62,  62,  60,  59,  58,  57,  56,  55,  53,  52,  51,  49,  48,  46,  45,  44,  42,  41,  39,  38,  37,  35,  34,  33,  32,  31,  30,  28,  28,  27,  26,  25,  24,  24,  23 },
  {  69,  68,  67,  66,  65,  64,  63,  62,  61,  60,  59,  58,  56,  55,  54,  52,  51,  49,  48,  47,  45,  43,  42,  41,  39,  38,  36,  35,  34,  32,  31,  30,  29,  28,  27,  26,  25,  24,  24,  23,  22 },
  {  69,  69,  68,  67,  66,  65,  64,  63,  62,  61,  60,  58,  57,  56,  54,  53,  51,  50,  48,  47,  45,  43,  42,  40,  39,  37,  36,  34,  33,  32,  30,  29,  28,  27,  26,  25,  24,  24,  23,  22,  21 },
  {  70,  69,  69,  68,  67,  66,  65,  64,  63,  61,  60,  59,  58,  56,  55,  53,  52,  50,  48,  47,  45,  43,  42,  40,  38,  37,  35,  34,  32,  31,  30,  29,  27,  26,  25,  24,  24,  23,  22,  21,  20 },
  {  71,  70,  69,  69,  68,  67,  66,  65,  63,  62,  61,  60,  58,  57,  55,  54,  52,  50,  49,  47,  45,  43,  41,  40,  38,  36,  35,  33,  32,  30,  29,  28,  27,  25,  24,  24,  23,  22,  21,  20,  19 },
  {  72,  71,  70,  69,  69,  68,  67,  65,  64,  63,  62,  60,  59,  58,  56,  54,  52,  51,  49,  47,  45,  43,  41,  39,  38,  36,  34,  32,  31,  30,  28,  27,  26,  25,  24,  23,  22,  21,  20,  19,  18 },
  {  73,  72,  71,  70,  69,  69,  68,  66,  65,  64,  63,  61,  60,  58,  57,  55,  53,  51,  49,  47,  45,  43,  41,  39,  37,  35,  33,  32,  30,  29,  27,  26,  25,  24,  23,  22,  21,  20,  19,  18,  17 },
  {  73,  73,  72,  71,  70,  70,  69,  68,  66,  65,  64,  62,  61,  59,  57,  56,  54,  51,  49,  47,  45,  43,  41,  39,  36,  34,  33,  31,  29,  28,  26,  25,  24,  23,  21,  20,  20,  19,  18,  17,  17 },
  {  75,  74,  73,  72,  72,  71,  70,  69,  68,  66,  65,  63,  62,  60,  58,  56,  54,  52,  50,  47,  45,  43,  40,  38,  36,  34,  32,  30,  28,  27,  25,  24,  23,  21,  20,  19,  18,  18,  17,  16,  15 },
  {  76,  75,  74,  74,  73,  72,  71,  70,  69,  68,  66,  65,  63,  61,  59,  57,  55,  53,  50,  48,  45,  42,  40,  37,  35,  33,  31,  29,  27,  25,  24,  23,  21,  20,  19,  18,  17,  16,  16,  15,  14 },
  {  77,  76,  75,  75,  74,  73,  72,  71,  70,  69,  68,  66,  64,  62,  60,  58,  56,  53,  51,  48,  45,  42,  39,  37,  34,  32,  30,  28,  26,  24,  23,  21,  20,  19,  18,  17,  16,  15,  15,  14,  13 },
  {  78,  77,  77,  76,  75,  75,  74,  73,  72,  70,  69,  68,  66,  64,  62,  60,  57,  54,  51,  48,  45,  42,  39,  36,  33,  30,  28,  26,  24,  23,  21,  20,  18,  17,  16,  15,  15,  14,  13,  13,  12 },
  {  79,  79,  78,  77,  77,  76,  75,  74,  73,  72,  71,  69,  68,  66,  63,  61,  58,  55,  52,  49,  45,  41,  38,  35,  32,  29,  27,  24,  23,  21,  19,  18,  17,  16,  15,  14,  13,  13,  12,  11,  11 },
  {  80,  80,  79,  79,  78,  77,  77,  76,  75,  74,  73,  71,  69,  68,  65,  63,  60,  57,  53,  49,  45,  41,  37,  33,  30,  27,  25,  23,  21,  19,  17,  16,  15,  14,  13,  13,  12,  11,  11,  10,  10 },
  {  82,  81,  81,  80,  80,  79,  78,  78,  77,  76,  75,  73,  72,  70,  68,  65,  62,  58,  54,  50,  45,  40,  36,  32,  28,  25,  23,  20,  18,  17,  15,  14,  13,  12,  12,  11,  10,  10,   9,   9,   8 },
  {  83,  83,  82,  82,  81,  81,  80,  79,  79,  78,  77,  75,  74,  72,  70,  68,  64,  60,  56,  51,  45,  39,  34,  30,  26,  23,  20,  18,  16,  15,  13,  12,  11,  11,  10,   9,   9,   8,   8,   7,   7 },
  {  84,  84,  84,  83,  83,  83,  82,  81,  81,  80,  79,  78,  77,  75,  73,  71,  68,  63,  58,  52,  45,  38,  32,  27,  23,  19,  17,  15,  13,  12,  11,  10,   9,   9,   8,   7,   7,   7,   6,   6,   6 },
  {  86,  86,  85,  85,  85,  84,  84,  84,  83,  82,  82,  81,  80,  78,  77,  75,  72,  68,  62,  54,  45,  36,  28,  23,  18,  15,  13,  12,  10,   9,   8,   8,   7,   6,   6,   6,   5,   5,   5,   4,   4 },
  {  87,  87,  87,  87,  86,  86,  86,  86,  85,  85,  84,  84,  83,  82,  81,  79,  77,  73,  68,  58,  45,  32,  23,  17,  13,  11,   9,   8,   7,   6,   6,   5,   5,   4,   4,   4,   4,   3,   3,   3,   3 },
  {  89,  88,  88,  88,  88,  88,  88,  88,  88,  87,  87,  87,  86,  86,  85,  84,  83,  81,  77,  68,  45,  23,  13,   9,   7,   6,   5,   4,   4,   3,   3,   3,   2,   2,   2,   2,   2,   2,   2,   2,   1 },
  {  90,  90,  90,  90,  90,  90,  90,  90,  90,  90,  90,  90,  90,  90,  90,  90,  90,  90,  90,  90, 255,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
  {  91,  92,  92,  92,  92,  92,  92,  92,  92,  93,  93,  93,  94,  94,  95,  96,  97,  99, 103, 113, 135, 158, 167, 171, 173, 174, 175, 176, 176, 177, 177, 177, 178, 178, 178, 178, 178, 178, 178, 178, 179 },
  {  93,  93,  93,  93,  94,  94,  94,  94,  95,  95,  96,  96,  97,  98,  99, 101, 103, 107, 113, 122, 135, 148, 158, 163, 167, 169, 171, 172, 173, 174, 174, 175, 175, 176, 176, 176, 176, 177, 177, 177, 177 },
  {  94,  94,  95,  95,  95,  96,  96,  96,  97,  98,  98,  99, 100, 102, 103, 105, 108, 113, 118, 126, 135, 144, 152, 158, 162, 165, 167, 168, 170, 171, 172, 172, 173, 174, 174, 174, 175, 175, 175, 176, 176 },
  {  96,  96,  96,  97,  97,  97,  98,  99,  99, 100, 101, 102, 103, 105, 107, 109, 113, 117, 122, 128, 135, 142, 148, 153, 158, 161, 163, 165, 167, 168, 169, 170, 171, 171, 172, 173, 173, 173, 174, 174, 174 },
  {  97,  97,  98,  98,  99,  99, 100, 101, 101, 102, 103, 105, 106, 108, 110, 113, 116, 120, 124, 129, 135, 141, 146, 150, 154, 158, 160, 162, 164, 165, 167, 168, 169, 169, 170, 171, 171, 172, 172, 173, 173 },
  {  98,  99,  99, 100, 100, 101, 102, 102, 103, 104, 105, 107, 108, 110, 113, 115, 118, 122, 126, 130, 135, 140, 144, 148, 152, 155, 158, 160, 162, 163, 165, 166, 167, 168, 168, 169, 170, 170, 171, 171, 172 },
  { 100, 100, 101, 101, 102, 103, 103, 104, 105, 106, 107, 109, 111, 113, 115, 117, 120, 123, 127, 131, 135, 139, 143, 147, 150, 153, 155, 158, 159, 161, 163, 164, 165, 166, 167, 167, 168, 169, 169, 170, 170 },
  { 101, 101, 102, 103, 103, 104, 105, 106, 107, 108, 109, 111, 113, 114, 117, 119, 122, 125, 128, 131, 135, 139, 142, 145, 148, 151, 153, 156, 158, 159, 161, 162, 163, 164, 165, 166, 167, 167, 168, 169, 169 },
  { 102, 103, 103, 104, 105, 105, 106, 107, 108, 110, 111, 113, 114, 116, 118, 120, 123, 126, 129, 132, 135, 138, 141, 144, 147, 150, 152, 154, 156, 158, 159, 160, 162, 163, 164, 165, 165, 166, 167, 167, 168 },
  { 103, 104, 105, 105, 106, 107, 108, 109, 110, 111, 113, 114, 116, 118, 120, 122, 124, 127, 129, 132, 135, 138, 141, 143, 146, 148, 150, 152, 154, 156, 158, 159, 160, 161, 162, 163, 164, 165, 165, 166, 167 },
  { 104, 105, 106, 106, 107, 108, 109, 110, 111, 113, 114, 115, 117, 119, 121, 123, 125, 127, 130, 132, 135, 138, 140, 143, 145, 147, 149, 151, 153, 155, 156, 158, 159, 160, 161, 162, 163, 164, 164, 165, 166 },
  { 105, 106, 107, 108, 108, 109, 110, 111, 113, 114, 115, 117, 118, 120, 122, 124, 126, 128, 130, 133, 135, 137, 140, 142, 144, 146, 148, 150, 152, 153, 155, 156, 158, 159, 160, 161, 162, 162, 163, 164, 165 },
  { 107, 107, 108, 109, 110, 110, 111, 113, 114, 115, 116, 118, 119, 121, 123, 124, 126, 129, 131, 133, 135, 137, 139, 141, 144, 146, 147, 149, 151, 152, 154, 155, 156, 158, 159, 160, 160, 161, 162, 163, 163 },
  { 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 119, 120, 122, 123, 125, 127, 129, 131, 133, 135, 137, 139, 141, 143, 145, 147, 148, 150, 151, 153, 154, 155, 156, 158, 159, 159, 160, 161, 162, 163 },
  { 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 120, 121, 122, 124, 126, 128, 129, 131, 133, 135, 137, 139, 141, 142, 144, 146, 148, 149, 150, 152, 153, 154, 155, 157, 158, 159, 159, 160, 161, 162 },
  { 109, 110, 111, 112, 113, 114, 114, 115, 117, 118, 119, 120, 122, 123, 125, 126, 128, 130, 131, 133, 135, 137, 139, 140, 142, 144, 145, 147, 148, 150, 151, 152, 153, 155, 156, 157, 158, 159, 159, 160, 161 },
  { 110, 111, 112, 113, 114, 114, 115, 116, 117, 119, 120, 121, 122, 124, 125, 127, 128, 130, 132, 133, 135, 137, 138, 140, 142, 143, 145, 146, 148, 149, 150, 151, 153, 154, 155, 156, 157, 158, 159, 159, 160 },
  { 111, 112, 113, 114, 114, 115, 116, 117, 118, 119, 120, 122, 123, 124, 126, 127, 129, 130, 132, 133, 135, 137, 138, 140, 141, 143, 144, 146, 147, 148, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 159 },
  { 112, 113, 114, 114, 115, 116, 117, 118, 119, 120, 121, 122, 124, 125, 126, 128, 129, 131, 132, 133, 135, 137, 138, 139, 141, 142, 144, 145, 146, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159 },
  { 113, 114, 114, 115, 116, 117, 118, 118, 120, 121, 122, 123, 124, 125, 127, 128, 129, 131, 132, 134, 135, 136, 138, 139, 141, 142, 143, 145, 146, 147, 148, 149, 150, 152, 152, 153, 154, 155, 156, 157, 158 }
};


/**
 * Set up to locate a square in a rectangular region of a chunk.
 *
 * \param top_left is the upper left corner of the rectangle to be searched.
 * \param bottom_right is the lower right corner of the rectangle to be
 * searched.
 * \return the state for the search.  When no longer needed, the returned
 * value should be passed to mem_free().
 */
int *cave_find_init(struct loc top_left, struct loc bottom_right)
{
	struct loc diff = loc_diff(bottom_right, top_left);
	int n = (diff.y < 0 || diff.x < 0) ? 0 : (diff.x + 1) * (diff.y + 1);
	int *state = mem_alloc((5 + n) * sizeof(*state));
	int i;

	state[0] = n;
	state[1] = diff.x + 1;
	state[2] = top_left.x;
	state[3] = top_left.y;
	/* The next to search is the first one. */
	state[4] = 0;
	/*
	 * Set up for left to right, top to bottom, search; will randomize in
	 * cave_find_get_grid().
	 */
	for (i = 5; i < 5 + n; ++i) {
		state[i] = i - 5;
	}
	return state;
}


/**
 * Reset a search created by cave_find_init() to start again from fresh.
 *
 * \param state is the search state created by cave_find_init().
 */
void cave_find_reset(int *state)
{
	/* The next to search is the first one. */
	state[4] = 0;
}

/**
 * Get the next grid for a search created by cave_find_init().
 *
 * \param grid is dereferenced and set to the grid to check.
 * \param state is the search state created by cave_find_init().
 * \return true if grid was dereferenced and set to the next grid to be
 * searched; otherwise return false to indicate that there are no more grids
 * available.
 */
bool cave_find_get_grid(struct loc *grid, int *state)
{
	int j, k;

	assert(state[4] >= 0);
	if (state[4] >= state[0]) return false;

	/*
	 * Choose one of the remaining ones at random.  Swap it with the one
	 * that's next in order.
	 */
	j = randint0(state[0] - state[4]) + state[4];
	k = state[5 + j];
	state[5 + j] = state[5 + state[4]];
	state[5 + state[4]] = k;

	grid->y = (k / state[1]) + state[3];
	grid->x = (k % state[1]) + state[2];

	/*
	 * Increment so a future call to cave_find_get_grid() will get the
	 * next one.
	 */
	++state[4];
	return true;
}


/**
 * Locate a square in a rectangle which satisfies the given predicate.
 *
 * \param c current chunk
 * \param grid found grid
 * \param top_left top left grid of rectangle
 * \param bottom_right bottom right grid of rectangle
 * \param pred square_predicate specifying what we're looking for
 * \return success
 */
bool cave_find_in_range(struct chunk *c, struct loc *grid,
		struct loc top_left, struct loc bottom_right,
		square_predicate pred)
{
	int *state = cave_find_init(top_left, bottom_right);
	bool found = false;

	while (!found && cave_find_get_grid(grid, state)) {
		found = pred(c, *grid);
	}
	mem_free(state);
	return found;
}


/**
 * Locate a square in the dungeon which satisfies the given predicate.
 * \param c current chunk
 * \param grid found grid
 * \param pred square_predicate specifying what we're looking for
 * \return success
 */
bool cave_find(struct chunk *c, struct loc *grid, square_predicate pred)
{
	struct loc top_left = loc(0, 0);
	struct loc bottom_right = loc(c->width - 1, c->height - 1);
	return cave_find_in_range(c, grid, top_left, bottom_right, pred);
}


/**
 * Locate an empty square for 0 <= y < ymax, 0 <= x < xmax.
 * \param c current chunk
 * \param grid found grid
 * \return success
 */
bool find_empty(struct chunk *c, struct loc *grid)
{
	return cave_find(c, grid, square_isempty);
}


/**
 * Locate an empty square in a given rectangle.
 * \param c current chunk
 * \param grid found grid
 * \param top_left top left grid of rectangle
 * \param bottom_right bottom right grid of rectangle
 * \return success
 */
bool find_empty_range(struct chunk *c, struct loc *grid, struct loc top_left,
	struct loc bottom_right)
{
	return cave_find_in_range(c, grid, top_left, bottom_right,
		square_isempty);
}


/**
 * Locate a grid within +/- yd, xd of a centre.
 * \param c current chunk
 * \param grid found grid
 * \param centre starting grid
 * \param yd y-range
 * \param xd x-range
 * \return success
 */
bool find_nearby_grid(struct chunk *c, struct loc *grid, struct loc centre,
	int yd, int xd)
{
	struct loc top_left = loc(centre.x - xd, centre.y - yd);
	struct loc bottom_right = loc(centre.x + xd, centre.y + yd);
	return cave_find_in_range(c, grid, top_left, bottom_right,
		square_in_bounds_fully);
}


/**
 * Place rubble at a given location, provided we are deep enough.
 * \param c current chunk
 * \param grid location
 */
static void place_rubble(struct chunk *c, struct loc grid)
{
	if (c->depth >= 4) {
		square_set_feat(c, grid, FEAT_RUBBLE);
	}
}


/**
 * Choose either an ordinary up staircase or an up shaft.
 */
static int choose_up_stairs(struct chunk *c)
{
	if (c->depth >= 2) {
		if (one_in_(2)) return FEAT_LESS_SHAFT;
	}
	return FEAT_LESS;
}

/**
 * Choose either an ordinary down staircase or an down shaft.
 */
static int choose_down_stairs(struct chunk *c)
{
	if (c->depth < z_info->dun_depth - 2) {
		if (one_in_(2)) return FEAT_MORE_SHAFT;
	}
	return FEAT_MORE;
}

/**
 * Place stairs (of the requested type 'feat' if allowed) at a given location.
 *
 * \param c current chunk
 * \param grid location
 * \param first is whether or not this is the first stair on the level.
 * \param feat stair terrain type
 *
 * All stairs from the surface go down. All stairs on the bottom level go up.
 */
static void place_stairs(struct chunk *c, struct loc grid, bool first, int feat)
{
	if (!c->depth) {
		/* Surface -- must go down */
		square_set_feat(c, grid, FEAT_MORE);
	} else if (c->depth >= z_info->dun_depth) {
		/* Bottom -- must go up */
		if (first) {
			square_set_feat(c, grid, FEAT_LESS);
		} else {
			square_set_feat(c, grid, choose_up_stairs(c));
		}
	} else {
		/* Allow shafts, but guarantee the first one is an ordinary stair */
		if (!first) {
			if (feat == FEAT_LESS) {
				feat = choose_up_stairs(c);
			} else if (feat == FEAT_MORE) {
				feat = choose_down_stairs(c);
			}
		}
		square_set_feat(c, grid, feat);
	}
}


/**
 * Generate the chosen item at a random spot near the player.
 */
void place_item_near_player(struct chunk *c, struct player *p, int tval,
							const char *name)
{
	struct loc grid;
	int count = 100;
	struct object *obj;
	struct object_kind *kind;

	/* Find a possible place */
	while (find_nearby_grid(c, &grid, p->grid, 5, 5) && count--) {
		/* Must be empty, in a room and in view of the player */
		if (square_isempty(c, grid) && square_isroom(c, grid) &&
			los(c, p->grid, grid)) {
			break;
		}
	}

	/* Get local object */
	obj = object_new();

	/* Get the object_kind */
	kind = lookup_kind(tval, lookup_sval(tval, name));

	/* Valid item? */
	if (!kind) return;

	/* Prepare the item */
	object_prep(obj, kind, c->depth, RANDOMISE);

	if (tval == TV_ARROW) {
		obj->number = 24;
	} else {
		obj->number = 1;
	}
	drop_near(c, &obj, 0, grid, false, false);	
}

/**
 * Place a random object at a given location.
 * \param c current chunk
 * \param grid location
 * \param level generation depth
 * \param good is it a good object?
 * \param great is it a great object?
 * \param origin item origin
 * \param drop constrains the type of object created or may be NULL to no
 * constraint on the object type
 */
void place_object(struct chunk *c, struct loc grid, int level, bool good,
		bool great, uint8_t origin, struct drop *drop)
{
	struct object *new_obj;
	bool dummy = true;

	if (!square_in_bounds(c, grid)) return;
	if (!square_canputitem(c, grid)) return;

	/* Make an appropriate object */
	new_obj = make_object(c, level, good, great, drop);
	if (!new_obj) return;
	new_obj->origin = origin;
	new_obj->origin_depth = convert_depth_to_origin(c->depth);

	/* Give it to the floor */
	if (!floor_carry(c, grid, new_obj, &dummy)) {
		if (new_obj->artifact) {
			mark_artifact_created(new_obj->artifact, false);
		}
		object_delete(c, NULL, &new_obj);
		return;
	} else {
		list_object(c, new_obj);
	}
}


/**
 * Place a secret door at a given location.
 * \param c current chunk
 * \param grid location
 */
void place_secret_door(struct chunk *c, struct loc grid)
{
	square_set_feat(c, grid, FEAT_SECRET);
}


/**
 * Place a closed (and possibly locked or jammed) door at a given location.
 * \param c current chunk
 * \param grid location
 */
void place_closed_door(struct chunk *c, struct loc grid)
{
	int value = randint0(100);
	square_set_feat(c, grid, FEAT_CLOSED);
	if (square_isvault(c, grid)) {
		int power = (10 + c->depth + randint1(15)) / 5;
		power = MIN(7, power);
		if (value < 4) {
			/* Locked doors (8%) */
			square_set_door_lock(c, grid, power);
		} else if (value < 8) {
			/* Jammed doors (4%) */
			square_set_door_jam(c, grid, power);
		}
	} else {
		int power = (c->depth + randint1(15)) / 5;
		power = MIN(7, power);
		if (value < 24) {
			/* Locked doors (24%) */
			square_set_door_lock(c, grid, power);
		} else if (value < 25) {
			/* Jammed doors (1%) */
			square_set_door_jam(c, grid, power);
		}
	}
}

/**
 * Place a random door at a given location.
 * \param c current chunk
 * \param grid location
 *
 * The door generated could be closed (and possibly locked), open, or secret.
 */
void place_random_door(struct chunk *c, struct loc grid)
{
	int tmp = randint0(60 + c->depth);

	if (tmp < 20) {
		square_set_feat(c, grid, FEAT_OPEN);
	} else if (tmp < 60) {
		place_closed_door(c, grid);
	} else {
		place_secret_door(c, grid);
	}
}

/**
 * Place a forge at a given location.
 * \param c current chunk
 * \param grid location
 */
void place_forge(struct chunk *c, struct loc grid)
{
	int i;
	int effective_depth = c->depth;
	int power = 1;
	int uses = damroll(2, 2);

	if (square_isgreatervault(c, grid)) {
		effective_depth *= 2;
	}

	/* Roll once per level of depth and keep the best roll */
	for (i = 0; i < effective_depth; i++) {
		int p = randint1(1000);
		power = MAX(power, p);
	}

	/* To prevent start-scumming on the initial forge */
	if (c->depth <= 2) {
		uses = 3;
		power = 0;
	}

	/* Pick the forge type */
	if ((power >= 1000) && !player->unique_forge_made) {
		/* Unique forge */
		uses = 3;
		square_set_feat(c, grid, FEAT_FORGE_UNIQUE);
		square_set_forge(c, grid, uses);
		player->unique_forge_made = true;
		if (OPT(player, cheat_room)) msg("Orodruth.");
	} else if (power >= 990) {
		/* Enchanted forge */
		square_set_feat(c, grid, FEAT_FORGE_GOOD);
		square_set_forge(c, grid, uses);
		if (OPT(player, cheat_room)) msg("Enchanted forge.");
	} else {
		/* Normal forge */
		square_set_feat(c, grid, FEAT_FORGE);
		square_set_forge(c, grid, uses);
		if (OPT(player, cheat_room)) msg("Forge.");
	}
}

/**
 * Place some staircases near walls.
 * \param c the current chunk
 * \param feat the stair terrain type
 * \param num number of staircases to place
 */
void alloc_stairs(struct chunk *c, int feat, int num)
{
	int i = 0;

	/* Smaller levels don't need that many stairs, but there are a minimum of
	 * 4 rooms*/
	if (dun->cent_n == 4) {
		num = 1;
	} else if (num > (dun->cent_n / 2)) {
		num = dun->cent_n / 2;
	}

	/* Place "num" stairs */
	while (i < num) {
		struct loc grid;
		bool first = (i == 0);

		/* Find a suitable grid */
		cave_find(c, &grid, square_suits_stairs);
		place_stairs(c, grid, first, feat);
		assert(square_isstairs(c, grid) || (!first && square_isshaft(c, grid)));
		++i;
	}
}

/**
 * Are there any stairs within line of sight?
 */
static bool stairs_within_los(struct chunk *c, struct loc grid0)
{
	int y, x;

	/* Scan the visible area */
	for (y = grid0.y - 15; y < grid0.y + 15; y++) {
		for (x = grid0.x - 15; x < grid0.x + 15; x++) {
			struct loc grid = loc(x, y);
			if (!square_in_bounds_fully(c, grid)) continue;
			if (!los(c, grid0, grid)) continue;

			/* Detect stairs */
			if (square_isstairs(c, grid)) {
				return true;
			}
		}
	}

	return false;
}

/**
 * Locate a valid starting point for the player in a chunk
 * \param c is the chunk of interest
 * \param grid is, when the search is successful, dereferenced and set to the
 * coordinates of the starting location
 * \return success
 */
static bool find_start(struct chunk *c, struct loc *grid)
{
	int *state = cave_find_init(loc(1, 1), loc(c->width - 2, c->height - 2));
	bool found = false;
	int count = 100;

	/* Find the best possible place */
	while (!found && cave_find_get_grid(grid, state) && count--) {
		/* Require empty square that isn't in an interesting room or vault */
		found = square_suits_start(c, *grid);

		/* Require a room if it is the first level */
		if ((player->turn == 0) && !square_isroom(c, *grid)) found = false;

		/* Don't generate stairs in line of sight if player arrived by stairs */
		if (stairs_within_los(c, *grid) && player->upkeep->create_stair) {
			found = false;
		}
	}

	mem_free(state);

	return found;
}


/**
 * Place the player at a random starting location.
 * \param c current chunk
 * \param p the player
 * \return true on success or false on failure
 */
bool new_player_spot(struct chunk *c, struct player *p)
{
	struct loc grid;

	/* Try to find a good place to put the player */
	if (!find_start(c, &grid)) {
		msg("Failed to place player; please report.  Restarting generation.");
		dump_level_simple(NULL, "Player Placement Failure", c);
		return false;
	}

	/* Destroy area if falling due to blasting through the floor */
    if (p->upkeep->create_stair == FEAT_RUBBLE) {
		effect_simple(EF_EARTHQUAKE, source_grid(grid), "0", 0, 5, 0, NULL);
	}

	if (p->upkeep->create_stair && square_changeable(c, grid)) {
		object_pile_free(c, NULL, square_object(c, grid));
		square_set_feat(c, grid, p->upkeep->create_stair);
	}
	player_place(c, p, grid);
	return true;
}


/**
 * Determines the chance (out of 1000) that a trap will be placed in a given
 * square.
 */
int trap_placement_chance(struct chunk *c, struct loc grid)
{
    int chance = 0;
    
    /* Extra chance of having a trap for certain squares inside rooms */
    if (square_isfloor(c, grid) && square_isroom(c, grid) &&
		!square_object(c, grid)) {
		int y, x, d;

        chance = 1;

        /* Check the squares that neighbour grid */
        for (y = grid.y - 1; y <= grid.y + 1; y++) {
            for (x = grid.x - 1; x <= grid.x + 1; x++) {
				struct loc check = loc(x, y);
				if (loc_eq(grid, check)) continue;

				/* Item */
				if (square_object(c, check)) chance += 10;

				/* Stairs */
				if (square_isstairs(c, check)) chance += 10;

				/* closed doors (including secret) */
				if (square_iscloseddoor(c, check)) chance += 10;
			}
		}

        /* Opposing impassable squares (chasm or wall) */
		for (d = 0; d < 4; d += 2) {
			struct loc adj1 = loc_sum(grid, ddgrid_ddd[d]);
			struct loc adj2 = loc_sum(grid, ddgrid_ddd[d + 1]);
			if (square_isimpassable(c, adj1) && square_isimpassable(c, adj2)) {
				chance += 10;
			}
		}
    }

    return chance;
}


/**
 * Place traps randomly on the level.
 * Biased towards certain sneaky locations.
 */
void place_traps(struct chunk *c)
{
    struct loc grid;

	/* scan the map */
	for (grid.y = 0; grid.y < c->height; grid.y++) {
		for (grid.x = 0; grid.x < c->width; grid.x++) {
            /* Randomly determine whether to place a trap based on the above */
            if (randint1(1000) <= trap_placement_chance(c, grid)) {
                square_add_trap(c, grid);
            }
		}
	}
}

/**
 * Allocates zero or more random objects in the dungeon.
 * \param c the current chunk
 * \param set where the entity is placed (corridor, room or either)
 * \param typ what is placed (rubble, trap, gold, item)
 * \param num is the number of objects to allocate
 * \param depth generation depth
 * \param origin item origin (if appropriate)
 * \return the number of objects actually placed
 *
 * 'set' controls where the object is placed (corridor, room, either).
 * 'typ' conrols the kind of object (rubble, trap, gold, item).
 */
int alloc_object(struct chunk *c, int set, int typ, int num, int depth,
						 uint8_t origin)
{
	int nrem = num;
	int *state = cave_find_init(loc(1, 1),
		loc(c->width - 2, c->height - 2));
	struct loc grid;

	while (nrem > 0 && cave_find_get_grid(&grid, state)) {
		/*
		 * If we're ok with a corridor and we're in one, we're done.
		 * If we are ok with a room and we're in one, we're done
		 */
		bool matched = ((set & SET_CORR) && !square_isroom(c, grid))
			|| ((set & SET_ROOM) && square_isroom(c, grid));
		if (square_isempty(c, grid) && matched) {
			/* Place something */
			switch (typ) {
			case TYP_RUBBLE:
				place_rubble(c, grid);
				break;
			case TYP_OBJECT:
				place_object(c, grid, depth, false, false, origin, 0);
				break;
			}
			--nrem;
		}
	}

	mem_free(state);
	return num - nrem;
}

/**
 * Lookup a room profile by name
 */
struct room_profile lookup_room_profile(const char *name)
{
	int num_rooms = dun->profile->n_room_profiles;
	struct room_profile profile;
	int i;

	for (i = 1; i < num_rooms; i++) {
		profile = dun->profile->room_profiles[i];
		if (streq(profile.name, name)) {
			return profile;
		}
	}

	return dun->profile->room_profiles[0];
}

/**
 * Mark artifacts in a failed chunk as not created
 */
void uncreate_artifacts(struct chunk *c)
{
	int y, x;
	
	/* Also mark created artifacts as not created... */
	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			struct loc grid = loc(x, y);
			struct object *obj = square_object(c, grid);
			while (obj) {
				if (obj->artifact) {
					mark_artifact_created(obj->artifact, false);
				}
				obj = obj->next;
			}
		}
	}
}

/**
 * Mark greater vaults in a failed chunk as not created.
 */
void uncreate_greater_vaults(struct chunk *c, struct player *p)
{
	const struct vault *v;

	if (!c->vault_name) return;
	for (v = vaults; v; v = v->next) {
		if (streq(v->typ, "Greater vault")
				&& streq(c->vault_name, v->name)) {
			p->vaults[v->index] = false;
			break;
		}
	}
}

/**
 * Validate that the chunk contains no NULL objects.
 * Only checks for nonzero tval.
 * \param c is the chunk to validate.
 */

void chunk_validate_objects(struct chunk *c) {
	int x, y;
	struct object *obj;

	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			struct loc grid = loc(x, y);
			for (obj = square_object(c, grid); obj; obj = obj->next)
				assert(obj->tval != 0);
			if (square(c, grid)->mon > 0) {
				struct monster *mon = square_monster(c, grid);
				if (mon->held_obj)
					for (obj = mon->held_obj; obj; obj = obj->next)
						assert(obj->tval != 0);
			}
		}
	}
}

	

/**
 * Dump the given level for post-mortem analysis; handle all I/O.
 * \param basefilename Is the base name (no directory or extension) for the
 * file to use.  If NULL, "dumpedlevel" will be used.
 * \param title Is the label to use within the file.  If NULL, "Dumped Level"
 * will be used.
 * \param c Is the chunk to dump.
 */
void dump_level_simple(const char *basefilename, const char *title,
	struct chunk *c)
{
	char path[1024];
	ang_file *fo;

	path_build(path, sizeof(path), ANGBAND_DIR_USER, (basefilename) ?
		format("%s.html", basefilename) : "dumpedlevel.html");
	fo = file_open(path, MODE_WRITE, FTYPE_TEXT);
	if (fo) {
		dump_level(fo, (title) ? title : "Dumped Level", c, NULL);
		if (file_close(fo)) {
			msg("Level dumped to %s.html",
				(basefilename) ? basefilename : "dumpedlevel");
		}
	}
}


/**
 * Dump the given level to a file for post-mortem analysis.
 * \param fo Is the file handle to use.  Must be capable of sequential writes
 * in text format.  The level is dumped starting at the current offset in the
 * file.
 * \param title Is the title to use for the contents.
 * \param c Is the chunk to dump.
 * \param dist If not NULL, must act like a two dimensional C array with the
 * first dimension being at least c->height elements and the second being at
 * least c->width elements.  For a location (x,y) in the level, if dist[y][x]
 * is negative, the contents will be rendered differently.
 *
 * The current output format is HTML since a typical browser will happily
 * display the content in a scrollable area without wrapping lines.  This
 * function is a convenience to replace a set of calls to dump_level_header(),
 * dump_level_body(), and dump_level_footer().
 */
void dump_level(ang_file *fo, const char *title, struct chunk *c, int **dist)
{
	dump_level_header(fo, title);
	dump_level_body(fo, title, c, dist);
	dump_level_footer(fo);
}


/**
 * Helper function to write a string while escaping any special characters.
 * \param fo Is the file handle to use.
 * \param s Is the string to write.
 */
static void dump_level_escaped_string(ang_file *fo, const char *s)
{
	while (*s) {
		switch (*s) {
		case '&':
			file_put(fo, "&amp;");
			break;

		case '<':
			file_put(fo, "&lt;");
			break;

		case '>':
			file_put(fo, "&gt;");
			break;

		case '\"':
			file_put(fo, "&quot;");
			break;

		default:
			file_putf(fo, "%c", *s);
			break;
		}
		++s;
	}
}


/**
 * Write the introductory material for the dump of one or move levels.
 * \param fo Is the file handle to use.  Must be capable of sequential writes
 * in text format.  Writes start at the current offset in the file.
 * \param title Is the title to use for the contents of the file.
 *
 * The current format uses HTML.  This should be called once per dump (or
 * take other measures to overwrite a previous call).
 */
void dump_level_header(ang_file *fo, const char *title)
{
	file_put(fo,
		"<!DOCTYPE html>\n"
		"<html lang=\"en\" xml:lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\">\n"
		"  <head>\n"
		"    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n"
		"    <title>");
	dump_level_escaped_string(fo, title);
	file_put(fo, "</title>\n  </head>\n  <body>\n");
}


/**
 * Dump the given level to a file.
 * \param fo Is the file handle to use.  Must be capable of sequential writes
 * in text format.  The level is dumped starting at the current offset in the
 * file.
 * \param title Is the title to use for the level.
 * \param c Is the chunk to dump.
 * \param dist If not NULL, must act like a two dimensional C array with the
 * first dimension being at least c->height elements and the second being at
 * least c->width elements.  For a location (x,y) in the level, if dist[y][x]
 * is negative, the contents will be rendered differently.
 *
 * The current output format is HTML.  You can dump more than one level to
 * the same file by calling dump_level_header() once for the file, followed
 * by calling dump_level_body() for each level of interest, then calling
 * dump_level_footer() once to finish things off before you close the file
 * with file_close().
 */
void dump_level_body(ang_file *fo, const char *title, struct chunk *c,
	int **dist)
{
	int y;

	file_put(fo, "    <p>");
	dump_level_escaped_string(fo, title);
	if (dist != NULL) {
		file_put(fo, "\n    <p>A location where the distance array was negative is marked with *.");
	}
	file_put(fo, "\n    <pre>\n");
	for (y = 0; y < c->height; ++y) {
		int x;

		for (x = 0; x < c->width; ++x) {
			struct loc grid = loc(x, y);
			const char *s = "#";

			if (square_in_bounds_fully(c, grid)) {
				if (square_isplayer(c, grid)) {
					s = "@";
				} else if (square_isoccupied(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						"M" : "*";
				} else if (square_isdoor(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						"+" : "*";
				} else if (square_isrubble(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						":" : "*";
				} else if (square_isdownstairs(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						"&gt;" : "*";
				} else if (square_isupstairs(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						"&lt;" : "*";
				} else if (square_isforge(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						"0" : "*";
				} else if (square_ischasm(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						"7" : "*";
				} else if (square_istrap(c, grid) ||
					square_isplayertrap(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						"^" : "*";
				} else if (square_iswebbed(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						"w" : "*";
				} else if (square_object(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						"$" : "*";
				} else if (square_isempty(c, grid) &&
						square_isvault(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						" " : "*";
				} else if (square_ispassable(c, grid)) {
					s = (dist == NULL || dist[y][x] >= 0) ?
						"." : "*";
				}
			}
			file_put(fo, s);
		}
		file_put(fo, "\n");
	}
	file_put(fo, "    </pre>\n");
}


/**
 * Write the concluding material for the dump of one or more levels.
 * \param fo Is the file handle to use.  Must be capable of sequential writes
 * in text format.  Writes start at the current offset in the file.
 */
void dump_level_footer(ang_file *fo)
{
	file_put(fo, "  </body>\n</html>\n");
}
