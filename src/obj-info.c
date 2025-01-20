/**
 * \file obj-info.c
 * \brief Object description code.
 *
 * Copyright (c) 2010 Andi Sidwell
 * Copyright (c) 2004 Robert Ruehlmann
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
#include "cmds.h"
#include "effects.h"
#include "effects-info.h"
#include "game-world.h"
#include "init.h"
#include "monster.h"
#include "mon-util.h"
#include "obj-gear.h"
#include "obj-info.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "project.h"
#include "tutorial.h"
#include "z-textblock.h"

/**
 * Describes the number of blows possible for given stat bonuses
 */
struct blow_info {
	int str_plus;
	int dex_plus;  
	int centiblows;
};

/**
 * ------------------------------------------------------------------------
 * Data tables
 * ------------------------------------------------------------------------ */

static const struct origin_type {
	int type;
	int args;
	const char *desc;
} origins[] = {
	#define ORIGIN(a, b, c) { ORIGIN_##a, b, c },
	#include "list-origins.h"
	#undef ORIGIN
};


/**
 * ------------------------------------------------------------------------
 * List-writing utility code
 * ------------------------------------------------------------------------ */

/**
 * Given an array of strings, as so:
 *  { "intelligence", "fish", "lens", "prime", "number" },
 *
 * ... output a list like "intelligence, fish, lens, prime, number.\n".
 */
static void info_out_list(textblock *tb, const char *list[], size_t count)
{
	size_t i;

	for (i = 0; i < count; i++) {
		textblock_append(tb, "%s", list[i]);
		if (i != (count - 1)) textblock_append(tb, ", ");
	}

	textblock_append(tb, ".\n");
}


/**
 * Fills recepticle with all the elements that correspond to the given `list`.
 */
static size_t element_info_collect(const bool list[], const char *recepticle[])
{
	int i, count = 0;

	for (i = 0; i < ELEM_MAX; i++) {
		if (list[i])
			recepticle[count++] = projections[i].name;
	}

	return count;
}


/**
 * ------------------------------------------------------------------------
 * Code that makes use of the data tables to describe aspects of an 
 * object's information
 * ------------------------------------------------------------------------ */
/**
 * Describe stat modifications.
 */
static bool describe_stats(textblock *tb, const struct object *obj,
						   oinfo_detail_t mode)
{
	size_t count = 0, i;
	bool detail = false;

	/* Don't give exact plusses for faked ego items as each real one will
	 * be different */
	bool suppress_details = mode & (OINFO_EGO | OINFO_FAKE) ? true : false;

	/* Fact of but not size of mods is known for egos and flavoured items
	 * the player is aware of */
	bool known_effect = false;
	if (obj->known->ego)
		known_effect = true;
	if (tval_can_have_flavor_k(obj->kind) && object_flavor_is_aware(obj)) {
		known_effect = true;
	}

	/* See what we've got */
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		if (obj->modifiers[i]) {
			count++;
			detail = true;
		}
	}

	if (!count)
		return false;

	for (i = 0; i < OBJ_MOD_MAX; i++) {
		const char *desc = lookup_obj_property(OBJ_PROPERTY_MOD, i)->name;
		int val = obj->known->modifiers[i];
		if (!val) continue;

		/* Actual object */
		if (detail && !suppress_details) {
			int attr = (val > 0) ? COLOUR_L_GREEN : COLOUR_RED;
			textblock_append_c(tb, attr, "%+i %s.\n", val, desc);
		} else if (known_effect) {
			/* Ego type or jewellery description */
			textblock_append(tb, "Affects your %s\n", desc);
		}
	}

	return true;
}


/**
 * Describe immunities, resistances and vulnerabilities granted by an object.
 */
static bool describe_elements(textblock *tb,
							  const struct element_info el_info[])
{
	const char *r_descs[ELEM_MAX];
	const char *v_descs[ELEM_MAX];
	size_t i, count;

	bool list[ELEM_MAX], prev = false;

	/* Resistances */
	for (i = 0; i < ELEM_MAX; i++)
		list[i] = (el_info[i].res_level == 1);
	count = element_info_collect(list, r_descs);
	if (count) {
		textblock_append(tb, "Provides resistance to ");
		info_out_list(tb, r_descs, count);
		prev = true;
	}

	/* Vulnerabilities */
	for (i = 0; i < ELEM_MAX; i++)
		list[i] = (el_info[i].res_level == -1);
	count = element_info_collect(list, v_descs);
	if (count) {
		textblock_append(tb, "Makes you vulnerable to ");
		info_out_list(tb, v_descs, count);
		prev = true;
	}

	return prev;
}


/**
 * Describe protections granted by an object.
 */
static bool describe_protects(textblock *tb, const bitflag flags[OF_SIZE])
{
	const char *p_descs[OF_MAX];
	int i, count = 0;

	/* Protections */
	for (i = 1; i < OF_MAX; i++) {
		struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_FLAG, i);
		if (!prop || (prop->subtype != OFT_PROT)) continue;
		if (of_has(flags, prop->index)) {
			p_descs[count++] = prop->desc;
		}
	}

	if (!count)
		return false;

	textblock_append(tb, "Provides protection from ");
	info_out_list(tb, p_descs, count);

	return  true;
}

/**
 * Describe elements an object ignores.
 */
static bool describe_ignores(textblock *tb, const struct element_info el_info[])
{
	const char *descs[ELEM_MAX];
	size_t i, count;
	bool list[ELEM_MAX];

	for (i = 0; i < ELEM_MAX; i++)
		list[i] = (el_info[i].flags & EL_INFO_IGNORE);
	count = element_info_collect(list, descs);

	if (!count)
		return false;

	textblock_append(tb, "Cannot be harmed by ");
	info_out_list(tb, descs, count);

	return true;
}

/**
 * Describe elements that damage or destroy an object.
 */
static bool describe_hates(textblock *tb, const struct element_info el_info[])
{
	const char *descs[ELEM_MAX];
	size_t i, count = 0;
	bool list[ELEM_MAX];

	for (i = 0; i < ELEM_MAX; i++)
		list[i] = (el_info[i].flags & EL_INFO_HATES);
	count = element_info_collect(list, descs);

	if (!count)
		return false;

	textblock_append(tb, "Can be destroyed by ");
	info_out_list(tb, descs, count);

	return true;
}


/**
 * Describe stat sustains.
 */
static bool describe_sustains(textblock *tb, const bitflag flags[OF_SIZE])
{
	const char *descs[STAT_MAX];
	int i, count = 0;

	for (i = 0; i < STAT_MAX; i++) {
		struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_STAT, i);
		if (of_has(flags, sustain_flag(prop->index)))
			descs[count++] = prop->name;
	}

	if (!count)
		return false;

	textblock_append(tb, "Sustains ");
	info_out_list(tb, descs, count);

	return true;
}


/**
 * Describe miscellaneous powers.
 */
static bool describe_misc_magic(textblock *tb, const bitflag flags[OF_SIZE])
{
	int i;
	bool printed = false;

	for (i = 1; i < OF_MAX; i++) {
		struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_FLAG, i);
		if (!prop || prop->subtype == OFT_PROT) continue;
		if (of_has(flags, prop->index) && prop->desc &&
				!contains_only_spaces(prop->desc)) {
			textblock_append(tb, "%s.  ", prop->desc);
			printed = true;
		}
	}

	if (printed)
		textblock_append(tb, "\n");

	return printed;
}


/**
 * Describe abilities granted by an object.
 */
static bool describe_abilities(textblock *tb, const struct object *obj,
		oinfo_detail_t mode)
{
	const char *name[8];
	int ac = 0;
	struct ability *ability;
	bool known, known_kind, known_ego;

	/* Count its abilities.  If we're neither spoiling nor smithing, only
	 * include known abilities or those known from the kind or ego. */
	known = (mode & OINFO_SPOIL) || (mode & OINFO_SMITH);
	known_kind = obj->kind && obj->kind->aware;
	known_ego = obj->ego && obj->ego->aware;
	for (ability = obj->abilities; ability; ability = ability->next) {
		if (!known
			&& (!known_kind || !locate_ability(obj->kind->abilities, ability))
			&& (!known_ego || !locate_ability(obj->ego->abilities, ability))
			&& (!locate_ability(obj->known->abilities, ability))) {
			continue;
		}
		assert(ac < (int)N_ELEMENTS(name));
		name[ac++] = ability->name;
	}

	/* Describe */
	if (ac) {
		/* Output intro */
		if (ac == 1) {
			textblock_append(tb, "It grants you the ability: ");
		} else {
			textblock_append(tb, "It grants you the abilities: ");
		}

		/* Output list */
		info_out_list(tb, name, ac);

		/* It granted abilities */
		return true;
	}

	/* No abilities granted */
	return false;
}

/**
 * Describe attributes of bows and arrows.
 */
static bool describe_archery(textblock *tb, const struct object *obj)
{
	if (tval_is_launcher(obj)) {
		textblock_append(tb, "It can shoot arrows %d squares (with "
			"your current strength).", archery_range(obj));
		textblock_append(tb, "\n");
		return true;
	}
	if (tval_is_ammo(obj)) {
		struct object *bow = equipped_item_by_slot_name(player, "shooting");
		if (bow) {
			if (obj->number == 1) {
				textblock_append(tb, "It can be shot %d "
					"squares (with your current strength "
					"and bow).", archery_range(bow));
			} else {
				textblock_append(tb, "They can be shot %d "
					"squares (with your current strength "
					"and bow).", archery_range(bow));
			}
		} else {
			if (obj->number == 1) {
				textblock_append(tb, "It can be shot by a bow.");
			} else {
				textblock_append(tb, "They can be shot by a bow.");
			}
		}
		textblock_append(tb, "\n");
		return true;
	}
	
	/* Not archery related */
	return false;
}

/**
 * Describe attributes of throwing weapons.
 */
static bool describe_throwing(textblock *tb, const struct object *obj)
{
	if (obj_is_throwing(obj)) {
		textblock_append(tb, "It can be thrown effectively (%d "
			"squares with your current strength).",
			throwing_range(obj));
		textblock_append(tb, "\n");
		return true;
	}

	return false;
}

/**
 * Describe slays and brands on weapons
 */
static bool describe_slays(textblock *tb, const struct object *obj,
		oinfo_detail_t mode)
{
	int i, count = 0;
	bool known = (mode & OINFO_SPOIL) || (mode & OINFO_SMITH);
	const bool *s = known ? obj->slays : obj->known->slays;

	if (!s) return false;


	/* Count its slays.  If we're neither spoiling nor smithing, only
	 * include known slays or those known from the kind or ego. */
	known = (mode & OINFO_SPOIL) || (mode & OINFO_SMITH);
	for (i = 1; i < z_info->slay_max; i++) {
		if (s[i]) {
			count++;
		}
	}
	if (!count) return false;

	if (tval_is_weapon(obj) || tval_is_fuel(obj))
		textblock_append(tb, "Slays ");
	else
		textblock_append(tb, "It causes your melee attacks to slay ");

	assert(count >= 1);
	for (i = 1; i < z_info->slay_max; i++) {
		if (!s[i]) continue;

		textblock_append(tb, "%s", slays[i].name);
		if (count > 1)
			textblock_append(tb, ", ");
		else
			textblock_append(tb, ".\n");
		count--;
	}

	return true;
}

/**
 * Describe slays and brands on weapons
 */
static bool describe_brands(textblock *tb, const struct object *obj,
		oinfo_detail_t mode)
{
	int i, count = 0;
	bool known = (mode & OINFO_SPOIL) || (mode & OINFO_SMITH);
	bool *b = known ? obj->brands : obj->known->brands;

	if (!b) return false;

	/* Count its brands.  If we're neither spoiling nor smithing, only
	 * include known brands or those known from the kind or ego. */
	for (i = 1; i < z_info->brand_max; i++) {
		if (b[i]) {
			count++;
		}
	}
	if (!count) return false;

	if (tval_is_weapon(obj) || tval_is_fuel(obj))
		textblock_append(tb, "Branded with ");
	else
		textblock_append(tb, "It brands your melee attacks with ");

	assert(count >= 1);
	for (i = 1; i < z_info->brand_max; i++) {
		if (!b[i]) continue;

		textblock_append(tb, "%s", brands[i].name);
		if (count > 1)
			textblock_append(tb, ", ");
		else
			textblock_append(tb, ".\n");
		count--;
	}

	return true;
}

/**
 * Get the object flags the player should know about for the given object/
 * viewing mode combination.
 */
static void get_known_flags(const struct object *obj, const oinfo_detail_t mode,
							bitflag flags[OF_SIZE])
{
	/* Grab the object flags */
	if ((mode & OINFO_EGO) || (mode & OINFO_SPOIL)
			|| (mode & OINFO_SMITH)) {
		object_flags(obj, flags);
	} else {
		object_flags_known(obj, flags);
	}
	/* Don't include base flags when terse */
	if (mode & OINFO_TERSE) {
		of_diff(flags, obj->kind->base->flags);
	}
}

/**
 * Get the object element info the player should know about for the given
 * object/viewing mode combination.
 */
static void get_known_elements(const struct object *obj,
							   const oinfo_detail_t mode,
							   struct element_info el_info[])
{
	size_t i;

	/* Grab the element info */
	for (i = 0; i < ELEM_MAX; i++) {
		/* Report fake egos or known element info */
		if (player->obj_k->el_info[i].res_level || (mode & OINFO_SPOIL)
				|| (mode & OINFO_SMITH))
			el_info[i].res_level = obj->known->el_info[i].res_level;
		else
			el_info[i].res_level = 0;
		el_info[i].flags = obj->known->el_info[i].flags;

		/* Ignoring an element: */
		if (obj->el_info[i].flags & EL_INFO_IGNORE) {
			/* If the object is usually destroyed, mention the ignoring; */
			if (obj->el_info[i].flags & EL_INFO_HATES)
				el_info[i].flags &= ~(EL_INFO_HATES);
			/* Otherwise, don't say anything */
			else
				el_info[i].flags &= ~(EL_INFO_IGNORE);
		}

		/* Don't include hates flag when terse */
		if (mode & OINFO_TERSE)
			el_info[i].flags &= ~(EL_INFO_HATES);
	}
}

/**
 * Gives the known light-sourcey characteristics of the given object.
 *
 * Fills in the intensity of the light in `intensity`, whether it uses fuel and
 * how many turns light it can refuel in similar items.
 *
 * Return false if the object is not known to be a light source (which 
 * includes it not actually being a light source).
 */
static bool obj_known_light(const struct object *obj, oinfo_detail_t mode,
		const bitflag flags[OF_SIZE], int *intensity, bool *uses_fuel,
		int *refuel_turns)
{
	bool no_fuel;
	bool is_light = tval_is_light(obj);

	if (!is_light)
		return false;

	/*
	 * Work out intensity; smithing can use pval for the special bonus so
	 * look at the kind's pval for the radius in that case
	 */
	*intensity = (mode & OINFO_SMITH) ? obj->kind->pval : obj->pval;
	if (of_has(flags, OF_LIGHT)) {
		++*intensity;
	}

	/* Prevent unidentified objects (especially artifact lights) from showing
	 * bad intensity and refueling info. */
	if (*intensity == 0)
		return false;

	no_fuel = of_has(flags, OF_NO_FUEL) ? true : false;

	if (no_fuel || obj->artifact) {
		*uses_fuel = false;
	} else {
		*uses_fuel = true;
	}

	if (is_light && of_has(flags, OF_TAKES_FUEL)) {
		*refuel_turns = z_info->fuel_lamp;
	} else {
		*refuel_turns = 0;
	}

	return true;
}

/**
 * Describe things that look like lights.
 */
static bool describe_light(textblock *tb, const struct object *obj,
		oinfo_detail_t mode, const bitflag flags[OF_SIZE])
{
	int intensity = 0;
	bool uses_fuel = false;
	int refuel_turns = 0;
	bool terse = mode & OINFO_TERSE ? true : false;

	if (!obj_known_light(obj, mode, flags, &intensity, &uses_fuel, &refuel_turns))
		return false;

	if (tval_is_light(obj)) {
		textblock_append(tb, "Intensity ");
		textblock_append_c(tb, COLOUR_L_GREEN, "%d", intensity);
		textblock_append(tb, " light.");

		if (!obj->artifact && !uses_fuel)
			textblock_append(tb, "  No fuel required.");

		if (!terse) {
			if (refuel_turns)
				textblock_append(tb, "  Refills other lanterns up to %d turns of fuel.", refuel_turns);
		}
		textblock_append(tb, "\n");
	}

	return true;
}


/**
 * Describe an item's origin
 */
static bool describe_origin(textblock *tb, const struct object *obj, bool terse)
{
	char loot_spot[80];
	char name[80];
	int origin;
	const char *dropper = NULL;
	const char *article;
	bool unique = false;
	bool comma = false;

	/* Only give this info in chardumps if wieldable */
	if (terse && !obj_can_wear(obj))
		return false;

	/* Set the origin */
	origin = obj->origin;

	/* Name the place of origin */
	if (obj->origin_depth)
		strnfmt(loot_spot, sizeof(loot_spot), "at %d feet",
		        obj->origin_depth * 50);
	else
		my_strcpy(loot_spot, "on the surface", sizeof(loot_spot));

	/* Name the monster of origin */
	if (obj->origin_race) {
		dropper = obj->origin_race->name;
		if (rf_has(obj->origin_race->flags, RF_UNIQUE)) {
			unique = true;
		}
		if (rf_has(obj->origin_race->flags, RF_NAME_COMMA)) {
			comma = true;
		}
	} else {
		dropper = "monster lost to history";
	}
	article = is_a_vowel(dropper[0]) ? "an " : "a ";
	if (unique)
		my_strcpy(name, dropper, sizeof(name));
	else {
		my_strcpy(name, article, sizeof(name));
		my_strcat(name, dropper, sizeof(name));
	}
	if (comma) {
		my_strcat(name, ",", sizeof(name));
	}

	/* Print an appropriate description */
	switch (origins[origin].args)
	{
		case -1: return false;
		case 0: textblock_append(tb, "%s", origins[origin].desc); break;
		case 1: textblock_append(tb, origins[origin].desc, loot_spot);
				break;
		case 2:
			textblock_append(tb, origins[origin].desc, name, loot_spot);
			break;
	}

	textblock_append(tb, "\n\n");

	return true;
}

/**
 * Print an item's flavour text.
 *
 * \param tb is the textblock to which we are adding.
 * \param obj is the object we are describing.
 * \param ego is whether we're describing an ego template (as opposed to a
 * real object)
 */
static void describe_flavor_text(textblock *tb, const struct object *obj,
								 bool ego, bool smith)
{
	/* Display the known artifact or object description */
	if (obj->artifact && obj->artifact->text) {
		textblock_append(tb, "%s\n\n", obj->artifact->text);
	} else if (obj->kind->tval == TV_NOTE
			&& streq(obj->kind->name, "tutorial note")) {
		/*
		 * Tutorial notes don't have a static description
		 * available in obj->kind->text.  Use
		 * tutorial_expand_message() instead.
		 */
		textblock *note_tb = tutorial_expand_message(obj->pval);

		textblock_append_textblock(tb, note_tb);
		textblock_free(note_tb);
	} else if (object_flavor_is_aware(obj) || ego || smith) {
		bool did_desc = false;

		if (!ego && obj->kind->text) {
			textblock_append(tb, "%s", obj->kind->text);
			did_desc = true;
		}

		/* Display an additional ego-item description */
		if ((ego || (obj->ego != NULL)) && obj->ego->text) {
			if (did_desc) textblock_append(tb, "  ");
			textblock_append(tb, "%s\n\n", obj->ego->text);
		} else if (did_desc) {
			textblock_append(tb, "\n\n");
		}
	}
}

/**
 * ------------------------------------------------------------------------
 * Output code
 * ------------------------------------------------------------------------ */
/**
 * Output object information
 */
static textblock *object_info_out(const struct object *obj, int mode)
{
	bitflag flags[OF_SIZE];
	struct element_info el_info[ELEM_MAX];
	bool something = false;

	bool terse = mode & OINFO_TERSE ? true : false;
	bool subjective = mode & OINFO_SUBJ ? true : false;
	bool ego = mode & OINFO_EGO ? true : false;
	bool smith = mode & OINFO_SMITH ? true : false;
	textblock *tb = textblock_new();

	assert(obj->known);

	/* Unaware objects get simple descriptions */
	if (obj->kind != obj->known->kind) {
		textblock_append(tb, "\n\nYou do not know what this is.\n");
		return tb;
	}

	/* Grab the object flags */
	get_known_flags(obj, mode, flags);

	/* Grab the element info */
	get_known_elements(obj, mode, el_info);

	if (subjective) describe_origin(tb, obj, terse);
	if (!terse) describe_flavor_text(tb, obj, ego, smith);

	if (!object_runes_known(obj) &&	(obj->known->notice & OBJ_NOTICE_ASSESSED)
		&& !tval_is_useable(obj)) {
		textblock_append(tb, "You do not know the full extent of this item's powers.\n");
		something = true;
	}

	if (describe_stats(tb, obj, mode)) something = true;
	if (describe_slays(tb, obj, mode)) something = true;
	if (describe_brands(tb, obj, mode)) something = true;
	if (describe_elements(tb, el_info)) something = true;
	if (describe_protects(tb, flags)) something = true;
	if (describe_sustains(tb, flags)) something = true;
	if (describe_misc_magic(tb, flags)) something = true;
	if (describe_abilities(tb, obj, mode)) something = true;
	if (describe_archery(tb, obj)) something = true;
	if (describe_throwing(tb, obj)) something = true;
	if (describe_light(tb, obj, mode, flags)) something = true;
	if (describe_ignores(tb, el_info)) something = true;
	if (describe_hates(tb, el_info)) something = true;
	if (something) textblock_append(tb, "\n");

	/* Don't append anything in terse (for chararacter dump) */
	if (!something && !terse && !smith && !object_effect(obj))
		textblock_append(tb, "\n\nThis item does not seem to possess any special abilities.");

	return tb;
}


/**
 * Provide information on an item, including how it would affect the current
 * player's state.
 *
 * returns true if anything is printed.
 */
textblock *object_info(const struct object *obj, oinfo_detail_t mode)
{
	mode |= OINFO_SUBJ;
	return object_info_out(obj, mode);
}

/**
 * Provide information on an ego-item type
 */
textblock *object_info_ego(struct ego_item *ego)
{
	struct object_kind *kind = NULL;
	struct object obj = OBJECT_NULL;
	size_t i;
	textblock *result;

	for (i = 0; i < z_info->k_max; i++) {
		kind = &k_info[i];
		if (!kind->name)
			continue;
		if (i == ego->poss_items->kidx)
			break;
	}

	obj.kind = kind;
	obj.tval = kind->tval;
	obj.sval = kind->sval;
	obj.ego = ego;
	ego_apply_magic(&obj, 0);


	result = object_info_out(&obj, OINFO_NONE | OINFO_EGO);
	object_wipe(&obj);
	return result;
}



/**
 * Provide information on an item suitable for writing to the character dump
 * - keep it brief.
 */
void object_info_chardump(ang_file *f, const struct object *obj, int indent,
						  int wrap)
{
	textblock *tb = object_info_out(obj, OINFO_TERSE | OINFO_SUBJ);
	textblock_to_file(tb, f, indent, wrap);
	textblock_free(tb);
}


/**
 * Provide spoiler information on an item.
 *
 * Practically, this means that we should not print anything which relies upon
 * the player's current state, since that is not suitable for spoiler material.
 */
void object_info_spoil(ang_file *f, const struct object *obj, int wrap)
{
	textblock *tb = object_info_out(obj, OINFO_SPOIL);
	textblock_to_file(tb, f, 0, wrap);
	textblock_free(tb);
}
