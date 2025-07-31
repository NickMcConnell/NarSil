/**
 * \file project-feat.c
 * \brief projection effects on terrain
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
#include "combat.h"
#include "game-world.h"
#include "generate.h"
#include "obj-ignore.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "project.h"
#include "source.h"
#include "trap.h"


/**
 * ------------------------------------------------------------------------
 * Feature handlers
 * ------------------------------------------------------------------------ */

typedef struct project_feature_handler_context_s {
	const struct source origin;
	const struct loc grid;
	const int dif;
	const int type;
	bool obvious;
} project_feature_handler_context_t;
typedef void (*project_feature_handler_f)(project_feature_handler_context_t *);

static void project_feature_handler_FIRE(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_COLD(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_POIS(project_feature_handler_context_t *context)
{
}

/* Darken the grid */
static void project_feature_handler_DARK(project_feature_handler_context_t *context)
{
	const struct loc grid = context->grid;

	if ((player->depth != 0 || !is_daytime())) {
		/* Turn off the light */
		sqinfo_off(square(cave, grid)->info, SQUARE_GLOW);
	}

	/* Grid is in line of sight */
	if (square_isview(cave, grid)) {
		/* Observe */
		context->obvious = true;

		/* Fully update the visuals */
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	}
}

static void project_feature_handler_NOTHING(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_HURT(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_ARROW(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_BOULDER(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_ACID(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_SOUND(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_FORCE(project_feature_handler_context_t *context)
{
}

/* Light up the grid */
static void project_feature_handler_LIGHT(project_feature_handler_context_t *context)
{
	const struct loc grid = context->grid;

	/* Turn on the light */
	sqinfo_on(square(cave, grid)->info, SQUARE_GLOW);

	/* Grid is in line of sight */
	if (square_isview(cave, grid)) {
		if (!player->timed[TMD_BLIND]) {
			/* Observe */
			context->obvious = true;

			/* Fully update the visuals */
			player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		}
	}
}

/* Destroy walls (and doors) */
static void project_feature_handler_KILL_WALL(project_feature_handler_context_t *context)
{
	const struct loc grid = context->grid;
	bool success;

	/* Non-walls (etc) */
	if (square_ispassable(cave, grid) && !square_seemslikewall(cave, grid))
		return;

	/* Permanent walls */
	if (square_isperm(cave, grid)) return;

	success = skill_check(context->origin, context->dif, 10, source_none()) > 0;

	/* Different treatment for different walls */
	if (square_isrubble(cave, grid)) {
		if (success) {
			/* Message */
			if (square_isseen(cave, grid)) {
				msg("The rubble is blown away!");
				context->obvious = true;

				/* Forget the rubble */
				square_forget(cave, grid);
				square_light_spot(cave, grid);
			}

			/* Destroy the rubble */
			square_destroy_rubble(cave, grid);
		} else if (square_isseen(cave, grid)) {
			/* Message */
			msg("You fail to blow hard enough to smash the rubble.");
		}
	} else if (square_iscloseddoor(cave, grid)) {
		if (success) {
			/* Message */
			if (square_isseen(cave, grid)) {
				msg("The door is blown from its hinges!");
				context->obvious = true;

				/* Forget the door */
				square_forget(cave, grid);
				square_light_spot(cave, grid);
			}

			/* Destroy the door */
			square_destroy_door(cave, grid);
		} else if (square_isseen(cave, grid)) {
			/* Message */
			msg("You fail to blow hard enough to force the door open.");
		}
	} else if (square_isquartz(cave, grid)) {
		if (success) {
			/* Message */
			if (square_isseen(cave, grid)) {
				msg("The vein shatters!");
				context->obvious = true;

				/* Forget the wall */
				square_forget(cave, grid);
				square_light_spot(cave, grid);
			}

			/* Destroy the wall */
			square_set_feat(cave, grid, FEAT_RUBBLE);
		} else if (square_isseen(cave, grid)) {
			/* Message */
			msg("You fail to blow hard enough to shatter the quartz.");
		}
	} else if (square_isgranite(cave, grid)) {
		if (success) {
			/* Message */
			if (square_isseen(cave, grid)) {
				msg("The wall shatters!");
				context->obvious = true;

				/* Forget the wall */
				square_forget(cave, grid);
				square_light_spot(cave, grid);
			}

			/* Destroy the wall */
			square_set_feat(cave, grid, FEAT_RUBBLE);
		} else if (square_isseen(cave, grid)) {
			/* Message */
			msg("You fail to blow hard enough to shatter the wall.");
		}
	}

	/* Update the visuals */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
}

static void project_feature_handler_SLEEP(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_SPEED(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_SLOW(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_CONFUSION(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_FEAR(project_feature_handler_context_t *context)
{
}

static void project_feature_handler_EARTHQUAKE(project_feature_handler_context_t *context)
{
}

/* Darken the grid */
static void project_feature_handler_DARK_WEAK(project_feature_handler_context_t *context)
{
	project_feature_handler_DARK(context);
}

/* Destroy Doors */
static void project_feature_handler_KILL_DOOR(project_feature_handler_context_t *context)
{
	const struct loc grid = context->grid;
	int result = skill_check(context->origin, context->dif, 0, source_none());

	/* Doors */
	if (square_isdoor(cave, grid)) {
		if (result <= 0) {
			/* Do nothing */
		} else if (result <= 5) {
			if (square_islockeddoor(cave, grid)) {
				/* Unlock the door */
				square_unlock_door(cave, grid);
				msg("You hear a 'click'.");
			}
		} else if (result <= 10) {
			if (!square_isopendoor(cave, grid)
					&& !square_isbrokendoor(cave, grid)) {
				/* Open the door */
				square_open_door(cave, grid);
				context->obvious = true;
				/* Message */
				if (square_isseen(cave, grid)) {
					msg("The door flies open.");
				} else {
					msg("You hear a door burst open.");
				}
			}
		} else if (!square_isbrokendoor(cave, grid)) {
			/* Break the door */
			square_smash_door(cave, grid);
			context->obvious = true;
			/* Message */
			if (square_isseen(cave, grid)) {
				msg("The door is ripped from its hinges.");
			} else {
				msg("You hear a door burst open.");
			}
		}
	} else if (square_isrubble(cave, grid)) {
		/* Rubble */
		if (result <= 0) {
			/* Do nothing */
		} else {
			/* Disperse the rubble */
			square_destroy_rubble(cave, grid);
			context->obvious = true;
			/* Message */
			if (square_isseen(cave, grid)) {
				msg("The rubble is scattered across the floor.");
			} else {
				msg("You hear a loud rumbling.");
			}
		}
	}
}

/* Make doors */
static void project_feature_handler_LOCK_DOOR(project_feature_handler_context_t *context)
{
	const struct loc grid = context->grid;
	int power = skill_check(context->origin, context->dif, 0, source_none());

	/* Require a grid without monsters */
	if (square_monster(cave, grid) || square_isplayer(cave, grid)) return;

	/* Broken doors are harder to lock */
	if (square_isbrokendoor(cave, grid)) power -= 10;

	/* Check power */
	if (power <= 0) return;

	/* Require a known door */
	if (!square_isdoor(cave, grid) || square_issecretdoor(cave, grid)) return;

	/* Close the door */
	if (square_isopendoor(cave, grid) || square_isbrokendoor(cave, grid)) {
		square_close_door(cave, grid);
		context->obvious = true;
		if (square_isseen(cave, grid)) {
			msg("The door slams shut.");
		} else {
			msg("You hear a door slam shut.");
		}
	} else {
		/* Or lock the door more firmly than it was before */
		if ((square_door_lock_power(cave, grid) < 7) && (power > 1)) {
			int lock_level = square_door_lock_power(cave, grid) + power / 2;
			square_set_door_lock(cave, grid, MIN(lock_level, 7));
			msg("You hear a 'click'.");
			context->obvious = true;
		}
	}

	/* Update the visuals */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
}

/* Disable traps, unlock doors */
static void project_feature_handler_KILL_TRAP(project_feature_handler_context_t *context)
{
	const struct loc grid = context->grid;

	/* Disable traps, unlock doors */
	if (square_isplayertrap(cave, grid)) {
		/* Check line of sight */
		if (square_isview(cave, grid) && square_isvisibletrap(cave, grid)) {
			context->obvious = true;
		}

		/* Destroy the trap */
		square_destroy_trap(cave, grid);
	}
}

static void project_feature_handler_DISP_ALL(project_feature_handler_context_t *context)
{
}


static const project_feature_handler_f feature_handlers[] = {
	#define ELEM(a) project_feature_handler_##a,
	#include "list-elements.h"
	#undef ELEM
	#define PROJ(a) project_feature_handler_##a,
	#include "list-projections.h"
	#undef PROJ
	NULL
};

/**
 * Called from project() to affect terrain features
 *
 * Called for projections with the PROJECT_GRID flag set, which includes
 * beam, ball and breath effects.
 *
 * \param origin is the origin of the effect
 * \param grid the coordinates of the grid being handled
 * \param dif is the difficulty for defending against the attack (i.e. what
 * is passed to the third argument of skill_check() if a defense is allowed)
 * \param typ is the projection (PROJ_) type
 * \return whether the effects were obvious
 *
 * Note that this function determines if the player can see anything that
 * happens by taking into account: blindness, line-of-sight, and illumination.
 *
 * Hack -- effects on grids which are memorized but not in view are also seen.
 */
bool project_f(struct source origin, struct loc grid, int dif, int typ)
{
	bool obvious = false;

	project_feature_handler_context_t context = {
		origin,
		grid,
		dif,
		typ,
		obvious,
	};
	project_feature_handler_f feature_handler = feature_handlers[typ];

	if (feature_handler != NULL)
		feature_handler(&context);

	/* Return "Anything seen?" */
	return context.obvious;
}

