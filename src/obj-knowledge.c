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
 * Check if an ego item type is known to the player
 *
 * \param p is the player
 * \param ego is the ego item type
 */
bool player_knows_ego(struct player *p, struct ego_item *ego)
{
	if (!ego) return false;
	return ego->aware;
}

/**
 * ------------------------------------------------------------------------
 * Functions for learning from the behaviour of indvidual objects or shapes
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
 * Object knowledge propagators
 * These functions transfer player knowledge to objects
 * ------------------------------------------------------------------------ */
/**
 * This function does a few book keeping things for item identification.
 *
 * It identifies visible objects for the Lore-Master ability, marks
 * artefacts/specials as seen and grants experience for the first sighting.
 *
 * \param p is the player
 * \param obj is the object
 */
static void player_know_object(struct player *p, struct object *obj)
{
	/* Identify seen items with Lore-Master */
	if (!object_is_known(obj) && player_active_ability(p, "Lore-Master") &&
        !tval_is_chest(obj)) {
		ident(obj);
	}

	/* Mark new identified artefacts/specials and gain experience for them */
	if (object_is_known(obj)) {
		int new_exp = 100;
		if (obj->artifact) {
			const struct artifact *art = obj->artifact;
			if (!is_artifact_seen(art)) {
				/* Mark */
				mark_artifact_seen(art, true);

				/* Gain experience for identification */
				player_exp_gain(p, new_exp);
				p->ident_exp += new_exp;

				/* Record in the history */
				history_find_artifact(p, art);
			}
		} else if (obj->ego) {
			/* We now know about the special item type */
			obj->ego->everseen = true;

			if (!obj->ego->aware) {
				/* Mark */
				obj->ego->aware = true;

				/* Gain experience for identification */
				player_exp_gain(p, new_exp);
				p->ident_exp += new_exp;
			}
		}
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
	if (cave) {
		for (i = 0; i < cave->obj_max; i++) {
			obj = cave->objects[i];

			/* Skip dead objects */
			if (!obj) continue;

			/* Skip held objects */
			if (object_is_carried(p, obj)) continue;

			/* If the object is in sight, or under the player... */
			if (square_isseen(cave, obj->grid) || loc_eq(obj->grid, p->grid)) {
				player_know_object(p, obj);
			}
		}
	}

	/* Player objects */
	for (obj = p->gear; obj; obj = obj->next) {
		player_know_object(p, obj);
	}

	/* Update */
	if (cave)
		autoinscribe_ground(p);
	autoinscribe_pack(p);
	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);
}


/**
 * ------------------------------------------------------------------------
 * Sil pseudo-ID functions
 * ------------------------------------------------------------------------ */
bool object_is_cursed(const struct object *obj)
{
	return obj->notice & OBJ_NOTICE_CURSED ? true : false;
}

bool object_is_broken(const struct object *obj)
{
	return obj->notice & OBJ_NOTICE_BROKEN ? true : false;
}

/**
 * Return a "feeling" (or NULL) about an item.  Method 1 (Weak).
 * Sil - this method can't distinguish artefacts from ego items
 */
int pseudo_id_check_weak(const struct object *obj)
{
	/* Artefacts and Ego-Items*/
	if (obj->artifact || obj->ego) {
		return OBJ_PSEUDO_SPECIAL;
	}

	/* Default to "average" */
	return OBJ_PSEUDO_AVERAGE;
}


/**
 * Return a "feeling" (or NULL) about an item.  Method 2 (Strong).
 * Sil - this method can distinguish artefacts from special items
 */
int pseudo_id_check_strong(const struct object *obj)
{
	/* Artefacts */
	if (obj->artifact) {
		/* Cursed */
		if (object_is_cursed(obj)) return OBJ_PSEUDO_CURSED_ART;

		/* Normal */
		return OBJ_PSEUDO_ARTEFACT;
	}

	/* Ego-Items */
	if (obj->ego) {
		/* Cursed */
		if (object_is_cursed(obj)) return OBJ_PSEUDO_CURSED_SPEC;

		/* Normal */
		return OBJ_PSEUDO_SPECIAL;
	}

	/* Default to "average" */
	return OBJ_PSEUDO_AVERAGE;
}


/**
 * Returns true if this object can be pseudo-ided.
 */
bool can_be_pseudo_ided(const struct object *obj)
{
	if (tval_is_weapon(obj)) return true;
	if (tval_is_armor(obj)) return true;
	if (tval_is_light(obj) && !easy_know(obj)) return true;
  	return false;
}


/**
 * Pseudo-id an item
 */
void pseudo_id(struct object *obj)
{
	/* Skip non-sense machines */
	if (!can_be_pseudo_ided(obj)) return;

	/* It is known, no information needed */
	if (object_is_known(obj)) return;

	/* Sense the object */
	if (player_active_ability(player, "Lore-Keeper")) {
		obj->pseudo = pseudo_id_check_strong(obj);
	} else {
		obj->pseudo = pseudo_id_check_weak(obj);
	}

	/* The object has been "sensed" */
	obj->notice |= OBJ_NOTICE_SENSE;
}


/**
 * Pseudo-id all objects
 */
void pseudo_id_everything(void)
{
	int i;
	struct object *obj;

	/* Dungeon objects */
	for (i = 1; i < cave->obj_max; i++) {
		/* Get the next object from the dungeon */
		obj = cave->objects[i];

		/* Skip dead objects */
		if (!obj || !obj->kind) continue;

		/* Ignore known objects */
		if (object_is_known(obj)) continue;

		/* Pseudo-id it */
		pseudo_id(obj);
	}

	/* Player's gear */
	for (obj = player->gear; obj; obj = obj->next) {
		/* Ignore known objects */
		if (object_is_known(obj)) continue;

		/* Pseudo-id it */
		pseudo_id(obj);
	}

	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
	handle_stuff(player);
}

/**
 * ------------------------------------------------------------------------
 * Sil ID functions
 * ------------------------------------------------------------------------ */
bool object_is_known(const struct object *obj)
{
	if (easy_know(obj)) return true;
	return obj->notice & OBJ_NOTICE_KNOWN ? true : false;
}

void object_know(struct object *obj)
{
	/* Remove special inscription, if any */
	obj->pseudo = OBJ_PSEUDO_NONE;

	/* The object is not "sensed" */
	obj->notice &= ~(OBJ_NOTICE_SENSE);

	/* Clear the "Empty" info */
	obj->notice &= ~(OBJ_NOTICE_EMPTY);

	/* Now we know about the item */
	obj->notice |= (OBJ_NOTICE_KNOWN);
}

void ident(struct object *obj)
{	
	/* Identify it */
	object_flavor_aware(player, obj);
	object_know(obj);

	/* Apply an autoinscription, if necessary */
	apply_autoinscription(player, obj);

	/* Recalculate bonuses */
	player->upkeep->update |= (PU_BONUS);

	/* Combine / Reorder the pack (later) */
	player->upkeep->notice |= (PN_COMBINE);

	/* Redraw stuff */
	player->upkeep->redraw |= (PR_BASIC | PR_INVEN | PR_EQUIP);

	return;
}

void ident_on_wield(struct player *p, struct object *obj)
{
	bool notice = false;
	char o_name[80];
	struct object_kind *kind = obj->kind;
	bitflag flags[OF_SIZE];

	/* Get the flags */
	of_copy(flags, obj->flags);

	/* Ignore previously identified items */
	if (object_is_known(obj)) return;
	
	/* Identify the special item types that do nothing much
	 * (since they have no hidden abilities, they must already be obvious) */
	if (obj->ego) {
		struct ego_item *ego = obj->ego;
		bool mods = false;
		bool elements = false;
		int i;
		for (i = 0; i < OBJ_MOD_MAX; i++) {
			if (obj->ego->modifiers[i]) mods = true;
		}
		for (i = 0; i < ELEM_MAX; i++) {
			if (obj->ego->el_info[i].res_level) elements = true;
		}
		if (of_is_empty(ego->flags) && (ego->abilities == NULL) &&
			!ego->slays && !ego->brands && !mods && !elements) {
			notice = true;
		}
	}

    /* Identify true sight if it cures blindness */
	if (p->timed[TMD_BLIND] && of_has(obj->flags, OF_SEE_INVIS)) {
		notice = true;
	}

	if (obj->artifact || obj->ego) {
		/* For special items and artefacts, we need to ignore the flags that
		 * are basic to the object type and focus on the special/artefact ones.
		 * We can do this by subtracting out the basic flags */
		of_diff(flags, kind->flags);
	}

	/* Identify noticed flags */
	if (of_has(flags, OF_DARKNESS)) {
		notice = true;
		msg("It creates an unnatural darkness.");
	} else if (of_has(flags, OF_LIGHT)) {
		if (!tval_is_light(obj)) {
			notice = true;
			msg("It glows with a wondrous light.");
		} else if (of_has(flags, OF_NO_FUEL) || (obj->timeout > 0)) {
			notice = true;
			msg("It glows very brightly.");
		}
	} else if (of_has(flags, OF_SPEED)) {
		notice = true;
		msg("It speeds your movement.");
	}

	/* Identify noticed mods */
	if (!notice) {
		int i;
		for (i = 0; i < OBJ_MOD_MAX; i++) {
			/* Can identify <+0> items if you already know the flavour */
			if (kind->flavor && object_flavor_is_aware(obj) &&
				randcalc_varies(kind->modifiers[i])) {
				notice = true;
				break;
			} else if (obj->modifiers[i] != 0) {
				mod_message(obj, i);
				notice = true;
				break;
			}
		}
	}
				

	/* Identify the special item types that grant abilities */
	if (!notice && obj->ego) {
		struct ego_item *ego = obj->ego;

		if (ego->abilities) {
			notice = true;
			msg("You have gained the ability '%s'.", ego->abilities->name);
		}
	}

	/* Identify the artefacts that grant abilities */
	if (!notice && obj->artifact) {
		struct artifact *art = (struct artifact *) obj->artifact;

		if (art->abilities) {
			notice = true;
			msg("You have gained the ability '%s'.", art->abilities->name);
		}
	}

    /* Can identify <+0> items if you already know the flavour */
	if (!notice && kind->flavor) {
		if (object_flavor_is_aware(obj)) {
			notice = true;
		} else if (obj->att > 0) {
			notice = true;
			msg("You somehow feel more accurate in combat.");
		} else if (obj->att < 0) {
			notice = true;
			msg("You somehow feel less accurate in combat.");
		} else if (obj->evn > 0) {
			notice = true;
			msg("You somehow feel harder to hit.");
		} else if (obj->evn < 0) {
			notice = true;
			msg("You somehow feel more vulnerable.");
		} else if (obj->pd > 0) {
			notice = true;
			msg("You somehow feel more protected.");
		}
	}

	if (notice) {
		/* Identify the object */
		ident(obj);

		/* Full object description */
		object_desc(o_name, sizeof(o_name), obj,
			ODESC_PREFIX | ODESC_FULL, p);

		/* Print the messages */
		msg("You recognize it as %s.", o_name);
	}

	return;
}

void ident_flag(struct player *p, int flag)
{
	int i;

	/* Scan the equipment */
	for (i = 0; i < p->body.count; i++) {
		struct object *obj = slot_object(p, i);
		struct object_kind *kind = obj ? obj->kind : NULL;
		bitflag flags[OF_SIZE];
		char o_full_name[80];
		char o_short_name[80];

		/* Skip non-objects */
		if (!kind) continue;

		/* Ignore previously identified items */
		if (object_is_known(obj)) continue;
	
		/* Get the flags */
		of_copy(flags, obj->flags);

		if (obj->artifact || obj->ego) {
			/* For special items and artefacts, we need to ignore the flags that
			 * are basic to the object type and focus on the special/artefact
			 * ones.  We can do this by subtracting out the basic flags */
			of_diff(flags, kind->flags);
		}

		/* Short, pre-identification object description */
		object_desc(o_short_name, sizeof(o_short_name), obj, ODESC_BASE, p);

		/* Check for presence */
		if (of_has(flags, flag)) {
			flag_message(flag, o_short_name);

			/* Identify the object */
			ident(obj);

			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), obj,
				ODESC_PREFIX | ODESC_FULL, p);

			/* Print the message */
			msg("You realize that it is %s.", o_full_name);
			return;
		}
	}
}


void ident_element(struct player *p, int element)
{
	int i;

	/* Scan the equipment */
	for (i = 0; i < p->body.count; i++) {
		bool notice = false;
		struct object *obj = slot_object(p, i);
		struct object_kind *kind = obj ? obj->kind : NULL;
		char o_full_name[80];
		char o_short_name[80];

		/* Skip non-objects */
		if (!kind) continue;

		/* Ignore previously identified items */
		if (object_is_known(obj)) continue;
	
		/* Ignore base object properties for special items and artefacts */
		if (obj->artifact || obj->ego) {
			if (kind->el_info[element].res_level ==
				obj->el_info[element].res_level) continue;
		}

		/* Short, pre-identification object description */
		object_desc(o_short_name, sizeof(o_short_name), obj, ODESC_BASE, p);

		/* Check for presence */
		if (obj->el_info[element].res_level > 0) {
			notice = true;
			element_message(element, o_short_name, false);
		} else if (obj->el_info[element].res_level < 0) {
			notice = true;
			element_message(element, o_short_name, true);
		}

		if (notice) {
			/* Identify the object */
			ident(obj);

			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), obj,
				ODESC_PREFIX | ODESC_FULL, p);

			/* Print the message */
			msg("You realize that it is %s.", o_full_name);
			return;
		}
	}
}


void ident_passive(struct player *p)
{
	int i;

	/* Scan the equipment */
	for (i = 0; i < p->body.count; i++) {
		struct object *obj = slot_object(p, i);
		struct object_kind *kind = obj ? obj->kind : NULL;
		char o_short_name[80];
		char o_full_name[80];
		bool notice = false;

		/* Skip non-objects */
		if (!kind) continue;

		/* Ignore previously identified items */
		if (object_is_known(obj)) continue;
	
		if (of_has(obj->flags, OF_REGEN) && (p->chp < p->mhp)) {
				notice = true;
				msg("You notice that you are recovering much faster than usual.");
		} else if (of_has(obj->flags, OF_AGGRAVATE)) {
			notice = true;
			msg("You notice that you are enraging your enemies.");
		} else if (of_has(obj->flags, OF_DANGER)) {
			notice = true;
			msg("You notice that you are attracting more powerful enemies.");
		}

		if (notice) {
			/* Short, pre-identification object description */
			object_desc(o_short_name, sizeof(o_short_name), obj, ODESC_BASE, p);

			/* Identify the object */
			ident(obj);

			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), obj,
				ODESC_PREFIX | ODESC_FULL, p);

			/* Print the message */
			msg("You realize that your %s is %s.", o_short_name,
					   o_full_name);
			return;
		}
	}
}


void ident_see_invisible(const struct monster *mon, struct player *p)
{
	int i;

	/* Scan the equipment */
	for (i = 0; i < p->body.count; i++) {
		struct object *obj = slot_object(p, i);
		struct object_kind *kind = obj ? obj->kind : NULL;

		/* Skip non-objects */
		if (!kind) continue;

		/* Ignore previously identified items */
		if (object_is_known(obj)) continue;
	
		if (of_has(obj->flags, OF_SEE_INVIS)) {
			char m_name[80];
			char o_full_name[80];
			char o_short_name[80];

			/* Get the monster name */
			monster_desc(m_name, sizeof(m_name), mon, MDESC_DEFAULT);

			/* Short, pre-identification object description */
			object_desc(o_short_name, sizeof(o_short_name), obj, ODESC_BASE, p);

			/* Identify the object */
			ident(obj);

			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), obj,
				ODESC_PREFIX | ODESC_FULL, p);
			
			/* Print the messages */
			msg("You notice that you can see %s very clearly.", m_name);
			msg("You realize that your %s is %s.", o_short_name,
					   o_full_name);
			return;
		}
	}
}

void ident_haunted(struct player *p)
{
	int i;

	/* Scan the equipment */
	for (i = 0; i < p->body.count; i++) {
		struct object *obj = slot_object(p, i);
		struct object_kind *kind = obj ? obj->kind : NULL;

		/* Skip non-objects */
		if (!kind) continue;

		/* Ignore previously identified items */
		if (object_is_known(obj)) continue;
	
		if (of_has(obj->flags, OF_HAUNTED)) {
			char o_full_name[80];
			char o_short_name[80];

			/* Short, pre-identification object description */
			object_desc(o_short_name, sizeof(o_short_name), obj, ODESC_BASE, p);

			/* Identify the object */
			ident(obj);

			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), obj,
				ODESC_PREFIX | ODESC_FULL, p);
			
			/* Print the messages */
			msg("You notice that wraiths are being drawn to you.");
			msg("You realize that your %s is %s.", o_short_name,
					   o_full_name);
			return;
		}
	}
}

void ident_cowardice(struct player *p)
{
	int i;

	/* Scan the equipment */
	for (i = 0; i < p->body.count; i++) {
		struct object *obj = slot_object(p, i);
		struct object_kind *kind = obj ? obj->kind : NULL;

		/* Skip non-objects */
		if (!kind) continue;

		/* Ignore previously identified items */
		if (object_is_known(obj)) continue;
	
		if (of_has(obj->flags, OF_COWARDICE)) {
			char o_full_name[80];
			char o_short_name[80];

			/* Short, pre-identification object description */
			object_desc(o_short_name, sizeof(o_short_name), obj, ODESC_BASE, p);

			/* Identify the object */
			ident(obj);

			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), obj,
				ODESC_PREFIX | ODESC_FULL, p);
			
			/* Print the messages */
			msg("You realize that your %s is %s.", o_short_name,
					   o_full_name);
			return;
		}
	}
}

/**
 * Identifies a hunger or sustenance item and prints a message
 */
void ident_hunger(struct player *p)
{
	int i;

	/* Scan the equipment */
	for (i = 0; i < p->body.count; i++) {
		bool notice = false;
		struct object *obj = slot_object(p, i);
		struct object_kind *kind = obj ? obj->kind : NULL;
		char o_full_name[80];
		char o_short_name[80];

		/* Skip non-objects */
		if (!kind) continue;

		/* Ignore previously identified items */
		if (object_is_known(obj)) continue;
	
		if ((of_has(obj->flags, OF_HUNGER) &&
			 (p->state.flags[OF_HUNGER] > 0))) {
			notice = true;
		}
		if ((of_has(obj->flags, OF_SLOW_DIGEST) &&
			 (p->state.flags[OF_HUNGER] < 0))) {
			notice = true;
		}

		if (notice) {
			/* Short, pre-identification object description */
			object_desc(o_short_name, sizeof(o_short_name), obj, ODESC_BASE, p);

			/* Identify the object */
			ident(obj);

			/* Full object description */
			object_desc(o_full_name, sizeof(o_full_name), obj,
				ODESC_PREFIX | ODESC_FULL, p);

			/* Print the messages */
            if (of_has(obj->flags, OF_HUNGER)) {
				msg("You notice that you are growing hungry much faster than before.");
            } else if (of_has(obj->flags, OF_SLOW_DIGEST)) {
				msg("You notice that you are growing hungry slower than before.");
            }
			msg("You realize that your %s is %s.", o_short_name, o_full_name);

			return;
		}
	}
}

/**
 * Describes the effect of a slay
 */
static void slay_desc(char *desc, int len, int flag, int brand, char *m_name)
{
	bool done = false;

	/* Description depends on the type of 'slay' */
	if (flag) {
		done = flag_slay_message(flag, m_name, desc, len);
	} else if (brand) {
		done = brand_message(brand, m_name, desc, len);
	}

	if (!done) {
		my_strcpy(desc, "strikes truly", len);
	}
}

/**
 * Identifies a weapon from one of its slays being active and prints a message
 */
void ident_weapon_by_use(struct object *obj, char *m_name, int flag, int brand,
						 int slay, struct player *p)
{	
	char o_short_name[80];
	char o_full_name[80];
	char slay_description[160];

	if (!obj || object_is_known(obj)) return;

	/* Short, pre-identification object description */
	object_desc(o_short_name, sizeof(o_short_name), obj, ODESC_BASE, p);
	
	/* identify the object */
	ident(obj);
	
	/* Full object description */
	object_desc(o_full_name, sizeof(o_full_name), obj,
		ODESC_PREFIX | ODESC_FULL, p);
	
	/* Description of the 'slay' */
	slay_desc(slay_description, sizeof(slay_description), flag, brand, m_name);

	/* Print the messages */
	msg("Your %s %s.", o_short_name, slay_description);
	msg("You recognize it as %s.", o_full_name);
}

void ident_bow_arrow_by_use(struct object *bow, struct object *arrows,
							char *m_name, int bow_brand, int bow_slay,
							int arrow_flag, int arrow_brand, int arrow_slay,
							struct player *p)
{
	char a_short_name[80];
	char a_full_name[80];
	char b_short_name[80];
	char b_full_name[80];
	char slay_description[160];

	/* Short, pre-identification bow and arrow description */
	object_desc(b_short_name, sizeof(b_short_name), bow, ODESC_BASE, p);
	object_desc(a_short_name, sizeof(a_short_name), arrows, ODESC_BASE, p);

	if (arrow_flag || arrow_brand || arrow_slay) {
		/* Identify the arrows */
		ident(arrows);

		/* Full arrow description */
		object_desc(a_full_name, sizeof(a_full_name), arrows,
			ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM | (1 << 16),
			p);

		slay_desc(slay_description, sizeof(slay_description), arrow_flag,
				  arrow_brand, m_name);

		msg("Your %s %s.", a_short_name, slay_description);
		msg("You recognize it as %s.", a_full_name);

		/* Don't carry on to identify the bow on the same shot */
		return;
	}

	if (bow_brand || bow_slay) {
		/* Identify the bow */
		ident(bow);

		/* Full bow description */
		object_desc(b_full_name, sizeof(b_full_name), bow,
			ODESC_PREFIX | ODESC_FULL, p);

		slay_desc(slay_description, sizeof(slay_description), 0, bow_brand,
				  m_name);

		msg("Your shot %s.", slay_description);
		msg("You recognize your %s to be %s.", b_short_name,
				   b_full_name);
	}
}

/**
 * Automatically identify items of {special} types that the player knows about
 */
void id_known_specials(void)
{
	int i;
	struct object *obj;

	/* Dungeon objects */
	for (i = 1; i < cave->obj_max; i++) {
		/* Get the next object from the dungeon */
		obj = cave->objects[i];

		/* Skip dead objects */
		if (!obj || !obj->kind) continue;

		/* Automatically identify any special items you have seen before */
		if (obj->ego && !object_is_known(obj) &&
			player_knows_ego(player, obj->ego)){
			ident(obj);
		}
	}

	/* Player's gear */
	for (obj = player->gear; obj; obj = obj->next) {
		/* Automatically identify any special items you have seen before */
		if (obj->ego && !object_is_known(obj) &&
			player_knows_ego(player, obj->ego)){
			ident(obj);
		}
	}

	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
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
		if (obj->tval == TV_ARROW) value /= 10;
		
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
	if (obj->artifact) {
		const struct artifact *art = obj->artifact;

		/* Hack -- "worthless" artefacts */
		if (!art->cost) return 0;

		/* Hack -- Use the artefact cost instead */
		value = art->cost;
	} else if (obj->ego) {
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
					value += (obj->modifiers[i] * 300);
				} else if (i < SKILL_MAX) {
					if (obj->modifiers[i] < 0) {
						return 0;
					} else {
						value += (obj->modifiers[i] * 100);
					}
				} else if (i == OBJ_MOD_TUNNEL) {
					if (obj->modifiers[i] < 0) {
						return 0;
					} else {
						value += (obj->modifiers[i] * 50);
					}
				}
			}

			/* Give credit for speed bonus */
			if (of_has(obj->flags, OF_SPEED)) value += 1000;

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
			value += ((value / 20) * (obj->pval / obj->number));

			/* Done */
			break;
		}

		/* Rings/Amulets */
		case TV_RING:
		case TV_AMULET: {
			/* Hack -- negative bonuses are bad */
			if (obj->att < 0) return 0;
			if (obj->evn < 0) return 0;

			/* Give credit for bonuses */
			value += ((obj->att + obj->evn + obj->ps) * 100);

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
			value += ((obj->att - kind->att) * 100);

			/* Give credit for evasion bonus */
			value += ((obj->evn - kind->evn) * 100);

			/* Give credit for sides bonus */
			value += ((obj->ps - kind->ps) * obj->pd * 50);

			/* Give credit for dice bonus */
			value += ((obj->pd - kind->pd) * obj->ps * 50);

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
			value += ((obj->att - kind->att) * 100);

			/* Give credit for evasion bonus */
			value += ((obj->evn - kind->evn) * 100);

			/* Give credit for sides bonus */
			value += ((obj->ds - kind->ds) * obj->dd * 51);

			/* Give credit for dice bonus */
			value += ((obj->dd - kind->dd) * obj->ds * 51);

			/* Done */
			break;
		}

		/* Arrows */
		case TV_ARROW: {
			/* Give credit for hit bonus */
			value += ((obj->att - kind->att) * 10);

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
	if (object_is_known(obj)) {
		/* Broken items -- worthless */
		if (object_is_broken(obj)) return 0;

		/* Cursed items -- worthless */
		if (object_is_cursed(obj)) return 0;

		/* Real value (see above) */
		value = object_value_real(obj);
	} else {
		/* Hack -- Felt broken items */
		if ((obj->notice & (OBJ_NOTICE_SENSE)) && object_is_broken(obj))
			return 0;

		/* Hack -- Felt cursed items */
		if ((obj->notice & (OBJ_NOTICE_SENSE)) && object_is_cursed(obj))
			return 0;

		/* Base value (see above) */
		value = object_value_base(obj);
	}

	/* Return the final value */
	return value;
}

