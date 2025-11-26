/**
 * \file project-obj.c
 * \brief projection effects on objects
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
#include "cmd-core.h"
#include "mon-util.h"
#include "obj-chest.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "project.h"
#include "source.h"


/**
 * Destroys a type of item on a given percent chance.
 * The chance 'cperc' is in hundredths of a percent (1-in-10000)
 * Note that missiles are no longer necessarily all destroyed
 *
 * Returns number of items destroyed.
 */
int inven_damage(struct player *p, int type, int perc, int resistance)
{
	int k = 0;
	struct object *obj = p->gear;

	/* No chance means no damage */
	if (perc <= 0) return 0;

	/* Scan through the gear */
	while (obj) {
		struct object *next = obj->next;
		if (object_is_equipped(p->body, obj)) {
			obj = next;
			continue;
		}

		/* Hack -- for now, skip artifacts */
		if (obj->artifact) {
			obj = next;
			continue;
		}

		/* Give this item slot a shot at death if it is vulnerable */
		if ((obj->el_info[type].flags & EL_INFO_HATES) &&
			!(obj->el_info[type].flags & EL_INFO_IGNORE)) {
			/* Chance to destroy this item */
			int j, amt;

			/* Count the casualties */
			for (amt = j = 0; j < obj->number; ++j) {
				if (percent_chance(perc) &&
					((resistance < 0) || one_in_(resistance))) {
					amt++;
				}
			}

			/* Some casualities */
			if (amt) {
				char o_name[80];
				struct object *destroyed;
				bool none_left = false;

				/* Get a description */
				object_desc(o_name, sizeof(o_name), obj,
					ODESC_BASE, p);

				/* Message */
				msgt(MSG_DESTROY, "%sour %s (%c) %s destroyed!",
					 ((obj->number > 1) ?
					  ((amt == obj->number) ? "All of y" :
					   (amt > 1 ? "Some of y" : "One of y")) : "Y"),
					 o_name,
					 gear_to_label(p, obj),
					 ((amt > 1) ? "were" : "was"));

				/* Destroy "amt" items */
				destroyed = gear_object_for_use(p, obj, amt, false, &none_left);
				if (destroyed->known)
					object_delete(NULL, NULL, &destroyed->known);
				object_delete(NULL, NULL, &destroyed);

				/* Count the casualties */
				k += amt;
			}
		}
		obj = next;
	}

	/* Return the casualty count */
	return k;
}

/**
 * ------------------------------------------------------------------------
 * Object handlers
 * ------------------------------------------------------------------------ */

typedef struct project_object_handler_context_s {
	const struct loc grid;
	const int type;
	const struct object *obj;
	bool obvious;
	bool do_kill;
	bool ignore;
	const char *note_kill;
} project_object_handler_context_t;
typedef void (*project_object_handler_f)(project_object_handler_context_t *);

/**
 * Project an effect onto an object.
 *
 * \param context is the project_o context.
 * \param element is for elements that will destroy an object, or that it will
 * ignore.
 * \param singular_verb is the verb that is displayed when one object is
 * destroyed.
 * \param plural_verb is the verb that is displayed in multiple objects are
 * destroyed.
 */
static void project_object_elemental(project_object_handler_context_t *context,
									 int element, const char *singular_verb,
									 const char *plural_verb)
{
	if (context->obj->el_info[element].flags & EL_INFO_HATES) {
		context->do_kill = true;
		context->note_kill = VERB_AGREEMENT(context->obj->number,
											singular_verb, plural_verb);
		context->ignore = (context->obj->el_info[element].flags &
						   EL_INFO_IGNORE) ? true : false;
	}
}

/* Fire -- Flammable objects */
static void project_object_handler_FIRE(project_object_handler_context_t *context)
{
	project_object_elemental(context, ELEM_FIRE, "burns up", "burn up");
}

/* Cold -- potions and flasks */
static void project_object_handler_COLD(project_object_handler_context_t *context)
{
	project_object_elemental(context, ELEM_COLD, "shatters", "shatter");
}

static void project_object_handler_POIS(project_object_handler_context_t *context)
{
}

static void project_object_handler_DARK(project_object_handler_context_t *context)
{
}

static void project_object_handler_NOTHING(project_object_handler_context_t *context)
{
}

static void project_object_handler_HURT(project_object_handler_context_t *context)
{
}

static void project_object_handler_ARROW(project_object_handler_context_t *context)
{
}

static void project_object_handler_BOULDER(project_object_handler_context_t *context)
{
}

/* Acid -- Lots of things */
static void project_object_handler_ACID(project_object_handler_context_t *context)
{
	project_object_elemental(context, ELEM_ACID, "melts", "melt");
}

/* Sound -- potions and flasks */
static void project_object_handler_SOUND(project_object_handler_context_t *context)
{
	project_object_elemental(context, ELEM_COLD, "shatters", "shatter");
}

static void project_object_handler_FORCE(project_object_handler_context_t *context)
{
}

static void project_object_handler_LIGHT(project_object_handler_context_t *context)
{
}

static void project_object_handler_KILL_WALL(project_object_handler_context_t *context)
{
}

static void project_object_handler_SLEEP(project_object_handler_context_t *context)
{
}

static void project_object_handler_SPEED(project_object_handler_context_t *context)
{
}

static void project_object_handler_SLOW(project_object_handler_context_t *context)
{
}

static void project_object_handler_CONFUSION(project_object_handler_context_t *context)
{
}

static void project_object_handler_FEAR(project_object_handler_context_t *context)
{
}

/* EARTHQUAKE -- potions and flasks */
static void project_object_handler_EARTHQUAKE(project_object_handler_context_t *context)
{
	project_object_elemental(context, ELEM_COLD, "shatters", "shatter");
}

static void project_object_handler_DARK_WEAK(project_object_handler_context_t *context)
{
}

static void project_object_handler_KILL_DOOR(project_object_handler_context_t *context)
{
	/* Chests are noticed only if trapped or locked */
	if (is_locked_chest(context->obj)) {
		/* Disarm or Unlock */
		unlock_chest((struct object * const)context->obj);

		/* Notice */
		if (context->obj->pval == context->obj->known->pval) {
			msg("Click!");
			context->obvious = true;
		}
	}
}

static void project_object_handler_LOCK_DOOR(project_object_handler_context_t *context)
{
}

/* Unlock chests */
static void project_object_handler_KILL_TRAP(project_object_handler_context_t *context)
{
	/* Chests are noticed only if trapped or locked */
	if (is_locked_chest(context->obj)) {
		/* Disarm or Unlock */
		unlock_chest((struct object * const)context->obj);

		/* Notice */
		if (context->obj->pval == context->obj->known->pval) {
			msg("Click!");
			context->obvious = true;
		}
	}
}

static void project_object_handler_DISP_ALL(project_object_handler_context_t *context)
{
}

static const project_object_handler_f object_handlers[] = {
	#define ELEM(a) project_object_handler_##a,
	#include "list-elements.h"
	#undef ELEM
	#define PROJ(a) project_object_handler_##a,
	#include "list-projections.h"
	#undef PROJ
	NULL
};

/**
 * Called from project() to affect objects
 *
 * Called for projections with the PROJECT_ITEM flag set, which includes
 * beam, ball and breath effects.
 *
 * \param grid the coordinates of the grid being handled
 * \param typ is the projection (PROJ_) type
 * \param protected_obj is an object that should not be affected by the
 *        projection, typically the object that created it
 * \return whether the effects were obvious
 *
 * Note that this function determines if the player can see anything that
 * happens by taking into account: blindness, line-of-sight, and illumination.
 *
 * Hack -- effects on objects which are memorized but not in view are also seen.
 */
bool project_o(struct loc grid, int typ, const struct object *protected_obj)
{
	struct object *obj = square_object(cave, grid);
	bool obvious = false;

	/* Scan all objects in the grid */
	while (obj) {
		bool ignore = false;
		bool do_kill = false;
		const char *note_kill = NULL;
		struct object *next = obj->next;

		project_object_handler_f object_handler = object_handlers[typ];
		project_object_handler_context_t context = {
			grid,
			typ,
			obj,
			obvious,
			do_kill,
			ignore,
			note_kill,
		};

		if (object_handler != NULL)
			object_handler(&context);

		obvious = context.obvious;
		do_kill = context.do_kill && (obj != protected_obj);
		ignore = context.ignore;
		note_kill = context.note_kill;

		/* Attempt to destroy the object */
		if (do_kill) {
			char o_name[80];

			/* Effect observed */
			if (obj->known && !ignore_item_ok(player, obj) &&
				square_isseen(cave, grid)) {
				obvious = true;
				object_desc(o_name, sizeof(o_name), obj,
					ODESC_BASE, player);
			}

			/* Artifacts, and other objects, get to resist */
			if (obj->artifact || ignore) {
				/* Observe the resist */
				if (obvious && !ignore_item_ok(player, obj)) {
					msg("The %s %s unaffected!", o_name,
						VERB_AGREEMENT(obj->number, "is", "are"));
				}
			} else {
				/* Describe if needed */
				if (obvious && note_kill && !ignore_item_ok(player, obj)) {
					msgt(MSG_DESTROY, "The %s %s!", o_name, note_kill);
				}

				/* Prevent command repetition, if necessary. */
				if (loc_eq(grid, player->grid)) {
					cmd_disable_repeat_floor_item();
				}

				/* Delete the object */
				square_delete_object(cave, grid, obj, true, true);
			}
		}

		/* Next object */
		obj = next;
	}

	/* Return "Anything seen?" */
	return obvious;
}
