/**
 * \file project-player.c
 * \brief projection effects on the player
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
#include "combat.h"
#include "effects.h"
#include "init.h"
#include "mon-desc.h"
#include "mon-predicate.h"
#include "mon-lore.h"
#include "mon-util.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "source.h"
#include "trap.h"

typedef struct project_player_handler_context_s {
	const struct source origin;
	const struct loc grid;
	struct monster *mon;
	int dd;
	int ds;
	int dam;
	const int type;
} project_player_handler_context_t;

/**
 * Adjust damage according to resistance or vulnerability.
 *
 * \param p is the player
 * \param dd is the unadjusted number of dice for that attack's damage
 * \param ds is the unadjusted number of sides for each of the attack's
 * damage dice
 * \param type is the attack type we are checking.
 */
int adjust_dam(struct player *p, int dd, int ds, int type)
{
	int prt = protection_roll(p, type, false, RANDOMISE);
	int dam = damroll(dd, ds);
	int resist = 1;
	int net_dam;

	/* If an actual player exists, get their actual resist */
	if (p && p->race) {
		resist = type < ELEM_MAX ? p->state.el_info[type].res_level : 0;
		/* Hack for acid */
		if (!type) resist = 1;
		if (resist < 1) resist -= 2;
	}

	/* Get the actual damage */
	net_dam = dam / resist;
	net_dam = net_dam > prt ? net_dam - prt : 0;

	event_signal_combat_damage(EVENT_COMBAT_DAMAGE, dd, ds, dam, -1, -1, prt,
							   100, type, false);

	return net_dam;
}

/**
 * ------------------------------------------------------------------------
 * Player handlers
 * ------------------------------------------------------------------------ */

typedef void (*project_player_handler_f)(project_player_handler_context_t *);

static void project_player_handler_FIRE(project_player_handler_context_t *context)
{
	inven_damage(player, PROJ_FIRE, MIN(context->dam / 10, 3), 1);
	equip_learn_element(player, ELEM_FIRE);
}

static void project_player_handler_COLD(project_player_handler_context_t *context)
{
	inven_damage(player, PROJ_FIRE, MIN(context->dam / 10, 3), 1);
	equip_learn_element(player, ELEM_COLD);
}

static void project_player_handler_POIS(project_player_handler_context_t *context)
{
	player_inc_timed(player, TMD_POISONED, context->dam, true, true, false);
	equip_learn_element(player, ELEM_POIS);
}

static void project_player_handler_DARK(project_player_handler_context_t *context)
{
	int resistance = MAX(1, square_light(cave, player->grid));
	if (one_in_(resistance)) {
		(void)player_inc_timed(player, TMD_BLIND, damroll(2, 4),
			true, true, true);
	}
}

static void project_player_handler_NOTHING(project_player_handler_context_t *context)
{
}

static void project_player_handler_HURT(project_player_handler_context_t *context)
{
}

static void project_player_handler_ARROW(project_player_handler_context_t *context)
{
}

static void project_player_handler_BOULDER(project_player_handler_context_t *context)
{
}

static void project_player_handler_ACID(project_player_handler_context_t *context)
{
	if (context->dam) {
		minus_ac(player);
		inven_damage(player, PROJ_ACID, MIN(context->dam / 10, 3), 1);
	}
}

static void project_player_handler_SOUND(project_player_handler_context_t *context)
{
	/* Stun */
	if (!player_inc_timed(player, TMD_STUN, context->dam, true, true,
			true)) {
		msg("You are unfazed");
	}}

static void project_player_handler_FORCE(project_player_handler_context_t *context)
{
}

static void project_player_handler_LIGHT(project_player_handler_context_t *context)
{
}

static void project_player_handler_KILL_WALL(project_player_handler_context_t *context)
{
}

static void project_player_handler_SLEEP(project_player_handler_context_t *context)
{
}

static void project_player_handler_SPEED(project_player_handler_context_t *context)
{
}

static void project_player_handler_SLOW(project_player_handler_context_t *context)
{
}

static void project_player_handler_CONFUSION(project_player_handler_context_t *context)
{
}

static void project_player_handler_FEAR(project_player_handler_context_t *context)
{
}

static void project_player_handler_EARTHQUAKE(project_player_handler_context_t *context)
{
}

static void project_player_handler_DARK_WEAK(project_player_handler_context_t *context)
{
}

static void project_player_handler_KILL_DOOR(project_player_handler_context_t *context)
{
}

static void project_player_handler_LOCK_DOOR(project_player_handler_context_t *context)
{
}

static void project_player_handler_KILL_TRAP(project_player_handler_context_t *context)
{
}

static void project_player_handler_DISP_ALL(project_player_handler_context_t *context)
{
}

/**
 * Handle monster missiles (arrows, boulders) incoming on the player
 *
 * This is not a player handler, but takes a context argument anyway.
 * This function assumes that the attack is either arrow or boulder.
 */
static void monster_ranged_attack(project_player_handler_context_t *context,
								  char *killer)
{
	int total_attack_mod, total_evasion_mod, crit_bonus_dice, hit_result;
	int total_dd, total_ds;
	int prt, dam, net_dam, weight;
	struct monster *mon = context->mon;
	struct monster_race *race = mon->race;
	bool arrow = context->type == PROJ_ARROW;

	/* Determine the monster's attack score */
	total_attack_mod = total_monster_attack(player, mon, race->spell_power);

	/* Determine the player's evasion score */
	total_evasion_mod = total_player_evasion(player, mon, false);

	/* Archery-specific stuff */
	if (arrow) {
		/* Target only gets half the evasion modifier against archery */
		total_evasion_mod /= 2;
			
		/* Simulate weights of longbows and shortbows */
		if (context->ds >= 11) {
			weight = 30;
		} else {
			weight = 20;
		}
	} else {
		weight = 100;
	}

	/* Perform the hit roll */
	hit_result = hit_roll(total_attack_mod, total_evasion_mod,
						  source_monster(mon->midx), source_player(), true);

	if (hit_result > 0) {
		crit_bonus_dice = crit_bonus(player, hit_result, weight, &r_info[0],
									 SKILL_ARCHERY, !arrow);
		total_dd = context->dd + crit_bonus_dice;
		total_ds = context->ds;

		dam = damroll(total_dd, total_ds);

		/* Armour is effective against arrows and boulders */
		prt = protection_roll(player, PROJ_HURT, false, RANDOMISE);
		net_dam = (dam - prt > 0) ? (dam - prt) : 0;

		if (player->timed[TMD_BLIND]) {
			msg("You are hit by %s.", projections[context->type].blind_desc);
		} else {
			if (net_dam > 0) {
				if (crit_bonus_dice == 0) {
					msg("It hits you.");
				} else {
					msg("It hits!");
				}
			}
		}

		event_signal_combat_damage(EVENT_COMBAT_DAMAGE, total_dd, total_ds, dam,
								   -1, -1, prt, 100, PROJ_HURT, false);
		event_signal_hit(EVENT_HIT, net_dam, PROJ_HURT, player->is_dead,
						 player->grid);

		if (net_dam) {
			take_hit(player, net_dam, killer);

			/* Deal with crippling shot ability */
			if (arrow && rf_has(race->flags, RF_CRIPPLING) &&
				(crit_bonus_dice >= 1) && (net_dam > 0)) {
				/* Sil-y: ideally we'd use a call to allow_player_slow()
				 * here, but that doesn't work as it can't take the
				 * level of the critical into account. Sadly my solution
				 * doesn't let you ID free action items. */
				int difficulty = player->state.skill_use[SKILL_WILL] +
					player->state.flags[OF_FREE_ACT] * 10;

				if (skill_check(source_monster(mon->midx), crit_bonus_dice * 4,
								difficulty,	source_player()) > 0) {
					struct monster_lore *lore = get_lore(race);

					/* Remember that the monster can do this */
					if (monster_is_visible(mon)) {
						rf_on(lore->flags, RF_CRIPPLING);
					}
					msg("The shot tears into your thigh!");
					/* Slow the player */
					player_inc_timed(player, TMD_SLOW,
						crit_bonus_dice,
						true, true, false);
				}
			}
		}

		/* Make some noise */
		monsters_hear(true, false, arrow ? -5 : -10);
	}
}

static const project_player_handler_f player_handlers[] = {
	#define ELEM(a) project_player_handler_##a,
	#include "list-elements.h"
	#undef ELEM
	#define PROJ(a) project_player_handler_##a,
	#include "list-projections.h"
	#undef PROJ
	NULL
};

/**
 * Called from project() to affect the player
 *
 * Called for projections with the PROJECT_PLAY flag set, which includes
 * bolt, beam, ball and breath effects.
 *
 * \param origin describes what generated the projection
 * \param grid is the coordinates of the grid being handled
 * \param dd is the number of dice of "damage" from the effect
 * \param ds is the number of sides for each of the damage dice
 * \param typ is the projection (PROJ_) type
 * \return whether the effects were obvious
 *
 * If "r" is non-zero, then the blast was centered elsewhere; the damage
 * is reduced in project() before being passed in here.  This can happen if a
 * monster breathes at the player and hits a wall instead.
 *
 * We assume the player is aware of some effect, and always return "true".
 */
bool project_p(struct source origin, struct loc grid, int dd, int ds, int typ)
{
	bool blind = (player->timed[TMD_BLIND] ? true : false);
	bool seen = !blind;
	int dam = damroll(dd, ds);

	/* Monster or trap name (for damage) */
	char killer[80];

	project_player_handler_f player_handler = player_handlers[typ];
	project_player_handler_context_t context = {
		origin,
		grid,
		NULL,
		dd,
		ds,
		dam,
		typ
	};

	/* No player here */
	if (!square_isplayer(cave, grid)) {
		return false;
	}

	switch (origin.what) {
		case SRC_PLAYER: {
			/* Don't affect projector */
			return false;

			break;
		}

		case SRC_MONSTER: {
			struct monster *mon = cave_monster(cave, origin.which.monster);
			context.mon = mon;

			/* Check it is visible */
			if (!monster_is_visible(mon))
				seen = false;

			/* Get the monster's real name */
			monster_desc(killer, sizeof(killer), mon, MDESC_DIED_FROM);

			break;
		}

		case SRC_TRAP: {
			struct trap *trap = origin.which.trap;

			/* Get the trap name */
			strnfmt(killer, sizeof(killer), "a %s", trap->kind->desc);

			break;
		}

		case SRC_OBJECT: {
			struct object *obj = origin.which.object;
			object_desc(killer, sizeof(killer), obj,
				ODESC_PREFIX | ODESC_BASE, player);
			break;
		}

		case SRC_CHEST_TRAP: {
			struct chest_trap *trap = origin.which.chest_trap;

			/* Get the trap name */
			strnfmt(killer, sizeof(killer), "%s", trap->msg_death);

			break;
		}

		default: {
			my_strcpy(killer, "a bug", sizeof(killer));
			break;
		}
	}

	/* Let player know what is going on */
	if (!seen) {
		msg("You are hit by %s!", projections[typ].blind_desc);
	}

	/* Adjust damage for resistance, immunity or vulnerability */
	if (typ < ELEM_MAX) {
		event_signal_combat_attack(EVENT_COMBAT_ATTACK, origin,
			source_player(), true, -1, -1, -1, -1, false);
		context.dam = adjust_dam(player, context.dd, context.ds, context.type);
	}

	/* Apply the damage */
	if (context.dam && projections[typ].damaging) {
		/* Some attacks get a chance to evade and/or protect */
		if (projections[typ].evade) {
			monster_ranged_attack(&context, killer);
		} else {
			take_hit(player, context.dam, killer);
		}
	}

	/* Handle side effects */
	if (player_handler != NULL && player->is_dead == false) {
		player_handler(&context);
	}

	/* Disturb */
	disturb(player, true);

	/* Return "Anything seen?" */
	return true;
}
