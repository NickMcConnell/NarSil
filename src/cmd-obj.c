/**
 * \file cmd-obj.c
 * \brief Handle objects in various ways
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007-9 Andi Sidwell, Chris Carr, Ed Graham, Erik Osheim
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
#include "cmds.h"
#include "combat.h"
#include "effects.h"
#include "game-input.h"
#include "init.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-info.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "songs.h"
#include "target.h"
#include "trap.h"

/**
 * ------------------------------------------------------------------------
 * Utility bits and bobs
 * ------------------------------------------------------------------------
 */
/**
 * Check to see if the player can use a staff.
 */
static int check_devices(struct object *obj)
{
	/* Base chance of success */
	int score = player->state.skill_use[SKILL_WILL];

	/* Base difficulty */
	int difficulty = obj->kind->level / 2;

	/* Bonus to roll for 'channeling' ability */
	if (player_active_ability(player, "Channeling")) {
		score += 5;
	}

	/* Confusion hurts skill */
	if (player->timed[TMD_CONFUSED]) difficulty += 5;

	/* Roll for usage */
	if (skill_check(source_player(), score, difficulty, source_none()) <= 0) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("You failed to use the staff properly.");
		return false;
	}

	/* Notice empty staffs */
	if (!obj_has_charges(obj)) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("That staff has no charges left.");
		obj->notice |= (OBJ_NOTICE_EMPTY);
		return false;
	}

	return true;
}

/**
 * ------------------------------------------------------------------------
 * Inscriptions
 * ------------------------------------------------------------------------
 */

/**
 * Remove inscription
 */
void do_cmd_uninscribe(struct command *cmd)
{
	struct object *obj;

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Uninscribe which item?",
			/* Error  */ "You have nothing you can uninscribe.",
			/* Filter */ obj_has_inscrip,
			/* Choice */ USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR) != CMD_OK)
		return;

	obj->note = 0;
	msg("Inscription removed.");

	player->upkeep->notice |= (PN_COMBINE | PN_IGNORE);
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
}

/**
 * Add inscription
 */
void do_cmd_inscribe(struct command *cmd)
{
	struct object *obj;
	const char *str;

	char prompt[1024];
	char o_name[80];

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Inscribe which item?",
			/* Error  */ "You have nothing to inscribe.",
			/* Filter */ NULL,
			/* Choice */ USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR | IS_HARMLESS) != CMD_OK)
		return;

	/* Form prompt */
	object_desc(o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL,
		player);
	strnfmt(prompt, sizeof prompt, "Inscribing %s.", o_name);

	if (cmd_get_string(cmd, "inscription", &str,
			quark_str(obj->note) /* Default */,
			prompt, "Inscribe with what? ") != CMD_OK)
		return;

	obj->note = quark_add(str);
	string_free((char *)str);

	player->upkeep->notice |= (PN_COMBINE | PN_IGNORE);
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
}


/**
 * Autoinscribe all appropriate objects
 */
void do_cmd_autoinscribe(struct command *cmd)
{
	autoinscribe_ground(player);
	autoinscribe_pack(player);

	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
}


/**
 * ------------------------------------------------------------------------
 * Taking off/putting on
 * ------------------------------------------------------------------------
 */

/**
 * Take off an item
 */
void do_cmd_takeoff(struct command *cmd)
{
	struct object *obj;

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Take off or unwield which item?",
			/* Error  */ "You have nothing to take off or unwield.",
			/* Filter */ obj_can_takeoff,
			/* Choice */ USE_EQUIP) != CMD_OK)
		return;

	/* Cannot take off stickied items without special measures. */
	if (handle_stickied_removal(player, obj)) {
		return;
	}

	inven_takeoff(obj);
	combine_pack(player);
	pack_overflow(obj);
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;
}


/**
 * Wield or wear an item
 */
void do_cmd_wield(struct command *cmd)
{
	struct object *equip_obj;
	struct object *weapon = equipped_item_by_slot_name(player, "weapon");
	int shield_slot = slot_by_name(player, "arm");
	char o_name[80];

	unsigned n;

	int slot;
	struct object *obj;
	struct ability *ability;
	bool two_weapon = false;
	bool combine = false;

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Wear or wield which item?",
			/* Error  */ "You have nothing to wear or wield.",
			/* Filter */ obj_can_wear,
			/* Choice */ USE_INVEN | USE_FLOOR | USE_QUIVER) != CMD_OK)
		return;

	/* Check whether it would be too heavy */
	if (!object_is_carried(player, obj) &&
		(player->upkeep->total_weight + obj->weight >
		 weight_limit(player->state)* 3 / 2)) {
		/* Describe it */
		object_desc(o_name, sizeof(o_name), obj,
			ODESC_PREFIX | ODESC_FULL, player);

		if (obj->kind) msg("You cannot lift %s.", o_name);

		/* Abort */
		return;
	}

	/*
	 * Get the slot the object wants to go in, and the item currently
	 * there.  Treat arrows specially to ease merging with what is in
	 * the quiver.
	 */
	if (tval_is_ammo(obj)) {
		int quiver1_slot = slot_by_name(player, "first quiver");
		struct object *quiver1_obj =
			equipped_item_by_slot_name(player, "first quiver");
		int quiver2_slot = slot_by_name(player, "second quiver");
		struct object *quiver2_obj =
			equipped_item_by_slot_name(player, "second quiver");

		if (quiver1_obj
				&& object_similar(quiver1_obj, obj, OSTACK_PACK)
				&& quiver1_obj->number
				< quiver1_obj->kind->base->max_stack) {
			slot = quiver1_slot;
			equip_obj = quiver1_obj;
			combine = true;
		} else if (quiver2_obj
				&& object_similar(quiver2_obj, obj, OSTACK_PACK)
				&& quiver2_obj->number
				< quiver2_obj->kind->base->max_stack) {
			slot = quiver2_slot;
			equip_obj = quiver2_obj;
			combine = true;
		} else if (quiver1_obj && quiver2_obj) {
			/* Ask for arrow set to replace */
			if (cmd_get_item(cmd, "replace", &equip_obj,
					/* Prompt */ "Replace which set of arrows? ",
					/* Error  */ "Error in do_cmd_wield(), please report.",
					/* Filter */ tval_is_ammo,
					/* Choice */ USE_EQUIP) != CMD_OK) {
				return;
			}
			slot = equipped_item_slot(player->body, equip_obj);
		} else {
			slot = (quiver1_obj) ? quiver2_slot : quiver1_slot;
			equip_obj = NULL;
			assert(!slot_object(player, slot));
		}
	} else {
		slot = wield_slot(obj);
		equip_obj = slot_object(player, slot);
	}

	/* Deal with wielding of two-handed weapons when already using a shield */
	if (of_has(obj->flags, OF_TWO_HANDED) && slot_object(player, shield_slot)) {
		bool shield = tval_is_shield(slot_object(player, shield_slot));
		const char *thing = shield ? "shield" : "off-hand weapon";
		if (object_is_cursed(slot_object(player, shield_slot))) {
			msg("You would need to remove your %s, but cannot bear to part with it.", thing);

			/* Cancel the command */
			return;
		}

		/* Warn about dropping item in left hand */
		if (!object_is_carried(player, obj) && pack_is_full()) {
			/* Flush input */
			event_signal(EVENT_INPUT_FLUSH);

			msg("This would require removing (and dropping) your %s.", thing);
			if (!get_check("Proceed? ")) {
				/* Cancel the command */
				return;
			}
		}
	}

	/* Deal with wielding of shield or second weapon when already wielding
	 * a two handed weapon */
	if ((slot == shield_slot) && weapon
			&& of_has(weapon->flags, OF_TWO_HANDED)) {
		if (object_is_cursed(weapon)) {
			msg("You would need to remove your weapon, but cannot bear to part with it.");

			/* Cancel the command */
			return;
		}

		/* Warn about dropping item in left hand */
		if (!object_is_carried(player, obj) && pack_is_full()) {
			/* Flush input */
			event_signal(EVENT_INPUT_FLUSH);

			msg("This would require removing (and dropping) your weapon.");
			if (!get_check("Proceed? ")) {
				/* Cancel the command */
				return;
			}
		}
	}

	/* If the slot is open, wield and be done */
	if (!equip_obj) {
		inven_wield(obj, slot);
		return;
	}

	/* Usually if the slot is taken we'll just replace the item in the slot,
	 * but for rings we need to ask the user which slot they actually
	 * want to replace */
	if (tval_is_ring(obj)) {
		if (cmd_get_item(cmd, "replace", &equip_obj,
						 /* Prompt */ "Replace which ring? ",
						 /* Error  */ "Error in do_cmd_wield(), please report.",
						 /* Filter */ tval_is_ring,
						 /* Choice */ USE_EQUIP) != CMD_OK)
			return;

		/* Change slot if necessary */
		slot = equipped_item_slot(player->body, equip_obj);
	}

	/* Ask about two weapon fighting if necessary */
	for (ability = obj->abilities; ability; ability = ability->next) {
		if (streq(ability->name, "Two Weapon Fighting") &&
			object_is_known(obj)) {
			two_weapon = true;
		}
	}
	if ((player_active_ability(player, "Two Weapon Fighting") || two_weapon) && 
	    tval_is_melee_weapon(obj)) {
		if (!of_has(obj->flags, OF_TWO_HANDED) &&
			!of_has(obj->flags, OF_HAND_AND_A_HALF)) {
			if (get_check("Do you wish to wield it in your off-hand? ")) {
				slot = shield_slot;
				equip_obj = slot_object(player, slot);
				if (!equip_obj) {
					inven_wield(obj, slot);
					return;
				}
			}
		}
	}

	/* Prevent wielding into a stickied slot */
	if (!obj_can_takeoff(equip_obj)) {
		object_desc(o_name, sizeof(o_name), equip_obj, ODESC_BASE,
			player);
		msg("You cannot remove the %s you are %s.", o_name,
			equip_describe(player, slot));
		return;
	}

	/* "!t" checks for taking off */
	n = check_for_inscrip(equip_obj, "!t");
	while (n--) {
		/* Prompt */
		object_desc(o_name, sizeof(o_name), equip_obj,
			ODESC_PREFIX | ODESC_FULL, player);
		
		/* Forget it */
		if (!get_check(format("Really take off %s? ", o_name))) return;
	}

	/* Replacing an equipped cursed item requires special measures. */
	if (handle_stickied_removal(player, equip_obj)) {
		return;
	}

	if (combine) {
		/*
		 * At most, only want as many as can be merged into the wielded
		 * stack.
		 */
		int quantity = MIN(obj->number, equip_obj->kind->base->max_stack
			- equip_obj->number);
		struct object *wielded;
		bool dummy = false;

		/*
		 * By the tests that set combine earlier, should have at least
		 * one to merge.
		 */
		assert(quantity);
		if (object_is_carried(player, obj)) {
			wielded = gear_object_for_use(player, obj, quantity,
				false, &dummy);
			object_absorb(equip_obj, wielded);
		} else {
			/*
			 * Limit the quantity by the player's weight limit.
			 * By the prior check on the weight limit, the quantity
			 * will be at least one.
			 */
			quantity = MIN(quantity, inven_carry_num(player, obj));
			assert(quantity);

			wielded = floor_object_for_use(player, obj, quantity,
				false, &dummy);
			inven_carry(player, wielded, true, true);
		}
	} else {
		inven_takeoff(equip_obj);
		/*
		 * Need to handle possible pack overflow if wielding from the
		 * floor.  Do not want to call combine_pack() if wielding from
		 * the pack because that could leave obj dangling if it combined
		 * with the taken off item.
		 */
		if (!object_is_carried(player, obj)) {
			combine_pack(player);
			pack_overflow(equip_obj);
		}
		inven_wield(obj, slot);
	}
}

/**
 * Drop an item
 */
void do_cmd_drop(struct command *cmd)
{
	int amt;
	struct object *obj;

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Drop which item?",
			/* Error  */ "You have nothing to drop.",
			/* Filter */ NULL,
			/* Choice */ USE_EQUIP | USE_INVEN | USE_QUIVER) != CMD_OK)
		return;

	/* Cannot remove equipped stickied items without special measures. */
	if (handle_stickied_removal(player, obj)) {
		return;
	}

	if (cmd_get_quantity(cmd, "quantity", &amt, obj->number) != CMD_OK)
		return;

	inven_drop(obj, amt);
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;
}

/**
 * ------------------------------------------------------------------------
 * Using items the traditional way
 * ------------------------------------------------------------------------
 */

enum use {
	USE_CHARGE,
	USE_VOICE,
	USE_SINGLE
};

/**
 * Use an object the right way.
 */
static void use_aux(struct command *cmd, struct object *obj, enum use use,
					int snd, bool allow_vertical)
{
	struct effect *effect = object_effect(obj);
	bool can_use = true;
	bool was_aware, from_floor;
	bool known_aim = false;
	bool none_left = false;
	int dir = 5;
	struct trap_kind *rune = lookup_trap("glyph of warding");

	/* Get arguments */
	if (cmd_get_arg_item(cmd, "item", &obj) != CMD_OK) assert(0);

	was_aware = object_flavor_is_aware(obj);

	/* Determine whether we know an item needs to be be aimed */
	if (tval_is_horn(obj) || was_aware) {
		known_aim = true;
	}

	if (obj_needs_aim(obj)) {
		/* Unknown things with no obvious aim get a random direction */
		if (!known_aim) {
			dir = ddd[randint0(8)];
		} else if (cmd_get_target(cmd, "target", &dir, 0, allow_vertical)
				   != CMD_OK) {
			return;
		}

		/* Confusion wrecks aim */
		player_confuse_dir(player, &dir, false);
	}

	/* Track the object used */
	track_object(player->upkeep, obj);

	/* Verify effect */
	assert(effect);

	/* Check voice */
	if (use == USE_VOICE) {
		int voice_cost = player_active_ability(player, "Channeling") ? 10 : 20;

		if (player->csp < voice_cost) {
			event_signal(EVENT_INPUT_FLUSH);
			msg("You are out of breath.");
			return;
		}

		msg("You sound a loud note on the horn.");
		player->csp -= voice_cost;
		player->upkeep->redraw |= PR_MANA;
	}

	/* Check for use if necessary */
	if (use == USE_CHARGE) {
		can_use = check_devices(obj);
	}

	/* Execute the effect */
	if (can_use) {
		int charges = 0;
		uint16_t number = 0;
		bool ident = false, used;
		struct object *work_obj;
		struct object *first_remainder = NULL;

		/* Sound and/or message */
		if (obj->kind->effect_msg) {
			msgt(snd, "%s", obj->kind->effect_msg);
		} else {
			/* Make a noise! */
			sound(snd);
		}

		/*
		 * Tentatively deduct the amount used - the effect could leave
		 * the object inaccessible making it difficult to do after a
		 * successful use.  For the same reason, get a copy of the
		 * object to use for propagating knowledge.
		 */
		if (use == USE_SINGLE) {
			/*
			 * In either case, record number for messages after
			 * use.
			 */
			if (object_is_carried(player, obj)) {
				work_obj = gear_object_for_use(player, obj, 1,
					false, &none_left);
				from_floor = false;
				number = object_pack_total(player, work_obj,
					false, &first_remainder);
				if (first_remainder	&& first_remainder->number == number) {
					first_remainder = NULL;
				}
			} else {
				work_obj = floor_object_for_use(player, obj, 1,
					false, &none_left);
				from_floor = true;
				number = (none_left) ? 0 : obj->number;
			}
		} else  {
			if (use == USE_CHARGE) {
				charges = obj->pval;
				/* Use a single charge */
				obj->pval--;
			}
			work_obj = object_new();
			object_copy(work_obj, obj);
			work_obj->oidx = 0;
			from_floor = !object_is_carried(player, obj);
		}

		/* Do effect; use original not copy (proj. effect handling) */
		target_fix();
		used = effect_do(effect,
						 source_player(),
						 obj,
						 &ident,
						 was_aware,
						 dir,
						 cmd);
		target_release();

		/* Using a horn stops singing.  Eating or quaffing do not. */
		if (use == USE_VOICE) {
			player_change_song(player, NULL, false);
		}

		if (!used) {
			/* Restore the tentative deduction. */
			if (use == USE_SINGLE) {
				/* Drop copy to simplify subsequent logic */
				struct object *dropped = object_new();

				object_copy(dropped, work_obj);
				if (from_floor) {
					drop_near(cave, &dropped, 0, player->grid, false, true);
				} else {
					inven_carry(player, dropped, true, false);
				}
			} else if (use == USE_CHARGE) {
				obj->pval = charges;
			}

			/*
			 * Quit if the item wasn't used and no knowledge was
			 * gained
			 */
			if (was_aware || !ident) {
				object_delete(cave, &work_obj);
				/*
				 * Selection of effect's target may have
				 * triggered update to windows while tentative
				 * deduction was in effect; signal another
				 * update to remedy that.
				 */
				player->upkeep->redraw |= (from_floor) ?
					(PR_OBJECT) : (PR_INVEN | PR_EQUIP);
				return;
			}
		}

		/* Increase knowledge */
		if (!was_aware && ident) {
			object_flavor_aware(player, work_obj);
		} else {
			object_flavor_tried(work_obj);
		}

		if (used && use == USE_CHARGE && object_is_known(work_obj)) {
			/* Describe charges */
			if (from_floor)
				floor_item_charges(work_obj);
			else
				inven_item_charges(work_obj);
		}

		/* Clean up created copy. */
		object_delete(cave, &work_obj);
	} else {
		from_floor = !object_is_carried(player, obj);
	}

	/* Use the turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;
	
	/* Autoinscribe if we are guaranteed to still have any */
	if (!none_left && !from_floor)
		apply_autoinscription(player, obj);

	/* Mark as tried and redisplay */
	player->upkeep->notice |= (PN_COMBINE);
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP | PR_OBJECT);

	/* Hack to make Glyph of Warding work properly */
	if (square_trap_specific(cave, player->grid, rune->tidx)) {
		/* Push objects off the grid */
		if (square_object(cave, player->grid))
			push_object(player->grid);
	}
}


/**
 * Use a staff 
 */
void do_cmd_use_staff(struct command *cmd)
{
	struct object *obj;

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Use which staff? ",
			"You have no staves to use.",
			tval_is_staff,
			USE_INVEN | USE_FLOOR | SHOW_FAIL) != CMD_OK) return;

	use_aux(cmd, obj, USE_CHARGE, MSG_USE_STAFF, false);
}

/**
 * Blow a horn 
 */
void do_cmd_blow_horn(struct command *cmd)
{
	struct object *obj;

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Blow which horn? ",
			"You have no horns to blow.",
			tval_is_horn,
			USE_INVEN | USE_FLOOR | SHOW_FAIL) != CMD_OK) return;

	use_aux(cmd, obj, USE_VOICE, MSG_ZAP_ROD, obj_allows_vertical_aim(obj));
}

/**
 * Eat some food 
 */
void do_cmd_eat_food(struct command *cmd)
{
	struct object *obj;

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Eat which food? ",
			"You have no food to eat.",
			tval_is_edible,
			USE_INVEN | USE_FLOOR) != CMD_OK) return;

	/* If gorged, you cannot eat food */
	if ((player->timed[TMD_FOOD] >= PY_FOOD_MAX) && obj_nourishes(obj)) {
		msg("You are too full to eat it.");
		return;
	}

	use_aux(cmd, obj, USE_SINGLE, MSG_EAT, false);
}

/**
 * Quaff a potion 
 */
void do_cmd_quaff_potion(struct command *cmd)
{
	struct object *obj;

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Quaff which potion? ",
			"You have no potions from which to quaff.",
			tval_is_potion,
			USE_INVEN | USE_FLOOR) != CMD_OK) return;

	/* If gorged, you cannot quaff nourishing potions */
	if ((player->timed[TMD_FOOD] >= PY_FOOD_MAX) && obj_nourishes(obj)) {
		msg("You are too full to drink it.");
		return;
	}

	use_aux(cmd, obj, USE_SINGLE, MSG_QUAFF, false);
}

/**
 * Use any usable item
 */
void do_cmd_use(struct command *cmd)
{
	struct object *obj;

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Use which item? ",
			"You have no items to use.",
			obj_is_useable,
			USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR | SHOW_FAIL) != CMD_OK)
		return;

	if (tval_is_ammo(obj))				do_cmd_fire(cmd);
	else if (tval_is_potion(obj))		do_cmd_quaff_potion(cmd);
	else if (tval_is_edible(obj))		do_cmd_eat_food(cmd);
	else if (tval_is_horn(obj))			do_cmd_blow_horn(cmd);
	else if (tval_is_staff(obj))		do_cmd_use_staff(cmd);
	else if (obj_can_refuel(obj))		do_cmd_refuel(cmd);
	else if (tval_is_wearable(obj))		do_cmd_wield(cmd);
	else
		msg("The item cannot be used at the moment");
}


/**
 * ------------------------------------------------------------------------
 * Refuelling
 * ------------------------------------------------------------------------
 */

static void refill_lamp(struct object *lamp, struct object *obj)
{
	int timeout = lamp->timeout + (obj->timeout ? obj->timeout : obj->pval);

	/* Message */
	if (timeout > z_info->fuel_lamp) {
		if (tval_is_light(obj)) {
			if (!get_check("Refueling from this lantern will waste some fuel. Proceed? ")) {
				return;
			}
		} else if (!get_check("Refueling from this flask will waste some fuel. Proceed? ")) {
			return;
		}
	} else {
		msg("You fuel your lamp.");
	}

	/* Refuel */
	lamp->timeout = timeout;

	/* Comment */
	if (lamp->timeout >= z_info->fuel_lamp) {
		lamp->timeout = z_info->fuel_lamp;
		msg("Your lamp is full.");
	}

	/* Refilled from a lantern */
	if (of_has(obj->flags, OF_TAKES_FUEL)) {
		/* Unstack if necessary */
		if (obj->number > 1) {
			/* Obtain a local object, split */
			struct object *used = object_split(obj, 1);

			/* Remove fuel */
			used->timeout = 0;

			/* Carry or drop */
			if (object_is_carried(player, obj) && inven_carry_okay(used))
				inven_carry(player, used, true, true);
			else
				drop_near(cave, &used, 0, player->grid, false, true);
		} else {
			/* Empty a single lantern */
			obj->timeout = 0;
		}

		/* Combine the pack (later) */
		player->upkeep->notice |= (PN_COMBINE);

		/* Redraw stuff */
		player->upkeep->redraw |= (PR_INVEN);
	} else { /* Refilled from a flask */
		struct object *used;
		bool none_left = false;

		/* Decrease the item from the pack or the floor */
		if (object_is_carried(player, obj)) {
			used = gear_object_for_use(player, obj, 1, true, &none_left);
		} else {
			used = floor_object_for_use(player, obj, 1, true, &none_left);
		}
		object_delete(cave, &used);
	}

	/* Recalculate torch */
	player->upkeep->update |= (PU_TORCH);

	/* Redraw stuff */
	player->upkeep->redraw |= (PR_EQUIP);
}

static void combine_torches(struct object *torch, struct object *obj)
{
	struct object *used;
	bool none_left = false;
	int timeout = torch->timeout + obj->timeout + 5;

	/* Message */
	if ((timeout > z_info->fuel_torch) && !get_check("Refueling from this torch will waste some fuel. Proceed? ")) {
		return;
	}

	/* Refuel */
	torch->timeout = timeout;

	/* Message */
	msg("You combine the torches.");

	/* Comment */
	if (torch->timeout >= z_info->fuel_torch) {
		torch->timeout = z_info->fuel_torch;
		msg("Your torch is fully fueled.");
	} else {
		msg("Your torch glows more brightly.");
	}

	/* Decrease the item from the pack or the floor */
	if (object_is_carried(player, obj)) {
		used = gear_object_for_use(player, obj, 1, true, &none_left);
	} else {
		used = floor_object_for_use(player, obj, 1, true, &none_left);
	}
	object_delete(cave, &used);

	/* Combine the pack (later) */
	player->upkeep->notice |= (PN_COMBINE);

	/* Recalculate torch */
	player->upkeep->update |= (PU_TORCH);

	/* Redraw stuff */
	player->upkeep->redraw |= (PR_EQUIP | PR_INVEN);
}


void do_cmd_refuel(struct command *cmd)
{
	struct object *light = equipped_item_by_slot_name(player, "light");
	struct object *obj;

	/* Check what we're wielding. */
	if (!light || !tval_is_light(light)) {
		msg("You are not wielding a light.");
		return;
	} else if (of_has(light->flags, OF_NO_FUEL)) {
		msg("Your light cannot be refilled.");
		return;
	}

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Refuel with which fuel source? ",
			"You have nothing you can refuel with.",
			obj_can_refuel,
			USE_INVEN | USE_FLOOR | USE_QUIVER) != CMD_OK) return;

	if (of_has(light->flags, OF_TAKES_FUEL)) {
		refill_lamp(light, obj);
	} else if (of_has(light->flags, OF_BURNS_OUT)) {
		combine_torches(light, obj);
	} else {
		return;
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;
}

/*
 * Destroy an item
 */
void do_cmd_destroy(struct command *cmd)
{
	int amt;
	struct object *obj;
	struct object *weapon = equipped_item_by_slot_name(player, "weapon");

	/* Special case for prising Silmarils from the Iron Crown of Morgoth */
	obj = square_object(cave, player->grid);
	if (obj && (obj->artifact == lookup_artifact_name("of Morgoth")) &&
		obj->pval) {
		/* No weapon */
		if (!weapon) {
			msg("To prise a Silmaril from the crown, you would need to wield a weapon.");
		} else {
			/* Wielding a weapon */
			if (get_check("Will you try to prise a Silmaril from the Iron Crown? ")) {
				prise_silmaril(player);

				/* Take a turn */
				player->upkeep->energy_use = z_info->move_energy;

				/* Store the action type */
				player->previous_action[0] = ACTION_MISC;

				return;
			}
		}
	}


	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Destroy which item? ",
			"You have nothing to destroy.",
			NULL,
			USE_INVEN | USE_FLOOR) != CMD_OK) return;

	/* Special case for Iron Crown of Morgoth, if it has Silmarils left */
	if ((obj->artifact == lookup_artifact_name("of Morgoth")) && obj->pval) {
		if (object_is_carried(player, obj)) {
			msg("You would have to put it down first.");
		} else {
			/* No weapon */
			if (!weapon) {
				msg("To prise a Silmaril from the crown, you would need to wield a weapon.");
			} else {
				msg("You decide to try to prise out a Silmaril after all.");
				prise_silmaril(player);

				/* Take a turn */
				player->upkeep->energy_use = z_info->move_energy;

				/* Store the action type */
				player->previous_action[0] = ACTION_MISC;
			}
		}
		return;
	}

	if (cmd_get_quantity(cmd, "quantity", &amt, obj->number) != CMD_OK)
		return;

	if (object_is_carried(player, obj)) {
		if (!inven_destroy(obj, amt)) return;
	} else {
		if (!floor_destroy(obj, amt)) return;
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	player->previous_action[0] = ACTION_MISC;
}


