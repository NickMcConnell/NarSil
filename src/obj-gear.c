/**
 * \file obj-gear.c
 * \brief management of inventory, equipment and quiver
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2014 Nick McConnell
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
#include "cmd-core.h"
#include "game-event.h"
#include "game-input.h"
#include "init.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-util.h"

static const struct slot_info {
	int index;
	bool acid_vuln;
	bool name_in_desc;
	const char *mention;
	const char *describe;
} slot_table[] = {
	#define EQUIP(a, b, c, d, e) { EQUIP_##a, b, c, d, e },
	#include "list-equip-slots.h"
	#undef EQUIP
	{ EQUIP_MAX, false, false, NULL, NULL }
};

/**
 * Return the slot number for a given name, or quit game
 */
int slot_by_name(struct player *p, const char *name)
{
	int i;

	/* Look for the correctly named slot */
	for (i = 0; i < p->body.count; i++) {
		if (streq(name, p->body.slots[i].name)) {
			break;
		}
	}

	assert(i < p->body.count);

	/* Index for that slot */
	return i;
}

/**
 * Gets a slot of the given type, preferentially empty unless full is true
 */
static int slot_by_type(struct player *p, int type, bool full)
{
	int i, fallback = p->body.count;

	/* Look for a correct slot type */
	for (i = 0; i < p->body.count; i++) {
		if (type == p->body.slots[i].type) {
			if (full) {
				/* Found a full slot */
				if (p->body.slots[i].obj != NULL) break;
			} else {
				/* Found an empty slot */
				if (p->body.slots[i].obj == NULL) break;
			}
			/* Not right for full/empty, but still the right type */
			if (fallback == p->body.count)
				fallback = i;
		}
	}

	/* Index for the best slot we found, or p->body.count if none found  */
	return (i != p->body.count) ? i : fallback;
}

/**
 * Indicate whether a slot is of a given type.
 *
 * \param p is the player to test; if NULL, will assume the default body plan.
 * \param slot is the slot index for the player.
 * \param type is one of the EQUIP_* constants from list-equip-slots.h.
 * \return true if the slot can hold that type; otherwise false
 */
bool slot_type_is(const struct player *p, int slot, int type)
{
	/* Assume default body if no player */
	struct player_body body = p ? p->body : bodies[0];

	return body.slots[slot].type == type ? true : false;
}

/**
 * Get the object in a specific slot (if any).  Quit if slot index is invalid.
 */
struct object *slot_object(struct player *p, int slot)
{
	/* Check bounds */
	assert(slot >= 0 && slot < p->body.count);

	/* Ensure a valid body */
	if (p->body.slots && p->body.slots[slot].obj) {
		return p->body.slots[slot].obj;
	}

	return NULL;
}

struct object *equipped_item_by_slot_name(struct player *p, const char *name)
{
	/* Ensure a valid body */
	if (p->body.slots) {
		return slot_object(p, slot_by_name(p, name));
	}

	return NULL;
}

int object_slot(struct player_body body, const struct object *obj)
{
	int i;

	for (i = 0; i < body.count; i++) {
		if (obj == body.slots[i].obj) {
			break;
		}
	}

	return i;
}

bool object_is_equipped(struct player_body body, const struct object *obj)
{
	/* The -2 is for quivers - hoping this is really that simple - NRM */
	return object_slot(body, obj) < body.count - 2;
}

bool object_is_carried(struct player *p, const struct object *obj)
{
	return pile_contains(p->gear, obj);
}

/**
 * Check if an object is in the quiver
 */
bool object_is_in_quiver(const struct player *p, const struct object *obj)
{
	struct player_body body = p->body;
	return (object_slot(body, obj) < body.count) &&
		!object_is_equipped(body, obj);
}

/**
 * Get the total number of objects in the pack or quiver that are like the
 * given object.
 *
 * \param p is the player whose inventory is used for the calculation.
 * \param obj is the template for the objects to look for.
 * \param ignore_inscrip if true, ignore the inscriptions when testing whether
 * an object is similar; otherwise, test the inscriptions as well.
 * \param first if not NULL, set to the first stack like obj (by ordering in
 * the quiver or pack with quiver taking precedence over pack; if the pack
 * and quiver haven't been computed, it will be the first non-equipped stack
 * in the gear).
 */
uint16_t object_pack_total(struct player *p, const struct object *obj,
		bool ignore_inscrip, struct object **first)
{
	uint16_t total = 0;
	char first_label = '\0';
	struct object *cursor;

	if (first) {
		*first = NULL;
	}
	for (cursor = p->gear; cursor; cursor = cursor->next) {
		bool like;

		if (cursor == obj) {
			/*
			 * object_similar() excludes cursor == obj so if
			 * obj is not equipped, account for it here.
			 */
			like = !object_is_equipped(p->body, obj);
		} else if (ignore_inscrip) {
			like = object_similar(obj, cursor, OSTACK_PACK);
		} else {
			like = object_stackable(obj, cursor, OSTACK_PACK);
		}
		if (like) {
			total += cursor->number;
			if (first) {
				char test_label = gear_to_label(p, cursor);

				if (!*first) {
					*first = cursor;
					first_label = test_label;
				} else {
					if (test_label >= 'a'
							&& test_label <= 'z') {
						if (first_label == '\0'
								|| (first_label >= 'a'
								&& first_label <= 'z'
								&& test_label < first_label)) {
							*first = cursor;
							first_label = test_label;
						}
					} else if (test_label >= '0'
							&& test_label <= '9') {
						if (first_label == '\0'
								|| (first_label >= 'a'
								&& first_label <= 'z')
								|| (first_label >= '0'
								&& first_label <= '9'
								&& test_label < first_label)) {
							*first = cursor;
							first_label = test_label;
						}
					}
				}
			}
		}
	}

	return total;
}

/**
 * Calculate the number of pack slots used by the current gear.
 */
int pack_slots_used(const struct player *p)
{
	const struct object *obj;
	int pack_slots = 0;

	for (obj = p->gear; obj; obj = obj->next) {
		/* Equipment doesn't count */
		if (!object_is_equipped(p->body, obj) && !object_is_in_quiver(p, obj)) {
			/* Count regular slots */
			pack_slots++;
		}
	}

	return pack_slots;
}

/**
 * Return a string mentioning how a given item is carried
 */
const char *equip_mention(struct player *p, int slot)
{
	int type = p->body.slots[slot].type;

	/* Heavy */
	if (slot_table[type].name_in_desc)
		return format(slot_table[type].mention, p->body.slots[slot].name);
	else
		return slot_table[type].mention;
}


/**
 * Return a string describing how a given item is being worn.
 * Currently, only used for items in the equipment, not inventory.
 */
const char *equip_describe(struct player *p, int slot)
{
	int type = p->body.slots[slot].type;

	/* Heavy */
	if (slot_table[type].name_in_desc)
		return format(slot_table[type].describe, p->body.slots[slot].name);
	else
		return slot_table[type].describe;
}

/**
 * Determine which equipment slot (if any) an item likes. The slot might (or
 * might not) be open, but it is a slot which the object could be equipped in.
 *
 * For items where multiple slots could work (e.g. rings), the function
 * will try to return an open slot if possible.
 */
int wield_slot(const struct object *obj)
{
	/* Slot for equipment */
	switch (obj->tval)
	{
		case TV_BOW: return slot_by_type(player, EQUIP_BOW, false);
		case TV_AMULET: return slot_by_type(player, EQUIP_AMULET, false);
		case TV_CLOAK: return slot_by_type(player, EQUIP_CLOAK, false);
		case TV_SHIELD: return slot_by_type(player, EQUIP_SHIELD, false);
		case TV_GLOVES: return slot_by_type(player, EQUIP_GLOVES, false);
		case TV_BOOTS: return slot_by_type(player, EQUIP_BOOTS, false);
		case TV_ARROW: return slot_by_type(player, EQUIP_QUIVER, false);
	}

	if (tval_is_melee_weapon(obj))
		return slot_by_type(player, EQUIP_WEAPON, false);
	else if (tval_is_ring(obj))
		return slot_by_type(player, EQUIP_RING, false);
	else if (tval_is_light(obj))
		return slot_by_type(player, EQUIP_LIGHT, false);
	else if (tval_is_body_armor(obj))
		return slot_by_type(player, EQUIP_BODY_ARMOR, false);
	else if (tval_is_head_armor(obj))
		return slot_by_type(player, EQUIP_HAT, false);

	/* No slot available */
	return -1;
}


/**
 * Acid has hit the player, attempt to affect some armor.
 *
 * Note that the "base armor" of an object never changes.
 * If any armor is damaged (or resists), the player takes less damage.
 */
bool minus_ac(struct player *p)
{
	int i, count = 0;
	struct object *obj = NULL;

	/* Avoid crash during monster power calculations */
	if (!p->gear) return false;

	/* Count the armor slots */
	for (i = 0; i < p->body.count; i++) {
		/* Ignore non-armor */
		if (slot_type_is(p, i, EQUIP_WEAPON)) continue;
		if (slot_type_is(p, i, EQUIP_BOW)) continue;
		if (slot_type_is(p, i, EQUIP_RING)) continue;
		if (slot_type_is(p, i, EQUIP_AMULET)) continue;
		if (slot_type_is(p, i, EQUIP_LIGHT)) continue;
		if (slot_type_is(p, i, EQUIP_QUIVER)) continue;

		/* Add */
		count++;
	}

	/* Pick one at random */
	for (i = p->body.count - 1; i >= 0; i--) {
		/* Ignore non-armor */
		if (slot_type_is(p, i, EQUIP_WEAPON)) continue;
		if (slot_type_is(p, i, EQUIP_BOW)) continue;
		if (slot_type_is(p, i, EQUIP_RING)) continue;
		if (slot_type_is(p, i, EQUIP_AMULET)) continue;
		if (slot_type_is(p, i, EQUIP_LIGHT)) continue;
		if (slot_type_is(p, i, EQUIP_QUIVER)) continue;

		if (one_in_(count--)) break;
	}

	/* Get the item */
	obj = slot_object(p, i);
	if (obj && slot_type_is(p, i, EQUIP_SHIELD) && tval_is_weapon(obj)) {
		obj = NULL;
	}

	/* Try to damage or destroy the item */
	if (obj) {
		char o_name[80];
		object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, p);

		/* Object resists */
		if (obj->el_info[ELEM_ACID].flags & EL_INFO_IGNORE) {
			msg("Your %s is unaffected!", o_name);
		} else if ((obj->ps <= 0) && (obj->evn <= 0)) {
			bool none_left;
			struct object *destroyed = gear_object_for_use(p, obj, 1, false, &none_left);
			object_delete(p->cave, NULL, &destroyed->known);
			object_delete(cave, p->cave, &destroyed);
			msg("Your %s is destroyed!", o_name);
		} else {
			msg("Your %s is damaged!", o_name);

			/* Damage the item */
			if (obj->evn >= 0) {
				obj->evn--;
			} else {
				obj->ps--;
			}

			p->upkeep->update |= (PU_BONUS);
			p->upkeep->redraw |= (PR_EQUIP);
		}

		/* There was an effect */
		return true;
	} else {
		/* No damage or effect */
		return false;
	}
}

/**
 * Convert a gear object into a one character label.
 */
char gear_to_label(struct player *p, struct object *obj)
{
	/* Skip rogue-like cardinal direction movement keys. */
	const char labels[] =
		 "abcdefgimnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;

	/* Equipment is easy */
	if (object_is_equipped(p->body, obj) || object_is_in_quiver(p, obj)) {
		return labels[equipped_item_slot(p->body, obj)];
	}

	/* Check the inventory */
	for (i = 0; i < z_info->pack_size; i++) {
		if (p->upkeep->inven[i] == obj) {
			return labels[i];
		}
	}

	return '\0';
}

/**
 * Remove an object from the gear list, leaving it unattached
 * \param p the player to affect
 * \param obj the object to remove
 * \return whether an object was removed
 */
bool gear_excise_object(struct player *p, struct object *obj)
{
	int i;

	pile_excise(&p->gear_k, obj->known);
	pile_excise(&p->gear, obj);

	/* Change the weight */
	p->upkeep->total_weight -= (obj->number * obj->weight);

	/* Make sure it isn't still equipped */
	for (i = 0; i < p->body.count; i++) {
		if (slot_object(p, i) == obj) {
			p->body.slots[i].obj = NULL;
			p->upkeep->equip_cnt--;
		}
	}

	/* Update the gear */
	calc_inventory(p);

	/* Housekeeping */
	p->upkeep->update |= (PU_BONUS);
	p->upkeep->notice |= (PN_COMBINE);
	p->upkeep->redraw |= (PR_INVEN | PR_EQUIP);

	return true;
}

struct object *gear_last_item(struct player *p)
{
	return pile_last_item(p->gear);
}

void gear_insert_end(struct player *p, struct object *obj)
{
	pile_insert_end(&p->gear, obj);
	pile_insert_end(&p->gear_k, obj->known);
}

/**
 * Remove an amount of an object from the inventory or quiver, returning
 * a detached object which can be used.
 *
 * Optionally describe what remains.
 */
struct object *gear_object_for_use(struct player *p, struct object *obj,
	int num, bool message, bool *none_left)
{
	struct object *usable;
	struct object *first_remainder = NULL;
	char name[80];
	char label = gear_to_label(p, obj);
	bool artifact = (obj->known->artifact != NULL);

	/* Bounds check */
	num = MIN(num, obj->number);

	/* Split off a usable object if necessary */
	if (obj->number > num) {
		usable = object_split(obj, num);

		/* Change the weight */
		p->upkeep->total_weight -= (num * obj->weight);

		if (message) {
			uint16_t total;

			/*
			 * Don't show aggregate total in pack if equipped or
			 * if the description could have a number of charges
			 * or recharging notice specific to the stack (not
			 * aggregating those quantities so there would be
			 * confusion if aggregating the count).
			 */
			if (object_is_equipped(p->body, obj)
					|| tval_can_have_charges(obj)
					|| obj->timeout > 0) {
				total = obj->number;
			} else {
				total = object_pack_total(p, obj, false, &first_remainder);
				assert(total >= first_remainder->number);
				if (total == first_remainder->number) {
					first_remainder = NULL;
				}
			}
			object_desc(name, sizeof(name), obj,
				ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
				(total << 16), p);
		}
	} else {
		if (message) {
			if (artifact) {
				object_desc(name, sizeof(name), obj,
					ODESC_FULL | ODESC_SINGULAR, p);
			} else {
				uint16_t total;

				/*
				 * Use same logic as above for showing an
				 * aggregate total.
				 */
				if (object_is_equipped(p->body, obj)
						|| tval_can_have_charges(obj)
						|| obj->timeout > 0) {
					total = obj->number;
				} else {
					total = object_pack_total(p, obj,
						false, &first_remainder);
				}

				assert(total >= num);
				total -= num;
				if (!total || total <= first_remainder->number) {
					first_remainder = NULL;
				}
				object_desc(name, sizeof(name), obj,
					ODESC_PREFIX | ODESC_FULL |
					ODESC_ALTNUM | (total << 16), p);
			}
		}

		/* We're using the entire stack */
		usable = obj;
		gear_excise_object(p, usable);
		*none_left = true;

		/* Stop tracking item */
		if (tracked_object_is(p->upkeep, obj))
			track_object(p->upkeep, NULL);

		/* Inventory has changed, so disable repeat command */
		cmd_disable_repeat();
	}

	/* Housekeeping */
	p->upkeep->update |= (PU_BONUS);
	p->upkeep->notice |= (PN_COMBINE);
	p->upkeep->redraw |= (PR_INVEN | PR_EQUIP);

	/* Print a message if desired */
	if (message) {
		if (artifact) {
			msg("You no longer have the %s (%c).", name, label);
		} else if (first_remainder) {
			label = gear_to_label(p, first_remainder);
			msg("You have %s (1st %c).", name, label);
		} else {
			msg("You have %s (%c).", name, label);
		}
	}

	return usable;
}

/**
 * Handle curse checks and messaging for dropping, removing or throwing an
 * item that may be equipped and may be cursed.
 *
 * \param p is the player trying to drop, remove, or throw the object in
 * question.
 * \param obj is the object the player is trying to drop, remove, or throw.
 * \return true if the object is equipped but can not be dropped, removed,
 * or thrown (it is cursed and the player has no ability to counteract the
 * stickiness).  Return false if the object can be dropped, removed, or thrown.
 */
bool handle_stickied_removal(struct player *p, struct object *obj)
{
		/* There's no problem and no messaging needed if the item is
		 * not equipped or is equipped but not cursed. */
	if (!object_is_equipped(player->body, obj) || !obj_is_cursed(obj)) {
		return false;
	}

	if (player_active_ability(player, "Curse Breaking")) {
		msg("With a great strength of will, you break the curse!");
		uncurse_object(obj);
		return false;
	}

	msg("You cannot bear to part with it.");
	return true;
}

/**
 * Calculate how much of an item is can be carried in the inventory or quiver.
 */
int inven_carry_num(const struct player *p, const struct object *obj)
{
	int max_weight = (weight_limit(p->state) * 3) / 2;
	int num_lim, num_to_quiver, num_left, i;

	/* Check how many can be carried without going over the weight limit. */
	if (p->upkeep->total_weight > max_weight) {
		return 0;
	}
	if (p->upkeep->total_weight + obj->weight * obj->number
			<= max_weight) {
		num_lim = obj->number;
	} else {
		num_lim = (max_weight - p->upkeep->total_weight) / obj->weight;
		if (!num_lim) {
			return 0;
		}
	}

	/* Absorb as many as we can in the quiver. */
	num_to_quiver = 0;
	for (i = 0; i < p->body.count; i++) {
		struct object *q_obj = p->body.slots[i].obj;
		int num_already = q_obj ? q_obj->number : 0;
		if (!slot_type_is(p, i, EQUIP_QUIVER)) continue;
		if (!tval_is_ammo(obj)) continue;
		if (!num_already || object_stackable(q_obj, obj, OSTACK_PACK)) {
			num_to_quiver += obj->kind->base->max_stack - num_already;
		}
	}

	/* The quiver will get everything, or the pack can hold what's left. */
	if (num_to_quiver >= num_lim
			|| z_info->pack_size - pack_slots_used(p) > 0) {
		return num_lim;
	}

	/* See if we can add to a partially full inventory slot. */
	num_left = num_lim - num_to_quiver;
	for (i = 0; i < z_info->pack_size; i++) {
		struct object *inven_obj = p->upkeep->inven[i];
		if (inven_obj && object_stackable(inven_obj, obj, OSTACK_PACK)) {
			num_left -= inven_obj->kind->base->max_stack -
				inven_obj->number;
			if (num_left <= 0) break;
		}
	}

	/* Return the number we can absorb */
	return num_lim - MAX(num_left, 0);
}

/**
 * Check if we have space for some of an item in the pack.
 */
bool inven_carry_okay(const struct object *obj)
{
	return inven_carry_num(player, obj) > 0;
}

/**
 * Describe the charges on an item in the inventory.
 */
void inven_item_charges(struct object *obj)
{
	/* Require staff/wand */
	if (tval_can_have_charges(obj) && object_flavor_is_aware(obj)) {
		msg("You have %d charge%s remaining.",
				obj->pval,
				PLURAL(obj->pval));
	}
}

/**
 * Add an item to the players inventory.
 *
 * If the new item can combine with an existing item in the inventory,
 * it will do so, using object_mergeable() and object_absorb(), else,
 * the item will be placed into the first available gear array index.
 *
 * This function can be used to "over-fill" the player's pack, but only
 * once, and such an action must trigger the "overflow" code immediately.
 * Note that when the pack is being "over-filled", the new item must be
 * placed into the "overflow" slot, and the "overflow" must take place
 * before the pack is reordered, but (optionally) after the pack is
 * combined.  This may be tricky.  See "dungeon.c" for info.
 *
 * Note that this code removes any location information from the object once
 * it is placed into the inventory, but takes no responsibility for removing
 * the object from any other pile it was in.
 */
void inven_carry(struct player *p, struct object *obj, bool absorb,
				 bool message)
{
	bool combining = false;

	/* Check for combining, if appropriate */
	if (absorb) {
		struct object *combine_item = NULL;

		struct object *gear_obj = p->gear;
		while ((combine_item == NULL) && (gear_obj != NULL)) {

			if (!object_is_equipped(p->body, gear_obj) &&
				object_mergeable(gear_obj, obj, OSTACK_PACK)) {
				combine_item = gear_obj;
			}

			gear_obj = gear_obj->next;
		}

		if (combine_item) {
			/* Increase the weight */
			p->upkeep->total_weight += (obj->number * obj->weight);

			/* Combine the items */
			object_absorb(combine_item->known, obj->known);
			obj->known = NULL;
			object_absorb(combine_item, obj);

			/* Ensure numbers are aligned (should not be necessary, but safe) */
			combine_item->known->number = combine_item->number;

			obj = combine_item;
			combining = true;
		}
	}

	/* We didn't manage the find an object to combine with */
	if (!combining) {
		/* Paranoia */
		assert(pack_slots_used(p) <= z_info->pack_size);

		gear_insert_end(p, obj);
		apply_autoinscription(p, obj);

		/* Remove cave object details */
		obj->held_m_idx = 0;
		obj->grid = loc(0, 0);
		obj->known->grid = loc(0, 0);

		/* Update the inventory */
		p->upkeep->total_weight += (obj->number * obj->weight);
		p->upkeep->notice |= (PN_COMBINE);
	}

	p->upkeep->update |= (PU_BONUS | PU_INVEN);
	p->upkeep->redraw |= (PR_INVEN);
	update_stuff(p);

	if (message) {
		char o_name[80];
		struct object *first;
		uint16_t total;
		char label;

		/*
		 * Show an aggregate total if the description doesn't have
		 * a charge/recharging notice that's specific to the stack.
		 */
		if (tval_can_have_charges(obj) || obj->timeout > 0) {
			total = obj->number;
			first = obj;
		} else {
			total = object_pack_total(p, obj, false, &first);
		}
		assert(first && total >= first->number);
		object_desc(o_name, sizeof(o_name), obj,
			ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
			(total << 16), p);
		label = gear_to_label(p, first);
		if (total > first->number) {
			msg("You have %s (1st %c).", o_name, label);
		} else {
			assert(first == obj);
			msg("You have %s (%c).", o_name, label);
		}
	}

	if (object_is_in_quiver(p, obj))
		sound(MSG_QUIVER);
}


/**
 * Wield or wear a single item from the pack or floor
 */
void inven_wield(struct object *obj, int slot)
{
	struct object *wielded, *old = player->body.slots[slot].obj;
	struct object *weapon = equipped_item_by_slot_name(player, "weapon");
	int shield_slot = slot_by_name(player, "arm");
	const char *fmt;
	char o_name[80];
	bool dummy = false;
	int num = tval_is_ammo(obj) ?
		((object_is_carried(player, obj)) ?
			obj->number : inven_carry_num(player, obj)) : 1;
	struct ability *ability;

	/* Deal with wielding of shield or second weapon when already wielding a
	 * hand and a half weapon */
	bool less_effective = weapon && (slot == shield_slot)
		&& of_has(weapon->flags, OF_HAND_AND_A_HALF) && !old;

	/* Increase equipment counter if empty slot */
	if (old == NULL)
		player->upkeep->equip_cnt++;

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;
	
	/* It's either a gear object or a floor object */
	if (object_is_carried(player, obj)) {
		/* Split off a new object if necessary */
		if (obj->number > num) {
			wielded = gear_object_for_use(player, obj, num, false, &dummy);

			/* It's still carried; keep its weight in the total. */
			assert(wielded->number == num);
			player->upkeep->total_weight += wielded->weight * num;

			/* The new item needs new gear and known gear entries */
			wielded->next = obj->next;
			obj->next = wielded;
			wielded->prev = obj;
			if (wielded->next)
				(wielded->next)->prev = wielded;
			wielded->known->next = obj->known->next;
			obj->known->next = wielded->known;
			wielded->known->prev = obj->known;
			if (wielded->known->next)
				(wielded->known->next)->prev = wielded->known;
		} else {
			/* Just use the object directly */
			wielded = obj;
		}
	} else {
		/* Get a floor item and carry it */
		wielded = floor_object_for_use(player, obj, num, false, &dummy);
		inven_carry(player, wielded, false, false);
	}

	/* Wear the new stuff */
	player->body.slots[slot].obj = wielded;

	/* Deal with wielding of two-handed weapons when already using a shield */
	if (of_has(obj->flags, OF_TWO_HANDED) && slot_object(player, shield_slot)) {
		/* Take off shield */
		inven_takeoff(player->body.slots[shield_slot].obj);
	}

	/* Deal with wielding of shield or second weapon when already wielding
	 * a two handed weapon */
	if ((slot == shield_slot) && weapon &&
		of_has(weapon->flags, OF_TWO_HANDED)) {
		/* Stop wielding two handed weapon */
		inven_takeoff(weapon);
	}

	/* Do any ID-on-wield */
	object_learn_on_wield(player, wielded);

	/* Where is the item now */
	if (tval_is_melee_weapon(wielded))
		fmt = "You are wielding %s (%c).";
	else if (wielded->tval == TV_BOW)
		fmt = "You are shooting with %s (%c).";
	else if (tval_is_light(wielded))
		fmt = "Your light source is %s (%c).";
	else if (tval_is_ammo(wielded))
		fmt = "In your quiver you have %s (%c)";
	else
		fmt = "You are wearing %s (%c).";

	/* Describe the result */
	object_desc(o_name, sizeof(o_name), wielded, ODESC_PREFIX | ODESC_FULL,
				player);

	/* Message */
	msgt(MSG_WIELD, fmt, o_name, gear_to_label(player, wielded));

	/* Sticky flag gets a special mention */
	if (obj_is_cursed(wielded)) {
		/* Warn the player */
		msgt(MSG_CURSED, "You have a bad feeling about this...");
		of_on(obj->known->flags, OF_CURSED);
	}

	if (less_effective) {
		/* Describe it */
		object_desc(o_name, sizeof(o_name), weapon, ODESC_BASE, player);

		/* Message */
		msg("You are no longer able to wield your %s as effectively.", o_name);
	}

	/* Activate all of its new abilities */
	for (ability = wielded->abilities; ability; ability = ability->next) {
		if (!player_has_ability(player, ability)) {
			add_ability(&player->item_abilities, ability);
			activate_ability(&player->item_abilities, ability);
		}
	}

	/* See if we have to overflow the pack */
	combine_pack(player);
	pack_overflow(old);

	/* Recalculate bonuses, torch, mana, gear */
	player->upkeep->notice |= (PN_IGNORE);
	player->upkeep->update |= (PU_BONUS | PU_INVEN | PU_UPDATE_VIEW);
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP | PR_ARC | PR_ARMOR);
	player->upkeep->redraw |= (PR_MELEE | PR_STATS | PR_HP | PR_MANA |PR_SPEED);
	update_stuff(player);

	/* Disable repeats */
	cmd_disable_repeat();
}


/**
 * Take off a non-cursed equipment item
 *
 * Note that taking off an item when "full" may cause that item
 * to fall to the ground.
 *
 * Note also that this function does not try to combine the taken off item
 * with other inventory items - that must be done by the calling function.
 */
void inven_takeoff(struct object *obj)
{
	int slot = equipped_item_slot(player->body, obj);
	const char *act;
	char o_name[80];
	struct ability *ability;

	/* Paranoia */
	if (slot == player->body.count) return;

	/* Describe the object */
	object_desc(o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL,
		player);

	/* Describe removal by slot */
	if (slot_type_is(player, slot, EQUIP_WEAPON))
		act = "You were wielding";
	else if (slot_type_is(player, slot, EQUIP_BOW))
		act = "You were holding";
	else if (slot_type_is(player, slot, EQUIP_LIGHT))
		act = "You were holding";
	else
		act = "You were wearing";

	/* De-equip the object */
	player->body.slots[slot].obj = NULL;
	player->upkeep->equip_cnt--;

	/* Remove all of its abilities from the player */
	for (ability = obj->abilities; ability; ability = ability->next) {
		remove_ability(&player->item_abilities, ability);
	}

	player->upkeep->update |= (PU_BONUS | PU_INVEN | PU_UPDATE_VIEW);
	player->upkeep->notice |= (PN_IGNORE);
	update_stuff(player);

	/* Message */
	msgt(MSG_WIELD, "%s %s (%c).", act, o_name, gear_to_label(player, obj));

	return;
}


/**
 * Drop (some of) a non-cursed inventory/equipment item "near" the current
 * location
 *
 * There are two cases here - a single object or entire stack is being dropped,
 * or part of a stack is being split off and dropped
 */
void inven_drop(struct object *obj, int amt)
{
	struct object *dropped;
	bool none_left = false;
	bool equipped = false;
	bool quiver;
	char name[80];
	char label;

	/* Error check */
	if (amt <= 0)
		return;

	/* Check it is still held, in case there were two drop commands queued
	 * for this item.  This is in theory not ideal, but in practice should
	 * be safe. */
	if (!object_is_carried(player, obj))
		return;

	/* Get where the object is now */
	label = gear_to_label(player, obj);

	/* Is it in the quiver? */
	quiver = object_is_in_quiver(player, obj);

	/* Not too many */
	if (amt > obj->number) amt = obj->number;

	/* Take off equipment, don't combine */
	if (object_is_equipped(player->body, obj)) {
		equipped = true;
		inven_takeoff(obj);
	}

	/* Get the object */
	dropped = gear_object_for_use(player, obj, amt, false, &none_left);

	/* Describe the dropped object */
	object_desc(name, sizeof(name), dropped, ODESC_PREFIX | ODESC_FULL,
		player);

	/* Message */
	msg("You drop %s (%c).", name, label);

	/* Describe what's left */
	if (dropped->artifact) {
		object_desc(name, sizeof(name), dropped,
			ODESC_FULL | ODESC_SINGULAR, player);
		msg("You no longer have the %s (%c).", name, label);
	} else {
		struct object *first;
		struct object *desc_target;
		uint16_t total;

		/*
		 * Like gear_object_for_use(), don't show an aggregate total
		 * if it was equipped or the item has charges/recharging
		 * notice that is specific to the stack.
		 */
		if (equipped || tval_can_have_charges(obj) || obj->timeout > 0) {
			first = NULL;
			if (none_left) {
				total = 0;
				desc_target = dropped;
			} else {
				total = obj->number;
				desc_target = obj;
			}
		} else {
			total = object_pack_total(player, obj, false, &first);
			desc_target = (total) ? obj : dropped;
		}

		object_desc(name, sizeof(name), desc_target,
			ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
			(total << 16), player);
		if (!first) {
			msg("You have %s (%c).", name, label);
		} else {
			label = gear_to_label(player, first);
			if (total > first->number) {
				msg("You have %s (1st %c).", name, label);
			} else {
				msg("You have %s (%c).", name, label);
			}
		}
	}

	/* Drop it near the player */
	drop_near(cave, &dropped, 0, player->grid, false, true);

	/* Sound for quiver objects */
	if (quiver)
		sound(MSG_QUIVER);

	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);
}


/**
 * Destroy (some of) a non-cursed inventory/equipment item "near" the current
 * location
 *
 * There are two cases here - a single object or entire stack is being
 * destroyed, or part of a stack is being split off and destroyed
 */
bool inven_destroy(struct object *obj, int amt)
{
	struct object *destroyed;
	bool none_left = false;
	bool equipped = false;
	bool quiver;

	char name[80];
	char out_val[160];
	char label;
	int num = obj->number;

	/* Error check */
	if (amt <= 0)
		return false;

	/* Check it is still held, in case there were two drop commands queued
	 * for this item.  This is in theory not ideal, but in practice should
	 * be safe. */
	if (!object_is_carried(player, obj))
		return false;

	/* Get where the object is now */
	label = gear_to_label(player, obj);

	/* Is it in the quiver? */
	quiver = object_is_in_quiver(player, obj);

	/* Not too many */
	if (amt > obj->number) amt = obj->number;

	/* Describe the destroyed object */
	obj->number = amt;
	object_desc(name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL, player);
	obj->number = num;

	/* Check for known special items */
	strnfmt(out_val, sizeof(out_val), "Really destroy %s? ", name);
	if (!get_check(out_val)) return false;

	/* Take off equipment, don't combine */
	if (object_is_equipped(player->body, obj)) {
		equipped = true;
		inven_takeoff(obj);
	}

	/* Get the object */
	destroyed = gear_object_for_use(player, obj, amt, false, &none_left);

	/* Message */
	msg("You destroy %s (%c).", name, label);

	/* Describe what's left */
	if (destroyed->artifact) {
		object_desc(name, sizeof(name), destroyed,
			ODESC_FULL | ODESC_SINGULAR, player);
		msg("You no longer have the %s (%c).", name, label);
	} else {
		struct object *first;
		struct object *desc_target;
		uint16_t total;

		/*
		 * Like gear_object_for_use(), don't show an aggregate total
		 * if it was equipped or the item has charges/recharging
		 * notice that is specific to the stack.
		 */
		if (equipped || tval_can_have_charges(obj)) {
			first = NULL;
			if (none_left) {
				total = 0;
				desc_target = destroyed;
			} else {
				total = obj->number;
				desc_target = obj;
			}
		} else {
			total = object_pack_total(player, obj, false, &first);
			desc_target = (total) ? obj : destroyed;
		}

		object_desc(name, sizeof(name), desc_target,
			ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
			(total << 16), player);
		if (!first) {
			msg("You have %s (%c).", name, label);
		} else {
			label = gear_to_label(player, first);
			if (total > first->number) {
				msg("You have %s (1st %c).", name, label);
			} else {
				msg("You have %s (%c).", name, label);
			}
		}
	}

	/* Destroy it */
	object_delete(player->cave, NULL, &destroyed->known);
	object_delete(cave, player->cave, &destroyed);

	/* Sound for quiver objects */
	if (quiver)
		sound(MSG_QUIVER);

	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);

	return true;
}


/**
 * Return whether each stack of objects can be merged into two uneven stacks.
 */
static bool inven_can_stack_partial(const struct object *obj1,
									const struct object *obj2)
{
	if (!object_stackable(obj1, obj2, OSTACK_PACK)) {
		return false;
	}

	/* Verify the numbers are suitable for uneven stacks.  Want the
	 * leading stack, obj1, to have its count maximized. */
	if (obj1->number == obj1->kind->base->max_stack) {
		return false;
	}

	return true;
}


/**
 * Combine items in the pack, confirming no blank objects
 */
void combine_pack(struct player *p)
{
	struct object *obj1, *obj2, *prev;
	bool display_message = false;
	bool disable_repeat = false;

	/* Combine the pack (backwards) */
	obj1 = gear_last_item(p);
	while (obj1) {
		assert(obj1->kind);
		prev = obj1->prev;

		/* Scan the items above that item */
		for (obj2 = p->gear; obj2 && obj2 != obj1; obj2 = obj2->next) {
			assert(obj2->kind);

			/* Can we drop "obj1" onto "obj2"? */
			if (object_mergeable(obj2, obj1, OSTACK_PACK)) {
				/*
				 * The quiver slots do not count as equipped
				 * so may be merged with something else.
				 * Handle the side effects of that.
				 */
				int quiver1_slot =
					slot_by_name(p, "first quiver");
				struct object *quiver1_obj =
					slot_object(p, quiver1_slot);
				int quiver2_slot =
					slot_by_name(p, "second quiver");
				struct object *quiver2_obj =
					slot_object(p, quiver2_slot);

				if (obj1 == quiver1_obj) {
					if (obj2 == quiver2_obj) {
						/*
						 * Merging the two quiver slots.
						 * Prefer to keep the first
						 * occupied.
						 */
						p->body.slots[quiver1_slot].obj
							= quiver2_obj;
						p->body.slots[quiver2_slot].obj
							= NULL;
						--p->upkeep->equip_cnt;
					} else {
						/*
						 * Merging with a stack in the
						 * pack.  Put that stack in the
						 * quiver.
						 */
						p->body.slots[quiver1_slot].obj
							= obj2;
					}
				} else if (obj1 == quiver2_obj) {
					if (obj2 == quiver1_obj) {
						/*
						 * Merging the two quiver slots.
						 * Prefer to keep the first
						 * occupied.
						 */
						p->body.slots[quiver2_slot].obj
							= NULL;
						--p->upkeep->equip_cnt;
					} else {
						/*
						 * Merging with a stack in the
						 * pack.  Put that stack in the
						 * quiver.
						 */
						p->body.slots[quiver2_slot].obj
							= obj2;
					}
				}

				display_message = true;
				disable_repeat = true;
				object_absorb(obj2->known, obj1->known);
				obj1->known = NULL;
				object_absorb(obj2, obj1);

				/* Ensure numbers align (should not be necessary, but safer) */
				obj2->known->number = obj2->number;

				break;
			} else {
				if (inven_can_stack_partial(obj2, obj1)) {
					/* Don't display a message for this case:  shuffling items
					 * between stacks isn't interesting to the player. */
					object_absorb_partial(obj2->known, obj1->known);
					object_absorb_partial(obj2, obj1);
					/* Ensure numbers align (should not be
					 * necessary, but safer) */
					obj2->known->number = obj2->number;
					obj1->known->number = obj1->number;

					break;
				}
			}
		}
		obj1 = prev;
	}

	calc_inventory(p);

	/* Redraw gear */
	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);

	/* Message */
	if (display_message) {
		msg("You combine some items in your pack.");

		/*
		 * Stop "repeat last command" from working if a stack was
		 * completely combined with another.
		 */
		if (disable_repeat) cmd_disable_repeat();
	}
}

/**
 * Returns whether the pack is holding the maximum number of items.
 */
bool pack_is_full(void)
{
	return pack_slots_used(player) == z_info->pack_size;
}

/**
 * Returns whether the pack is holding the more than the maximum number of
 * items. If this is true, calling pack_overflow() will trigger a pack overflow.
 */
bool pack_is_overfull(void)
{
	return pack_slots_used(player) > z_info->pack_size;
}

/**
 * Overflow an item from the pack, if it is overfull.
 */
void pack_overflow(struct object *obj)
{
	int i;
	char o_name[80];

	if (!pack_is_overfull()) return;

	/* Disturbing */
	disturb(player, false);

	/* Warning */
	msg("Your pack overflows!");

	/* Get the last proper item */
	for (i = 1; i <= z_info->pack_size; i++)
		if (!player->upkeep->inven[i])
			break;

	/* Drop the last inventory item unless requested otherwise */
	if (!obj) {
		obj = player->upkeep->inven[i - 1];
	}

	/* Rule out weirdness (like pack full, but inventory empty) */
	assert(obj != NULL);

	/* Describe */
	object_desc(o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL,
		player);

	/* Message */
	msg("You drop %s.", o_name);

	/* Excise the object and drop it (carefully) near the player */
	gear_excise_object(player, obj);
	drop_near(cave, &obj, 0, player->grid, false, true);

	/* Describe */
	msg("You no longer have %s.", o_name);

	/* Notice, update, redraw */
	if (player->upkeep->notice) notice_stuff(player);
	if (player->upkeep->update) update_stuff(player);
	if (player->upkeep->redraw) redraw_stuff(player);
}

/**
 * Return true if the player has something in their inventory designed for
 * throwing.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_has_throwable(struct player *p, bool show_msg)
{
	struct object *thrown;
	int nthrow = scan_items(&thrown, 1, player, USE_INVEN, obj_is_throwing);

	if (nthrow <= 0) {
		if (show_msg) {
			msg("You don't have anything designed for throwing in your inventory.");
		}
		return false;
	}
	return true;
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_has_throwable_prereq(void)
{
	return player_has_throwable(player, true);
}
