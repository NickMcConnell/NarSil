/**
 * \file mon-blows.c
 * \brief Monster melee module.
 *
 * Copyright (c) 1997 Ben Harrison, David Reeve Sward, Keldon Jones.
 *               2013 Ben Semmler
 *               2016 Nick McConnell
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
#include "combat.h"
#include "effects.h"
#include "init.h"
#include "monster.h"
#include "mon-attack.h"
#include "mon-blows.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-msg.h"
#include "mon-util.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"

/**
 * ------------------------------------------------------------------------
 * Monster blow methods
 * ------------------------------------------------------------------------ */

typedef enum {
	BLOW_TAG_NONE,
	BLOW_TAG_TARGET,
	BLOW_TAG_OF_TARGET,
	BLOW_TAG_HAS
} blow_tag_t;

static blow_tag_t blow_tag_lookup(const char *tag)
{
	if (strncmp(tag, "target", 6) == 0)
		return BLOW_TAG_TARGET;
	else if (strncmp(tag, "oftarget", 8) == 0)
		return BLOW_TAG_OF_TARGET;
	else if (strncmp(tag, "has", 3) == 0)
		return BLOW_TAG_HAS;
	else
		return BLOW_TAG_NONE;
}

/**
 * Print a monster blow message.
 *
 * We fill in the monster name and/or pronoun where necessary in
 * the message to replace instances of {name} or {pronoun}.
 */
char *monster_blow_method_action(struct blow_method *method, int midx)
{
	const char punct[] = ".!?;:,'";
	char buf[1024] = "\0";
	const char *next;
	const char *s;
	const char *tag;
	const char *in_cursor;
	size_t end = 0;
	struct monster *t_mon = NULL;

	int choice = randint0(method->num_messages);
	struct blow_message *msg = method->messages;

	/* Get the target monster, if any */
	if (midx > 0) {
		t_mon = cave_monster(cave, midx);
	}

	/* Pick a message */
	while (choice--) {
		msg = msg->next;
	}
	in_cursor = msg->act_msg;

	/* Add info to the message */
	next = strchr(in_cursor, '{');
	while (next) {
		/* Copy the text leading up to this { */
		strnfcat(buf, 1024, &end, "%.*s", (int) (next - in_cursor),
			in_cursor);

		s = next + 1;
		while (*s && isalpha((unsigned char) *s)) s++;

		/* Valid tag */
		if (*s == '}') {
			/* Start the tag after the { */
			tag = next + 1;
			in_cursor = s + 1;

			switch (blow_tag_lookup(tag)) {
				case BLOW_TAG_TARGET: {
					char m_name[80];
					if (midx > 0) {
						int mdesc_mode = MDESC_TARG;

						if (!strchr(punct, *in_cursor)) {
							mdesc_mode |= MDESC_COMMA;
						}
						monster_desc(m_name,
							sizeof(m_name), t_mon,
							mdesc_mode);
						strnfcat(buf, sizeof(buf),
							&end, "%s", m_name);
					} else {
						strnfcat(buf, sizeof(buf), &end, "you");
					}
					break;
				}
				case BLOW_TAG_OF_TARGET: {
					char m_name[80];
					if (midx > 0) {
						monster_desc(m_name,
							sizeof(m_name), t_mon,
							MDESC_TARG | MDESC_POSS);
						strnfcat(buf, sizeof(buf), &end, "%s", m_name);
					} else {
						strnfcat(buf, sizeof(buf), &end, "your");
					}
					break;
				}
				case BLOW_TAG_HAS: {
					if (midx > 0) {
						strnfcat(buf, sizeof(buf), &end, "has");
					} else {
						strnfcat(buf, sizeof(buf), &end, "have");
					}
					break;
				}

				default: {
					break;
				}
			}
		} else {
			/* An invalid tag, skip it */
			in_cursor = next + 1;
		}

		next = strchr(in_cursor, '{');
	}
	strnfcat(buf, 1024, &end, "%s", in_cursor);
	return string_make(buf);
}

/**
 * ------------------------------------------------------------------------
 * Monster blow effect helper functions
 * ------------------------------------------------------------------------ */
int blow_index(const char *name)
{
	int i;

	for (i = 1; i < z_info->blow_effects_max; i++) {
		struct blow_effect *effect = &blow_effects[i];
		if (my_stricmp(name, effect->name) == 0)
			return i;
	}
	return 0;
}

/**
 * Monster steals an item from the player
 */
static void steal_player_item(melee_effect_handler_context_t *context)
{
	int tries;

    /* Find an item */
    for (tries = 0; tries < 10; tries++) {
		struct object *obj, *stolen;
		char o_name[80];
		bool split = false;
		bool none_left = false;

        /* Pick an item */
		int index = randint0(z_info->pack_size);

        /* Obtain the item */
        obj = context->p->upkeep->inven[index];

		/* Skip non-objects */
		if (obj == NULL) continue;

        /* Skip artifacts */
        if (obj->artifact) continue;

        /* Get a description */
        object_desc(o_name, sizeof(o_name), obj, ODESC_FULL, context->p);

		/* Is it one of a stack being stolen? */
		if (obj->number > 1)
			split = true;

		/* Message */
		msg("%s %s (%c) was stolen!",
			(split ? "One of your" : "Your"), o_name,
			gear_to_label(context->p, obj));

		/* Steal and carry */
		stolen = gear_object_for_use(context->p, obj, 1,
									 false, &none_left);
		(void)monster_carry(cave, context->mon, stolen);

		/* Obvious */
		context->obvious = true;

		/* Done */
		break;
	}
}

/**
 * Deal the actual melee damage from a monster to a target player or monster
 *
 * This function is used in handlers where there is no further processing of
 * a monster after damage, so we always return true for monster targets
 */
static bool monster_damage_target(melee_effect_handler_context_t *context,
								  bool no_further_monster_effect)
{
	/* Take damage */
	if (context->p) {
		take_hit(context->p, context->net_dam, context->ddesc);
		if (context->p->is_dead) return true;
	}
	return false;
}

/**
 * ------------------------------------------------------------------------
 * Monster blow multi-effect handlers
 * These are each called by several individual effect handlers
 * ------------------------------------------------------------------------ */
/**
 * Do damage as the result of a melee attack that has an elemental aspect.
 *
 * \param context is information for the current attack.
 * \param type is the PROJ_ constant for the element.
 */
static void melee_effect_elemental(melee_effect_handler_context_t *context,
								   int type)
{
	int res = type < ELEM_MAX ? context->p->state.el_info[type].res_level : 0;

	/* Obvious */
	context->obvious = true;

	if (!context->damage) return;

	switch (type) {
		case PROJ_ACID: msg("You are covered in acid!");
			break;
		case PROJ_FIRE: msg("You are enveloped in flames!");
			break;
		case PROJ_COLD: msg("You are covered with frost!");
			break;
	}

	take_hit(context->p, context->net_dam, context->ddesc);
	if (!context->p->is_dead) {
		if (type == PROJ_ACID) {
			minus_ac(context->p);
		}
		inven_damage(context->p, type, MIN((context->net_dam / 10) + 1, 3), res);
		equip_learn_element(context->p, type);
	}
}

/**
 * Do damage as the result of a melee attack that has a status effect.
 *
 * \param context is the information for the current attack.
 * \param type is the TMD_ constant for the effect.
 * \param amount is the amount that the timer should be increased by.
 * \param save_msg is the message that is displayed if the saving throw is
 * successful.
 */
static void melee_effect_timed(melee_effect_handler_context_t *context,
							   int type, int amount, const char *save_msg)
{
	/* Take damage */
	if (monster_damage_target(context, false)) return;

	/* No status effect if tried and failed to damage */
	if (context->damage && !context->net_dam) return;

	/* Handle status */
	if (player_inc_timed(context->p, type, amount, true, true, true)) {
		context->obvious = true;
	} else if (save_msg != NULL) {
		msg("%s", save_msg);
	}
}

/**
 * Do damage as the result of a melee attack that drains a stat.
 *
 * \param context is the information for the current attack.
 * \param stat is the STAT_ constant for the desired stat.
 * \param damage is whether to inflict damage (needed for multiple stat effects)
 */
static void melee_effect_stat(melee_effect_handler_context_t *context, int stat,
							  bool damage)
{
	/* Take damage */
	if (damage) {
		if (monster_damage_target(context, true)) return;

		/* No stat effect if tried and failed to damage */
		if (context->damage && !context->net_dam) return;
	}

	/* Drain stat */
	effect_simple(EF_DRAIN_STAT,
			source_monster(context->mon->midx),
			"0",
			stat,
			0,
			0,
			&context->obvious);
}

/**
 * ------------------------------------------------------------------------
 * Monster blow effect handlers
 * ------------------------------------------------------------------------ */
/**
 * Melee effect handler: Hit the player, but don't do any damage.
 */
static void melee_effect_handler_NONE(melee_effect_handler_context_t *context)
{
	context->obvious = true;
	context->damage = 0;
}

/**
 * Melee effect handler: Hurt the player with no side effects.
 */
static void melee_effect_handler_HURT(melee_effect_handler_context_t *context)
{
	/* Obvious */
	context->obvious = true;

	/* Take damage */
	(void) monster_damage_target(context, true);
}

/**
 * Melee effect handler: Hurt the player with increased chance to wound.
 */
static void melee_effect_handler_WOUND(melee_effect_handler_context_t *context)
{
	/* Obvious */
	context->obvious = true;

	/* Take damage */
	(void) monster_damage_target(context, true);

	/* Usually don't stun */
	if (context->stun && !one_in_(5)) {
		context->stun = false;
	}

	/* Always give a chance to inflict cuts */
	context->cut = true;
}

/**
 * Melee effect handler: Hurt the player with increased chance to stun.
 */
static void melee_effect_handler_BATTER(melee_effect_handler_context_t *context)
{
	/* Obvious */
	context->obvious = true;

	/* Take damage */
	(void) monster_damage_target(context, true);

	/* Usually don't cut */
	if (context->cut && !one_in_(5)) {
		context->cut = false;
	}

	/* Always give a chance to inflict stuns */
	context->stun = true;
}

/**
 * Melee effect handler: Hurt the player with increased chance to stun, 
 * causes an earthquake around the player if it misses.
 */
static void melee_effect_handler_SHATTER(melee_effect_handler_context_t *context)
{
	/* Obvious */
	context->obvious = true;

	/* Take damage */
	(void) monster_damage_target(context, true);

	/* Usually don't cut */
	if (context->cut && !one_in_(5)) {
		context->cut = false;
	}

	/* Always give a chance to inflict stuns */
	context->stun = true;
}

/**
 * Melee effect handler: Take something from the player's inventory.
 */
static void melee_effect_handler_EAT_ITEM(melee_effect_handler_context_t *context)
{
    /* Take damage */
	if (monster_damage_target(context, false)) return;

	/* Steal from player */
	steal_player_item(context);
}

/**
 * Melee effect handler: Attack the player with darkness.
 */
static void melee_effect_handler_DARK(melee_effect_handler_context_t *context)
{
	if (!context->damage) return;
	
	/* Take damage */
	(void) monster_damage_target(context, true);
	equip_learn_element(context->p, PROJ_DARK);
}

/**
 * Melee effect handler: Hit to reduce nutrition.
 */
static void melee_effect_handler_HUNGER(melee_effect_handler_context_t *context)
{
	int amount = 500;

	/* Take damage */
	if (!monster_damage_target(context, true) && (context->damage > 0)) {
		/* Message -- only if appropriate */
		if (!player_saving_throw(context->p, context->mon, 0)) {
			msg("You feel an unnatural hunger...");

			/* Modify the hunger caused by the player's hunger rate
			 * but go up/down by factors of 1.5 rather than 3 */
			if (context->p->state.hunger < 0) { 
				amount *= int_exp(2, -(context->p->state.hunger));
				amount /= int_exp(3, -(context->p->state.hunger));
			} else if (context->p->state.hunger > 0) { 
				amount *= int_exp(3, context->p->state.hunger);
				amount /= int_exp(2, context->p->state.hunger);
			}

			/* Reduce food counter, but not too much. */
			player_dec_timed(context->p, TMD_FOOD, amount, false,
				true);
		}
	}
}

/**
 * Melee effect handler: Poison the player.
 *
 * We can't use melee_effect_timed(), because this is both and elemental attack
 * and a status attack. Note the false value for pure_element for
 * melee_effect_elemental().
 */
static void melee_effect_handler_POISON(melee_effect_handler_context_t *context)
{
	if (!context->damage) return;
	
	/* Take "poison" effect */
	if (player_inc_timed(context->p, TMD_POISONED, context->damage, true,
			true, true))
		context->obvious = true;
	equip_learn_element(context->p, PROJ_POIS);
}

/**
 * Melee effect handler: Attack the player with acid.
 */
static void melee_effect_handler_ACID(melee_effect_handler_context_t *context)
{
	melee_effect_elemental(context, PROJ_ACID);
}

/**
 * Melee effect handler: Attack the player with fire.
 */
static void melee_effect_handler_FIRE(melee_effect_handler_context_t *context)
{
	melee_effect_elemental(context, PROJ_FIRE);
}

/**
 * Melee effect handler: Attack the player with cold.
 */
static void melee_effect_handler_COLD(melee_effect_handler_context_t *context)
{
	melee_effect_elemental(context, PROJ_COLD);
}

/**
 * Melee effect handler: Blind the player.
 */
static void melee_effect_handler_BLIND(melee_effect_handler_context_t *context)
{
	bool blind = context->p->timed[TMD_BLIND] > 0;
	melee_effect_timed(context, TMD_BLIND, damroll(5, 4),
					   blind ? NULL : "Your vision quickly clears.");
}

/**
 * Melee effect handler: Confuse the player.
 */
static void melee_effect_handler_CONFUSE(melee_effect_handler_context_t *context)
{
	melee_effect_timed(context, TMD_CONFUSED, damroll(2, 4),
					   "You resist the effects.");
}

/**
 * Melee effect handler: Paralyze the player.
 */
static void melee_effect_handler_ENTRANCE(melee_effect_handler_context_t *context)
{
	melee_effect_timed(context, TMD_ENTRANCED, damroll(4, 4),
					   "You are unaffected!");
}

/**
 * Melee effect handler: Make the player hallucinate.
 */
static void melee_effect_handler_HALLU(melee_effect_handler_context_t *context)
{
	melee_effect_timed(context, TMD_IMAGE, damroll(10, 4),
					   "You resist the effects.");
}

/**
 * Melee effect handler: Drain the player's strength.
 */
static void melee_effect_handler_LOSE_STR(melee_effect_handler_context_t *context)
{
	melee_effect_stat(context, STAT_STR, true);
}

/**
 * Melee effect handler: Drain the player's dexterity.
 */
static void melee_effect_handler_LOSE_DEX(melee_effect_handler_context_t *context)
{
	melee_effect_stat(context, STAT_DEX, true);
}

/**
 * Melee effect handler: Drain the player's constitution.
 */
static void melee_effect_handler_LOSE_CON(melee_effect_handler_context_t *context)
{
	melee_effect_stat(context, STAT_CON, true);
}

/**
 * Melee effect handler: Drain the player's grace.
 */
static void melee_effect_handler_LOSE_GRA(melee_effect_handler_context_t *context)
{
	melee_effect_stat(context, STAT_GRA, true);
}

/**
 * Melee effect handler: Drain the player's strength and constitution.
 */
static void melee_effect_handler_LOSE_STR_CON(melee_effect_handler_context_t *context)
{
	melee_effect_stat(context, STAT_STR, true);
	melee_effect_stat(context, STAT_CON, false);
}

/**
 * Melee effect handler: Drain all of the player's stats.
 */
static void melee_effect_handler_LOSE_ALL(melee_effect_handler_context_t *context)
{
	melee_effect_stat(context, STAT_STR, true);
	melee_effect_stat(context, STAT_DEX, false);
	melee_effect_stat(context, STAT_CON, false);
	melee_effect_stat(context, STAT_GRA, false);
}

/**
 * Melee effect handler: Hit to disarm.
 *
 * Note that we don't use melee_effect_timed(), due to the different monster
 * learning function.
 */
static void melee_effect_handler_DISARM(melee_effect_handler_context_t *context)
{
	struct object *obj = equipped_item_by_slot_name(context->p, "weapon");
	char o_name[120];
	char m_name[80];

	/* Base difficulty */
	int difficulty = 2;

	/* Nothing to disarm */
	if (!obj) return;

	/* Describe */
	object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, context->p);

	/* Get the monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), context->mon, MDESC_STANDARD);

	/* Adjustment for two handed weapons */
	if (two_handed_melee(context->p)) {
		difficulty -= 4;
	}

	/* Attempt a skill check against strength */
	if (skill_check(source_monster(context->mon->midx), difficulty,
					context->p->state.stat_use[STAT_STR] * 2,
					source_player()) <= 0) {
		msg("%s tries to disarm you, but you keep a grip on your weapon.",
			m_name);
	} else {
		struct object *dislodged;
		bool none_left = false;

		/* Oops */
		msg("%s disarms you! Your %s falls to the ground nearby.", m_name,
			o_name);

		/* Take off equipment */
		inven_takeoff(obj);

		/* Get the original object */
		dislodged = gear_object_for_use(context->p, obj, 1, false, &none_left);

		drop_near(cave, &dislodged, 0, context->p->grid, true, false);
	}
}

/**
 * ------------------------------------------------------------------------
 * Monster blow melee handler selection
 * ------------------------------------------------------------------------ */
melee_effect_handler_f melee_handler_for_blow_effect(const char *name)
{
	static const struct effect_handler_s {
		const char *name;
		melee_effect_handler_f function;
	} effect_handlers[] = {
		{ "NONE", melee_effect_handler_NONE },
		{ "HURT", melee_effect_handler_HURT },
		{ "WOUND", melee_effect_handler_WOUND },
		{ "BATTER", melee_effect_handler_BATTER },
		{ "SHATTER", melee_effect_handler_SHATTER },
		{ "EAT_ITEM", melee_effect_handler_EAT_ITEM },
		{ "DARK", melee_effect_handler_DARK },
		{ "HUNGER", melee_effect_handler_HUNGER },
		{ "POISON", melee_effect_handler_POISON },
		{ "ACID", melee_effect_handler_ACID },
		{ "FIRE", melee_effect_handler_FIRE },
		{ "COLD", melee_effect_handler_COLD },
		{ "BLIND", melee_effect_handler_BLIND },
		{ "CONFUSE", melee_effect_handler_CONFUSE },
		{ "ENTRANCE", melee_effect_handler_ENTRANCE },
		{ "HALLU", melee_effect_handler_HALLU },
		{ "LOSE_STR", melee_effect_handler_LOSE_STR },
		{ "LOSE_DEX", melee_effect_handler_LOSE_DEX },
		{ "LOSE_CON", melee_effect_handler_LOSE_CON },
		{ "LOSE_GRA", melee_effect_handler_LOSE_GRA },
		{ "LOSE_STR_CON", melee_effect_handler_LOSE_STR_CON },
		{ "LOSE_ALL", melee_effect_handler_LOSE_ALL },
		{ "DISARM", melee_effect_handler_DISARM },
		{ NULL, NULL },
	};
	const struct effect_handler_s *current = effect_handlers;

	while (current->name != NULL && current->function != NULL) {
		if (my_stricmp(name, current->name) == 0)
			return current->function;

		current++;
	}

	return NULL;
}
