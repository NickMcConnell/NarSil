/**
 * \file obj-knowledge.c
 * \brief Object knowledge
 *
 * Copyright (c) 2016 Nick McConnell
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
#include "mon-desc.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-properties.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"

/**
 * Overview
 * ========
 * This file deals with the new "rune-based ID" system.  This system operates
 * as follows:
 * - struct player has an object struct attached to it (obj_k) which contains
 *   the player's knowledge of object properties (runes)
 * - whenever the player learns a rune, 
 *   - if it's an object flag, that flag is set in obj_k
 *   - if it's an integer value, that value in obj_k is set to 1
 *   - if it's element info, the res_level value is set to 1
 *   - if it's a brand, a brand is added to obj_k with the relevant element
 *   - if it's a slay, a slay is added to obj_k with the right race flag or name
 * - every object has a known version which is filled in with details as the
 *   player learns them
 * - whenever the player learns a rune, that knowledge is applied to the known
 *   version of every object that the player has picked up or walked over
 */

/**
 * ------------------------------------------------------------------------
 * Object knowledge data
 * This section covers initialisation, access and cleanup of rune data
 * ------------------------------------------------------------------------ */
static size_t rune_max;
static struct rune *rune_list;

/**
 * Initialise the rune module
 */
static void init_rune(void)
{
	int i, j, count;

	/* Count runes */
	count = 0;
	for (i = 1; i < OF_MAX; i++) {
		struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_FLAG, i);
		if (prop->subtype == OFT_NONE) continue;
		if (prop->subtype == OFT_BASIC) continue;
		count++;
	}
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		count++;
	}
	for (i = 0; i < ELEM_MAX; i++) {
		count++;
	}
	/* Note brand runes cover all brands with the same name */
	for (i = 1; i < z_info->brand_max; i++) {
		bool counted = false;
		if (brands[i].name) {
			for (j = 1; j < i; j++) {
				if (streq(brands[i].name, brands[j].name)) {
					counted = true;
				}
			}
			if (!counted) {
				count++;
			}
		}
	}
	/* Note slay runes cover all slays with the same flag/base */
	for (i = 1; i < z_info->slay_max; i++) {
		bool counted = false;
		if (slays[i].name) {
			for (j = 1; j < i; j++) {
				if (same_monsters_slain(i, j)) {
					counted = true;
				}
			}
			if (!counted) {
				count++;
			}
		}
	}

	/* Now allocate and fill the rune list */
	rune_max = count;
	rune_list = mem_zalloc(rune_max * sizeof(struct rune));
	count = 0;
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_MOD, i);
		rune_list[count++] = (struct rune) { RUNE_VAR_MOD, i, 0, prop->name };
	}
	for (i = 0; i < ELEM_MAX; i++) {
		rune_list[count++] = (struct rune) { RUNE_VAR_RESIST, i, 0, projections[i].name };
	}
	for (i = 1; i < z_info->brand_max; i++) {
		bool counted = false;
		if (brands[i].name) {
			for (j = 1; j < i; j++) {
				if (streq(brands[i].name, brands[j].name)) {
					counted = true;
				}
			}
			if (!counted) {
				rune_list[count++] =
					(struct rune) { RUNE_VAR_BRAND, i, 0, brands[i].name };
			}
		}
	}
	for (i = 1; i < z_info->slay_max; i++) {
		bool counted = false;
		if (slays[i].name) {
			for (j = 1; j < i; j++) {
				if (same_monsters_slain(i, j)) {
					counted = true;
				}
			}
			if (!counted) {
				rune_list[count++] =
					(struct rune) { RUNE_VAR_SLAY, i, 0, slays[i].name };
			}
		}
	}
	for (i = 1; i < OF_MAX; i++) {
		struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_FLAG, i);
		if (prop->subtype == OFT_NONE) continue;
		if (prop->subtype == OFT_BASIC) continue;

		rune_list[count++] = (struct rune)
			{ RUNE_VAR_FLAG, i, 0, prop->name };
	}
}

/**
 * Get a rune by variety and index
 */
static int rune_index(size_t variety, int index)
{
	size_t i;

	/* Look for the rune */
	for (i = 0; i < rune_max; i++)
		if ((rune_list[i].variety == variety) && (rune_list[i].index == index))
			return i;

	/* Can't find it */
	return -1;
}

/**
 * Cleanup the rune module
 */
static void cleanup_rune(void)
{
	mem_free(rune_list);
}

struct init_module rune_module = {
	.name = "rune",
	.init = init_rune,
	.cleanup = cleanup_rune
};

/**
 * ------------------------------------------------------------------------
 * Rune knowledge functions
 * These functions provide details about the rune list for use in 
 * player knowledge screens
 * ------------------------------------------------------------------------ */
/**
 * The number of runes
 */
int max_runes(void)
{
	return rune_max;
}

/**
 * The variety of a rune
 */
enum rune_variety rune_variety(size_t i)
{
	return rune_list[i].variety;
}

/**
 * Reports if the player knows a given rune
 *
 * \param p is the player
 * \param i is the rune's number in the rune list
 */
bool player_knows_rune(struct player *p, size_t i)
{
	struct rune *r = &rune_list[i];

	switch (r->variety) {
		/* Mod runes */
		case RUNE_VAR_MOD: {
			if (p->obj_k->modifiers[r->index])
				return true;
			break;
		}
		/* Element runes */
		case RUNE_VAR_RESIST: {
			if (p->obj_k->el_info[r->index].res_level)
				return true;
			break;
		}
		/* Brand runes */
		case RUNE_VAR_BRAND: {
			assert(r->index < z_info->brand_max);
			if (p->obj_k->brands[r->index]) {
				return true;
			}
			break;
		}
		/* Slay runes */
		case RUNE_VAR_SLAY: {
			assert(r->index < z_info->slay_max);
			if (p->obj_k->slays[r->index]) {
				return true;
			}
			break;
		}
		/* Flag runes */
		case RUNE_VAR_FLAG: {
			if (of_has(p->obj_k->flags, r->index))
				return true;
			break;
		}
		default: {
			break;
		}
	}

	return false;
}

/**
 * The name of a rune
 */
const char *rune_name(size_t i)
{
	struct rune *r = &rune_list[i];

	if (r->variety == RUNE_VAR_BRAND)
		return format("%s brand", r->name);
	else if (r->variety == RUNE_VAR_SLAY)
		return format("slay %s", r->name);
	else if (r->variety == RUNE_VAR_RESIST)
		return format("resist %s", r->name);
	else
		return format("%s", r->name);

	return NULL;
}

/**
 * The description of a rune
 */
const char *rune_desc(size_t i)
{
	struct rune *r = &rune_list[i];

	switch (r->variety) {
		/* Mod runes */
		case RUNE_VAR_MOD: {
			return format("Object gives the player a magical bonus to %s.",
						  r->name);
			break;
		}
		/* Element runes */
		case RUNE_VAR_RESIST: {
			return format("Object affects the player's resistance to %s.",
						  r->name);
			break;
		}
		/* Brand runes */
		case RUNE_VAR_BRAND: {
			return format("Object brands the player's attacks with %s.",
						  r->name);
			break;
		}
		/* Slay runes */
		case RUNE_VAR_SLAY: {
			return format("Object makes the player's attacks against %s more powerful.", r->name);
			break;
		}
		/* Flag runes */
		case RUNE_VAR_FLAG: {
			return format("Object gives the player the property of %s.",
						  r->name);
			break;
		}
		default: {
			break;
		}
	}

	return NULL;
}

/**
 * The autoinscription index (if any) of a rune
 */
quark_t rune_note(size_t i)
{
	return rune_list[i].note;
}

/**
 * Set an autoinscription on a rune
 */
void rune_set_note(size_t i, const char *inscription)
{
	struct rune *r = &rune_list[i];

	if (!inscription)
		r->note = 0;
	else
		r->note = quark_add(inscription);
}

/**
 * ------------------------------------------------------------------------
 * Object knowledge predicates
 * These functions tell how much the player knows about an object
 * ------------------------------------------------------------------------ */

/**
 * Check if a flag is known to the player
 *
 * \param p is the player
 * \param f is the flag index
 */
bool player_knows_flag(struct player *p, int f)
{
	return of_has(p->obj_k->flags, f);
}

/**
 * Check if a brand is known to the player
 *
 * \param p is the player
 * \param i is the brand index
 */
bool player_knows_brand(struct player *p, int i)
{
	return p->obj_k->brands[i];
}

/**
 * Check if a slay is known to the player
 *
 * \param p is the player
 * \param i is the slay index
 */
bool player_knows_slay(struct player *p, int i)
{
	return p->obj_k->slays[i];
}

/**
 * Check if an ego item type is known to the player
 *
 * \param p is the player
 * \param ego is the ego item type
 */
bool player_knows_ego(struct player *p, struct ego_item *ego)
{
	int i;

	if (!ego) return false;

	/* All flags known */
	if (!of_is_subset(p->obj_k->flags, ego->flags)) return false;

	/* All modifiers known */
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		if ((ABS(ego->modifiers[i]) > 0) && !p->obj_k->modifiers[i]) {
				return false;
		}
	}

	/* All elements known */
	for (i = 0; i < ELEM_MAX; i++)
		if (ego->el_info[i].res_level && !p->obj_k->el_info[i].res_level)
			return false;

	/* All brands known */
	for (i = 1; i < z_info->brand_max; i++) {
		if (ego->brands && ego->brands[i] && !player_knows_brand(p, i)) {
			return false;
		}
	}

	/* All slays known */
	for (i = 1; i < z_info->slay_max; i++) {
		if (ego->slays && ego->slays[i] && !player_knows_slay(p, i)) {
			return false;
		}
	}

	return true;
}

/**
 * Checks whether the object is known to be an artifact
 *
 * \param obj is the object
 */
bool object_is_known_artifact(const struct object *obj)
{
	if (!obj->known) return false;
	return obj->known->artifact ? true : false;
}

/**
 * Check if an object has a rune
 *
 * \param obj is the object
 * \param rune_no is the rune's number in the rune list
 */
bool object_has_rune(const struct object *obj, int rune_no)
{
	struct rune *r = &rune_list[rune_no];

	switch (r->variety) {
		/* Mod runes */
		case RUNE_VAR_MOD: {
			if (obj->modifiers[r->index] != 0)
				return true;
			break;
		}
		/* Element runes */
		case RUNE_VAR_RESIST: {
			if (obj->el_info[r->index].res_level != 0)
				return true;
			break;
		}
		/* Brand runes */
		case RUNE_VAR_BRAND: {
			if (obj->brands) {
				int i;
				for (i = 0; i < z_info->brand_max; i++) {
					if (obj->brands[i] && streq(brands[i].name, r->name)) {
						return true;
					}
				}
			}
			break;
		}
		/* Slay runes */
		case RUNE_VAR_SLAY: {
			if (obj->slays) {
				int i;
				for (i = 0; i < z_info->slay_max; i++) {
					if (obj->slays[i] && same_monsters_slain(r->index, i)) {
						return true;
					}
				}
			}
			break;
		}
		/* Flag runes */
		case RUNE_VAR_FLAG: {
			if (of_has(obj->flags, r->index))
				return true;
			break;
		}
		default: break;
	}

	return false;
}

/**
 * Check if all the runes on an object are known to the player
 *
 * \param obj is the object
 */
bool object_runes_known(const struct object *obj)
{
	int i;

	/* No known object */
	if (!obj->known) return false;

	/* Not all modifiers known */
	for (i = 0; i < OBJ_MOD_MAX; i++)
		if (obj->modifiers[i] != obj->known->modifiers[i])
			return false;

	/* Not all elements known */
	for (i = 0; i < ELEM_MAX; i++)
		if ((obj->el_info[i].res_level != 0) &&
			(obj->known->el_info[i].res_level == 0))
			return false;

	/* Not all brands known */
	if (obj->brands) {
		if (!obj->known->brands)
			return false;
		for (i = 0; i < z_info->brand_max; i++) {
			if (obj->brands[i] && !obj->known->brands[i]) {
				return false;
			}
		}
	}

	/* Not all slays known */
	if (obj->slays) {
		if (!obj->known->slays)
			return false;
		for (i = 0; i < z_info->slay_max; i++) {
			if (obj->slays[i] && !obj->known->slays[i]) {
				return false;
			}
		}
	}

	/* Not all flags known */
	if (!of_is_subset(obj->known->flags, obj->flags)) return false;

	return true;
}

/**
 * Checks whether a player knows whether an object has a given flag
 *
 * \param p is the player
 * \param obj is the object
 * \param flag is the flag
 */
bool object_flag_is_known(const struct player *p, const struct object *obj,
	int flag)
{
	/* Object runes known means OK */
	if (object_runes_known(obj)) return true;

	/* Player knows the flag means OK */
	if (of_has(p->obj_k->flags, flag)) return true;

	/* Object has had a chance to display the flag means OK */
	if (of_has(obj->known->flags, flag)) return true;

	return false;
}

/**
 * Checks whether a player knows the given element properties of an object
 *
 * \param p is the player
 * \param obj is the object
 * \param element is the element
 */
bool object_element_is_known(const struct player *p, const struct object *obj,
	int element)
{
	if (element < 0 || element >= ELEM_MAX) return false;

	/* Object runes known means OK */
	if (object_runes_known(obj)) return true;

	/* Player knows the element means OK */
	if (p->obj_k->el_info[element].res_level) return true;

	/* Object has been exposed to the element means OK */
	if (obj->known->el_info[element].res_level) return true;

	return false;
}

/**
 * ------------------------------------------------------------------------
 * Object knowledge propagators
 * These functions transfer player knowledge to objects
 * ------------------------------------------------------------------------ */
/**
 * Sets the basic details on a known object
 */
void object_set_base_known(struct player *p, struct object *obj)
{
	assert(obj->known);
	obj->known->kind = obj->kind;
	obj->known->tval = obj->tval;
	obj->known->sval = obj->sval;
	obj->known->weight = obj->weight;
	obj->known->number = obj->number;

	/* Dice, attack and evasion */
	if (!obj->known->dd) {
		obj->known->dd = obj->kind->dd * p->obj_k->dd;
	}
	if (!obj->known->ds) {
		obj->known->ds = obj->kind->ds * p->obj_k->ds;
	}
	if (!obj->known->pd) {
		obj->known->pd = obj->kind->pd * p->obj_k->pd;
	}
	if (!obj->known->ps) {
		obj->known->ps = obj->kind->ps * p->obj_k->ps;
	}
	if (!obj->known->att) {
		obj->known->att = obj->kind->att * p->obj_k->att;
	}
	if (!obj->known->evn) {
		obj->known->evn = obj->kind->evn * p->obj_k->evn;
	}

	/* Aware flavours and unflavored non-wearables get info now */
	if ((obj->kind->aware && obj->kind->flavor) ||
		(!tval_is_wearable(obj) && !obj->kind->flavor)) {
		obj->known->pval = obj->pval;
	}
}

/**
 * Gain knowledge based on seeing an object on the floor
 */
void object_see(struct player *p, struct object *obj)
{
	struct object *known_obj = p->cave->objects[obj->oidx];
	struct loc grid = obj->grid;

	/* Make new known objects, fully know sensed ones, relocate old ones */
	if (known_obj == NULL) {
		/* Make a new one */
		struct object *new_obj;

		assert(! obj->known);
		new_obj = object_new();
		obj->known = new_obj;
		new_obj->kind = obj->kind;
		new_obj->tval = obj->tval;
		new_obj->sval = obj->sval;
		new_obj->number = obj->number;

		/* List the known object */
		p->cave->objects[obj->oidx] = new_obj;
		new_obj->oidx = obj->oidx;

		/* If monster held, we're done */
		if (obj->held_m_idx) return;

		/* Attach it to the current floor pile */
		new_obj->grid = grid;
		pile_insert_end(&p->cave->squares[grid.y][grid.x].obj, new_obj);
	} else {
		struct loc old = known_obj->grid;

		/* Make sure knowledge is correct */
		assert(known_obj == obj->known);
		known_obj->kind = obj->kind;
		known_obj->tval = obj->tval;
		known_obj->sval = obj->sval;
		known_obj->number = obj->number;

		/* If monster held, we're done */
		if (obj->held_m_idx) return;

		/* Attach it to the current floor pile if necessary */
		if (! square_holds_object(p->cave, grid, known_obj)) {
			/* Detach from any old pile */
			if (!loc_is_zero(old) && square_holds_object(p->cave, old, known_obj)) {
				square_excise_object(p->cave, old, known_obj);
			}

			known_obj->grid = grid;
			pile_insert_end(&p->cave->squares[grid.y][grid.x].obj, known_obj);
		}
	}
}

/**
 * Gain knowledge based on being an the same square as an object
 */
void object_touch(struct player *p, struct object *obj)
{
	/* Automatically notice artifacts, mark as assessed */
	obj->known->artifact = obj->artifact;
	obj->known->notice |= OBJ_NOTICE_ASSESSED;

	/* Apply known properties to the object */
	player_know_object(p, obj);

	/* Log artifacts if found */
	if (obj->artifact && !is_artifact_seen(obj->artifact)) {
		const struct artifact *art = obj->artifact;
		int new_exp = 100;

		/* Mark */
		mark_artifact_seen(art, true);

		/* Gain experience for identification */
		player_exp_gain(p, new_exp);
		p->ident_exp += new_exp;

		/* Record in the history */
		history_find_artifact(p, art);
	}
}

/**
 * Transfer player object knowledge to an object
 *
 * \param p is the player
 * \param obj is the object
 */
void player_know_object(struct player *p, struct object *obj)
{
	int i, flag;
	bool seen = true;

	/* Unseen or only sensed objects don't get any ID */
	if (!obj) return;
	if (!obj->known) return;
	if (obj->kind != obj->known->kind) return;

	/* Distant objects just get base properties */
	if (obj->kind && !(obj->known->notice & OBJ_NOTICE_ASSESSED)) {
		object_set_base_known(p, obj);
		return;
	}

	/* Know flavored objects with Item Lore */
	if (player_active_ability(p, "Item Lore")) {
		object_flavor_aware(p, obj);
	}

	/* Know worn objects with Lore-Master */
	if (player_active_ability(p, "Lore-Master")) {
		while (!object_runes_known(obj)) {
			object_learn_unknown_rune(p, obj);
		}
	}

	/* Get the combat properties, and the pval for anything but chests */
	obj->known->dd = obj->dd * p->obj_k->dd;
	obj->known->ds = obj->ds * p->obj_k->ds;
	obj->known->pd = obj->pd * p->obj_k->pd;
	obj->known->ps = obj->ps * p->obj_k->ps;
	obj->known->att = obj->att * p->obj_k->att;
	obj->known->evn = obj->evn * p->obj_k->evn;
	if (!tval_is_chest(obj))
		obj->known->pval = obj->pval;

	/* Set modifiers */
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		if (p->obj_k->modifiers[i]) {
			obj->known->modifiers[i] = obj->modifiers[i];
		} else {
			obj->known->modifiers[i] = 0;
		}
	}

	/* Set elements */
	for (i = 0; i < ELEM_MAX; i++) {
		if (p->obj_k->el_info[i].res_level == 1) {
			obj->known->el_info[i].res_level = obj->el_info[i].res_level;
			obj->known->el_info[i].flags = obj->el_info[i].flags;
		} else {
			obj->known->el_info[i].res_level = 0;
			obj->known->el_info[i].flags = 0;
		}
	}

	/* Set object flags */
	of_wipe(obj->known->flags);
	for (flag = of_next(p->obj_k->flags, FLAG_START); flag != FLAG_END;
		 flag = of_next(p->obj_k->flags, flag + 1)) {
		if (of_has(obj->flags, flag)) {
			of_on(obj->known->flags, flag);
		}
	}

	/* Set brands */
	if (obj->brands) {
		bool known_brand = false;

		for (i = 1; i < z_info->brand_max; i++) {
			if (player_knows_brand(p, i) && obj->brands[i]) {
				if (!obj->known->brands) {
					obj->known->brands = mem_zalloc(
						z_info->brand_max *
						sizeof(bool));
				}
				obj->known->brands[i] = true;
				known_brand = true;
			} else if (obj->known->brands) {
				obj->known->brands[i] = false;
			}
		}
		if (!known_brand && obj->known->brands) {
			mem_free(obj->known->brands);
			obj->known->brands = NULL;
		}
	}

	/* Set slays */
	if (obj->slays) {
		bool known_slay = false;

		for (i = 1; i < z_info->slay_max; i++) {
			if (player_knows_slay(p, i) && obj->slays[i]) {
				if (!obj->known->slays) {
					obj->known->slays = mem_zalloc(
						z_info->slay_max *
						sizeof(bool));
				}
				obj->known->slays[i] = true;
				known_slay = true;
			} else if (obj->known->slays) {
				obj->known->slays[i] = false;
			}
		}
		if (!known_slay && obj->known->slays) {
			mem_free(obj->known->slays);
			obj->known->slays = NULL;
		}
	}

	/* Set ego type, jewellery type if known */
	if (player_knows_ego(p, obj->ego)) {
		seen = obj->ego->everseen;
		obj->known->ego = obj->ego;
	} else {
		obj->known->ego = NULL;
	}

	if (tval_is_jewelry(obj)) {
		if (object_runes_known(obj)) {
			seen = (obj->artifact) ? true : obj->kind->everseen;
			object_flavor_aware(p, obj);
		}
	} else if (obj->kind->kidx >= z_info->ordinary_kind_max) {
		/*
		 * Become aware if it is a special artifact that isn't
		 * jewelry.
		 */
		seen = true;
		object_flavor_aware(p, obj);
	}

	/* Report on new stuff */
	if (!seen) {
		char o_name[80];

		/* Describe the object if it's available */
		if (object_is_carried(p, obj)) {
			object_desc(o_name, sizeof(o_name), obj,
				ODESC_PREFIX | ODESC_FULL, p);
			msg("You have %s (%c).", o_name, gear_to_label(p, obj));
		} else if (cave && square_holds_object(cave, p->grid, obj)) {
			object_desc(o_name, sizeof(o_name), obj,
				ODESC_PREFIX | ODESC_FULL, p);
			msg("On the ground: %s.", o_name);
		}
	}

	/* Fully known objects have their known element and flag info set to 
	 * match the actual info, rather than showing what elements and flags
	 * the would be displaying if they had them */
	if (object_runes_known(obj)) {
		for (i = 0; i < ELEM_MAX; i++) {
			obj->known->el_info[i].res_level = obj->el_info[i].res_level;
			obj->known->el_info[i].flags = obj->el_info[i].flags;
		}
		of_wipe(obj->known->flags);
		of_copy(obj->known->flags, obj->flags);
	}
}

/**
 * Propagate player knowledge of objects to all objects
 *
 * \param p is the player
 */
void update_player_object_knowledge(struct player *p)
{
	int i;
	struct object *obj;

	/* Level objects */
	if (cave)
		for (i = 0; i < cave->obj_max; i++)
			player_know_object(p, cave->objects[i]);

	/* Player objects */
	for (obj = p->gear; obj; obj = obj->next)
		player_know_object(p, obj);

	/* Update */
	if (cave)
		autoinscribe_ground(p);
	autoinscribe_pack(p);
	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);
}

/**
 * ------------------------------------------------------------------------
 * Object knowledge learners
 * These functions are for increasing player knowledge of object properties
 * ------------------------------------------------------------------------ */
/**
 * Learn a given rune
 *
 * \param p is the player
 * \param i is the rune index
 * \param message is whether or not to print a message
 */
static void player_learn_rune(struct player *p, size_t i, bool message)
{
	struct rune *r = &rune_list[i];
	bool learned = false;

	switch (r->variety) {
		/* Mod runes */
		case RUNE_VAR_MOD: {
			if (!p->obj_k->modifiers[r->index]) {
				p->obj_k->modifiers[r->index] = 1;
				learned = true;
			}
			break;
		}
		/* Element runes */
		case RUNE_VAR_RESIST: {
			if (!p->obj_k->el_info[r->index].res_level) {
				p->obj_k->el_info[r->index].res_level = 1;
				learned = true;
			}
			break;
		}
		/* Brand runes */
		case RUNE_VAR_BRAND: {
			assert(r->index < z_info->brand_max);

			/* If the brand was unknown, add it to known brands */
			if (!player_knows_brand(p, r->index)) {
				int j;
				for (j = 1; j < z_info->brand_max; j++) {
					/* Check base and race flag */
					if (streq(brands[r->index].name, brands[j].name)) {
						p->obj_k->brands[j] = true;
						learned = true;
					}
				}
			}
			break;
		}
		/* Slay runes */
		case RUNE_VAR_SLAY: {
			assert(r->index < z_info->slay_max);

			/* If the slay was unknown, add it to known slays */
			if (!player_knows_slay(p, r->index)) {
				int j;
				for (j = 1; j < z_info->slay_max; j++) {
					/* Check base and race flag */
					if (same_monsters_slain(r->index, j)) {
						p->obj_k->slays[j] = true;
						learned = true;
					}
				}
			}
			break;
		}
		/* Flag runes */
		case RUNE_VAR_FLAG: {
			if (of_on(p->obj_k->flags, r->index))
				learned = true;
			break;
		}
		default: {
			learned = false;
			break;
		}
	}

	/* Nothing learned */
	if (!learned) return;

	/* Give a message */
	if (message)
		msgt(MSG_RUNE, "You have learned the property of %s.", rune_name(i));

	/* Update knowledge */
	update_player_object_knowledge(p);
}

/**
 * Learn a flag
 */
void player_learn_flag(struct player *p, int flag)
{
	player_learn_rune(p, rune_index(RUNE_VAR_FLAG, flag), true);
	update_player_object_knowledge(p);
}

/**
 * Learn a slay.
 */
void player_learn_slay(struct player *p, int index)
{
	/* Learn about the slay */
	if (!player_knows_slay(p, index)) {
		int i;

		/* Find the rune index */
		for (i = 1; i < z_info->slay_max; i++) {
			if (same_monsters_slain(i, index)) {
				break;
			}
		}
		assert(i < z_info->slay_max);

		/* Learn the rune */
		player_learn_rune(p, rune_index(RUNE_VAR_SLAY, i), true);
		update_player_object_knowledge(p);
	}
}

/**
 * Learn a brand.
 */
void player_learn_brand(struct player *p, int index)
{
	/* Learn about the brand */
	if (!player_knows_brand(p, index)) {
		int i;

		/* Find the rune index */
		for (i = 1; i < z_info->brand_max; i++) {
			if (streq(brands[i].name, brands[index].name)) {
				break;
			}
		}
		assert(i < z_info->brand_max);

		/* Learn the rune */
		player_learn_rune(p, rune_index(RUNE_VAR_BRAND, i), true);
		update_player_object_knowledge(p);
	}
}

/**
 * Learn absolutely everything
 *
 * \param p is the player
 */
void player_learn_all_runes(struct player *p)
{
	size_t i;

	for (i = 0; i < rune_max; i++)
		player_learn_rune(p, i, false);
}

/**
 * ------------------------------------------------------------------------
 * Functions for learning from the behaviour of indvidual objects
 * ------------------------------------------------------------------------ */
/**
 * Print a message when an object modifier is identified by use.
 *
 * \param obj is the object 
 * \param mod is the modifier being noticed
 */
static void mod_message(struct object *obj, int mod)
{
	/* Special messages for individual properties */
	switch (mod) {
		case OBJ_MOD_STR:
			if (obj->modifiers[OBJ_MOD_STR] > 0)
				msg("You feel stronger.");
			else if (obj->modifiers[OBJ_MOD_STR] < 0)
				msg("You feel less strong");
			break;
		case OBJ_MOD_DEX:
			if (obj->modifiers[OBJ_MOD_DEX] > 0)
				msg("You feel more agile.");
			else if (obj->modifiers[OBJ_MOD_DEX] < 0)
				msg("You feel less agile.");
			break;
		case OBJ_MOD_CON:
			if (obj->modifiers[OBJ_MOD_CON] > 0)
				msg("You feel mmore resilient.");
			else if (obj->modifiers[OBJ_MOD_CON] < 0)
				msg("You feel less resilient.");
			break;
		case OBJ_MOD_GRA:
			if (obj->modifiers[OBJ_MOD_GRA] > 0)
				msg("You feel more attuned to the world.");
			else if (obj->modifiers[OBJ_MOD_GRA] < 0)
				msg("You feel less attuned to the world.");
			break;
		case OBJ_MOD_MELEE:
			if (obj->modifiers[OBJ_MOD_MELEE] > 0)
				msg("You feel more in control of your weapon.");
			else if (obj->modifiers[OBJ_MOD_MELEE] < 0)
				msg("You feel less in control of your weapon.");
			break;
		case OBJ_MOD_ARCHERY:
			if (obj->modifiers[OBJ_MOD_ARCHERY] > 0)
				msg("You feel more accurate at archery.");
			else if (obj->modifiers[OBJ_MOD_ARCHERY] < 0)
				msg("You feel less accurate at archery.");
			break;
		case OBJ_MOD_STEALTH:
			if (obj->modifiers[OBJ_MOD_STEALTH] > 0)
				msg("Your movements become quieter.");
			else if (obj->modifiers[OBJ_MOD_STEALTH] < 0)
				msg("Your movements become less quiet.");
			break;
		case OBJ_MOD_PERCEPTION:
			if (obj->modifiers[OBJ_MOD_PERCEPTION] > 0)
				msg("You feel more perceptive.");
			else if (obj->modifiers[OBJ_MOD_PERCEPTION] < 0)
				msg("You feel less perceptive.");
			break;
		case OBJ_MOD_WILL:
			if (obj->modifiers[OBJ_MOD_WILL] > 0)
				msg("You feel more firm of will.");
			else if (obj->modifiers[OBJ_MOD_WILL] < 0)
				msg("You feel less firm of will.");
			break;
		case OBJ_MOD_SMITHING:
			if (obj->modifiers[OBJ_MOD_SMITHING] > 0)
				msg("You feel a desire to craft things with your hands.");
			else if (obj->modifiers[OBJ_MOD_SMITHING] < 0)
				msg("You feel less able to craft things.");
			break;
		case OBJ_MOD_SONG:
			if (obj->modifiers[OBJ_MOD_SONG] > 0)
				msg("You are filled with inspiration.");
			else if (obj->modifiers[OBJ_MOD_SONG] < 0)
				msg("You feel a loss of inspiration.");
			break;
		case OBJ_MOD_DAMAGE_SIDES:
			if (obj->modifiers[OBJ_MOD_DAMAGE_SIDES] > 0)
				msg("You feel more forceful in melee.");
			else if (obj->modifiers[OBJ_MOD_DAMAGE_SIDES] < 0)
				msg("You feel less forceful in melee.");
			break;
		default:
			break;
	}
}

/**
 * Get a random unknown rune from an object
 *
 * \param p is the player
 * \param obj is the object
 * \return the index into the rune list, or -1 for no unknown runes
 */
static int object_find_unknown_rune(struct player *p, struct object *obj)
{
	size_t i, num = 0;
	int *poss_runes;
	int chosen = -1;

	if (object_runes_known(obj)) return -1;

	poss_runes = mem_zalloc(rune_max * sizeof(int));
	for (i = 0; i < rune_max; i++)
		if (object_has_rune(obj, i) && !player_knows_rune(p, i))
			poss_runes[num++] = i;

	/* Grab a random rune from among the unknowns  */
	if (num) {
		chosen = poss_runes[randint0(num)];
	}

	mem_free(poss_runes);
	return chosen;
}

/**
 * Learn a random unknown rune from an object
 *
 * \param p is the player
 * \param obj is the object
 */
void object_learn_unknown_rune(struct player *p, struct object *obj)
{
	/* Get a random unknown rune from the object */
	int i = object_find_unknown_rune(p, obj);

	/* No unknown runes */
	if (i < 0) {
		obj->known->notice |= OBJ_NOTICE_ASSESSED;
		player_know_object(player, obj);
		return;
	}

	/* Learn the rune */
	player_learn_rune(p, i, true);
}

/**
 * Learn object properties that become obvious on wielding or wearing
 *
 * \param p is the player
 * \param obj is the wielded object
 */
void object_learn_on_wield(struct player *p, struct object *obj)
{
	bitflag f[OF_SIZE], obvious_mask[OF_SIZE];
	int i, flag;
	char o_name[80];

	assert(obj->known);
	object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, p);

	/* Check the worn flag */
	if (obj->known->notice & OBJ_NOTICE_WORN) {
		return;
	} else {
		obj->known->notice |= OBJ_NOTICE_WORN;
	}

	/* Worn means tried (for flavored wearables) */
	object_flavor_tried(obj);

	/* Get the obvious object flags */
	create_obj_flag_mask(obvious_mask, true, OFID_WIELD, OFT_MAX);

	/* Make sustains obvious for items with that stat bonus */
	for (i = 0; i < STAT_MAX; i++) {
		int sust = sustain_flag(i);
		if (obj->modifiers[i]) {
			of_on(obvious_mask, sust);
		}
	}

	/* Learn about obvious, previously unknown flags */
	object_flags(obj, f);
	of_inter(f, obvious_mask);
	for (flag = of_next(f, FLAG_START); flag != FLAG_END;
		 flag = of_next(f, flag + 1)) {
		if (!of_has(p->obj_k->flags, flag)) {
			if (p->upkeep->playing) {
				flag_message(flag, o_name);
			}
			player_learn_rune(p, rune_index(RUNE_VAR_FLAG, flag), true);
		}
	}

	/* Learn all modifiers */
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		if (obj->modifiers[i] && !p->obj_k->modifiers[i]) {
			if (p->upkeep->playing) {
				mod_message(obj, i);
			}
			player_learn_rune(p, rune_index(RUNE_VAR_MOD, i), true);
		}
	}

	/* Learn abilities from special item types */
	if (obj->ego && obj->ego->abilities) {
		struct ability *ability = obj->ego->abilities;
		while (ability) {
			msg("You have gained the ability '%s'.", ability->name);
			ability = ability->next;
		}
	}

	/* Learn abilities from artefacts */
	if (obj->artifact && obj->artifact->abilities) {
		struct ability *ability = obj->artifact->abilities;
		while (ability) {
			msg("You have gained the ability '%s'.", ability->name);
			ability = ability->next;
		}
	}

}

/**
 * ------------------------------------------------------------------------
 * Functions for learning about equipment properties
 * These functions are for gaining object knowledge from the behaviour of
 * the player's equipment or shape
 * ------------------------------------------------------------------------ */
/**
 * Learn a given object flag on wielded items.
 *
 * \param p is the player
 * \param flag is the flag to notice
 */
void equip_learn_flag(struct player *p, int flag)
{
	int i;
	bitflag f[OF_SIZE];
	of_wipe(f);
	of_on(f, flag);

	/* No flag */
	if (!flag) return;

	/* All wielded items eligible */
	for (i = 0; i < p->body.count; i++) {
		struct object *obj = slot_object(p, i);
		if (!obj) continue;
		assert(obj->known);

		/* Does the object have the flag? */
		if (of_has(obj->flags, flag)) {
			if (!of_has(p->obj_k->flags, flag)) {
				char o_name[80];
				object_desc(o_name, sizeof(o_name), obj,
					ODESC_BASE, p);
				flag_message(flag, o_name);
				player_learn_rune(p, rune_index(RUNE_VAR_FLAG, flag), true);
			}
		} else if (!object_runes_known(obj)) {
			/* Objects not fully known yet get marked as having had a chance
			 * to display the flag */
			of_on(obj->known->flags, flag);
		}
	}
}

/**
 * Learn the elemental resistance properties on wielded items.
 *
 * \param p is the player
 * \param element is the element to notice
 */
void equip_learn_element(struct player *p, int element)
{
	int i;

	/* Invalid element or element already known */
	if (element < 0 || element >= ELEM_MAX) return;
	if (p->obj_k->el_info[element].res_level == 1) return;

	/* All wielded items eligible */
	for (i = 0; i < p->body.count; i++) {
		struct object *obj = slot_object(p, i);
		if (!obj) continue;
		assert(obj->known);

		/* Does the object affect the player's resistance to the element? */
		if (obj->el_info[element].res_level != 0) {
			char o_name[80];
			object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, p);

			/* Message */
			msg("Your %s glows.", o_name);

			/* Learn the element properties */
			player_learn_rune(p, rune_index(RUNE_VAR_RESIST, element), true);
		} else if (!object_runes_known(obj)) {
			/* Objects not fully known yet get marked as having had a chance
			 * to display the element */
			obj->known->el_info[element].res_level = 1;
			obj->known->el_info[element].flags = obj->el_info[element].flags;
		}
	}
}

/**
 * Learn things that would be noticed in time.
 *
 * \param p is the player
 */
void equip_learn_after_time(struct player *p)
{
	int i, flag;
	bitflag f[OF_SIZE], timed_mask[OF_SIZE];

	/* Get the timed flags */
	create_obj_flag_mask(timed_mask, true, OFID_TIMED, OFT_MAX);

	/* Get the unknown timed flags, and return if there are none */
	object_flags(p->obj_k, f);
	of_negate(f);
	of_inter(timed_mask, f);
	if (of_is_empty(timed_mask)) return;

	/* All wielded items eligible */
	for (i = 0; i < p->body.count; i++) {
		char o_name[80];
		struct object *obj = slot_object(p, i);

		if (!obj) continue;
		assert(obj->known);
		object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, p);

		/* Get the unknown timed flags for this object */
		object_flags(obj, f);
		of_inter(f, timed_mask);

		/* Attempt to learn every flag */
		for (flag = of_next(f, FLAG_START); flag != FLAG_END;
			 flag = of_next(f, flag + 1)) {
			if (!of_has(p->obj_k->flags, flag)) {
				flag_message(flag, o_name);
			}
			player_learn_rune(p, rune_index(RUNE_VAR_FLAG, flag), true);
		}

		if (!object_runes_known(obj)) {
			/* Objects not fully known yet get marked as having had a chance
			 * to display all the timed flags */
			of_union(obj->known->flags, timed_mask);
		}
	}
}


/**
 * ------------------------------------------------------------------------
 * Object kind functions
 * These deal with knowledge of an object's kind
 * ------------------------------------------------------------------------ */

/**
 * Checks whether an object counts as "known" due to EASY_KNOW status
 *
 * \param obj is the object
 */
bool easy_know(const struct object *obj)
{
	assert(obj->kind);
	if (obj->kind->aware && kf_has(obj->kind->kind_flags, KF_EASY_KNOW))
		return true;
	else
		return false;
}

/**
 * Checks whether the player is aware of the object's flavour
 *
 * \param obj is the object
 */
bool object_flavor_is_aware(const struct object *obj)
{
	assert(obj->kind);
	return obj->kind->aware;
}

/**
 * Checks whether the player has tried to use other objects of the same kind
 *
 * \param obj is the object
 */
bool object_flavor_was_tried(const struct object *obj)
{
	assert(obj->kind);
	return obj->kind->tried;
}

/**
 * Mark an object's flavour as as one the player is aware of.
 *
 * \param p is the player becoming aware of the flavor
 * \param obj is the object whose flavour should be marked as aware
 */
void object_flavor_aware(struct player *p, struct object *obj)
{
	int y, x;
	int new_exp = 100;

	if (obj->kind->aware) return;
	obj->kind->aware = true;

	/* Quit if no dungeon yet */
	if (!cave) return;

	/* Gain experience for identification */
	player_exp_gain(p, new_exp);
	p->ident_exp += new_exp;
	update_player_object_knowledge(p);
	p->upkeep->notice |= PN_IGNORE;

	/* Some objects change tile on awareness, so update display for all
	 * floor objects of this kind */
	for (y = 1; y < cave->height; y++) {
		for (x = 1; x < cave->width; x++) {
			bool light = false;
			const struct object *floor_obj;
			struct loc grid = loc(x, y);

			for (floor_obj = square_object(cave, grid); floor_obj;
				 floor_obj = floor_obj->next)
				if (floor_obj->kind == obj->kind) {
					light = true;
					break;
				}

			if (light) square_light_spot(cave, grid);
		}
	}
}

/**
 * Mark an object's flavour as tried.
 *
 * \param obj is the object whose flavour should be marked
 */
void object_flavor_tried(struct object *obj)
{
	assert(obj);
	assert(obj->kind);
	/* Don't mark artifacts as tried */
	if (obj->kind->kidx >= z_info->ordinary_kind_max) {
		return;
	}
	obj->kind->tried = true;
}


/**
 * ------------------------------------------------------------------------
 * Object value
 * ------------------------------------------------------------------------ */
/**
 * Return the "value" of an "unknown" item
 * Make a guess at the value of non-aware items
 */
static int object_value_base(const struct object *obj)
{
	int value = 0;
	struct object_kind *kind = obj->kind;
	
	/* Use template cost for aware objects */
	if (object_flavor_is_aware(obj)) {		
		/* Give credit for hit bonus */
		value += (obj->att - kind->att) * 100;

		/* Give credit for evasion bonus */
		value += (obj->evn - kind->evn) * 100;

		/* Give credit for sides bonus */
		value += (obj->ps - kind->ps) * obj->pd * 100;

		/* Give credit for dice bonus */
		value += (obj->pd - kind->pd) * obj->ps * 100;
		
		/* Give credit for sides bonus */
		value += (obj->ds - kind->ds) * 100;

		/* Give credit for dice bonus */
		value += (obj->dd - kind->dd) * obj->ds * 100;
		
		/* Arrows are worth less since they are perishable */
		if (tval_is_ammo(obj)) value /= 10;
		
		/* Add in the base cost from the template */
		value += kind->cost;
	} else {
		/* Analyze the type */
		switch (obj->tval) {
			/* Un-aware Food */
			case TV_FOOD: return 5;

			/* Un-aware Potions */
			case TV_POTION: return 20;

			/* Un-aware Staffs */
			case TV_STAFF: return 70;

			/* Un-aware Rods */
			case TV_HORN: return 90;

			/* Un-aware Rings */
			case TV_RING: return 45;

			/* Un-aware Amulets */
			case TV_AMULET: return 45;
		}
	}

	return value;
}


/**
 * Return the "real" price of a "known" item, not including discounts.
 *
 * Wand and staffs get cost for each charge.
 *
 * Armor is worth an extra 100 gold per bonus point to armor class.
 *
 * Weapons are worth an extra 100 gold per bonus point (AC,TH,TD).
 *
 * Missiles are only worth 5 gold per bonus point, since they
 * usually appear in groups of 20, and we want the player to get
 * the same amount of cash for any "equivalent" item.  Note that
 * missiles never have any of the "pval" flags, and in fact, they
 * only have a few of the available flags, primarily of the "slay"
 * and "brand" and "ignore" variety.
 *
 * Weapons with negative hit+damage bonuses are worthless.
 *
 * Every wearable item with a "pval" bonus is worth extra (see below).
 */
static int object_value_real(const struct object *obj)
{
	int value, i;
	struct object_kind *kind = obj->kind;

	/* Hack -- "worthless" items */
	if (!kind->cost) return 0;

	/* Base cost */
	value = kind->cost;

	/* Artefact */
	if (obj->known->artifact) {
		const struct artifact *art = obj->artifact;

		/* Hack -- "worthless" artefacts */
		if (!art->cost) return 0;

		/* Hack -- Use the artefact cost instead */
		value = art->cost;
	} else if (obj->known->ego) {
		/* Ego-Item */
		struct ego_item *ego = obj->ego;

		/* Hack -- "worthless" special items */
		if (!ego->cost) return 0;

		/* Hack -- Reward the special item with a bonus */
		value += ego->cost;
	}


	/* Analyze modifiers and speed */
	switch (obj->tval) {
		case TV_ARROW:
		case TV_BOW:
		case TV_DIGGING:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_SWORD:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		case TV_LIGHT:
		case TV_AMULET:
		case TV_RING: {
			for (i = 0; i < OBJ_MOD_MAX; i++) {
				if (i < STAT_MAX) {
					value += (obj->known->modifiers[i] * 300);
				} else if (i < SKILL_MAX) {
					if (obj->known->modifiers[i] < 0) {
						return 0;
					} else {
						value += (obj->known->modifiers[i] * 100);
					}
				} else if (i == OBJ_MOD_TUNNEL) {
					if (obj->known->modifiers[i] < 0) {
						return 0;
					} else {
						value += (obj->known->modifiers[i] * 50);
					}
				}
			}

			/* Give credit for speed bonus */
			if (of_has(obj->known->flags, OF_SPEED)) value += 1000;

			break;
		}
	}


	/* Analyze the item */
	switch (obj->tval) {
		/* Staffs */
		case TV_STAFF:
		{
			/* Pay extra for charges, depending on standard number of
			 * charges.  Handle new-style wands correctly.
			 */
			value += ((value / 20) * (obj->known->pval / obj->known->number));

			/* Done */
			break;
		}

		/* Rings/Amulets */
		case TV_RING:
		case TV_AMULET: {
			/* Hack -- negative bonuses are bad */
			if (obj->known->att < 0) return 0;
			if (obj->known->evn < 0) return 0;

			/* Give credit for bonuses */
			value += ((obj->known->att + obj->known->evn + obj->known->ps)
					  * 100);

			/* Done */
			break;
		}

		/* Armor */
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_CLOAK:
		case TV_CROWN:
		case TV_HELM:
		case TV_SHIELD:
		case TV_SOFT_ARMOR:
		case TV_MAIL: {
			/* Give credit for hit bonus */
			value += ((obj->known->att - kind->att) * 100);

			/* Give credit for evasion bonus */
			value += ((obj->known->evn - kind->evn) * 100);

			/* Give credit for sides bonus */
			value += ((obj->known->ps - kind->ps) * obj->pd * 50);

			/* Give credit for dice bonus */
			value += ((obj->known->pd - kind->pd) * obj->ps * 50);

			/* Done */
			break;
		}

		/* Bows/Weapons */
		case TV_BOW:
		case TV_DIGGING:
		case TV_HAFTED:
		case TV_SWORD:
		case TV_POLEARM: {
			/* Give credit for hit bonus */
			value += ((obj->known->att - kind->att) * 100);

			/* Give credit for evasion bonus */
			value += ((obj->known->evn - kind->evn) * 100);

			/* Give credit for sides bonus */
			value += ((obj->known->ds - kind->ds) * obj->known->dd * 51);

			/* Give credit for dice bonus */
			value += ((obj->known->dd - kind->dd) * obj->known->ds * 51);

			/* Done */
			break;
		}

		/* Arrows */
		case TV_ARROW: {
			/* Give credit for hit bonus */
			value += ((obj->known->att - kind->att) * 10);

			/* Done */
			break;
		}
	}

	/* No negative value */
	if (value < 0) value = 0;

	/* Return the value */
	return value;
}


/**
 * Return the price of an item including plusses (and charges).
 *
 * This function returns the "value" of the given item (qty one).
 *
 * Never notice "unknown" bonuses or properties, including "curses",
 * since that would give the player information he did not have.
 */
int object_value(const struct object *obj)
{
	int value;

	/* Known items acquire the actual value, unknown items the base value */
	if (obj->known) {
		/* Real value (see above) */
		value = object_value_real(obj);
	} else {
		/* Base value (see above) */
		value = object_value_base(obj);
	}

	/* Return the final value */
	return value;
}

