/**
 * \file player-util.c
 * \brief Player utility functions
 *
 * Copyright (c) 2011 The Angband Developers. See COPYING.
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
#include "combat.h"
#include "effects.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-move.h"
#include "mon-util.h"
#include "obj-chest.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "score.h"
#include "target.h"
#include "trap.h"
#include "tutorial.h"
#include "songs.h"

static const char *skill_names[] = {
	#define SKILL(a, b) b,
	#include "list-skills.h"
	#undef SKILL
	""
};

/**
 * Determines the shallowest a player is allowed to go.
 * As time goes on, they are forced deeper and deeper.
 */
int player_min_depth(struct player *p)
{
	int turns = 0;
	int depth = 0;

	/* Base minimum depth */
	while (turns < p->turn) {
		depth += 1;
		turns += 1000 + 50 * depth;
	}

	/* Bounds on the base */
	depth = MIN(MAX(depth, 1), z_info->dun_depth);

	/* Can't leave the throne room */
	if (p->depth == z_info->dun_depth) {
		depth = z_info->dun_depth;
	}

	/* No limits in the endgame */
	if (p->on_the_run) {
		depth = 0;
	}

	return depth;
}

/**
 * Increment to the next or decrement to the preceeding level.
 * Keep in mind to check all intermediate level for unskippable quests
*/
int dungeon_get_next_level(struct player *p, int dlev, int added)
{
	int target_level;

	/* Get target level */
	target_level = dlev + added;

	/* Don't allow levels below max */
	if (target_level > z_info->dun_depth)
		target_level = z_info->dun_depth;

	/* Don't allow levels above the town */
	if (target_level < 0) target_level = 0;

	return target_level;
}

/**
 * Change dungeon level - e.g. by going up stairs or with WoR.
 */
void dungeon_change_level(struct player *p, int dlev)
{
	/* New depth */
	p->depth = dlev;

	/* Leaving, make new level */
	p->upkeep->generate_level = true;

	/* Save the game when we arrive on the new level. */
	p->upkeep->autosave = true;
}


/**
 * Simple exponential function for integers with non-negative powers
 */
int int_exp(int base, int power)
{
	int i;
	int result = 1;

	for (i = 0; i < power; i++) {
		result *= base;
	}

	return result;
}

/**
 * Decreases players hit points and sets death flag if necessary
 *
 * Hack -- this function allows the user to save (or quit) the game
 * when he dies, since the "You die." message is shown before setting
 * the player to "dead".
 */
void take_hit(struct player *p, int dam, const char *kb_str)
{
	int old_chp = p->chp;

	int warning = (p->mhp * p->opts.hitpoint_warn / 10);

	time_t ct = time((time_t*)0);
	char long_day[40];
	char buf[120];

	/* Paranoia */
	if (p->is_dead) return;

	if (dam <= 0) return;

	/* Disturb */
	disturb(p, true);

	/* Hurt the player */
	p->chp -= dam;

	/* Display the hitpoints */
	p->upkeep->redraw |= (PR_HP);

	/* Dead player */
	if (p->chp <= 0) {
		/*
		 * Note cause of death.  Do it here so EVENT_CHEAT_DEATH
		 * handlers or things looking for the "Die? " prompt have
		 * access to it.
		 */
		if (p->timed[TMD_IMAGE]) {
			strnfmt(p->died_from, sizeof(p->died_from),
					"%s (while halluciinating)", kb_str);
		} else {
			my_strcpy(p->died_from, kb_str, sizeof(p->died_from));
		}

		if ((p->wizard || OPT(p, cheat_live)) && !get_check("Die? ")) {
			event_signal(EVENT_CHEAT_DEATH);
		} else {
			/* Hack -- Note death */
			msgt(MSG_DEATH, "You die.");
			event_signal(EVENT_MESSAGE_FLUSH);
			event_signal(EVENT_DEATH);

			/* Note death */
			p->is_dead = true;

			/* Killed by */
			strnfmt(buf, sizeof(buf), "Slain by %s.", p->died_from);
			history_add(p, buf, HIST_PLAYER_DEATH);

			/* Get time */
			(void)strftime(long_day, 40, "%d %B %Y", localtime(&ct));
			strnfmt(buf, sizeof(buf), "Died on %s.", long_day);
			history_add(p, buf, HIST_PLAYER_DEATH);

			/* Dead */
			return;
		}
	}

	/* Hitpoint warning */
	if (p->chp < warning) {
		/* Hack -- bell on first notice */
		if (old_chp > warning)
			bell();

		/* Message */
		msgt(MSG_HITPOINT_WARN, "*** LOW HITPOINT WARNING! ***");
		event_signal(EVENT_MESSAGE_FLUSH);
	}

	/* Cancel entrancement */
	player_set_timed(p, TMD_ENTRANCED, 0, false, true);
}

/**
 * Win or not, know inventory and history upon death, enter score
 */
void death_knowledge(struct player *p)
{
	struct object *obj;
	time_t death_time = (time_t)0;

	player_learn_all_runes(p);
	for (obj = p->gear; obj; obj = obj->next) {
		object_flavor_aware(p, obj);
	}

	history_unmask_unknown(p);

	/* Get time of death */
	(void)time(&death_time);
	enter_score(p, &death_time);

	/* Hack -- Recalculate bonuses */
	p->upkeep->update |= (PU_BONUS);
	handle_stuff(p);
}

/**
 * Regenerate one turn's worth of hit points
 */
void player_regen_hp(struct player *p)
{
	int old_chp = p->chp;
	int regen_multiplier = p->state.flags[OF_REGEN] + 1;
	int regen_period = z_info->player_regen_period;
	struct song *este = lookup_song("Este");

	/* Various things interfere with physical healing */
	if (p->timed[TMD_FOOD] < PY_FOOD_STARVE) return;
	if (p->timed[TMD_POISONED]) return;
	if (p->timed[TMD_CUT]) return;

	/* Various things speed up regeneration */
	if (player_is_singing(p, este)) {
		regen_multiplier *= song_bonus(p, p->state.skill_use[SKILL_SONG], este);
	}

	/* Complete healing every z_info->player_regen_period player turns, modified */
	if (regen_multiplier > 0) {
		regen_period /= regen_multiplier;
	} else {
		return;
	}

	/* Work out how much increase is due */
	p->chp += regen_amount(p->turn, p->mhp, regen_period);
	p->chp = MIN(p->chp, p->mhp);

	/* Notice changes */
	if (old_chp != p->chp) {
		equip_learn_flag(p, OF_REGEN);
		p->upkeep->redraw |= (PR_HP);
	}
}


/**
 * Regenerate one turn's worth of voice
 */
void player_regen_mana(struct player *p)
{
	int old_csp = p->csp;
	int regen_multiplier = p->state.flags[OF_REGEN] + 1;
	int regen_period = z_info->player_regen_period;

	/* Don't regenerate voice if singing */
	if (p->song[SONG_MAIN]) return;

	/* Complete healing every z_info->player_regen_period player turns,
	 * modified */
	if (regen_multiplier > 0) {
		regen_period /= regen_multiplier;
	} else {
		return;
	}

	/* Work out how much increase is due */
	p->csp += regen_amount(p->turn, p->msp, regen_period);
	p->csp = MIN(p->csp, p->msp);

	/* Notice changes */
	if (old_csp != p->csp) {
		equip_learn_flag(p, OF_REGEN);
		p->upkeep->redraw |= (PR_MANA);
	}
}


/**
 * Digest food.
 *
 * Speed and regeneration are taken into account already in the hunger rate.
 */
void player_digest(struct player *p)
{
	/* Basic digestion rate */
	int i = 1;

	/* Slow hunger rates are done statistically */
	if (p->state.hunger < 0) {
		if (!one_in_(int_exp(3, -(p->state.hunger)))) {
			i = 0;
		}
	} else if (p->state.hunger > 0) {
		i *= int_exp(3, p->state.hunger);
	}

	/* Digest quickly when gorged */
	if (p->timed[TMD_FOOD] >= PY_FOOD_MAX) i *= 9;

	/* Digest some food */
	(void)player_dec_timed(p, TMD_FOOD, i, false, true);

	/* Starve to death (slowly) */
	if (p->timed[TMD_FOOD] < PY_FOOD_STARVE) {
		/* Take damage */
		take_hit(p, 1, "starvation");
	}
}

/**
 * Update the player's light fuel
 */
void player_update_light(struct player *p)
{
	/* Check for light being wielded */
	struct object *obj = equipped_item_by_slot_name(p, "light");

	/* Burn some fuel in the current light */
	if (obj && tval_is_light(obj)) {
		bool burn_fuel = true;

		/* If the light has the NO_FUEL flag, well... */
		if (of_has(obj->flags, OF_NO_FUEL))
		    burn_fuel = false;

		/* Use some fuel (except on artifacts, or during the day) */
		if (burn_fuel && obj->timeout > 0) {
			/* Decrease life-span */
			obj->timeout--;

			/* Hack -- notice interesting fuel steps */
			if ((obj->timeout < 100) || (!(obj->timeout % 100)))
				/* Redraw stuff */
				p->upkeep->redraw |= (PR_EQUIP);

			/* Hack -- Special treatment when blind */
			if (p->timed[TMD_BLIND]) {
				/* Hack -- save some light for later */
				if (obj->timeout == 0) obj->timeout++;
			} else if (obj->timeout == 0) {
				/* The light is now out */
				disturb(p, false);
				msg("Your light has gone out!");
			} else if ((obj->timeout <= 100) && (!(obj->timeout % 20))) {
				/* The light is getting dim */
				if (obj->timeout == 100) {
					/* Only disturb the first time */
					disturb(p, false);
				}
				msg("Your light is growing faint.");
			}
		}
	}

	/* Calculate torch radius */
	p->upkeep->update |= (PU_TORCH);
}

/**
 * Check the player for boots of radiance
 */
bool player_radiates(struct player *p)
{
	struct object *boots = equipped_item_by_slot_name(p, "feet");
	if (boots && of_has(boots->flags, OF_RADIANCE) &&
		!square_isglow(cave, p->grid)) {
		if (!of_has(boots->known->flags, OF_RADIANCE) && one_in_(10)) {
			char short_name[80];
			char full_name[80];
			object_desc(short_name, sizeof(short_name), boots, ODESC_BASE, p);
			player_learn_flag(p, OF_RADIANCE);
			object_desc(full_name, sizeof(full_name), boots, ODESC_FULL, p);
			msg("Your footsteps leave a trail of light!");
			msg("You recognize your %s to be %s", short_name, full_name);
		}
		return true;
	}
	return false;
}

/**
 * Player falls in a pit, maybe spiked
 */
void player_fall_in_pit(struct player *p, bool spiked)
{
	/* Falling damage */
	int dam = damroll(2, 4);
	const char *prefix = square_apparent_look_prefix(cave, p->grid);
	char name[50];
	square_apparent_name(cave, p->grid, name, sizeof(name));

	msg("You fall into %s%s!", prefix, name);

	/* Update combat rolls */
	event_signal_combat_attack(EVENT_COMBAT_ATTACK, source_grid(p->grid),
							   source_player(), true, -1, -1, -1, -1, false);
	event_signal_combat_damage(EVENT_COMBAT_DAMAGE, 2, 4, dam, -1, -1, 0, 0,
							   PROJ_HURT, false);

	/* Take the damage */
	take_hit(p, dam, name);

	/* Make some noise */
	p->stealth_score -= 5;

	/* Deal with spikes */
	if (spiked) {
		int prt, net_dam;

		/* Extra spike damage */
		dam = damroll(4, 5);

		/* Protection */
		prt = protection_roll(p, PROJ_HURT, true, RANDOMISE);
		net_dam = (dam - prt > 0) ? (dam - prt) : 0;

		/* Update combat rolls */
		event_signal_combat_attack(EVENT_COMBAT_ATTACK, source_grid(p->grid),
								   source_player(), true, -1, -1, -1, -1,
								   false);
		event_signal_combat_damage(EVENT_COMBAT_DAMAGE, 4, 5, dam, -1, -1, prt,
								   100, PROJ_HURT, true);

		if (net_dam > 0) {
			msg("You are impaled!");

			/* Take the damage */
			take_hit(p, net_dam, name);
				
			(void)player_inc_timed(p, TMD_CUT,
				p->timed[TMD_CUT] + (net_dam + 1) / 2,
				true, true, false);
		} else {				
			msg("Your armour protects you.");
		}

		/* Make some more noise */
		p->stealth_score -= 5;
	}
}

/**
 * Player takes damage from falling
 */
void player_falling_damage(struct player *p, bool stun)
{
	int dice = 3, dam;
	const char *message;

	if (square_ischasm(cave, p->grid)) {
		if (p->depth != z_info->dun_depth - 2) {
			/* Fall two floors if there's room */
			dice = 6;
		}
        message = "falling down a chasm";
	} else if (square_isstairs(cave, p->grid) || square_isshaft(cave, p->grid)){
        message = "a collapsing stair";
	} else {		
        message = "a collapsing floor";
    }

	/* Calculate the damage */
	dam = damroll(dice, 4);

	/* Update combat rolls */
	event_signal_combat_attack(EVENT_COMBAT_ATTACK, source_grid(p->grid),
							   source_player(), true, -1, -1, -1, -1, false);
	event_signal_combat_damage(EVENT_COMBAT_DAMAGE, dice, 4, dam, -1, -1, 0, 0,
							   PROJ_HURT, false);

	/* Take the damage */
	take_hit(p, dam, message);

	if (stun) { 
		(void)player_inc_timed(p, TMD_STUN, dam * 5, true, true, true);
	}

	/* Reset staircasiness */
	p->staircasiness = 0;
}

/**
 * Player falls in a chasm
 */
void player_fall_in_chasm(struct player *p)
{
	/* Special handling for the tutorial */
	if (in_tutorial()) {
		tutorial_leave_section(p);
		return;
	}

	/* Several messages so the player has a chance to see it happen */
	msg("You fall into the darkness!");
	event_signal(EVENT_MESSAGE_FLUSH);
	msg("...and land somewhere deeper in the Iron Hells.");
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Add to the history */
	history_add(p, "Fell into a chasm", HIST_FELL_IN_CHASM);

	/* Take some damage */
	player_falling_damage(p, false);

	/* New level */
	dungeon_change_level(p, MIN(p->depth + 2, z_info->dun_depth - 1));
}

/**
 * Does any flanking or controlled retreat attack necessary when player moves
 */
void player_flanking_or_retreat(struct player *p, struct loc grid)
{
	int d, start;
	struct monster *mon = target_get_monster();
	bool flanking = player_active_ability(p, "Flanking");
	bool controlled_retreat = false;

	/* No attack if player is confused or afraid, or if the truce is in force */
	if (p->timed[TMD_CONFUSED] || p->timed[TMD_AFRAID] || p->truce) return;
	
	/* Need to have the ability, and to have not moved last round */
	if (player_active_ability(p, "Controlled Retreat") && 
	    ((p->previous_action[1] > 9) || (p->previous_action[1] == 5))) {
		controlled_retreat = true;
	}

	/* Player needs one of the abilities */
	if (!(flanking || controlled_retreat)) return;

	/* First see if the targetted monster is eligible and attack it if so */
	if (mon) {
		/* Base conditions for an attack */
		if (monster_is_visible(mon) && (!OPT(p, forgo_attacking_unwary) ||
										(mon->alertness >= ALERTNESS_ALERT))) {
			/* Try a flanking attack */
			if (flanking && (distance(p->grid, mon->grid) == 1) &&
				(distance(grid, mon->grid) == 1)) {
				py_attack(p, mon->grid, ATT_FLANKING);
				return;
			}
			/* Try a controlled retreat attack */
			if (controlled_retreat && (distance(p->grid, mon->grid) == 1) &&
				(distance(grid, mon->grid) > 1)) {
				py_attack(p, mon->grid, ATT_CONTROLLED_RETREAT);
				return;
			}
		}
	}

	/* Otherwise look through eligible monsters and choose one randomly */
	start = randint0(8);

	/* Look for adjacent monsters */
	for (d = start; d < 8 + start; d++) {
		struct loc check = loc_sum(p->grid, ddgrid_ddd[d % 8]);

		/* Check Bounds */
		if (!square_in_bounds(cave, check)) continue;

		/* Check for a monster, and player readiness */
		mon = square_monster(cave, check);
		if (mon) {
			/* Base conditions for an attack */
			if (monster_is_visible(mon) &&
				(!OPT(p, forgo_attacking_unwary) ||
				 (mon->alertness >= ALERTNESS_ALERT))) {
				/* Try a flanking attack */
				if (flanking && (distance(p->grid, mon->grid) == 1) &&
					(distance(grid, mon->grid) == 1)) {
					py_attack(p, mon->grid, ATT_FLANKING);
					return;
				}
				/* Try a controlled retreat attack */
				if (controlled_retreat && (distance(p->grid, mon->grid) == 1) &&
					(distance(grid, mon->grid) > 1)) {
					py_attack(p, mon->grid, ATT_CONTROLLED_RETREAT);
					return;
				}
			}
		}
	}
}

/**
 * Does any opportunist or zone of control attack necessary when player moves
 *
 * Note the use of skip_next_turn to stop the player getting opportunist
 * attacks afer knocking back
 */
void player_opportunist_or_zone(struct player *p, struct loc grid1,
								struct loc grid2, bool opp_only)
{
	bool opp = player_active_ability(p, "Opportunist");
	bool zone = player_active_ability(p, "Zone of Control") && !opp_only;

	/* Monster */
	char m_name[80];
	struct monster *mon = square_monster(cave, grid1);

	/* Handle Opportunist and Zone of Control */
	if ((opp || zone) && monster_is_visible(mon) &&	!mon->skip_next_turn &&
		!p->truce && !p->timed[TMD_CONFUSED] &&	!p->timed[TMD_AFRAID] &&
		!p->timed[TMD_ENTRANCED] &&	(p->timed[TMD_STUN] < 100) &&
		(distance(grid1, p->grid) == 1) &&
		(!OPT(p, forgo_attacking_unwary) ||
		 (mon->alertness >= ALERTNESS_ALERT))) {
		monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);

		/* Zone of control */
		if (zone && (distance(grid2, p->grid) == 1)) {
			msg("%s moves through your zone of control.", m_name);
			py_attack_real(p, grid1, ATT_ZONE_OF_CONTROL);
		}

		/* Opportunist */
		if (opp && (distance(grid2, p->grid) > 1)) {
			msg("%s moves away from you.", m_name);
			py_attack_real(p, grid1, ATT_OPPORTUNIST);
		}
	}
}

/**
 * Does any polearm attack when a monster moves close to the player
 */
void player_polearm_passive_attack(struct player *p, struct loc grid_from,
								struct loc grid_to)
{
	char m_name[80];
	struct monster *mon = square_monster(cave, grid_to);
	if (mon && monster_is_visible(mon)) {
		struct object *obj = equipped_item_by_slot_name(p, "weapon");

		if (!OPT(p, forgo_attacking_unwary) ||
			(mon->alertness >= ALERTNESS_ALERT)) {
			if ((distance(grid_from, p->grid) > 1) &&
				(distance(grid_to, p->grid) == 1) &&
				!p->truce && !p->timed[TMD_CONFUSED] &&
				!p->timed[TMD_AFRAID] && of_has(obj->flags, OF_POLEARM)
				&& p->focused) {
				char o_name[80];

				/* Get the basic name of the object */
				object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, p);

				monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);

				msg("%s comes into reach of your %s.", m_name, o_name);
				py_attack_real(p, grid_to, ATT_POLEARM);
			}
		}
	}
}

/**
 * Player is able to start a leap
 */
bool player_can_leap(struct player *p, struct loc grid, int dir)
{
	int i, d;
	bool run_up = false;
	struct loc mid, end;

	if (p->timed[TMD_CONFUSED]) return false;
	if (!square_isleapable(cave, grid)) return false;
	if (!player_active_ability(p, "Leaping")) return false;

	/* Test all three directions roughly towards the chasm/pit */
	for (i = -1; i <= 1; i++) {
		d = cycle[chome[dir_from_delta(grid.y - p->grid.y,
									   grid.x - p->grid.x)] + i];

		/* If the last action was a move in this direction, run_up is valid */
		if (p->previous_action[1] == d) run_up = true;
	}

	/* Get location */
	mid = loc_sum(p->grid, ddgrid[dir]);
	end = loc_sum(mid, ddgrid[dir]);

	/* Disturb the player */
	disturb(p, false);

	/* Flush messages */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Test legality */
	if (square_ispit(cave, p->grid)) {
		/* Can't jump from within pits */
		msg("You cannot leap from within a pit.");
		return false;
	} else if (square_iswebbed(cave, p->grid)) {
		/* Can't jump from within webs */
		msg("You cannot leap from within a web.");
		return false;
	} else if (!run_up) {
		/* Can't jump without a run up */
		msg("You cannot leap without a run up.");
		return false;
	} else if (square_isknown(cave, end) && !square_ispassable(cave, end)) {
		/* Need room to land */
		msg("You cannot leap over as there is no room to land.");
		return false;
	}

	return true;
}

/**
 * Attempts to break free of a web.
 */
bool player_break_web(struct player *p)
{
	int difficulty = 7; //NRM - better related to the "trap"

	/* Capped so you always have some chance */
	int score = MAX(p->state.stat_use[STAT_STR] * 2, difficulty - 8);

	/* Disturb the player */
	disturb(p, false);

	/* Free action helps a lot */
	difficulty -= 10 * p->state.flags[OF_FREE_ACT];

	/* Spider bane bonus helps */
	difficulty -= player_spider_bane_bonus(p);

	if (skill_check(source_player(), score, difficulty, source_none()) <= 0) {	
		msg("You fail to break free of the web.");

		/* Take a full turn */
		p->upkeep->energy_use = z_info->move_energy;

		/* Store the action type */
		p->previous_action[0] = ACTION_MISC;

		return false;
	} else {
		msg("You break free!");
		square_destroy_trap(cave, p->grid);

		return true;
	}
}


/**
 * Attempts to climb out of a pit.
 */
bool player_escape_pit(struct player *p)
{
	/* Disturb the player */
	disturb(p, false);

	if (check_hit(square_pit_difficulty(cave, p->grid), false,
				  source_grid(p->grid))) {
		msg("You try to climb out of the pit, but fail.");

		/* Take a full turn */
		p->upkeep->energy_use = z_info->move_energy;

		/* Store the action type */
		p->previous_action[0] = ACTION_MISC;

		return false;
	}

	msg("You climb out of the pit.");
	return true;
}

/**
 * Aim a horn of blasting at the ceiling
 */
void player_blast_ceiling(struct player *p)
{
	int will = p->state.skill_use[SKILL_WILL];
	if (player_active_ability(p, "Channeling")) {
		will += 5;
	}

	/* Skill check of Will vs 10 */
	if (skill_check(source_player(), will, 10, source_none()) > 0) {
		int dam = damroll(4, 8);
		int prt = protection_roll(p, PROJ_HURT, false, RANDOMISE);
		int net_dam = MAX(0, dam - prt);

		msg("The ceiling cracks and rock rains down upon you!");
		effect_simple(EF_EARTHQUAKE, source_player(), "0", 0, 3, 0, NULL);

		/* Update combat rolls */
		event_signal_combat_attack(EVENT_COMBAT_ATTACK, source_player(),
								   source_player(), true, -1, -1, -1, -1,
								   false);
		event_signal_combat_damage(EVENT_COMBAT_DAMAGE, 4, 8, dam, -1, -1, prt,
								   100, PROJ_HURT, false);

		take_hit(p, net_dam, "a collapsing ceiling");
		(void)player_inc_timed(p, TMD_STUN, dam * 4, true, true, true);
	} else {
		msg("The blast hits the ceiling, but you did not blow hard enough to bring it down.");
	}
}

/**
 * Aim a horn of blasting at the floor
 */
void player_blast_floor(struct player *p)
{
	int will = p->state.skill_use[SKILL_WILL];
	if (player_active_ability(p, "Channeling")) {
		will += 5;
	}

	/* Skill check of Will vs 10 */
	if (skill_check(source_player(), will, 10, source_none()) > 0) {
		if (p->depth < z_info->dun_depth - 1 && !in_tutorial()) {
			msg("The floor crumbles beneath you!");
			event_signal(EVENT_MESSAGE_FLUSH);
			msg("You fall through...");
			event_signal(EVENT_MESSAGE_FLUSH);
			msg("...and land somewhere deeper in the Iron Hells.");
			event_signal(EVENT_MESSAGE_FLUSH);

			/* Add to the history */
			history_add(p, "Fell through the floor with a horn blast.",
						HIST_FELL_DOWN_LEVEL);

			/* Take some damage */
			player_falling_damage(p, true);

			event_signal(EVENT_MESSAGE_FLUSH);

			/* Change level */
			dungeon_change_level(p, p->depth + 1);
		} else {
			msg("Cracks spread across the floor, but it holds firm.");
		}
	} else {
		msg("The blast hits the floor, but you did not blow hard enough to collapse it.");
	}
}


/**
 * Find a skill given its name
 */
int lookup_skill(const char *name)
{
	int i;
	for (i = 0; i < SKILL_MAX; i++) {
		if (streq(skill_names[i], name)) {
			return i;
		}
	}
	msg("Could not find %s skill!", name);
	return -1;
}

/**
 * Check if the player moved n moves ago
 */
bool player_action_is_movement(struct player *p, int n)
{
	return ((p->previous_action[n] != ACTION_NOTHING) &&
			(p->previous_action[n] != ACTION_MISC) &&
			(p->previous_action[n] != ACTION_STAND)); 
}

/**
 * Determines the size of the player evasion bonus due to dodging (if any)
 */
int player_dodging_bonus(struct player *p)
{
	if (player_active_ability(p, "Dodging") && player_action_is_movement(p, 0)){
		return 3;
	} else {
		return 0;
	}
}


/**
 * Player can riposte
 */
bool player_can_riposte(struct player *p, int hit_result)
{
	struct object *weapon = equipped_item_by_slot_name(p, "weapon");

	return (weapon && player_active_ability(p, "Riposte") &&
			!p->upkeep->riposte &&
			!p->timed[TMD_AFRAID] &&
			!p->timed[TMD_CONFUSED] &&
			!p->timed[TMD_ENTRANCED] &&
			(p->timed[TMD_STUN] <= 100) &&
			(hit_result <= -10 - ((weapon->weight + 9) / 10)));
}

/**
 * Check if the player is sprinting
 */
bool player_is_sprinting(struct player *p)
{
	int i;
	int turns = 1;

	if (player_active_ability(p, "Sprinting")) {
		for (i = 1; i < 4; i++) {
			if (player_action_is_movement(p, i) &&
				player_action_is_movement(p, i + 1)) {
				if (p->previous_action[i] == p->previous_action[i + 1]) {
					/* Moving in the same direction */
					turns++;
				} else if (p->previous_action[i] ==
						   cycle[chome[p->previous_action[i + 1]] - 1]) {
					turns++;
				} else if (p->previous_action[i] ==
						   cycle[chome[p->previous_action[i + 1]] + 1]) {
					turns++;
				}
			}
		}
	}

	return (turns >= 4);
}

static const int bane_flag[] = {
	#define BANE(a, b) RF_##a,
	#include "list-bane-types.h"
	#undef BANE
};

int player_bane_type_killed(int bane_type)
{
	int j, k;

	if (bane_type < 0 || bane_type >= (int)N_ELEMENTS(bane_flag)) {
		return 0;
	}

	/* Scan the monster races */
	for (j = 1, k = 0; j < z_info->r_max; j++) {
		struct monster_race *race = &r_info[j];
		struct monster_lore *lore = get_lore(race);

		if (rf_has(race->flags, bane_flag[bane_type])) {
			k += lore->pkills;
		}
	}

	return k;
}

int calc_bane_bonus(struct player *p)
{
	int i = 2;
	int bonus = 0;
	int killed = player_bane_type_killed(p->bane_type);
	while (i <= killed) {
		i *= 2;
		bonus++;
	}
	return bonus;
}

int player_bane_bonus(struct player *p, struct monster *mon)
{
	int bonus = 0;

	/* Paranoia */
	if (!mon) return 0;

	/* Entranced players don't get the bonus */
	if (p->timed[TMD_ENTRANCED]) return 0;

	/* Knocked out players don't get the bonus */
	if (player_timed_grade_eq(p, TMD_STUN, "Knocked Out")) return 0;

	/* Calculate the bonus */
	if (rf_has(mon->race->flags, bane_flag[p->bane_type])) {
		bonus = calc_bane_bonus(p);
	}

	return bonus;
}

int player_spider_bane_bonus(struct player *p)
{
	return (bane_flag[p->bane_type] == RF_SPIDER) ? calc_bane_bonus(p) : 0;
}

/**
 * Return true if the player can fire something with a launcher.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_fire(struct player *p, bool show_msg)
{
	struct object *obj = equipped_item_by_slot_name(p, "shooting");

	/* Require a usable launcher */
	if (!obj || !p->state.ammo_tval) {
		if (show_msg)
			msg("You have nothing to fire with.");
		return false;
	}

	return true;
}

/**
 * Return true if the player can fire from the first quiver.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_fire_quiver1(struct player *p, bool show_msg)
{
	const struct object *ammo;

	if (!player_can_fire(p, show_msg)) {
		return false;
	}
	ammo = equipped_item_by_slot_name(p, "first quiver");
	if (!ammo) {
		if (show_msg) {
			msg("You have nothing in the first quiver to fire.");
		}
		return false;
	}
	if (ammo->tval != p->state.ammo_tval) {
		if (show_msg) {
			msg("The ammunition in the first quiver is not compatible with your launcher.");
		}
		return false;
	}
	return true;
}

/**
 * Return true if the player can fire from the second quiver.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_fire_quiver2(struct player *p, bool show_msg)
{
	const struct object *ammo;

	if (!player_can_fire(p, show_msg)) {
		return false;
	}
	ammo = equipped_item_by_slot_name(p, "second quiver");
	if (!ammo) {
		if (show_msg) {
			msg("You have nothing in the second quiver to fire.");
		}
		return false;
	}
	if (ammo->tval != p->state.ammo_tval) {
		if (show_msg) {
			msg("The ammunition in the second quiver is not compatible with your launcher.");
		}
		return false;
	}
	return true;
}

/**
 * Return true if the player can refuel their light source.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_refuel(struct player *p, bool show_msg)
{
	struct object *obj = equipped_item_by_slot_name(p, "light");

	if (!obj && show_msg) {
		msg("You are not wielding a light");
	}

	if (of_has(obj->flags, OF_TAKES_FUEL) || of_has(obj->flags, OF_BURNS_OUT)) {
		return true;
	}

	if (show_msg) {
		msg("Your light cannot be refuelled.");
	}

	return false;
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_fire_prereq(void)
{
	return player_can_fire(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_fire_quiver1_prereq(void)
{
	return player_can_fire_quiver1(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_fire_quiver2_prereq(void)
{
	return player_can_fire_quiver2(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_refuel_prereq(void)
{
	return player_can_refuel(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_debug_prereq(void)
{
	if (player->noscore & NOSCORE_DEBUG) {
		return true;
	}
	if (confirm_debug()) {
		/* Mark savefile */
		player->noscore |= NOSCORE_DEBUG;
		return true;
	}
	return false;
}


/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * its use in ui-game.c.
 */
bool player_can_save_prereq(void)
{
	return !in_tutorial();
}


/**
 * Apply confusion, if needed, to a direction
 *
 * Display a message and return true if direction changes.
 */
bool player_confuse_dir(struct player *p, int *dp, bool too)
{
	int dir = *dp;

	if (p->timed[TMD_CONFUSED]) {
		if ((dir == 5) || (randint0(100) < 75)) {
			/* Random direction */
			dir = ddd[randint0(8)];
		}

		/* Running attempts always fail */
		if (too) {
			msg("You are too confused.");
			return true;
		}

		if (*dp != dir) {
			msg("You are confused.");
			*dp = dir;
			return true;
		}
	}

	return false;
}

/**
 * Return true if the provided count is one of the conditional REST_ flags.
 */
bool player_resting_is_special(int16_t count)
{
	switch (count) {
		case REST_COMPLETE:
		case REST_ALL_POINTS:
		case REST_SOME_POINTS:
			return true;
	}

	return false;
}

/**
 * Return true if the player is resting.
 */
bool player_is_resting(const struct player *p)
{
	return (p->upkeep->resting > 0 ||
			player_resting_is_special(p->upkeep->resting));
}

/**
 * Return the remaining number of resting turns.
 */
int16_t player_resting_count(const struct player *p)
{
	return p->upkeep->resting;
}

/**
 * In order to prevent the regeneration bonus from the first few turns, we have
 * to store the number of turns the player has rested. Otherwise, the first
 * few turns will have the bonus and the last few will not.
 */
static int player_turns_rested = 0;
static bool player_rest_disturb = false;

/**
 * Set the number of resting turns.
 *
 * \param p is the player trying to rest
 * \param count is the number of turns to rest or one of the REST_ constants.
 */
void player_resting_set_count(struct player *p, int16_t count)
{
	/* Cancel if player is disturbed */
	if (player_rest_disturb) {
		p->upkeep->resting = 0;
		player_rest_disturb = false;
		return;
	}

	/* Ignore if the rest count is negative. */
	if ((count < 0) && !player_resting_is_special(count)) {
		p->upkeep->resting = 0;
		return;
	}

	/* Save the rest code */
	p->upkeep->resting = count;

	/* Truncate overlarge values */
	if (p->upkeep->resting > 9999) p->upkeep->resting = 9999;
}

/**
 * Cancel current rest.
 */
void player_resting_cancel(struct player *p, bool disturb)
{
	player_resting_set_count(p, 0);
	player_turns_rested = 0;
	player_rest_disturb = disturb;
}

/**
 * Return true if the player should get a regeneration bonus for the current
 * rest.
 */
bool player_resting_can_regenerate(const struct player *p)
{
	return player_turns_rested >= REST_REQUIRED_FOR_REGEN ||
		player_resting_is_special(p->upkeep->resting);
}

/**
 * Perform one turn of resting. This only handles the bookkeeping of resting
 * itself, and does not calculate any possible other effects of resting (see
 * process_world() for regeneration).
 */
void player_resting_step_turn(struct player *p)
{
	/* Timed rest */
	if (p->upkeep->resting > 0) {
		/* Reduce rest count */
		p->upkeep->resting--;

		/* Redraw the state */
		p->upkeep->redraw |= (PR_STATE);
	}

	/* Take a turn */
	p->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	p->previous_action[0] = ACTION_STAND;

	/* Store the 'focus' attribute */
	p->focused = true;

	/* Searching */ 
	search(p);

	/* Increment the resting counters */
	p->resting_turn++;
	player_turns_rested++;
}

/**
 * Handle the conditions for conditional resting (resting with the REST_
 * constants).
 */
void player_resting_complete_special(struct player *p)
{
	/* Complete resting */
	if (!player_resting_is_special(p->upkeep->resting)) return;

	if (p->upkeep->resting == REST_ALL_POINTS) {
		if ((p->chp == p->mhp) && (p->csp == p->msp))
			/* Stop resting */
			disturb(p, false);
	} else if (p->upkeep->resting == REST_COMPLETE) {
		if ((p->chp == p->mhp) &&
			(p->csp == p->msp || !player_is_singing(p, NULL)) &&
			!p->timed[TMD_BLIND] && !p->timed[TMD_CONFUSED] &&
			!p->timed[TMD_POISONED] && !p->timed[TMD_AFRAID] &&
			!p->timed[TMD_STUN] &&
			!p->timed[TMD_CUT] && !p->timed[TMD_SLOW] &&
			!p->timed[TMD_ENTRANCED] && !p->timed[TMD_IMAGE])
			/* Stop resting */
			disturb(p, false);
	} else if (p->upkeep->resting == REST_SOME_POINTS) {
		if ((p->chp == p->mhp) || (p->csp == p->msp))
			/* Stop resting */
			disturb(p, false);
	}
}

/* Record the player's last rest count for repeating */
static int player_resting_repeat_count = 0;

/**
 * Get the number of resting turns to repeat.
 *
 * \param p The current player.
 */
int player_get_resting_repeat_count(struct player *p)
{
	return player_resting_repeat_count;
}

/**
 * Set the number of resting turns to repeat.
 *
 * \param p is the player trying to rest
 * \param count is the number of turns requested for rest most recently.
 */
void player_set_resting_repeat_count(struct player *p, int16_t count)
{
	player_resting_repeat_count = count;
}

/**
 * Check if the player resists (or better) an element
 */
bool player_resists(const struct player *p, int element)
{
	return (p->state.el_info[element].res_level > 0);
}

/**
 * Places the player at the given coordinates in the cave.
 */
void player_place(struct chunk *c, struct player *p, struct loc grid)
{
	assert(!square_monster(c, grid));

	/* Save player location */
	p->grid = grid;

	/* Mark cave grid */
	square_set_mon(c, grid, -1);
}

/**
 * Take care of bookkeeping after moving the player with monster_swap().
 *
 * \param p is the player that was moved.
 * \param eval_trap will, if true, cause evaluation (possibly affecting the
 * player) of the traps in the grid.
 * \param is_involuntary will, if true, do appropriate actions (flush the
 * command queue) for a move not expected by the player.
 */
void player_handle_post_move(struct player *p, bool eval_trap,
							 bool is_involuntary)
{
	/* Flush command queue for involuntary moves */
	if (is_involuntary) {
		cmdq_flush();
	}

	/* Notice objects */
	square_know_pile(cave, p->grid);

	/* Discover stairs if blind */
	if (square_isstairs(cave, p->grid)) {
		square_memorize(cave, p->grid);
		square_light_spot(cave, p->grid);
	}

	/* Remark on Forge and discover it if blind */
	if (square_isforge(cave, p->grid)) {
		struct feature *feat = square_feat(cave, p->grid);
		if ((feat->fidx == FEAT_FORGE_UNIQUE) && !p->unique_forge_seen) {
			msg("You enter the forge 'Orodruth' - the Mountain's Anger - where Grond was made in days of old.");
			msg("The fires burn still.");
			p->unique_forge_seen = true;
			history_add(p, "Entered the forge 'Orodruth'", HIST_FORGE_FOUND);
		} else {
			const char *article = square_apparent_look_prefix(cave, p->grid);
			char name[50];
			square_apparent_name(cave, p->grid, name, sizeof(name));
			msg("You enter %s%s.", article, name);
		}
		square_memorize(cave, p->grid);
		square_light_spot(cave, p->grid);
	}

	/* Discover invisible traps, set off visible ones */
	if (eval_trap && square_isplayertrap(cave, p->grid)) {
		disturb(p, false);
		square_reveal_trap(cave, p->grid, true);
		hit_trap(p->grid);
	} else if (square_ischasm(cave, p->grid)) {
		player_fall_in_chasm(p);
	}

	/* Check for having left the level by falling */
	if (!p->upkeep->generate_level) {
		/* Update view */
		update_view(cave, p);
		cmdq_push(CMD_AUTOPICKUP);
		/*
		 * The autopickup is a side effect of the move:  whatever
		 * command triggered the move will be the target for CMD_REPEAT
		 * rather than repeating the autopickup.
		 */
		cmdq_peek()->is_background_command = true;
	}
}

/**
 * Something has happened to disturb the player.
 *
 * All disturbance cancels repeated commands, resting, and running.
 * Stealth mode is canceled if the second argument is true.
 *
 * XXX-AS: Make callers either pass in a command
 * or call cmd_cancel_repeat inside the function calling this
 */
void disturb(struct player *p, bool stop_stealth)
{
	int repeats = cmd_get_nrepeats();

	/* Cancel repeated commands */
	cmd_cancel_repeat();

	/* Cancel Resting */
	if (player_is_resting(p)) {
		player_resting_cancel(p, true);
		p->upkeep->redraw |= PR_STATE;
	}

	/* Cancel Smithing */
	if (p->upkeep->smithing) {
		/* Cancel */
		p->upkeep->smithing = false;

		/* Store the number of smithing turns left */
		p->smithing_leftover = repeats;

		/* Display a message */
		msg("Your work is interrupted!");

		/* Redraw the state (later) */
		p->upkeep->redraw |= (PR_STATE);
	}

	/* Cancel running */
	if (p->upkeep->running) {
		p->upkeep->running = 0;

		/* Cancel queued commands */
		cmdq_flush();

		/* Check for new panel if appropriate */
		event_signal(EVENT_PLAYERMOVED);
		p->upkeep->update |= PU_TORCH;

		/* Mark the whole map to be redrawn */
		event_signal_point(EVENT_MAP, -1, -1);
	}

	/* Cancel stealth if requested */
	if (stop_stealth && p->stealth_mode) {
		/* Signal that it will be stopped at the end of the turn */
		p->stealth_mode = STEALTH_MODE_STOPPING;
	}

	/* Flush input */
	event_signal(EVENT_INPUT_FLUSH);
}

/**
 * Search a single square for hidden things 
 * (a utility function called by 'search' and 'perceive')
 */
static void search_square(struct player *p, struct loc grid, int dist,
						  int searching)
{
	int score = 0;
	int difficulty = 0;
	struct object *obj = chest_check(p, grid, CHEST_TRAPPED);
	int chest_level = obj && is_trapped_chest(obj) ? obj->pval : 0;

	/* If searching, discover unknown adjacent squares of interest */
	if (searching) {
		if ((dist == 1) && !square_isknown(cave, grid)) {
			struct object *square_obj = square_object(cave, grid);

			/* Remember all non-floor non-trap squares */
			if (!(square_isfloor(cave, grid) ||
					square_issecrettrap(cave, grid))) {
				square_memorize(cave, grid);
			}
			
			/* Mark an object, but not the square it is in */
			if (square_obj) {
				square_know_pile(cave, grid);
			}

			square_light_spot(cave, grid);
		}
	}

	/* If there is anything to notice... */
	if (obj || square_issecrettrap(cave, grid) ||
		square_issecretdoor(cave, grid)) {
		/* Give up if the square is unseen and not adjacent */
		if ((dist > 1) && !square_isseen(cave, grid)) return;

		/* Determine the base score */
		score = p->state.skill_use[SKILL_PERCEPTION];

		/* If using the search command give a score bonus */
		if (searching) score += 5;

		/* Eye for Detail ability */
		if (player_active_ability(p, "Eye for Detail")) score += 5;

		/* Determine the base difficulty */
		if (obj) {
			difficulty = chest_level / 2;
		} else {
			if (p->depth > 0) {
				difficulty = p->depth / 2;
			} else {
				difficulty = 10;
			}
		}

		/* Penalise distance */
		if (dist < 1) {
			/* No bonus for searching on your own square */
			dist = 1;
		}
		difficulty += 5 * (dist - 1);

		/* Give various penalties */
		if (p->timed[TMD_BLIND] || no_light(p) || p->timed[TMD_IMAGE]) {
			/* Can't see properly */
			difficulty += 5;
		}
		if (p->timed[TMD_CONFUSED]) {
			/* Confused */
			difficulty += 5;
		}
		if (square_issecrettrap(cave, grid)) {
			/* Dungeon trap */
			difficulty += 5;
		}
		if (square_issecretdoor(cave, grid)) {
			/* Secret door */
			difficulty += 10;
		}
		if (obj) {
			/* Chest trap */
			difficulty += 15;
		}

		/* Spider bane bonus helps to find webs */
		if (square_iswebbed(cave, grid)) {
			difficulty -= player_spider_bane_bonus(p);
		}

		/* Sometimes, notice things */
		if (skill_check(source_player(), score, difficulty, source_none()) > 0){
			/* Traps */
			if (square_issecrettrap(cave, grid)) {
				square_reveal_trap(cave, grid, true);
				disturb(p, false);
			}

			/* Secret doors */
			if (square_issecretdoor(cave, grid)) {
				msg("You have found a secret door.");
				place_closed_door(cave, grid);
				disturb(p, false);
			}

			/* Traps on chests */
			if (obj && obj->known && !obj->known->pval) {
				msg("You have discovered a trap on the chest!");
				obj->known->pval = obj->pval;
				disturb(p, false);
			}
		}
	}
}

/**
 * Search for adjacent hidden things
 */
void search(struct player *p)
{
	struct loc grid;

	/* Search the adjacent grids */
	for (grid.y = (p->grid.y - 1); grid.y <= (p->grid.y + 1); grid.y++) {
		for (grid.x = (p->grid.x - 1); grid.x <= (p->grid.x + 1); grid.x++) {
			if (!loc_eq(grid, p->grid)) {
				search_square(p, grid, 1, true);
			}
		}
	}

	/* Also make the normal perception check */
	perceive(p);
}

/**
 * Maybe notice hidden things nearby
 */
void perceive(struct player *p)
{
	struct loc grid;

	/* Search nearby grids */
	for (grid.y = (p->grid.y - 4); grid.y <= (p->grid.y + 4); grid.y++) {
		for (grid.x = (p->grid.x - 4); grid.x <= (p->grid.x + 4); grid.x++) {
			if (square_in_bounds(cave, grid)) {
				int dist = distance(p->grid, grid);

				/* Search only if adjacent, player lit or permanently lit */
				if ((dist <= 1) || (p->upkeep->cur_light >= dist) ||
					square_isglow(cave, grid)) {
					/* Search only if within four grids and in line of sight */
					if ((dist <= 4) && los(cave, p->grid, grid)) {
						search_square(p, grid, dist, false);
					}
				}
			}
		}
	}
}
