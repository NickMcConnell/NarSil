/**
 * \file obj-smith.c
 * \brief Smithing of objects
 *
 * Copyright (c) 1997 Ben Harrison
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
 */
#include "angband.h"
#include "cave.h"
#include "effects.h"
#include "game-input.h"
#include "init.h"
#include "obj-chest.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-properties.h"
#include "obj-slays.h"
#include "obj-smith.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-history.h"
#include "trap.h"

/**
 * A list of tvals and their textual names and smithing categories
 */
const struct smithing_tval_desc smithing_tvals[MAX_SMITHING_TVALS] =
{
	#define SMITH_TYPE(a, b, c) { SMITH_TYPE_##a, TV_##b, c },
	#include "list-smith-types.h"
	#undef SMITH_TYPE
};

/**
 * ------------------------------------------------------------------------
 * Helper functions used mainly in the numbers menu
 * ------------------------------------------------------------------------ */

/**
 * Determines whether the attack bonus of an item is eligible for modification.
 */
int att_valid(struct object *obj)
{
	struct object_base *base = obj->kind->base;
	struct object_kind *kind;

	if (base->smith_attack_valid) return true;

	/* Rings are a special case */
	if (strstr(base->name, "Ring")) {
		kind = lookup_kind(base->tval, lookup_sval(base->tval, "Accuracy"));
		if (kind == obj->kind) return true;
		if (obj->artifact) return true;
	}

	return false;
}


/**
 * Determines the maximum legal attack bonus for an item.
 */
int att_max(struct object *obj, bool assume_artistry)
{
	struct object_base *base = obj->kind->base;
	struct object_kind *kind = obj->kind;
	int att = kind->att;
    bool artistry = assume_artistry ||
		player_active_ability(player, "Artistry");

	if (artistry) att += base->smith_attack_artistry;
	if (!tval_is_weapon(obj)) att = MIN(0, att);
	if (obj->artifact) att += base->smith_attack_artefact;

	/* Rings are a special case */
	if (strstr(base->name, "Ring")) {
		kind = lookup_kind(base->tval, lookup_sval(base->tval, "Accuracy"));
		if (kind == obj->kind) att = 4;
		if (obj->artifact) att = 4;
	}

	return att;
}


/**
 * Determines the minimum legal attack bonus for an item.
 */
int att_min(struct object *obj)
{
	struct object_base *base = obj->kind->base;
	struct object_kind *kind = obj->kind;
	int att = kind->att;

	/* Rings are a special case */
	if (strstr(base->name, "Ring")) {
		kind = lookup_kind(base->tval, lookup_sval(base->tval, "Accuracy"));
		if (kind == obj->kind) att = 1;;
	}

	return att;
}


/**
 * Determines whether the damage sides of an item is eligible for modification.
 */
int ds_valid(struct object *obj)
{
	return tval_is_melee_weapon(obj) || tval_is_launcher(obj);
}


/**
 * Determines the maximum legal damage sides for an item.
 */
int ds_max(struct object *obj, bool assume_artistry)
{
	struct object_kind *kind = obj->kind;
	struct ego_item *ego = obj->ego;
	int ds = kind->ds;
    bool artistry = assume_artistry ||
		player_active_ability(player, "Artistry");

	if (artistry) ds += 1;
	if (ego) ds += ego->ds;
	if (obj->artifact) ds += 2;

	return ds;
}


/**
 * Determines the minimum legal damage sides for an item.
 */
int ds_min(struct object *obj)
{
	struct object_kind *kind = obj->kind;
	struct ego_item *ego = obj->ego;
	int ds = kind->ds;

	if (ds_valid(obj) && ego && (ego->ds > 0)) ds += 1;

	return ds;
}


/**
 * Determines whether the evasion bonus of an item is eligible for modification.
 */
int evn_valid(struct object *obj)
{
	struct object_base *base = obj->kind->base;
	struct object_kind *kind;

	if (tval_is_armor(obj)) return true;
	if (tval_is_melee_weapon(obj) && !tval_is_digger(obj)) return true;

	/* Rings are a special case */
	if (strstr(base->name, "Ring")) {
		kind = lookup_kind(base->tval, lookup_sval(base->tval, "Evasion"));
		if (kind == obj->kind) return true;
		if (obj->artifact) return true;
	}

	return false;
}


/**
 * Determines the maximum legal evasion bonus for an item.
 */
int evn_max(struct object *obj, bool assume_artistry)
{
	struct object_kind *kind = obj->kind;
	struct object_base *base = kind->base;
	struct ego_item *ego = obj->ego;
	int evn = kind->evn;
    bool artistry = assume_artistry ||
		player_active_ability(player, "Artistry");

	if (tval_is_armor(obj) && artistry) evn += 1;
	if (ego) evn += ego->evn;
	if (obj->artifact) evn += 1;

	/* Rings are a special case */
	if (strstr(base->name, "Ring")) {
		kind = lookup_kind(base->tval, lookup_sval(base->tval, "Evasion"));
		if (kind == obj->kind) evn = 4;
		if (obj->artifact) evn = 4;
	}

	return evn;
}


/**
 * Determines the minimum legal evasion bonus for an item.
 */
int evn_min(struct object *obj)
{
	struct object_kind *kind = obj->kind;
	struct object_base *base = kind->base;
	struct ego_item *ego = obj->ego;
	int evn = kind->evn;

	if (ego && (ego->evn > 0)) evn += 1;

	/* Rings are a special case */
	if (strstr(base->name, "Ring")) {
		kind = lookup_kind(base->tval, lookup_sval(base->tval, "Evasion"));
		if (kind == obj->kind) evn = 1;
	}

	return evn;
}


/**
 * Determines whether the protection sides of an item is eligible for
 * modification.
 */
int ps_valid(struct object *obj)
{
	struct object_base *base = obj->kind->base;
	struct object_kind *kind;

	if (tval_is_armor(obj)) return true;

	/* Rings are a special case */
	if (strstr(base->name, "Ring")) {
		kind = lookup_kind(base->tval, lookup_sval(base->tval, "Protection"));
		if (kind == obj->kind) return true;
		if (obj->artifact) return true;
	}

	return false;
}


/**
 * Determines the maximum legal protection sides for an item.
 */
int ps_max(struct object *obj, bool assume_artistry)
{
	struct object_kind *kind = obj->kind;
	struct object_base *base = kind->base;
	struct ego_item *ego = obj->ego;
	int ps = kind->ps;
    bool artistry = assume_artistry ||
		player_active_ability(player, "Artistry");

	if (artistry) ps += 1;

	/* Cloaks, robes and filthy rags cannot get extra protection sides */
	if (strstr(base->name, "Cloak")) ps = 0;
	if (strstr(base->name, "Soft Armor")) {
		if (kind == lookup_kind(base->tval,
								lookup_sval(base->tval, "Filthy Rag"))) ps = 0;
		if (kind == lookup_kind(base->tval,
								lookup_sval(base->tval, "Robe"))) ps = 0;
	}

	if (ego) ps += ego->ps;
	if (obj->artifact) ps += 2;

	/* Rings are a special case */
	if (strstr(base->name, "Ring")) {
		kind = lookup_kind(base->tval, lookup_sval(base->tval, "Protection"));
		if (kind == obj->kind) ps = 3;
		if (obj->artifact) ps = 3;
	}

	return ps;
}


/**
 * Determines the minimum legal protection sides for an item.
 */
int ps_min(struct object *obj)
{
	struct object_kind *kind = obj->kind;
	struct object_base *base = kind->base;
	struct ego_item *ego = obj->ego;
	int ps = kind->ps;

	if (ego && (ego->ps > 0)) ps += 1;

	/* Rings are a special case */
	if (strstr(base->name, "Ring")) {
		kind = lookup_kind(base->tval, lookup_sval(base->tval, "Protection"));
		if (kind == obj->kind) ps = 1;
	}

	return ps;
}


/**
 * Determines whether the pval of an item is eligible for modification.
 */
int pval_valid(struct object *obj)
{
	int i;
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		if (obj->modifiers[i] != 0) return true;
	}
	return (obj->pval != 0);
}


/**
 * Determines the maximum legal pval for an item.
 */
int pval_max(struct object *obj)
{
	int pval = obj->kind->pval;

	/* Artefacts have pvals that are mostly unlimited  */
	if (obj->artifact) {
		pval += 4;
	} else if (tval_is_jewelry(obj)) {
		/* Non-artefact rings and amulets have a maximum pval of 4 */
		pval = 4;
	}

	/* Special items have limited pvals */
	if (obj->ego) {
		if (object_is_cursed(obj)) {
			if (obj->ego->pval > 0) pval -= 1;
		} else {
			pval += obj->ego->pval;
		}
	}

	return pval;
}


/**
 * Determines the minimum legal pval for an item.
 */
int pval_min(struct object *obj)
{
	int pval = obj->kind->pval;

	/* Artefacts have pvals that are mostly unlimited  */
	if (obj->artifact) {
		pval -= 4;
	} else if (tval_is_jewelry(obj)) {
		/* Non-artefact rings and amulets have a maximum pval of 4 */
		pval = -4;
	}

	/* Special items have limited pvals */
	if (obj->ego) {
		if (object_is_cursed(obj)) {
			if (obj->ego->pval > 0) pval -= obj->ego->pval;
		} else if (obj->ego->pval > 0) {
			pval -= 1;
		}
	}

	return pval;
}


/**
 * Determines whether the weight of an item is eligible for modification.
 */
int wgt_valid(struct object *obj)
{
	switch (obj->tval) {
		case TV_ARROW:
		case TV_RING:
		case TV_AMULET:
		case TV_LIGHT:
		case TV_HORN: {
			return false;
		}
	}

	return true;
}


/**
 * Determines the maximum legal weight for an item.
 */
int wgt_max(struct object *obj)
{
	return obj->kind->weight * 2;
}


/**
 * Determines the minimum legal weight for an item.
 */
int wgt_min(struct object *obj)
{
    return ((obj->kind->weight + 9) / 10) * 5;
}


/**
 * Actually modifies the numbers on an item.
 */
void modify_numbers(struct object *obj, int choice, int *pval)
{
	switch (choice) {
		case SMITH_NUM_INC_ATT: {
			if (tval_is_ammo(obj) && !obj->artifact) {
				obj->att += 3;
			} else {
				obj->att++;
			}
			break;
		}
		case SMITH_NUM_DEC_ATT: {
			if (tval_is_ammo(obj) && !obj->artifact) {
				obj->att -= 3;
			} else {
				obj->att--;
			}
			break;
		}
		case SMITH_NUM_INC_DS:		obj->ds++;			break;
		case SMITH_NUM_DEC_DS:		obj->ds--;			break;
		case SMITH_NUM_INC_EVN:	obj->evn++;			break;
		case SMITH_NUM_DEC_EVN:	obj->evn--;			break;
		case SMITH_NUM_INC_PS:		obj->ps++;			break;
		case SMITH_NUM_DEC_PS:		obj->ps--;			break;
		case SMITH_NUM_INC_PVAL: {
			//if (*pval == -1) {
			//	*pval = 1;
			//} else {
				(*pval)++;
				//}
			break;
		}
		case SMITH_NUM_DEC_PVAL: {
			//if (*pval == 1) {
			//	*pval = -1;
			//} else {
				(*pval)--;
				//}
			break;
		}
		case SMITH_NUM_INC_WGT:	obj->weight += 5;	break;
		case SMITH_NUM_DEC_WGT:	obj->weight -= 5;	break;
	}
	
	return;
}


/**
 * ------------------------------------------------------------------------
 * Handling of mithril
 * ------------------------------------------------------------------------ */

bool object_is_mithril(const struct object *obj)
{
	return of_has(obj->flags, OF_MITHRIL);
}

bool melt_mithril_item(struct player *p, struct object *obj)
{
	int slots_needed = obj->weight / obj->kind->base->max_stack;
	int empty_slots = z_info->pack_size - pack_slots_used(p);

	/* Equipment needs an extra slot */
	if (object_is_equipped(p->body, obj)) slots_needed++;

	if (empty_slots < slots_needed) {
		msg("You do not have enough room in your pack.");
		if (slots_needed - empty_slots == 1) {
			msg("You must free up another slot.");
		} else {
			msg("You must free up %d more slots.", slots_needed - empty_slots);
		}
		return false;
	}

	if (get_check("Are you sure you wish to melt this item down? ")) {
		struct object *new = object_new();
		struct object_kind *kind = lookup_kind(TV_METAL,
											   lookup_sval(TV_METAL,
														   "Piece of Mithril"));

		/* Prepare the base object for the mithril */
		object_prep(new, kind, p->depth, RANDOMISE);

		/* Set the appropriate quantity */
		new->number = obj->weight;

		/* Stop tracking item */
		if (tracked_object_is(p->upkeep, obj))
			track_object(p->upkeep, NULL);

		/* Delete */
		gear_excise_object(p, obj);
		object_delete(cave, &obj);

		/* Inventory has changed, so disable repeat command */
		cmd_disable_repeat();
				
		/* Give the mithril to the player, breaking it up if there's too much */
		while (new->number > new->kind->base->max_stack) {
			struct object *new2 = object_new();

			/* Decrease the main stack */
			new->number -= new->kind->base->max_stack;

			/* Prepare the base object for the mithril */
			object_prep(new2, kind, 0, MINIMISE);

			/* Increase the new stack */
			new2->number = new->kind->base->max_stack;

			/* Give it to the player */
			inven_carry(p, new2, false, false);
		}

		/* Now give the last stack of mithril to the player */
		inven_carry(p, new, false, false);

		return true;
	}

	return false;
}


int mithril_items_carried(struct player *p)
{
	int number = 0;
	struct object *obj;

	for (obj = p->gear; obj; obj = obj->next) {
		if (of_has(obj->flags, OF_MITHRIL)) {
			number++;
		}
	}

	return number;
}

int mithril_carried(struct player *p)
{
	int weight = 0;
	struct object *obj;
		struct object_kind *kind = lookup_kind(TV_METAL,
											   lookup_sval(TV_METAL,
														   "Piece of Mithril"));

	for (obj = p->gear; obj; obj = obj->next) {
		if (obj->kind == kind) {
			weight += obj->number;
		}
	}

	return weight;
}


static void use_mithril(struct player *p, int cost)
{
	struct object *obj;
		struct object_kind *kind = lookup_kind(TV_METAL,
											   lookup_sval(TV_METAL,
														   "Piece of Mithril"));
	int to_go = cost;
	
	for (obj = p->gear; obj; obj = obj->next) {
		if (obj->kind == kind) {
			int amount = MIN(to_go, obj->number);
			bool none_left = false;
			struct object *used = gear_object_for_use(p, obj, amount, true,
													  &none_left);
			object_delete(cave, &used);
			to_go -= amount;
		}
		if (!to_go) break;
	}
	
	return;
}


/**
 * ------------------------------------------------------------------------
 * Difficulty and cost routines
 * ------------------------------------------------------------------------ */
/**
 * Determines the difficulty modifier for pvals.
 *
 * The marginal difficulty of increasing a pval increases by 1 each time,
 * if the base is up to 5, by 2 each time if the base is 6--10, and so on.
 */
static void dif_mod(int value, int positive_base, int *dif_inc)
{
	int mod = 1 + ((positive_base - 1) / 5);

	/* Deal with positive values in a triangular number influenced way */
	if (value > 0) {
		*dif_inc += positive_base * value + mod * (value * (value - 1) / 2);
	}
}

/**
 * Adjust smithing cost for a given object property
 */
static void adjust_smithing_cost(int diff, struct obj_property *prop, struct smithing_cost *smithing_cost)
{
	switch (prop->smith_cost_type) {
		case SMITH_COST_STR: smithing_cost->stat[STAT_STR] += diff; break;
		case SMITH_COST_DEX: smithing_cost->stat[STAT_DEX] += diff; break;
		case SMITH_COST_CON: smithing_cost->stat[STAT_CON] += diff; break;
		case SMITH_COST_GRA: smithing_cost->stat[STAT_GRA] += diff; break;
		case SMITH_COST_EXP: smithing_cost->exp += diff * 100; break;
	}
}


/**
 * Determines the difficulty of a given object.
 */
int object_difficulty(struct object *obj, struct smithing_cost *smithing_cost)
{
	struct object_kind *kind = obj->kind;
	int att = (kind->att == SPECIAL_VALUE) ? 0 : kind->att;
	int evn = (kind->evn == SPECIAL_VALUE) ? 0 : kind->evn;
	int ps = (kind->ps == SPECIAL_VALUE) ? 0 : kind->ps;
	bitflag flags[OF_MAX];
	int dif_inc = 0, dif_dec = 0;
	int i, weight_factor, diff, new, base;
	int smith_brands = 0;
	struct ability *ability = obj->abilities;
	int dif_mult = 100;
	int drain = player->state.skill_use[SKILL_SMITHING] +
		square_forge_bonus(cave, player->grid);
    int cat = 0;

	/* Reset smithing costs */
	smithing_cost->stat[STAT_STR] = 0;
	smithing_cost->stat[STAT_DEX] = 0;
	smithing_cost->stat[STAT_CON] = 0;
	smithing_cost->stat[STAT_GRA] = 0;
	smithing_cost->exp = 0;
	smithing_cost->mithril = 0;
	smithing_cost->uses = 1;
	smithing_cost->drain = 0;
    smithing_cost->weaponsmith = 0;
    smithing_cost->armoursmith = 0;
    smithing_cost->jeweller = 0;
    smithing_cost->enchantment = 0;
    smithing_cost->artistry = 0;
    smithing_cost->artifice = 0;

	of_copy(flags, obj->flags);
    
   /* Special rules for horns */
    if (tval_is_horn(obj)) {
        dif_inc += kind->level;
		if (strstr(obj->kind->name, "Terror")) {
			smithing_cost->stat[STAT_GRA] += 1;
		} else if (strstr(obj->kind->name, "Thunder")) {
			smithing_cost->stat[STAT_DEX] += 1;
		} else if (strstr(obj->kind->name, "Force")) {
			smithing_cost->stat[STAT_STR] += 1;
		} else if (strstr(obj->kind->name, "Blasting")) {
			smithing_cost->stat[STAT_CON] += 1;
		}
	} else if (!tval_is_jewelry(obj)) {
		dif_inc += kind->level / 2;
		/* We need to ignore the flags that are basic to the object type
		 * and focus on the special/artefact ones. */
		of_diff(flags, kind->flags);
	}

	/* Unusual weight */
    if (obj->weight == 0) {
		weight_factor = 1100;
	} else if (obj->weight > kind->weight)	{
		weight_factor = 100 * obj->weight / kind->weight;
	} else {
		weight_factor = 100 * kind->weight / obj->weight;
	}
	dif_inc += (weight_factor - 100) / 10;

	/* Attack bonus */
	diff = obj->att - att;

	/* Special costs for attack bonus for arrows (half difficulty modifier) */
	if (tval_is_ammo(obj) && (diff > 0)) {	
		int old_dif_inc = dif_inc;
		dif_mod(diff, 5, &dif_inc);
		dif_inc = (dif_inc - old_dif_inc) / 2;
	} else {
		/* Normal costs for other items */
		dif_mod(diff, 5, &dif_inc);
	}

	/* Evasion bonus */
	diff = obj->evn - evn;
	dif_mod(diff, 5, &dif_inc);

	/* Damage bonus */
	diff = (obj->ds - kind->ds);
	dif_mod(diff, 8 + obj->dd, &dif_inc);

	/* Protection bonus */
	base = (ps > 0) ? ((ps + 1) * kind->pd) : 0;
	new = (obj->ps > 0) ? ((obj->ps + 1) * obj->pd) : 0;
	diff = new - base;
	dif_mod(diff, 4, &dif_inc);

	/* Object properties */
	for (i = 1; i < z_info->property_max; i++) {
		struct obj_property *prop = &obj_properties[i];
		switch (prop->type) {
			case OBJ_PROPERTY_STAT:
			case OBJ_PROPERTY_SKILL:
			case OBJ_PROPERTY_MOD: {
				diff = obj->modifiers[prop->index];
				if (diff != 0) {
					dif_mod(diff, prop->smith_diff, &dif_inc);
					adjust_smithing_cost(diff, prop, smithing_cost);
				}
				break;
			}
			case OBJ_PROPERTY_FLAG: {
				if (of_has(flags, prop->index)) {
					if (prop->smith_diff > 0) {
						dif_inc += prop->smith_diff;
						adjust_smithing_cost(1, prop, smithing_cost);
					} else if (prop->smith_diff < 0) {
						dif_dec -= prop->smith_diff;
					}
				}
				break;
			}
			case OBJ_PROPERTY_RESIST: {
				if (obj->el_info[prop->index].res_level == 1) {
					dif_inc += prop->smith_diff;
					adjust_smithing_cost(1, prop, smithing_cost);
				}
				break;	
			}
			case OBJ_PROPERTY_SLAY: {
				if (obj->slays && (obj->slays[prop->index])) {
					dif_inc += prop->smith_diff;
				}
				break;
			}
			case OBJ_PROPERTY_BRAND: {
				if (obj->brands && (obj->brands[prop->index])) {
					dif_inc += prop->smith_diff;
					adjust_smithing_cost(1, prop, smithing_cost);
					smith_brands++;
				}
				break;
			}
		}
	}

	/* Extra difficulty for multiple brands */
	if (smith_brands > 1) {
		dif_inc += (smith_brands - 1) * 20;
	}

	/* Abilities */
	while (ability) {
		dif_inc += 5 + ability->level / 2;
		smithing_cost->exp += 500;
		ability = ability->next;
	}

	/* Mithril */
	if (of_has(kind->flags, OF_MITHRIL)) {
		smithing_cost->mithril += obj->weight;
	}

	/* Penalty for being an artefact */
	if (obj->artifact) {
		smithing_cost->uses += 2;
	}

	/* Cap the difficulty reduction at 8 */
	dif_dec = MIN(dif_dec, 8);

	/* Set the overall difficulty */
	diff = dif_inc - dif_dec;

	/* Increased difficulties for minor slots */
	if (tval_is_ring(obj) || tval_is_light(obj) || tval_is_cloak(obj) ||
		tval_is_gloves(obj) || tval_is_boots(obj) || tval_is_ammo(obj)) {
		dif_mult += 20;
	}

	/* Decreased difficulties for easily enchatable items */
	if (of_has(kind->flags, OF_ENCHANTABLE)) {
		dif_mult -= 20;
	}

	/* Apply the difficulty multiplier */
	diff = diff * dif_mult / 100;

    /* Artefact arrows are much easier */
    if (tval_is_ammo(obj) && (obj->number == 1)) diff /= 2;

	/* Deal with masterpiece */
	if ((diff > drain) && player_active_ability(player, "Masterpiece")) {
		smithing_cost->drain += diff - drain;
	}

    /* Determine which additional smithing abilities would be required */
    for (i = 0; i < MAX_SMITHING_TVALS; i++) {
        if (smithing_tvals[i].tval == obj->tval) {
			cat = smithing_tvals[i].category;
		}
	}

    if ((cat == SMITH_TYPE_WEAPON) &&
		!player_active_ability(player, "Weaponsmith")) {
		smithing_cost->weaponsmith = 1;
    }
    if ((cat == SMITH_TYPE_ARMOUR) &&
		!player_active_ability(player, "Armoursmith")) {
		smithing_cost->armoursmith = 1;
    }
    if ((cat == SMITH_TYPE_JEWELRY)
		&& !player_active_ability(player, "Jeweller")) {
		smithing_cost->jeweller = 1;
    }
    if (obj->artifact && !player_active_ability(player, "Artifice")) {
		smithing_cost->artifice = 1;
    }
    if (obj->ego && !player_active_ability(player, "Enchantment")) {
		smithing_cost->enchantment = 1;
    }
    if ((att_valid(obj) && (obj->att > att_max(obj, false))) ||
		(ds_valid(obj) && (obj->ds > ds_max(obj, false))) ||
		(evn_valid(obj) && (obj->evn > evn_max(obj, false))) ||
		(ps_valid(obj) && (obj->ps > ps_max(obj, false)))) {
		smithing_cost->artistry = 1;
    }

	return diff;
}



/**
 * Determines whether an item is too difficult to make.
 */
static int too_difficult(struct object *obj)
{
	struct smithing_cost dummy;
	int dif = object_difficulty(obj, &dummy);
	int ability = player->state.skill_use[SKILL_SMITHING] +
		square_forge_bonus(cave, player->grid);

	if (player_active_ability(player, "Masterpiece")) {
		ability += player->skill_base[SKILL_SMITHING];
	}

	return ability < dif;
}

/**
 * Checks whether a stat is great enough to accommodate a given cost
 */
static bool check_stat_drain(struct player *p, int stat, int cost)
{
	int usable_stat = p->stat_base[stat] + p->stat_drain[stat];
	if (cost <= 0) return true;
	return usable_stat - cost >= -5;
}

/**
 * Checks whether you can pay the costs in terms of ability points and
 * experience needed to make the object.
 */
bool smith_affordable(struct object *obj, struct smithing_cost *smithing_cost)
{
	int stat;

	/* Can't afford non-existent items */
	if (!obj->kind) return false;

	/* Check difficulty */
	if (too_difficult(obj)) return false;

	/* Check stat costs */
	for (stat = 0; stat < STAT_MAX; stat++) {
		if (!check_stat_drain(player, stat, smithing_cost->stat[stat])) {
			return false;
		}
	}

	/* Check XP cost */
	if (smithing_cost->exp > player->new_exp) return false;

	/* Check mithril */
	if ((smithing_cost->mithril > 0) &&
		(smithing_cost->mithril > mithril_carried(player))) {
		return false;
	}

	/* Check forge uses */
	if (square_forge_uses(cave, player->grid) < smithing_cost->uses) {
		return false;
    }

	/* Check abilities */
    if (smithing_cost->weaponsmith || smithing_cost->armoursmith ||
		smithing_cost->jeweller || smithing_cost->enchantment ||
		smithing_cost->artistry || smithing_cost->artifice) {
		return false;
	}

	return true;
}


/**
 * Pay the costs in terms of ability points and experience needed to smith
 * the current object.
 */
static void smith_pay_costs(struct smithing_cost *smithing_cost)
{
	int stat;

	/* Charge */
	for (stat = 0; stat < STAT_MAX; stat++) {
		if (smithing_cost->stat[stat] > 0) {
			player->stat_drain[stat] -= smithing_cost->stat[stat];
		}
	}
	
	/* Charge */
	if (smithing_cost->exp > 0) {
		player->new_exp -= smithing_cost->exp;
	}

	/* Charge */
	if (smithing_cost->mithril > 0) {
		use_mithril(player, smithing_cost->mithril);
	}

	/* Charge */
	if (smithing_cost->uses > 0) {
		int uses = square_forge_uses(cave, player->grid);
		assert(uses >= smithing_cost->uses);
		square_set_forge(cave, player->grid, uses - smithing_cost->uses);
	}

	/* Charge */
	if (smithing_cost->drain > 0) {
		player->skill_base[SKILL_SMITHING] -= smithing_cost->drain;
	}

	/* Calculate the bonuses */
	player->upkeep->update |= (PU_BONUS);

	/* Set the redraw flag for everything */
	player->upkeep->redraw |= (PR_EXP | PR_BASIC);	
}


/**
 * ------------------------------------------------------------------------
 * Object creation routines
 * ------------------------------------------------------------------------ */
/**
 * Set modifiers or other values for base object to 1 where needed
 */
static void set_base_values(struct object *obj)
{
	int i;

	if (obj->kind->att == SPECIAL_VALUE) {
		obj->att = 1;
	}
	if (obj->kind->evn == SPECIAL_VALUE) {
		obj->evn = 1;
	}
	if (obj->kind->ps == SPECIAL_VALUE) {
		obj->ps = 1;
	}
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		/* Hackish calculations needed here */
		if ((obj->kind->modifiers[i].base == SPECIAL_VALUE) ||
			(obj->kind->modifiers[i].m_bonus)) {
			obj->modifiers[i] = 1;
			obj->pval = 1;
		}
	}
}

/**
 * Creates the base object (not in the dungeon, but just as a work in progress).
 */
void create_base_object(struct object_kind *kind, struct object *obj)
{
	/* Wipe the object */
	memset(obj, 0, sizeof(*obj));

	/* Prepare the item */
	object_prep(obj, kind, 0, AVERAGE);

	/* Set the pval to 1 if needed (and evasion/accuracy for rings) */
	set_base_values(obj);

	/* Create arrows by the two dozen */
	if (tval_is_ammo(obj)) {
		obj->number = 24;
	}
}

/**
 * Set an object to the specified special type
 */
void create_special(struct object *obj, struct ego_item *ego)
{
	/* Recreate base object */
	struct object_kind *kind = obj->kind;
	if (obj->slays) mem_free(obj->slays);
	if (obj->brands) mem_free(obj->brands);
	if (obj->abilities) {
		release_ability_list(obj->abilities);
	}
	create_base_object(kind, obj);

	/* Set its 'special' type to reflect the chosen type */
	obj->ego = ego;

	/* Make it into that special type */
	ego_apply_magic(obj, true);
}



/**
 * Copy artifact fields from a_src to a_dst
 */
void artefact_copy(struct artifact *a_dst, struct artifact *a_src)
{
	mem_free(a_dst->slays);
	mem_free(a_dst->brands);
	release_ability_list(a_dst->abilities);

	/* Copy the structure */
	memcpy(a_dst, a_src, sizeof(struct artifact));

	a_dst->next = NULL;
	a_dst->slays = NULL;
	a_dst->brands = NULL;
	a_dst->abilities = NULL;

	if (a_src->slays) {
		a_dst->slays = mem_zalloc(z_info->slay_max * sizeof(bool));
		memcpy(a_dst->slays, a_src->slays, z_info->slay_max * sizeof(bool));
	}
	if (a_src->brands) {
		a_dst->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
		memcpy(a_dst->brands, a_src->brands, z_info->brand_max * sizeof(bool));
	}
	if (a_src->abilities) {
		a_dst->abilities = copy_ability_list(a_src->abilities);
	}
}


/**
 * Fills in the details on an artefact type from an object.
 */
void add_artefact_details(struct artifact *art, struct object *obj)
{
	int i;
	struct smithing_cost dummy;

	art->aidx = z_info->a_max;
	art->tval = obj->tval;
	art->sval = obj->sval;
	art->pval = obj->pval;
	art->att = obj->att;
	art->evn = obj->evn;
	art->dd = obj->dd;
	art->ds = obj->ds;
	art->pd = obj->pd;
	art->ps = obj->ps;
	art->weight = obj->weight;
	of_union(art->flags, obj->flags);
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		art->modifiers[i] = obj->modifiers[i];
	}
	for (i = 0; i < ELEM_MAX; i++) {
		art->el_info[i].res_level = obj->el_info[i].res_level;
		art->el_info[i].flags = obj->el_info[i].flags;
	}
	copy_slays(&art->slays, obj->slays);
	copy_brands(&art->brands, obj->brands);
	if (obj->abilities) {
		struct ability *ability = obj->abilities;
		while (ability) {
			add_ability(&art->abilities, ability);
			ability = ability->next;
		}
	}
	art->level = object_difficulty(obj, &dummy);
	art->rarity = 10;
}


/**
 * Does the given object type support the given property type?
 */
bool applicable_property(struct obj_property *prop, struct object *obj)
{
	struct object_base *base = obj->kind->base;
	bool valid = false;
	int idx = prop ? prop->index : -1;
	char name[80];

	assert(idx >= 0);
	switch (prop->type) {
		case OBJ_PROPERTY_STAT:
		case OBJ_PROPERTY_SKILL:
		case OBJ_PROPERTY_MOD: {
			if (base->smith_modifiers[idx] != 0) valid = true;
			break;
		}
		case OBJ_PROPERTY_FLAG: {
			if (of_has(base->smith_flags, idx)) valid = true;
			break;
		}
		case OBJ_PROPERTY_RESIST: {
			if (base->smith_el_info[idx].res_level >= 1) valid = true;
			break;
		}
		case OBJ_PROPERTY_VULN: {
			if ((base->smith_el_info[idx].res_level == -1) ||
				(base->smith_el_info[idx].res_level == 2))
				valid = true;
			break;
		}
		case OBJ_PROPERTY_SLAY: {
			if (base->smith_slays && (base->smith_slays[idx]))
				valid = true;
			break;
		}
		case OBJ_PROPERTY_BRAND: {
			if (base->smith_brands && (base->smith_brands[idx]))
				valid = true;
			break;
		}
	}

	/* Smithing is OK for War Hammers */
	object_short_name(name, sizeof(name), obj->kind->name);
	if (streq(name, "War Hammer") && (prop->type == OBJ_PROPERTY_SKILL) &&
		(idx == OBJ_MOD_SMITHING)) {
		valid = true;
	}

	return valid;
}


/**
 * Reports if a given property is already on an artefact.
 */
bool object_has_property(struct obj_property *prop, struct object *obj,
						 bool negative)
{	
	int idx = prop ? prop->index : -1;
	assert(idx >= 0);
	switch (prop->type) {
		case OBJ_PROPERTY_STAT: {
			return negative ? obj->modifiers[idx] < 0 : obj->modifiers[idx] > 0;
			break;
		}
		case OBJ_PROPERTY_SKILL:
		case OBJ_PROPERTY_MOD: {
			return obj->modifiers[idx] != 0;
			break;
		}
		case OBJ_PROPERTY_FLAG: {
			return of_has(obj->flags, idx);
			break;
		}
		case OBJ_PROPERTY_RESIST: {
			return obj->el_info[idx].res_level == 1;
			break;
		}
		case OBJ_PROPERTY_VULN: {
			return obj->el_info[idx].res_level == -1;
			break;
		}
		case OBJ_PROPERTY_SLAY: {
			return (obj->slays != NULL) && (obj->slays[idx] == true);
			break;
		}
		case OBJ_PROPERTY_BRAND: {
			return (obj->brands != NULL) && (obj->brands[idx] == true);
			break;
		}
	}
	return false;
}


/**
 * Adds a given property to an artefact.
 */
void add_object_property(struct obj_property *prop, struct object *obj,
						   bool negative)
{	
	int idx = prop ? prop->index : -1;
	assert(idx >= 0);
	switch (prop->type) {
		case OBJ_PROPERTY_STAT:
		case OBJ_PROPERTY_SKILL:
		case OBJ_PROPERTY_MOD: {
			obj->modifiers[idx] = negative ? -1 : 1;
			break;
		}
		case OBJ_PROPERTY_FLAG: {
			of_on(obj->flags, idx);
			break;
		}
		case OBJ_PROPERTY_RESIST: {
			obj->el_info[idx].res_level = 1;
			break;
		}
		case OBJ_PROPERTY_VULN: {
			obj->el_info[idx].res_level = -1;
			break;
		}
		case OBJ_PROPERTY_SLAY: {
			if (!obj->slays) {
				obj->slays = mem_zalloc(z_info->slay_max * sizeof(bool));
			}
			obj->slays[idx] = true;
			break;
		}
		case OBJ_PROPERTY_BRAND: {
			if (!obj->brands) {
				obj->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
			}
			obj->brands[idx] = true;
			break;
		}
	}
}


/**
 * Removes a given property from an artefact.
 */
void remove_object_property(struct obj_property *prop, struct object *obj)
{
	int idx = prop ? prop->index : -1;
	assert(idx >= 0);
	switch (prop->type) {
		case OBJ_PROPERTY_STAT:
		case OBJ_PROPERTY_SKILL:
		case OBJ_PROPERTY_MOD: {
			obj->modifiers[idx] = 0;
			break;
		}
		case OBJ_PROPERTY_FLAG: {
			of_off(obj->flags, idx);
			break;
		}
		case OBJ_PROPERTY_RESIST:
		case OBJ_PROPERTY_VULN: {
			obj->el_info[idx].res_level = 0;
			break;
		}
		case OBJ_PROPERTY_SLAY: {
			obj->slays[idx] = false;
			for (idx = 0; idx < z_info->slay_max; idx++) {
				if (obj->slays[idx]) break;
			}
			if (idx == z_info->slay_max) {
				mem_free(obj->slays);
				obj->slays = NULL;
			}
			break;
		}
		case OBJ_PROPERTY_BRAND: {
			obj->brands[idx] = false;
			for (idx = 0; idx < z_info->brand_max; idx++) {
				if (obj->brands[idx]) break;
			}
			if (idx == z_info->brand_max) {
				mem_free(obj->brands);
				obj->brands = NULL;
			}
			break;
		}
	}
}


/**
 * Actually create the item.
 */
static void create_smithing_item(struct object *obj, struct smithing_cost *cost)
{
	struct object *created = object_new();
	char o_name[80];

	player->smithing_leftover = 0;
	msg("You complete your work.");

	/* Pay the ability/experience costs of smithing */
	smith_pay_costs(cost);

	/* If making an artefact, copy its attributes into the proper place in
	 * the a_info array (noting aidx has already been set) */
	if (obj->artifact) {
		uint16_t aidx = z_info->a_max;
		assert(aidx == obj->artifact->aidx);
		z_info->a_max++;
		a_info = mem_realloc(a_info, z_info->a_max * sizeof(struct artifact));
		aup_info = mem_realloc(aup_info, z_info->a_max * sizeof(*aup_info));

		memset(&a_info[aidx], 0, sizeof(a_info[aidx]));
		artefact_copy(&a_info[aidx], (struct artifact *) obj->artifact);
		a_info[aidx].name = string_make(a_info[aidx].name);
		player->self_made_arts++;

		/* Set update info by hand */
		aup_info[aidx].aidx = aidx;
		aup_info[aidx].created = true;
		aup_info[aidx].seen = true;
		aup_info[aidx].everseen = true;
	}

	/* Create the object */
	object_copy(created, obj);
	
	/* Identify the object */
	object_flavor_aware(player, created);
	object_know(created);

	/* Create description */
	object_desc(o_name, sizeof(o_name), created, ODESC_COMBAT | ODESC_EXTRA,
				player);
	
	/* Record the object creation */
	history_add(player, format("Made %s  %d.%d lb", o_name,
							   (created->weight * created->number) / 10, 
							   (created->weight * created->number) % 10),
				HIST_OBJECT_SMITHED);

	/* Carry the forged item */
	inven_carry(player, created, false, true);
}


/**
 * Start or resume smithing an item.
 */
static void start_smithing(struct player *p, int turns)
{
	/* Flag that we are in the middle of smithing */
	p->upkeep->smithing = true;

	/* Set repeats */
	cmd_set_repeat(turns);

	/* Recalculate bonuses */
	p->upkeep->update |= (PU_BONUS);

	/* Redraw the state */
	p->upkeep->redraw |= (PR_STATE);

	/* Handle stuff */
	handle_stuff(p);
}

/**
 * Start or continue smithing an item.
 */
void do_cmd_smith_aux(void)
{
	bool forge = square_isforge(cave, player->grid);
	bool useless = (square_forge_uses(cave, player->grid) == 0);
	struct object *obj;
	int turns = 0;
	struct smithing_cost cost;

	/* Are we just starting? */
	if (!player->upkeep->smithing) {
		/* Check for actual usability, or warn of exploration mode */
		if (forge && useless) {
			msg("The resources of this forge are exhausted.");
			msg("You will be able to browse options but not make new things.");
		}

		/* Go to the smithing menu */
		obj = smith_object(&cost);

		/* If it was just a test run, leave now */
		if (!obj) return;

		/* Allow the resumption of interrupted smithing */
		if (player->smithing_leftover > 0) {
			turns = player->smithing_leftover;
		} else {
			/* Get the number of turns required */
			turns = MAX(10, object_difficulty(obj, &cost) * 10);

			/* Also set the smithing leftover counter
			 * (to allow you to resume if interrupted) */
			player->smithing_leftover = turns;

			/* Display a message */
			msg("You begin your work.");
		}

		/* Cancel stealth mode */
		player->stealth_mode = false;

		start_smithing(player, turns);
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Finish */
	if (cmd_get_nrepeats() == 1) {
		obj = smith_object(&cost);
		create_smithing_item(obj, &cost);
		player->upkeep->smithing = false;
	}

	/* Redraw the state if requested */
	handle_stuff(player);
}

/**
 * Start or continue smithing an item.
 */
void do_cmd_smith(struct command *cmd)
{
	do_cmd_smith_aux();
}
