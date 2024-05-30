/**
 * \file obj-make.c
 * \brief Object generation functions.
 *
 * Copyright (c) 1987-2007 Angband contributors
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
#include "alloc.h"
#include "cave.h"
#include "effects.h"
#include "game-world.h"
#include "init.h"
#include "obj-chest.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"

/**
 * ------------------------------------------------------------------------
 * Object kind allocation
 *
 * Object kind allocation is done using an allocation table (see alloc.h).
 * This table is sorted by depth.  Each line of the table contains the
 * object kind index, the object kind level, and three probabilities:
 * - prob1 is the base probability of the kind, calculated from object.txt.
 * - prob2 is calculated by get_obj_num_prep(), which decides whether an
 *         object is appropriate based on drop type; prob2 is always either
 *         prob1 or 0.
 * - prob3 is calculated by get_obj_num(), which checks whether universal
 *         restrictions apply (for example, aretfacts can only appear
 *         once); prob3 is always either prob2 or 0.
 * ------------------------------------------------------------------------ */
static int16_t alloc_kind_size = 0;
static struct alloc_entry *alloc_kind_table;

static int16_t alloc_ego_size = 0;
static struct alloc_entry *alloc_ego_table;

struct drop *drops;

/**
 * Initialize object allocation info
 */
static void alloc_init_objects(void) {
	int i;
	struct allocation *allocation;
	struct object_kind *kind;
	struct alloc_entry *table;
	int16_t *num = mem_zalloc(z_info->max_depth * sizeof(int16_t));
	int16_t *already_counted =
		mem_zalloc(z_info->max_depth * sizeof(int16_t));

	/* Size of "alloc_kind_table" */
	alloc_kind_size = 0;

	/* Scan the objects */
	for (i = 1; i < z_info->k_max - 1; i++) {
		/* Get the i'th kind */
		kind = &k_info[i];

		/* Scan allocation entries */
		allocation = kind->alloc;
		while (allocation) {
			/* Count the entries */
			alloc_kind_size++;

			/* Group by level */
			num[allocation->locale]++;
			allocation = allocation->next;
		}
	}

	/* Calculate the cumultive level totals */
	for (i = 1; i < z_info->max_depth; i++) {
		/* Group by level */
		num[i] += num[i - 1];
	}

	/* Allocate the alloc_kind_table */
	alloc_kind_table = mem_zalloc_alt(alloc_kind_size *
									  sizeof(struct alloc_entry));

	/* Get the table entry */
	table = alloc_kind_table;

	/* Scan the objects */
	for (i = 1; i < z_info->k_max - 1; i++) {
		/* Get the i'th kind */
		kind = &k_info[i];

		/* Scan allocation entries */
		allocation = kind->alloc;
		while (allocation) {
			int p, lev, prev_lev_count, kind_index;

			/* Extract the base level */
			lev = allocation->locale;

			/* Extract the base probability */
			p = allocation->chance ? (100 / allocation->chance) : 0;

			/* Skip entries preceding this locale */
			prev_lev_count = (lev > 0) ? num[lev - 1] : 0;

			/* Skip entries already counted for this level */
			kind_index = prev_lev_count + already_counted[lev];

			/* Load the entry */
			table[kind_index].index = i;
			table[kind_index].level = lev;
			table[kind_index].prob1 = p;
			table[kind_index].prob2 = p;
			table[kind_index].prob3 = p;

			/* Another entry complete for this locale */
			already_counted[lev]++;
			allocation = allocation->next;
		}
	}
	mem_free(already_counted);
	mem_free(num);
}

/**
 * Initialize ego-item allocation info
 *
 * The ego allocation probabilities table (alloc_ego_table) is sorted in
 * order of minimum depth.  Precisely why, I'm not sure!  But that is what
 * the code below is doing with the arrays 'num' and 'level_total'. -AS
 */
static void alloc_init_egos(void) {
	int *num = mem_zalloc((z_info->max_obj_depth + 1) * sizeof(int));
	int *level_total = mem_zalloc((z_info->max_obj_depth + 1) * sizeof(int));

	int i;

	for (i = 0; i < z_info->e_max; i++) {
		struct ego_item *ego = &e_info[i];

		if (ego->rarity) {
			/* Count the entries */
			alloc_ego_size++;

			/* Group by level */
			num[ego->level]++;
		}
	}

	/* Collect the level indexes */
	for (i = 1; i < z_info->max_obj_depth; i++)
		num[i] += num[i - 1];

	/* Allocate the alloc_ego_table */
	alloc_ego_table = mem_zalloc(alloc_ego_size * sizeof(struct alloc_entry));

	/* Scan the ego-items */
	for (i = 0; i < z_info->e_max; i++) {
		struct ego_item *ego = &e_info[i];

		/* Count valid pairs */
		if (ego->rarity) {
			int min_level = ego->level;

			/* Skip entries preceding our locale */
			int y = (min_level > 0) ? num[min_level - 1] : 0;

			/* Skip previous entries at this locale */
			int z = y + level_total[min_level];

			/* Load the entry */
			alloc_ego_table[z].index = i;
			alloc_ego_table[z].level = min_level;			/* Unused */
			alloc_ego_table[z].prob1 = ego->rarity;
			alloc_ego_table[z].prob2 = ego->rarity;
			alloc_ego_table[z].prob3 = ego->rarity;

			/* Another entry complete for this locale */
			level_total[min_level]++;
		}
	}

	mem_free(level_total);
	mem_free(num);
}

static void init_obj_make(void) {
	alloc_init_objects();
	alloc_init_egos();
}

static void cleanup_obj_make(void) {
	mem_free(alloc_ego_table);
	mem_free_alt(alloc_kind_table);
}

/**
 * ------------------------------------------------------------------------
 * Make an ego item
 * ------------------------------------------------------------------------ */
/**
 * Select an ego-item that fits the object's tval and sval.
 */
static struct ego_item *ego_find_random(struct object *obj, int level,
										bool only_good)
{
	int i;
	long total = 0L;

	struct alloc_entry *table = alloc_ego_table;

	/* Go through all possible ego items and find ones which fit this item */
	for (i = 0; i < alloc_ego_size; i++) {
		struct ego_item *ego = &e_info[table[i].index];
		struct poss_item *poss;

		/* Reset any previous probability of this type being picked */
		table[i].prob3 = 0;

		/* Objects are sorted by depth */
		if (table[i].level > level) continue;

		/* Some special items can't be generated too deep */
		if ((ego->alloc_max > 0) && (player->depth > ego->alloc_max)) continue;

		/* If we force fine/special, don't create cursed */
		if (only_good && of_has(ego->flags, OF_CURSED)) continue;

		/* If we force fine/special, don't create useless */
		if (only_good && (ego->cost == 0)) continue;

		/* Test if this is a legal special item type for this object */
		for (poss = ego->poss_items; poss; poss = poss->next)
			if (poss->kidx == obj->kind->kidx) {
				table[i].prob3 = table[i].prob2;
				break;
			}

		/* Total */
		total += table[i].prob3;
	}

	if (total) {
		long value = randint0(total);
		for (i = 0; i < alloc_ego_size; i++) {
			/* Found the entry */
			if (value < table[i].prob3) {
				return &e_info[table[i].index];
			} else {
				/* Decrement */
				value = value - table[i].prob3;
			}
		}
	}

	return NULL;
}


/**
 * Apply generation magic to an ego-item.
 */
void ego_apply_magic(struct object *obj, bool smithing)
{
	int i;
	struct ego_item *ego = obj->ego;
	struct ability *ability = ego->abilities;

	/* Add the abilities */
	while (ability) {
		add_ability(&obj->abilities, ability);
		ability = ability->next;
	}

	/* Hack -- acquire "broken" flag */
	if (!ego->cost) obj->notice |= (OBJ_NOTICE_BROKEN);

	/* Hack -- acquire "cursed" flag */
	if (of_has(ego->flags, OF_CURSED)) obj->notice |= (OBJ_NOTICE_CURSED);

	/* Bonuses apply differently for smithed objects */
	if (smithing) {
		/* Apply extra ego bonuses */
		if (ego->att) obj->att += 1;
		if (ego->evn) obj->evn += 1;
		if (ego->dd) obj->dd += 1;
		if (ego->ds) obj->ds += 1;
		if (ego->pd) obj->pd += 1;
		if (ego->ps) obj->ps += 1;

		if (ego->pval > 0) {
			if (of_has(ego->flags, OF_CURSED)) {
				obj->pval = -1;
			} else {
				obj->pval = 1;
			}
		}

		/* Apply modifiers */
		for (i = 0; i < OBJ_MOD_MAX; i++) {
			obj->modifiers[i] = ego->modifiers[i] != 0 ? obj->pval : 0;
		}
	} else {
		/* Remember pval for lights */
		int light_pval = tval_is_light(obj) ? obj->pval : 0;

		/* Apply extra ego bonuses */
		if (ego->att) obj->att += randint1(ego->att);
		if (ego->evn) obj->evn += randint1(ego->evn);
		if (ego->dd) obj->dd += randint1(ego->dd);
		if (ego->ds) obj->ds += randint1(ego->ds);
		if (ego->pd) obj->pd += randint1(ego->pd);
		if (ego->ps) obj->ps += randint1(ego->ps);

		if (of_has(ego->flags, OF_CURSED) && ego->pval) {
			obj->pval -= randint1(ego->pval);
		} else if (ego->pval) {
			obj->pval += randint1(ego->pval);
		}

		/* Apply modifiers */
		for (i = 0; i < OBJ_MOD_MAX; i++) {
			obj->modifiers[i] = ego->modifiers[i] != 0 ? obj->pval : 0;
		}
		obj->pval = light_pval;
	}

	/* Apply flags */
	of_union(obj->flags, ego->flags);

	/* Add slays, brands and curses */
	copy_slays(&obj->slays, ego->slays);
	copy_brands(&obj->brands, ego->brands);

	/* Add resists */
	for (i = 0; i < ELEM_MAX; i++) {
		/* Use the larger of ego resist level if it's notable */
		if (ego->el_info[i].res_level != 0) {
			obj->el_info[i].res_level = ego->el_info[i].res_level;
		}

		/* Union of flags so as to know when ignoring is notable */
		obj->el_info[i].flags |= ego->el_info[i].flags;
	}

	/* Pseudo-id the item */
	pseudo_id(obj);
}

/**
 * Try to find an ego-item for an object, setting obj->ego if successful and
 * applying various bonuses.
 */
static bool make_special_item(struct object *obj, int level, bool only_good)
{
	/* Cannot further improve artifacts or ego items */
	if (obj->artifact || obj->ego) return false;

	/* Occasionally boost the generation level of an item */
	if (level > 0 && one_in_(z_info->great_ego)) {
		/* Usually choose a deeper depth, weighted towards the current depth */
		if (level < z_info->dun_depth) {
			int level1 = rand_range(level + 1, z_info->dun_depth);
			int level2 = rand_range(level + 1, z_info->dun_depth);
			level = MIN(level1, level2);
		} else {
			level++;
		}

		/* Ensure valid allocation level */
		if (level >= z_info->max_obj_depth)
			level = z_info->max_obj_depth - 1;
	}

	/* Try to get a legal ego type for this item */
	obj->ego = ego_find_random(obj, level, only_good);

	/* Actually apply the ego template to the item */
	if (obj->ego) {
		ego_apply_magic(obj, false);
		return true;
	}

	return false;
}


/**
 * ------------------------------------------------------------------------
 * Make an artifact
 * ------------------------------------------------------------------------ */

/**
 * Copy artifact data to a normal object.
 */
void copy_artifact_data(struct object *obj, const struct artifact *art)
{
	int i;
	struct ability *ability = art->abilities;

	/* Extract the data */
	for (i = 0; i < OBJ_MOD_MAX; i++)
		obj->modifiers[i] = art->modifiers[i];
	obj->att = art->att;
	obj->dd = art->dd;
	obj->ds = art->ds;
	obj->evn = art->evn;
	obj->pd = art->pd;
	obj->ps = art->ps;
	obj->weight = art->weight;
	obj->pval = art->pval;

	/* Add the abilities */
	while (ability) {
		add_ability(&obj->abilities, ability);
		ability = ability->next;
	}

	of_union(obj->flags, art->flags);
	copy_slays(&obj->slays, art->slays);
	copy_brands(&obj->brands, art->brands);

	for (i = 0; i < ELEM_MAX; i++) {
		/* Use any non-zero artifact resist level */
		if (art->el_info[i].res_level != 0) {
			obj->el_info[i].res_level = art->el_info[i].res_level;
		}

		/* Union of flags so as to know when ignoring is notable */
		obj->el_info[i].flags |= art->el_info[i].flags;
	}
}


/**
 * As artefacts are generated, there is an increasing chance to fail to make
 * the next one
 */
static bool too_many_artefacts(void)
{
	int i;

	for (i = 0; i < player->num_artefacts; i++) {
		if (percent_chance(10)) return true;
	}

	return false;
}

/**
 * Mega-Hack -- Attempt to create one of the "Special Objects".
 *
 * We are only called from "make_object()"
 *
 * Note -- see "make_artifact()" and "apply_magic()".
 *
 * We *prefer* to create the special artifacts in order, but this is
 * normally outweighed by the "rarity" rolls for those artifacts.
 */
static struct object *make_artifact_special(int level)
{
	int i;
	struct object *new_obj;

	/* No artifacts, do nothing */
	if (OPT(player, birth_no_artifacts)) return NULL;

	/* As more artefacts are generated, the chance for another decreases */
	if (too_many_artefacts()) return NULL;

	/* Check the special artifacts */
	for (i = 0; i < z_info->a_max; ++i) {
		const struct artifact *art = &a_info[i];
		struct object_kind *kind = lookup_kind(art->tval, art->sval);

		/* Skip "empty" artifacts */
		if (!art->name) continue;

		/* Make sure the kind was found */
		if (!kind) continue;

		/* Skip non-special artifacts */
		if (!kf_has(kind->kind_flags, KF_INSTA_ART)) continue;

		/* Skip specified artifacts */
		if (of_has(art->flags, OF_NO_RANDOM)) continue;

		/* Cannot make an artifact twice */
		if (is_artifact_created(art)) continue;

		/* Enforce minimum "depth" (loosely) */
		if (art->level > level) {
			/* Get the "out-of-depth factor" */
			int d = (art->level - level) * 2;

			/* Roll for out-of-depth creation */
			if (randint0(d) != 0) continue;
		}

		/* Artifact "rarity roll" */
		if (!one_in_(art->rarity)) continue;

		/* Enforce minimum "object" level (loosely) */
		if (kind->level > level) {
			/* Get the "out-of-depth factor" */
			int d = (kind->level - level) * 5;

			/* Roll for out-of-depth creation */
			if (randint0(d) != 0) continue;
		}

		/* Assign the template */
		new_obj = object_new();
		object_prep(new_obj, kind, art->level, RANDOMISE);

		/* Mark the item as an artifact */
		new_obj->artifact = art;

		/* Copy across all the data from the artifact struct */
		copy_artifact_data(new_obj, art);

		/* Mark the artifact as "created" */
		mark_artifact_created(art, true);

		/* Pseudo-id the item */
		pseudo_id(new_obj);

		/* Success */
		return new_obj;
	}

	/* Failure */
	return NULL;
}


/**
 * Attempt to change an object into an artifact.  If the object is already
 * set to be an artifact, use that, or otherwise use a suitable randomly-
 * selected artifact.
 *
 * This routine should only be called by "apply_magic()"
 *
 * Note -- see "make_artifact_special()" and "apply_magic()"
 */
static bool make_artifact(struct object *obj, int lev)
{
	int i;

	/* Make sure birth no artifacts isn't set */
	if (OPT(player, birth_no_artifacts)) return false;

	/* As more artefacts are generated, the chance for another decreases */
	if (too_many_artefacts()) return NULL;

	/* Check the artifact list (skip the "specials") */
	for (i = 0; !obj->artifact && i < z_info->a_max; i++) {
		const struct artifact *art = &a_info[i];
		struct object_kind *kind = lookup_kind(art->tval, art->sval);

		/* Skip "empty" items */
		if (!art->name) continue;

		/* Make sure the kind was found */
		if (!kind) continue;

		/* Skip special artifacts */
		if (kf_has(kind->kind_flags, KF_INSTA_ART)) continue;

		/* Skip specified artifacts */
		if (of_has(art->flags, OF_NO_RANDOM)) continue;

		/* Cannot make an artifact twice */
		if (is_artifact_created(art)) continue;

		/* Must have the correct fields */
		if (art->tval != obj->tval) continue;
		if (art->sval != obj->sval) continue;

		/* XXX XXX Enforce minimum "depth" (loosely) */
		if (art->level > lev) {
			/* Get the "out-of-depth factor" */
			int d = (art->level - lev) * 2;

			/* Roll for out-of-depth creation */
			if (randint0(d) != 0) continue;
		}

		/* We must make the "rarity roll" */
		if (!one_in_(art->rarity)) continue;

		/* Mark the item as an artifact */
		obj->artifact = art;

		/* Paranoia -- no "plural" artifacts */
		obj->number = 1;
	}

	if (obj->artifact) {
		copy_artifact_data(obj, obj->artifact);
		mark_artifact_created(obj->artifact, true);
		pseudo_id(obj);
		return true;
	}

	return false;
}


/**
 * Create a fake artifact directly from a blank object
 *
 * This function is used for describing artifacts, and for creating them for
 * debugging.
 *
 * Since this is now in no way marked as fake, we must make sure this function
 * is never used to create an actual game object
 */
bool make_fake_artifact(struct object *obj, const struct artifact *artifact)
{
	struct object_kind *kind;

	/* Don't bother with empty artifacts */
	if (!artifact->tval) return false;

	/* Get the "kind" index */
	kind = lookup_kind(artifact->tval, artifact->sval);
	if (!kind) return false;

	/* Create the artifact */
	object_prep(obj, kind, 0, MAXIMISE);
	obj->artifact = artifact;
	copy_artifact_data(obj, artifact);

	return (true);
}


/**
 * ------------------------------------------------------------------------
 * Apply magic to an item
 * ------------------------------------------------------------------------ */
/**
 * Apply magic to a weapon.
 */
static void apply_magic_weapon(struct object *obj, int level)
{
	bool boost_dam = false;
	bool boost_att = false;

	/* Arrows can only have increased attack value */
	if (tval_is_ammo(obj)) {
		obj->att += 3;
		return;	
	} else {
		/* Small chance of boosting both */
		if (percent_chance(level)) {
			boost_dam = true;
			boost_att = true;
		} else if (one_in_(2)) {
			/* Otherwise 50/50 chance of dam or att */
			boost_dam = true;
		} else {
			boost_att = true;
		}
	}

	if (boost_dam) {
		obj->ds++;
	}
	if (boost_att) {
		obj->att++;
	}
}


/**
 * Apply magic to armour
 */
static void apply_magic_armour(struct object *obj, int level)
{
	bool boost_prot = false;
	bool boost_other = false;
	
	/* For cloaks and robes and filthy rags go for evasion only */
	if (tval_is_cloak(obj) ||
		(tval_is_body_armor(obj) &&
		 (obj->sval == lookup_sval(TV_SOFT_ARMOR, "Robe") ||
		  obj->sval == lookup_sval(TV_SOFT_ARMOR, "Filthy Rag")))) {
		boost_other = true;
	} else if ((obj->att >= 0) && (obj->evn >= 0)) {
		/* Otherwise if there are no penalties to fix, go for protection only */
		boost_prot = true;
	} else {
		/* Small chance of boosting both */
		if (percent_chance(level)) {
			boost_prot = true;
			boost_other = true;
		} else if (one_in_(2)) {
			/* Otherwise 50/50 chance of dam or att */
			boost_prot = true;
		} else {
			boost_other = true;
		}
	}

	if (boost_other) {
		if ((obj->att < 0) && (obj->evn < 0)) {
			if (one_in_(2)) obj->evn++;
			else obj->att++;
		} else if (obj->att < 0) {
			obj->att++;
		} else {
			obj->evn++;
		}
	}
	if (boost_prot) {
		obj->ps++;
	}
}


/**
 * Complete the "creation" of an object by applying "magic" to the item
 *
 * This includes not only rolling for random bonuses, but also putting the
 * finishing touches on special items and artefacts, giving charges to wands and
 * staffs, giving fuel to lites, and placing traps on chests.
 *
 * In particular, note that "Instant Artefacts", if "created" by an external
 * routine, must pass through this function to complete the actual creation.
 *
 * The base chance of the item being "fine" increases with the "level"
 * parameter, which is usually derived from the dungeon level, being equal
 * to (level)%.
 * The chance that the object will be "special" (special item or artefact), 
 * is also (level)%.
 * If "good" is true, then the object is guaranteed to be either "fine" or
 * "special". 
 * If "great" is true, then the object is guaranteed to be both "fine" and
 * "special".
 *
 * If "okay" is true, and the object is going to be "special", then there is
 * a chance that an artefact will be created.  This is true even if both the
 * "good" and "great" arguments are false.  Objects which have both "good" and
 * "great" flags get three extra "attempts" to become an artefact.
 *
 * Note that in the above we are using the new terminology of 'fine' and
 * 'special' where Vanilla Angband used 'good' and 'great'. A big change is
 * that these are now independent: you can have ego items that don't have
 * extra mundane bonuses (+att, +evn, +sides...)
 */
void apply_magic(struct object *obj, int lev, bool allow_artifacts, bool good,
				 bool great)
{
	int i;
	bool fine = false;
	bool special = false;

	/* Maximum "level" for various things */
	lev = MIN(lev, z_info->max_depth - 1);

	/* Roll for "fine" */
	if (percent_chance(lev * 2)) fine = true;

	/* Roll for "special" */
	if (percent_chance(lev * 2)) special = true;

	/* Guarantee "fine" or "special" for "good" drops */
	if (good) {
		if (one_in_(2)) {
			fine = true;
		} else {
			special = true;
		}
	}

	/* Guarantee "fine" and "special" for "great" drops */
	if (great) {
		fine = true;
		special = true;
	}

	/* Roll for artifact creation */
	if (allow_artifacts) {
		int rolls = 0;

		/* Get 2 rolls if special */
		if (special) rolls = 2;

		/* Get 8 rolls if good and great are both set */
		if (good && great) rolls = 8;
		
		/* Roll for artifacts if allowed */
		for (i = 0; i < rolls; i++) {
			if (make_artifact(obj, lev)) return;
		}
	}

	/* Apply magic */
	if (tval_is_held_weapon(obj)) {
		/* Special treatment for deathblades */
		int sval = lookup_sval(TV_SWORD, "Deathblade");
		if (obj->kind == lookup_kind(TV_SWORD, sval)) {
			while (one_in_(2)) obj->att++;
		} else {
			/* Deal with special items */
			if (special && !make_special_item(obj, lev, good || great)) {
				fine = true;
			}

			/* Deal with fine items */
			if (fine) {
				apply_magic_weapon(obj, lev);
			}

			/* Deal with throwing items */
			if (of_has(obj->flags, OF_THROWING)) {
				/* Throwing items always have typical weight for stacking */
				obj->weight = obj->kind->weight;

				/* And often come in multiples */
				if (one_in_(2)) {
					obj->number = rand_range(2, 5);
				}
			}
		}
	} else if (tval_is_ammo(obj)) {
		/* Note that arrows can't be both fine and special */
		if (special) {
			(void) make_special_item(obj, lev, good || great);
			if (obj->number > 1) obj->number /= 2;
		} else if (fine) {
			apply_magic_weapon(obj, lev);
			if (obj->number > 1) obj->number /= 2;
		}
	} else if (tval_is_armor(obj)) {
		/* Deal with special items */
		if (special && !make_special_item(obj, lev, good || great)) {
			fine = true;
		}

		/* Deal with fine items */
		if (fine) {
			apply_magic_armour(obj, lev);
		}
	} else if (tval_is_jewelry(obj)) {
		/* For jewellery, some negative values mean cursed and broken */
		if ((obj->att < 0) || (obj->evn < 0)) {
			of_on(obj->flags, OF_CURSED);
			obj->notice |= (OBJ_NOTICE_CURSED | OBJ_NOTICE_BROKEN);
		}
		for (i = 0; i < OBJ_MOD_MAX; i++) {
			if (obj->modifiers[i] < 0) {
				of_on(obj->flags, OF_CURSED);
				obj->notice |=
					(OBJ_NOTICE_CURSED | OBJ_NOTICE_BROKEN);
			}
		}
	} else if (tval_is_light(obj)) {
		if (special) {
			(void) make_special_item(obj, lev, good || great);
		}
	} else if (tval_is_chest(obj)) {
		/* Set chest level */
		obj->pval = lev;
		if (fine) obj->pval += 2;
		if (special) obj->pval += 2;
		obj->pval = MAX(1, MIN(obj->pval, 25));
	}

	/* Done with special items */
	if (obj->ego) return;

	/* Hack -- acquire "broken" flag */
	if (!obj->kind->cost) obj->notice |= (OBJ_NOTICE_BROKEN);

	/* Hack -- acquire "cursed" flag */
	if (of_has(obj->kind->flags, OF_CURSED)) obj->notice |= (OBJ_NOTICE_CURSED);

	/* Identify non-special non-artefact weapons/armour */
	if (tval_has_variable_power(obj) && !tval_is_jewelry(obj)) {
		object_know(obj);
	}
}


/**
 * ------------------------------------------------------------------------
 * Generate a random object
 * ------------------------------------------------------------------------ */
/**
 * Evaluate the special value of an object kind.
 *
 * Special values are a stored random value for the case where the kind
 * almost always just needs an integer.
 */
static int eval_special_value(struct object_kind *kind, int lev)
{
	int val = randcalc(kind->special1, lev, RANDOMISE);
	if (!val && (kind->special2 != 0)) {
		val = kind->special2;
	}
	return val;
}

/**
 * Wipe an object clean and make it a standard object of the specified kind.
 */
void object_prep(struct object *obj, struct object_kind *k, int lev,
				 aspect rand_aspect)
{
	int i;
	struct ability *ability = k->abilities;

	/* Clean slate */
	memset(obj, 0, sizeof(*obj));

	/* Assign the kind and copy across data */
	obj->kind = k;
	obj->image_kind = &k_info[randint0(z_info->k_max)];
	obj->tval = k->tval;
	obj->sval = k->sval;
	if (k->att == SPECIAL_VALUE) {
		obj->att = eval_special_value(k, lev);
	} else {
		obj->att = k->att;
	}
	obj->dd = k->dd;
	obj->ds = k->ds;
	if (k->evn == SPECIAL_VALUE) {
		obj->evn = eval_special_value(k, lev);
	} else {
		obj->evn = k->evn;
	}
	obj->pd = k->pd;
	if (k->ps == SPECIAL_VALUE) {
		obj->ps = eval_special_value(k, lev);
	} else {
		obj->ps = k->ps;
	}

	/* Exact weight for most items, approximate weight for weapons and armour */
	if ((tval_is_weapon(obj) || tval_is_armor(obj)) && !tval_is_ammo(obj)) {
		obj->weight = k->weight;
		switch (rand_aspect) {
			case EXTREMIFY:
			case MINIMISE: {
				while (obj->weight * 2 > k->weight * 3) obj->weight -= 5;
				break;
			}
			case AVERAGE: {
				break;
			}
			case MAXIMISE: {
				while (obj->weight * 3 < k->weight * 2) obj->weight += 5;
				break;
			}
			case RANDOMISE: {
				obj->weight = Rand_normal(k->weight, k->weight / 6 + 1);

				/* Round to the nearest multiple of 0.5 lb */
				obj->weight = (obj->weight * 2 + 9) / 10;
				obj->weight *= 5;

				/* Restrict weight to within [2/3, 3/2] of the standard */
				while (obj->weight * 3 < k->weight * 2) obj->weight += 5;
				while (obj->weight * 2 > k->weight * 3) obj->weight -= 5;
				break;
			}
			default:  {
				obj->weight = k->weight;
			}
		}
	} else {
		obj->weight = k->weight;
	}

	/* Default number */
	obj->number = 1;

	/* Copy flags */
	of_copy(obj->flags, k->base->flags);
	of_copy(obj->flags, k->flags);

	/* Assign charges (staves only) */
	if (tval_can_have_charges(obj))
		obj->pval = randcalc(k->charge, lev, rand_aspect);

	/* Default fuel, light */
	if (tval_is_light(obj)) {
		if (of_has(obj->flags, OF_BURNS_OUT)) {
			if (one_in_(3) && character_generated) {
				obj->timeout = rand_range(500, z_info->default_torch);
			} else {
				obj->timeout = z_info->default_torch;
			}
		} else if (of_has(obj->flags, OF_TAKES_FUEL)) {
			if (one_in_(3)) {
				obj->timeout = rand_range(500, z_info->default_lamp);
			} else {
				obj->timeout = z_info->default_lamp;
			}
		}
		obj->pval = k->pval;
	}

	/* Assign pval for oil */
	if (tval_is_fuel(obj))
		obj->pval = k->pval;

	/* Assign modifiers */
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		obj->modifiers[i] = randcalc(k->modifiers[i], lev, rand_aspect);
		if (obj->modifiers[i] == SPECIAL_VALUE) {
			obj->modifiers[i] = eval_special_value(k, lev);
		}
	}

	/* Default slays, brands and curses */
	copy_slays(&obj->slays, k->slays);
	copy_brands(&obj->brands, k->brands);

	/* Default resists */
	for (i = 0; i < ELEM_MAX; i++) {
		obj->el_info[i].res_level = k->el_info[i].res_level;
		obj->el_info[i].flags = k->el_info[i].flags;
		obj->el_info[i].flags |= k->base->el_info[i].flags;
	}

	/* Add the abilities */
	while (ability) {
		add_ability(&obj->abilities, ability);
		ability = ability->next;
	}

	/* Hack -- acquire "broken" flag */
	if (k->cost <= 0) obj->notice |= (OBJ_NOTICE_BROKEN);

	/* Hack -- acquire "cursed" flag */
	if (of_has(k->flags, OF_CURSED)) obj->notice |= (OBJ_NOTICE_CURSED);
}

/**
 * Lookup a drop type by name.
 * This function fails gracefully; if the drop type is incorrect, it returns
 * NULL, which means no drop restrictions will be enforced.
 */
struct drop *lookup_drop(const char *name)
{
	int i;
	for (i = 0; i < z_info->drop_max; i++) {
		struct drop *drop = &drops[i];
		if (streq(name, drop->name)) return drop;
	}
	return NULL;
}

/**
 * Verify a drop type
 */
static bool drop_is(struct drop *drop, const char *name)
{
	return streq(name, drop->name);
}

/**
 * Apply a drop restriction to the object allocation table.
 * This way, we can use get_obj_num() to get a level-appropriate object of
 * the specified drop type.
 */
static void get_obj_num_prep(struct drop *drop)
{
	int i;

	/* Scan the allocation table */
	for (i = 0; i < alloc_kind_size; i++) {
		struct alloc_entry *entry = &alloc_kind_table[i];

		/* Check the restriction, if any */
		if (drop) {
			struct poss_item *item;
			if (drop->poss) {
				/*
				 * Unless determined otherwise, this object is
				 * not included.
				 */
				entry->prob2 = 0;
				item = drop->poss;
				while (item) {
					if ((int) item->kidx == entry->index) {
						/* Accept this object */
						entry->prob2 = entry->prob1;
						break;
					}
					item = item->next;
				}
			} else if (drop->imposs) {
				/*
				 * Unless determined otherwise, this object is
				 * included.
				 */
				entry->prob2 = entry->prob1;
				item = drop->imposs;
				while (item) {
					if ((int) item->kidx == entry->index) {
						/* Do not use this object */
						entry->prob2 = 0;
						break;
					}
					item = item->next;
				}
			} else {
				quit("Invalid object drop type!");
			}
		} else {
			/* Accept this object */
			entry->prob2 = entry->prob1;
		}
	}
}

/**
 * Choose an object kind given a dungeon level to choose it for.
 */
struct object_kind *get_obj_num(int level)
{
	int i, j, p;
	long total = 0, value;
	struct alloc_entry *table = alloc_kind_table;

	/* Occasional level boost */
	if ((level > 0) && one_in_(z_info->great_obj)) {
		/* Mostly choose a deeper depth, weighted towards the current depth */
		if (level < z_info->max_depth) {
			int x = rand_range(level + 1, z_info->max_depth);
			int y = rand_range(level + 1, z_info->max_depth);
			level = MIN(x, y);
		} else {
			/* But if it was already very deep, just increment it */
			level++;
		}
	}

	/* Paranoia */
	level = MIN(level, z_info->max_obj_depth);
	level = MAX(level, 0);

	/* Process probabilities */
	for (i = 0; i < alloc_kind_size; i++) {
		/* Objects are sorted by depth */
		if (table[i].level > level) break;

		/* Default */
		table[i].prob3 = 0;

		/* Accept */
		table[i].prob3 = table[i].prob2;

		/* Total */
		total += table[i].prob3;
	}

	/* No legal objects */
	if (total <= 0) return NULL;

	/* Pick an object */
	value = randint0(total);

	/* Find the object */
	for (i = 0; i < alloc_kind_size; i++) {
		/* Found the entry */
		if (value < table[i].prob3) break;

		/* Decrement */
		value = value - table[i].prob3;
	}

	/* Power boost */
	p = randint0(100);

	/* Try for a "better" object once (50%) or twice (10%) */
	if (p < 60) {
		/* Save old */
		j = i;

		/* Pick an object */
		value = randint0(total);

		/* Find the object */
		for (i = 0; i < alloc_kind_size; i++) {
			/* Found the entry */
			if (value < table[i].prob3) break;

			/* Decrement */
			value = value - table[i].prob3;
		}

		/* Keep the "best" one */
		if (table[i].level < table[j].level) i = j;
	}

	/* Try for a "better" object twice (10%) */
	if (p < 10) {
		/* Save old */
		j = i;

		/* Pick a object */
		value = randint0(total);

		/* Find the object */
		for (i = 0; i < alloc_kind_size; i++) {
			/* Found the entry */
			if (value < table[i].prob3) break;

			/* Decrement */
			value = value - table[i].prob3;
		}

		/* Keep the "best" one */
		if (table[i].level < table[j].level) i = j;
	}

	/* Result */
	return &k_info[table[i].index];
}


/**
 * Attempt to make an object
 *
 * \param c is the current dungeon level.
 * \param lev is the creation level of the object (not necessarily == depth).
 * \param good is whether the object is to be good
 * \param great is whether the object is to be great
 * \param drop constrains the type of object created or may be NULL to have no
 * constraint on the object type
 *
 * \return a pointer to the newly allocated object, or NULL on failure.
 */
struct object *make_object(struct chunk *c, int lev, bool good, bool great,
		struct drop *drop)
{
	struct object_kind *kind = NULL;
	struct object *new_obj;

	/* Base level for the object */
	int base = good || great ? lev + 3 : lev;

	/* Chance of "special object" */
	int prob = ((good || great) ? 10 : 1000);

	/* Better chance to check special artefacts if there is a jewellery theme */
	if (drop && drop_is(drop, "jewellery")) prob /= 2;

	/* Try to make a special artifact */
	if (one_in_(prob)) {
		new_obj = make_artifact_special(lev);
		if (new_obj) return new_obj;
	}

	/* Prepare allocation table if needed */
	if (drop) {
		get_obj_num_prep(drop);
	} else if (great) {
		get_obj_num_prep(lookup_drop("great"));
	} else if (good) {
		get_obj_num_prep(lookup_drop("good"));
	}
		
	/* Try to choose an object kind */
	kind = get_obj_num(base);

	/* Clear the object restriction */
	if (drop || good || great) {
		get_obj_num_prep(NULL);
	}

	/* Handle failure */
	if (!kind) return NULL;

	/* Make the object, prep it and apply magic */
	new_obj = object_new();
	object_prep(new_obj, kind, lev, RANDOMISE);

	/* Generate multiple items */
	if (tval_is_ammo(new_obj)) {
		if (one_in_(3)) {
			new_obj->number = damroll(4, 6);
		} else {
			/* 3/6 chance of 12, 2/6 chance of 24, 1/6 chance of 36 */
			new_obj->number = 12;
			if (one_in_(2)) { 
				new_obj->number += 12;
				if (one_in_(3)) {
					new_obj->number += 12;
				}
			}
		}
	} else if (tval_is_metal(new_obj)) {
		new_obj->number = damroll(2, 40);
	}

	/* Apply magic */
	apply_magic(new_obj, lev, true, good, great);

	return new_obj;
}


struct init_module obj_make_module = {
	.name = "object/obj-make",
	.init = init_obj_make,
	.cleanup = cleanup_obj_make
};
