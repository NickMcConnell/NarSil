/**
 * \file obj-util.c
 * \brief Object utilities
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
#include "effects.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "grafmode.h"
#include "init.h"
#include "mon-make.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-util.h"
#include "randname.h"
#include "z-queue.h"

struct object_base *kb_info;
struct object_kind *k_info;
struct artifact *a_info;
struct artifact_upkeep *aup_info;
struct ego_item *e_info;
struct flavor *flavors;

static void flavor_assign_fixed(void)
{
	int i;
	struct flavor *f;

	for (f = flavors; f; f = f->next) {
		if (f->sval == SV_UNKNOWN)
			continue;

		for (i = 0; i < z_info->k_max; i++) {
			struct object_kind *k = &k_info[i];
			if (k->tval == f->tval && k->sval == f->sval)
				k->flavor = f;
		}
	}
}


static void flavor_assign_random(uint8_t tval)
{
	int i;
	int flavor_count = 0;
	int choice;
	struct flavor *f;

	/* Count the random flavors for the given tval */
	for (f = flavors; f; f = f->next)
		if (f->tval == tval && f->sval == SV_UNKNOWN)
			flavor_count++;

	for (i = 0; i < z_info->k_max; i++) {
		if (k_info[i].tval != tval || k_info[i].flavor)
			continue;

		if (!flavor_count)
			quit_fmt("Not enough flavors for tval %d.", tval);

		choice = randint0(flavor_count);
	
		for (f = flavors; f; f = f->next) {
			if (f->tval != tval || f->sval != SV_UNKNOWN)
				continue;

			if (choice == 0) {
				k_info[i].flavor = f;
				f->sval = k_info[i].sval;
				flavor_count--;
				break;
			}

			choice--;
		}
	}
}

/**
 * Prepare the "variable" part of the "k_info" array.
 *
 * The "color"/"metal"/"type" of an item is its "flavor".
 * For the most part, flavors are assigned randomly each game.
 *
 * Initialize descriptions for the "colored" objects, including:
 * Rings, Amulets, Staffs, Wands, Rods, Mushrooms, Potions, Scrolls.
 *
 * Scroll titles are always between 6 and 14 letters long.  This is
 * ensured because every title is composed of whole words, where every
 * word is from 2 to 8 letters long, and that no scroll is finished
 * until it attempts to grow beyond 15 letters.  The first time this
 * can happen is when the current title has 6 letters and the new word
 * has 8 letters, which would result in a 6 letter scroll title.
 *
 * Hack -- make sure everything stays the same for each saved game
 * This is accomplished by the use of a saved "random seed", as in
 * "town_gen()".  Since no other functions are called while the special
 * seed is in effect, so this function is pretty "safe".
 */
void flavor_init(void)
{
	int i;

	/* Hack -- Use the "simple" RNG */
	Rand_quick = true;

	/* Hack -- Induce consistant flavors */
	Rand_value = seed_flavor;

	/* Scrub all flavors and re-parse for new players */
	if (turn == 1) {
		struct flavor *f;

		for (i = 0; i < z_info->k_max; i++) {
			k_info[i].flavor = NULL;
		}
		for (f = flavors; f; f = f->next) {
			f->sval = SV_UNKNOWN;
		}
		cleanup_parser(&flavor_parser);
		run_parser(&flavor_parser);
	}

	flavor_assign_fixed();

	flavor_assign_random(TV_RING);
	flavor_assign_random(TV_AMULET);
	flavor_assign_random(TV_STAFF);
	flavor_assign_random(TV_HORN);
	flavor_assign_random(TV_HERB);
	flavor_assign_random(TV_POTION);

	/* Hack -- Use the "complex" RNG */
	Rand_quick = false;

	/* Analyze every object */
	for (i = 0; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];

		/* Skip "empty" objects */
		if (!kind->name) continue;

		/* No flavor yields aware */
		if (!kind->flavor) kind->aware = true;
	}
}

/**
 * Set all flavors as aware
 */
void flavor_set_all_aware(void)
{
	int i;

	/* Analyze every object */
	for (i = 0; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];

		/* Skip empty objects */
		if (!kind->name) continue;

		/* Flavor yields aware */
		if (kind->flavor) kind->aware = true;
	}
}

/**
 * Obtain the flags for an item
 */
void object_flags(const struct object *obj, bitflag flags[OF_SIZE])
{
	of_wipe(flags);
	if (!obj) return;
	of_copy(flags, obj->flags);
}


/**
 * Obtain the flags for an item which are known to the player
 */
void object_flags_known(const struct object *obj, bitflag flags[OF_SIZE])
{
	object_flags(obj, flags);
	of_inter(flags, obj->known->flags);

	if (!obj->kind) {
		return;
	}

	if (object_flavor_is_aware(obj)) {
		of_union(flags, obj->kind->flags);
	}
	if (obj->ego && easy_know(obj)) {
		of_union(flags, obj->ego->flags);
	}
}

/**
 * Apply a tester function, skipping all non-objects and gold
 */
bool object_test(item_tester tester, const struct object *obj)
{
	/* Require object */
	if (!obj) return false;

	/* Pass without a tester, or tail-call the tester if it exists */
	return !tester || tester(obj);
}


/**
 * Looks if "inscrip" is present on the given object.
 */
unsigned check_for_inscrip(const struct object *obj, const char *inscrip)
{
	unsigned i = 0;
	const char *s;

	if (!obj->note) return 0;

	s = quark_str(obj->note);

	/* Needing this implies there are bad instances of obj->note around,
	 * but I haven't been able to track down their origins - NRM */
	if (!s) return 0;

	do {
		s = strstr(s, inscrip);
		if (!s) break;

		i++;
		s++;
	} while (s);

	return i;
}

/**
 * Looks if "inscrip" immediately followed by a decimal integer without a
 * leading sign character is present on the given object.  Returns the number
 * of times such an inscription occurs and, if that value is at least one,
 * sets *ival to the value of the integer that followed the first such
 * inscription.
 */
unsigned check_for_inscrip_with_int(const struct object *obj, const char *inscrip, int* ival)
{
	unsigned i = 0;
	size_t inlen = strlen(inscrip);
	const char *s;

	if (!obj->note) return 0;

	s = quark_str(obj->note);

	/* Needing this implies there are bad instances of obj->note around,
	 * but I haven't been able to track down their origins - NRM */
	if (!s) return 0;

	do {
		s = strstr(s, inscrip);
		if (!s) break;
		if (isdigit(s[inlen])) {
			if (i == 0) {
				long inarg = strtol(s + inlen, 0, 10);

				*ival = (inarg < INT_MAX) ? (int) inarg : INT_MAX;
			}
			i++;
		}
		s++;
	} while (s);

	return i;
}

/*** Object kind lookup functions ***/

/**
 * Return the object kind with the given `tval` and `sval`, or NULL.
 */
struct object_kind *lookup_kind(int tval, int sval)
{
	int k;

	/* Look for it */
	for (k = 0; k < z_info->k_max; k++) {
		struct object_kind *kind = &k_info[k];
		if (kind->tval == tval && kind->sval == sval)
			return kind;
	}

	/* Failure */
	msg("No object: %d:%d (%s)", tval, sval, tval_find_name(tval));
	return NULL;
}

/**
 * Return the object kind that should be used as the basis for any smithed
 * artifact belonging to the given tval.  If there is no such kind -
 * indicating smithed artifacts should use a standard kind from that tval -
 * return NULL.
 */
struct object_kind *lookup_selfmade_kind(int tval)
{
	int k;

	for (k = 0; k < z_info->k_max; k++) {
		struct object_kind *kind = &k_info[k];

		if (kind->tval == tval
				&& kf_has(kind->kind_flags, KF_SMITH_ART)) {
			return kind;
		}
	}
	return NULL;
}

struct object_kind *objkind_byid(int kidx) {
	if (kidx < 0 || kidx >= z_info->k_max)
		return NULL;
	return &k_info[kidx];
}


/*** Textual<->numeric conversion ***/

/**
 * Return the a_idx of the artifact with the given name
 */
const struct artifact *lookup_artifact_name(const char *name)
{
	int i;
	int a_idx = -1;

	/* Look for it */
	for (i = 0; i < z_info->a_max; i++) {
		const struct artifact *art = &a_info[i];

		/* Test for equality */
		if (art->name && streq(name, art->name))
			return art;
		
		/* Test for close matches */
		if (strlen(name) >= 3 && art->name && my_stristr(art->name, name)
			&& a_idx == -1)
			a_idx = i;
	}

	/* Return our best match */
	return a_idx > 0 ? &a_info[a_idx] : NULL;
}

/**
 * \param name ego type name
 * \param tval object tval
 * \param sval object sval
 * \return eidx of the ego item type
 */
struct ego_item *lookup_ego_item(const char *name, int tval, int sval)
{
	struct object_kind *kind = lookup_kind(tval, sval);
	int i;

	/* Look for it */
	if (!kind) return NULL;
	for (i = 0; i < z_info->e_max; i++) {
		struct ego_item *ego = &e_info[i];
		struct poss_item *poss_item = ego->poss_items;

		/* Reject nameless and wrong names */
		if (!ego->name) continue;
		if (!streq(name, ego->name)) continue;

		/* Check tval and sval */
		while (poss_item) {
			if (kind->kidx == poss_item->kidx) {
				return ego;
			}
			poss_item = poss_item->next;
		}
	}

	return NULL;
}

/**
 * Return the numeric sval of the object kind with the given `tval` and
 * name `name`.
 */
int lookup_sval(int tval, const char *name)
{
	int k;
	char *pe;
	unsigned long r = strtoul(name, &pe, 10);

	if (pe != name) {
		return (contains_only_spaces(pe) && r < INT_MAX) ? (int)r : -1;
	}

	/* Look for it */
	for (k = 0; k < z_info->k_max; k++) {
		struct object_kind *kind = &k_info[k];
		char cmp_name[1024];

		if (!kind || !kind->name || kind->tval != tval) continue;

		obj_desc_name_format(cmp_name, sizeof cmp_name, 0, kind->name, 0,
							 false);

		/* Found a match */
		if (!my_stricmp(cmp_name, name)) return kind->sval;
	}

	return -1;
}

void object_short_name(char *buf, size_t max, const char *name)
{
	size_t j, k;
	/* Copy across the name, stripping modifiers & and ~) */
	size_t len = strlen(name);
	for (j = 0, k = 0; j < len && k < max - 1; j++) {
		if (j == 0 && name[0] == '&' && name[1] == ' ')
			j += 2;
		if (name[j] == '~')
			continue;

		buf[k++] = name[j];
	}
	buf[k] = 0;
}

/**
 * Sort comparator for objects using only tval and sval.
 * -1 if o1 should be first
 *  1 if o2 should be first
 *  0 if it doesn't matter
 */
static int compare_types(const struct object *o1, const struct object *o2)
{
	if (o1->tval == o2->tval)
		return CMP(o1->sval, o2->sval);
	else
		return CMP(o1->tval, o2->tval);
}	


/**
 * Sort comparator for objects
 * -1 if o1 should be first
 *  1 if o2 should be first
 *  0 if it doesn't matter
 *
 * The sort order is designed with the "list items" command in mind.
 */
int compare_items(const struct object *o1, const struct object *o2)
{
	/* known artifacts will sort first */
	if (o1->artifact && o2->artifact)
		return compare_types(o1, o2);
	if (o1->artifact) return -1;
	if (o2->artifact) return 1;

	/* unknown objects will sort next */
	if (!object_flavor_is_aware(o1) && !object_flavor_is_aware(o2))
		return compare_types(o1, o2);
	if (!object_flavor_is_aware(o1)) return -1;
	if (!object_flavor_is_aware(o2)) return 1;

	/* if only one of them is worthless, the other comes first */
	if (o1->kind->cost == 0 && o2->kind->cost != 0) return 1;
	if (o1->kind->cost != 0 && o2->kind->cost == 0) return -1;

	/* otherwise, just compare tvals and svals */
	/* NOTE: arguably there could be a better order than this */
	return compare_types(o1, o2);
}


/**
 * Convert a depth from a chunk or player to a value appropriate for an
 * object's origin.
 *
 * \param depth is the value to convert.
 *
 * Necessary since savefiles use 16-bit type to record the depth of a player
 * or chunk and uint8_t to record the origin depth.
 */
uint8_t convert_depth_to_origin(int depth)
{
	if (depth < 0) return 0;
	if (depth > 255) return 255;
	return (uint8_t) depth;
}


/**
 * Calculate the digging score for a digging implement
 */
int obj_digging_score(const struct object *obj)
{
	int base = 0;
	if (!obj) return 0;
	if (of_has(obj->flags, OF_DIG_1)) base = 1;
	if (of_has(obj->flags, OF_DIG_2)) base = 2;
	return base + obj->modifiers[OBJ_MOD_TUNNEL];
}

/**
 * Determine if an object is a digging implement
 */
bool obj_can_dig(const struct object *obj)
{
	return obj_digging_score(obj) > 0;
}

/**
 * Determine if an object has charges
 */
bool obj_has_charges(const struct object *obj)
{
	if (!tval_can_have_charges(obj)) return false;

	if (obj->pval <= 0) return false;

	return true;
}

/**
 * Check if an object can be used to refuel other objects.
 */
bool obj_can_refuel(const struct object *obj)
{
	const struct object *light = equipped_item_by_slot_name(player, "light");

	/* Player must be carrying a light which needs fuel */
	if (!light || of_has(light->flags, OF_NO_FUEL)) return false;

	/* A lantern can be refueled from a flask or another lantern */
	if (of_has(light->flags, OF_TAKES_FUEL)) {
		if (tval_is_fuel(obj)) {
			return true;
		} else if (tval_is_light(obj) && of_has(obj->flags, OF_TAKES_FUEL) &&
				   obj->timeout > 0) {
			return true;
		}
	}

	/* A torch can be refueled from another torch */
	if (of_has(light->flags, OF_BURNS_OUT)) {
		if (tval_is_light(obj) && of_has(obj->flags, OF_BURNS_OUT) &&
				   obj->timeout > 0) {
			return true;
		}
	}

	return false;
}

/* Can only take off cursed items in special circumstances */
bool obj_can_takeoff(const struct object *obj)
{
	return !obj_has_flag(obj, OF_CURSED)
		|| player_active_ability(player, "Curse Breaking");
}

/*
 * Can only throw an item that is not equipped or the equipped weapon if it
 * can be taken off.
 *
 * Note: that is Angband's behavior; Sil 1.3 allows other equippped items to
 * be thrown if they can be taken off.
 */
bool obj_can_throw(const struct object *obj)
{
	return !object_is_equipped(player->body, obj)
		|| (tval_is_melee_weapon(obj) && obj_can_takeoff(obj));
}

/* Can only put on wieldable items */
bool obj_can_wear(const struct object *obj)
{
	return (wield_slot(obj) >= 0);
}

/* Can only fire an item with the right tval */
bool obj_can_fire(const struct object *obj)
{
	return obj->tval == player->state.ammo_tval;
}

/**
 * Determine if an object is designed for throwing
 */
bool obj_is_throwing(const struct object *obj)
{
	return of_has(obj->flags, OF_THROWING);
}

bool obj_is_cursed(const struct object *obj)
{
	return of_has(obj->flags, OF_CURSED);
}

bool obj_is_broken(const struct object *obj)
{
	return (object_value(obj) <= 0);
}

/* Can has inscrip pls */
bool obj_has_inscrip(const struct object *obj)
{
	return (obj->note ? true : false);
}

bool obj_has_flag(const struct object *obj, int flag)
{
	/* Check the object's own flags */
	if (of_has(obj->flags, flag)) {
		return true;
	}

	return false;
}

bool obj_is_useable(const struct object *obj)
{
	if (tval_is_useable(obj))
		return true;

	if (object_effect(obj))
		return true;

	/*
	 * Could do further testing to see if the equipped object, if any, can
	 * be removed, but for items (like rings) that can go in more than one
	 * slot there are extra complications to the logic (using wield_slot()
	 * and slot_object() is not sufficient if all the appropriate slots
	 * are full and the slot from wield_slot() has a cursed item but
	 * another appropriate slot has an uncursed item).
	 */
	if (tval_is_wearable(obj) && !object_is_equipped(player->body, obj))
		return true;

	if (obj_can_refuel(obj))
		return true;

	if (tval_is_ammo(obj))
		return obj->tval == player->state.ammo_tval;

	return false;
}

bool obj_nourishes(const struct object *obj)
{
	struct effect *effect = obj->kind->effect;
	if (!effect) return false;
	while (effect) {
		if (effect->index == EF_NOURISH && effect->dice) {
			random_value rv;
			(void) dice_roll(effect->dice, &rv);
			if (rv.base > 0) {
				return true;
			}
		}
		effect = effect->next;
	}
	return false;
}

/*** Generic utility functions ***/

/**
 * Return an object's effect.
 */
struct effect *object_effect(const struct object *obj)
{
	if (obj->kind->effect)
		return obj->kind->effect;
	else
		return NULL;
}

/**
 * Does the given object need to be aimed?
 */ 
bool obj_needs_aim(const struct object *obj)
{
	const struct effect *effect = object_effect(obj);

	/* If the effect needs aiming, or if the object type needs
	   aiming, this object needs aiming. */
	return effect_aim(effect) || tval_is_ammo(obj);
}

/**
 * Can the given object be aimed vertically?
 */
bool obj_allows_vertical_aim(const struct object *obj)
{
	const struct effect *effect = object_effect(obj);

	while (effect) {
		if (effect->index == EF_TERRAIN_BEAM && effect->other == 1) {
			return true;
		}
		effect = effect->next;
	}
	return false;
}

/**
 * Distribute charges of rods, staves, or wands.
 *
 * \param source is the source item
 * \param dest is the target item, must be of the same type as source
 * \param amt is the number of items that are transfered
 * \param dest_new will, if true, ignore whatever charges dest has (i.e.
 * treat it as a new stack).
 */
void distribute_charges(struct object *source, struct object *dest, int amt,
		bool dest_new)
{
	/*
	 * Hack -- If rods, staves, or wands are dropped, the total maximum
	 * timeout or charges need to be allocated between the two stacks.
	 * If all the items are being dropped, it makes for a neater message
	 * to leave the original stack's pval alone. -LM-
	 */
	if (tval_can_have_charges(source)) {
		int change = source->pval * amt / source->number;

		if (dest_new) {
			dest->pval = change;
		} else {
			dest->pval += change;
		}
		if (amt < source->number) {
			source->pval -= change;
		}
	}
}

/**
 * Removes the curse from an object.
 */
void uncurse_object(struct object *obj)
{
	/* Uncurse it */
	if (!of_off(obj->flags, OF_CURSED)) {
		msg("Attempt to uncurse non-cursed object - please report this bug");
	}
	of_off(obj->known->flags, OF_CURSED);

	player->upkeep->notice |= (PN_COMBINE);
	player->upkeep->update |= (PU_BONUS);
	player->upkeep->redraw |= (PR_EQUIP | PR_INVEN);
}


/**
 * Verify the choice of an item.
 *
 * The item can be negative to mean "item on floor".
 */
bool verify_object(const char *prompt, const struct object *obj,
		const struct player *p)
{
	char o_name[80];

	char out_val[160];

	/* Describe */
	object_desc(o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL, p);

	/* Prompt */
	strnfmt(out_val, sizeof(out_val), "%s %s? ", prompt, o_name);

	/* Query */
	return (get_check(out_val));
}


typedef enum {
	MSG_TAG_NONE,
	MSG_TAG_NAME,
	MSG_TAG_KIND,
	MSG_TAG_VERB,
	MSG_TAG_VERB_IS
} msg_tag_t;

static msg_tag_t msg_tag_lookup(const char *tag)
{
	if (strncmp(tag, "name", 4) == 0) {
		return MSG_TAG_NAME;
	} else if (strncmp(tag, "kind", 4) == 0) {
		return MSG_TAG_KIND;
	} else if (strncmp(tag, "s", 1) == 0) {
		return MSG_TAG_VERB;
	} else if (strncmp(tag, "is", 2) == 0) {
		return MSG_TAG_VERB_IS;
	} else {
		return MSG_TAG_NONE;
	}
}

/**
 * Print a message from a string, customised to include details about an object
 */
void print_custom_message(const struct object *obj, const char *string,
		int msg_type, const struct player *p)
{
	char buf[1024] = "\0";
	const char *next;
	const char *s;
	const char *tag;
	size_t end = 0;

	/* Not always a string */
	if (!string) return;

	next = strchr(string, '{');
	while (next) {
		/* Copy the text leading up to this { */
		strnfcat(buf, 1024, &end, "%.*s", (int) (next - string),
			string);

		s = next + 1;
		while (*s && isalpha((unsigned char) *s)) s++;

		/* Valid tag */
		if (*s == '}') {
			/* Start the tag after the { */
			tag = next + 1;
			string = s + 1;

			switch(msg_tag_lookup(tag)) {
			case MSG_TAG_NAME:
				if (obj) {
					end += object_desc(buf, 1024, obj,
						ODESC_PREFIX | ODESC_BASE, p);
				} else {
					strnfcat(buf, 1024, &end, "hands");
				}
				break;
			case MSG_TAG_KIND:
				if (obj) {
					object_kind_name(&buf[end], 1024 - end, obj->kind, true);
					end += strlen(&buf[end]);
				} else {
					strnfcat(buf, 1024, &end, "hands");
				}
				break;
			case MSG_TAG_VERB:
				if (obj && obj->number == 1) {
					strnfcat(buf, 1024, &end, "s");
				}
				break;
			case MSG_TAG_VERB_IS:
				if ((!obj) || (obj->number > 1)) {
					strnfcat(buf, 1024, &end, "are");
				} else {
					strnfcat(buf, 1024, &end, "is");
				}
			default:
				break;
			}
		} else
			/* An invalid tag, skip it */
			string = next + 1;

		next = strchr(string, '{');
	}
	strnfcat(buf, 1024, &end, "%s", string);

	msgt(msg_type, "%s", buf);
}

/**
 * Return if the given artifact has been created.
 */
bool is_artifact_created(const struct artifact *art)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	return aup_info[art->aidx].created;
}

/**
 * Return if the given artifact has been seen.
 */
bool is_artifact_seen(const struct artifact *art)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	return aup_info[art->aidx].seen;
}

/**
 * Return if the given artifact has ever been seen.
 */
bool is_artifact_everseen(const struct artifact *art)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	return aup_info[art->aidx].everseen;
}

/**
 * Set whether the given artifact has been created or not.
 */
void mark_artifact_created(const struct artifact *art, bool created)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	aup_info[art->aidx].created = created;
}

/**
 * Set whether the given artifact has been created or not.
 */
void mark_artifact_seen(const struct artifact *art, bool seen)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	aup_info[art->aidx].seen = seen;
}

/**
 * Set whether the given artifact has been seen or not.
 */
void mark_artifact_everseen(const struct artifact *art, bool seen)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	aup_info[art->aidx].everseen = seen;
}

/**
 * Write ability lines for a set of abilities.
 */
static void write_abilities(ang_file *fff, const struct ability *abils)
{
	struct ability *ability = (struct ability *) abils;

	static const char *skill_names[] = {
		#define SKILL(a, b) b,
		#include "list-skills.h"
		#undef SKILL
		""
	};

	/* Write abilities */
	while (ability) {
		file_putf(fff, "ability:");
		file_putf(fff, "%s:", skill_names[ability->skill]);
		file_putf(fff, "%s\n", ability->name);
		ability = ability->next;
	}
}

/**
 * Write an artifact data file
 */
void write_self_made_artefact_entries(ang_file *fff)
{
	int i;
	static const char *obj_flags[] = {
		"NONE",
		#define OF(a, b) #a,
		#include "list-object-flags.h"
		#undef OF
		NULL
	};

	/* Write individual entries */
	for (i = z_info->a_max - player->self_made_arts; i < z_info->a_max; i++) {
		const struct artifact *art = &a_info[i];
		char name[120] = "";
		struct object_kind *kind = lookup_kind(art->tval, art->sval);
		int j;

		/* Ignore non-existent artifacts */
		if (!art->name) continue;

		/* Output description */
		file_putf(fff, "# %s\n", art->text);

		/* Output name */
		file_putf(fff, "name:%s\n", art->name);

		/* Output tval and sval */
		object_short_name(name, sizeof name, kind->name);
		file_putf(fff, "base-object:%s:%s\n", tval_find_name(art->tval), name);

		/* Output graphics if necessary */
		if (kind->kidx >= z_info->ordinary_kind_max) {
			const char *attr = attr_to_text(kind->d_attr);
			char *d_char = mem_alloc(text_wcsz() + 1);
			int nbyte = text_wctomb(d_char, kind->d_char);

			if (nbyte > 0) {
				d_char[nbyte] = '\0';
				file_putf(fff, "graphics:%s:%s\n", d_char,
					attr);
			}
			mem_free(d_char);
		}

		/* Output pval, level, weight and cost */
		file_putf(fff, "pval:%d\n", art->pval);
		file_putf(fff, "depth:%d\n", art->level);
		file_putf(fff, "rarity:%d\n", art->rarity);
		file_putf(fff, "weight:%d\n", art->weight);
		file_putf(fff, "cost:%d\n", art->cost);

		/* Output combat power */
		file_putf(fff, "attack:%d:%dd%d\n", art->att, art->dd, art->ds);
		file_putf(fff, "defence:%d:%dd%d\n", art->evn, art->pd, art->ps);

		/* Output flags */
		write_flags(fff, "flags:", art->flags, OF_SIZE, obj_flags);

		/* Output modifiers */
		write_mods(fff, art->modifiers);

		/* Output resists, immunities and vulnerabilities */
		write_elements(fff, art->el_info);

		/* Output slays */
		if (art->slays) {
			for (j = 1; j < z_info->slay_max; j++) {
				if (art->slays[j]) {
					file_putf(fff, "slay:%s\n", slays[j].code);
				}
			}
		}

		/* Output brands */
		if (art->brands) {
			for (j = 1; j < z_info->brand_max; j++) {
				if (art->brands[j]) {
					file_putf(fff, "brand:%s\n", brands[j].code);
				}
			}
		}

		/* Output abilities */
		write_abilities(fff, art->abilities);

		file_putf(fff, "\n");
	}
}

