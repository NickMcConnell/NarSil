/**
 * \file mon-attack.c
 * \brief Monster attacks
 *
 * Monster ranged attacks - choosing an attack spell or shot and making it.
 * Monster melee attacks - monster critical blows, whether a monster 
 * attack hits, what happens when a monster attacks an adjacent player.
 *
 * Copyright (c) 1997 Ben Harrison, David Reeve Sward, Keldon Jones.
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
#include "mon-attack.h"
#include "mon-blows.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "obj-knowledge.h"
#include "player-attack.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"

/**
 * ------------------------------------------------------------------------
 * Ranged attacks
 * ------------------------------------------------------------------------ */
/**
 * Count the number of castable spells.
 *
 * If exactly 1 spell is available cast it.  If more than one is
 * available, and random is set, pick one.
 */
static int choose_attack_spell_fast(const bitflag f[RSF_SIZE], bool do_random)
{
	int i, num = 0;
	uint8_t spells[RSF_MAX];

	/* Paranoid initialization */
	for (i = 0; i < RSF_MAX; i++) {
		spells[i] = 0;
	}

	/* Extract the spells  */
	for (i = FLAG_START; i != FLAG_END; i = rsf_next(f, i + 1)) {
		if (rsf_has(f, i)) {
			spells[num++] = i;
		}
	}

	/* Paranoia */
	if (num == 0) return 0;

	/* Go quick if possible */
	if (num == 1) {
		/* Cast the one spell */
		return (spells[0]);
	}

	/* If we aren't allowed to choose at random and we have multiple spells
	 * left, give up on quick selection */
	if (!do_random) return 0;

	/* Pick at random */
	return (spells[randint0(num)]);
}


/**
 * Have a monster choose a spell.
 *
 * Monsters use this function to select a legal attack spell.
 * Spell casting AI is based here.
 *
 * First the code will try to save time by seeing if
 * choose_attack_spell_fast is helpful.  Otherwise, various AI
 * parameters are used to calculate a 'desirability' for each spell.
 * There is some randomness.  The most desirable spell is cast.
 *
 * Returns the spell number, or '0' if no spell is selected.
 */
static int choose_ranged_attack(struct monster *mon)
{
	bitflag f[RSF_SIZE];
	bool do_random = false;
	int best_spell, best_spell_rating = 0;
	int i;

	/* Extract the racial spell flags */
	rsf_copy(f, mon->race->spell_flags);

	/* Remove spells that cost too much or have unfulfilled conditions */
	remove_bad_spells(mon, f);

	/* No spells left */
	if (!rsf_count(f)) return 0;

	/* Mindless monsters always cast randomly and sometimes non-smart
	 * do (though both always use the restricted list). */
	if (rf_has(mon->race->flags, RF_MINDLESS)
			|| (!rf_has(mon->race->flags, RF_SMART) && one_in_(5))) {
		do_random = true;
	}

	/* Try fast selection first. If there is only one spell, choose that spell.
	 * If there are multiple spells, choose one randomly if the 'random' flag
	 * is set. Otherwise fail, and let the AI choose. */
	best_spell = choose_attack_spell_fast(f, do_random);
	if (best_spell) return best_spell;

	/* Use full AI */
	for (i = FLAG_START; i != FLAG_END; i = rsf_next(mon->race->spell_flags, i + 1)) {
		int spell_range, cur_spell_rating;
		const struct monster_spell *spell;

		/* Do we even have this spell? */
		if (!rsf_has(f, i)) continue;
		spell = monster_spell_by_index(i);

		/* Get base range and desirability */
		spell_range = spell->best_range;
		cur_spell_rating = spell->desire;

		/* Penalty for range if attack drops off in power */
		if (spell_range) {
			int cur_range = mon->cdis;

			while (cur_range > spell_range) {
				cur_spell_rating *= spell->use_past_range;
				cur_spell_rating /= 100;
				--cur_range;
			}
		}

		/* Random factor; less random for smart monsters */
		if (rf_has(mon->race->flags, RF_SMART)) {
			cur_spell_rating += randint0(10);
		} else {
			cur_spell_rating += randint0(50);
		}

		/* Is this the best spell yet?, or alternate between equal spells */
		if ((cur_spell_rating > best_spell_rating) ||
			((cur_spell_rating == best_spell_rating) && one_in_(2))) {
			best_spell_rating = cur_spell_rating;
			best_spell = i;
		}
	}

	if (player->wizard) {
		msg("Spell rating: %i.", best_spell_rating);
	}

	/* Abort if there are no good spells */
	if (!best_spell_rating) return 0;

	/* Return best spell */
	assert(best_spell);
	return best_spell;
}

/**
 * Creatures can cast spells, shoot missiles, and breathe.
 *
 * Returns "true" if a spell (or whatever) was (successfully) cast.
 */
bool make_ranged_attack(struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);
	bool seen = (player->timed[TMD_BLIND] == 0) && monster_is_visible(mon);

	/* Choose attack, or give up */
	int choice = choose_ranged_attack(mon);
	if (!choice) return false;

	/* Monster has cast a spell*/
	mflag_off(mon->mflag, MFLAG_ALWAYS_CAST);

	/* Cast the spell. */
	do_mon_spell(choice, mon, seen);

	/* Mark minimum desired range for recalculation */
	mon->min_range = 0;

	/* Remember what the monster did */
	if (seen) {
		rsf_on(lore->spell_flags, choice);
		if (lore->ranged < UCHAR_MAX)
			lore->ranged++;
	}

	/* Always take note of monsters that kill you */
	if (player->is_dead && (lore->deaths < SHRT_MAX)) {
		lore->deaths++;
	}
	lore_update(mon->race, lore);

	/* A spell was cast */
	return true;
}



/**
 * ------------------------------------------------------------------------
 * Melee attack
 * ------------------------------------------------------------------------ */
/**
 * Determine whether a monster is making a valid charge attack
 */
static bool monster_charge(struct monster *mon, struct player *p)
{
    int speed = mon->race->speed;
    int deltay = p->grid.y - mon->grid.y;
    int deltax = p->grid.x - mon->grid.x;
    
    /* Paranoia */
    if (distance(mon->grid, p->grid) > 1) return false;

    /* Determine the monster speed */
    if (mon->m_timed[MON_TMD_SLOW]) speed--;

    /* If it has the ability and isn't slow */
    if (rf_has(mon->race->flags, RF_CHARGE) && (speed >= 2)) {
		int d, i;

        /* Try all three directions */
        for (i = -1; i <= 1; i++) {
            d = cycle[chome[dir_from_delta(deltay, deltax)] + i];
            if (mon->previous_action[1] == d) {
                return true;
            }
        }
    }

    return false;
}

/**
 * Determine whether there is a bonus die for an elemental attack that
 * the player doesn't resist
 *
 * Ideally this would be incorporated into melee_effect_elemental - NRM
 */
static int elem_bonus(struct player *p, struct blow_effect *effect)
{
	int resistance = 1;

	if (streq(effect->name, "FIRE")) {
		resistance = p->state.el_info[ELEM_FIRE].res_level;
	} else if (streq(effect->name, "COLD")) {
		resistance = p->state.el_info[ELEM_COLD].res_level;
	} else if (streq(effect->name, "POISON")) {
		resistance = p->state.el_info[ELEM_POIS].res_level;
	} else if (streq(effect->name, "DARK")) {
		resistance = p->state.el_info[ELEM_DARK].res_level;
	} else {
		return 0;
	}

	if (resistance == 1) {
		return 1;
	} else if (resistance < 0) {
		return -resistance;
	}
	return 0;
}

/**
 * Critical hits by monsters can inflict cuts and stuns.
 *
 * The chance is greater for WOUND and BATTER attacks
 */
static bool monster_cut_or_stun(int dice, int dam, struct blow_effect *effect)
{
	if (dam <= 0) return false;

	/* Special case -- wounding/battering attack */
	if (streq(effect->name, "WOUND") || streq(effect->name, "BATTER")) {
		if (dice >= randint1(2)) return true;
	} else if (one_in_(10)) {
		/* Standard attack */
		if (dice >= randint1(2)) return true;
	}

	return false;
}

/**
 * Monster cruel blow ability
 *
 * Ideally we'd use a call to allow_player_confuse() here, but that doesn't
 * work as it can't take the level of the critical into account.
 * Sadly my solution doesn't let you ID confusion resistance items.
 */
static void cruel_blow(struct monster *mon, struct player *p, int dice)
{
	struct monster_lore *lore = get_lore(mon->race);
	int difficulty = p->state.skill_use[SKILL_WILL] +
		(p->state.flags[OF_PROT_CONF] * 10);

	if (skill_check(source_monster(mon->midx), dice * 4, difficulty,
					source_player()) > 0) {
		/* Remember that the monster can do this */
		if (monster_is_visible(mon)) {
			rf_on(lore->flags, RF_CRUEL_BLOW);
		}

		msg("You reel in pain!");

		/* Confuse the player */
		player_inc_timed(p, TMD_CONFUSED, dice, true, true, true);
	}
}

/**
 * Attack the player via physical attacks.
 */
bool make_attack_normal(struct monster *mon, struct player *p)
{
	struct monster_lore *lore = get_lore(mon->race);
	int rlev = ((mon->race->level >= 1) ? mon->race->level : 1);
	char m_name[80];
	char ddesc[80];
	int blow;
	struct blow_effect *effect;
	struct blow_method *method;
	int att, dd, ds;
	int total_attack_mod, total_evasion_mod;
	bool visible = monster_is_visible(mon) || (mon->race->light > 0);
	bool obvious = false;

	bool do_cut, do_stun, do_prt;
	int sound_msg = MSG_GENERIC;
	int hit_result = 0;
	int net_dam = 0;

	char *act = NULL;
	bool act_allocated = false;

	/* Get the monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);

	/* Get the "died from" information (i.e. "a kobold") */
	monster_desc(ddesc, sizeof(ddesc), mon, MDESC_SHOW | MDESC_IND_VIS);

	/* Monsters might notice */
	p->been_attacked = true;

	/* Use the alternate attack one in three times */
	blow = mon->race->blow[1].method && one_in_(3) ? 1 : 0;
	effect = mon->race->blow[blow].effect;
	method = mon->race->blow[blow].method;
	att = mon->race->blow[blow].dice.base;
	dd = mon->race->blow[blow].dice.dice;
	ds = mon->race->blow[blow].dice.sides;

	/* Determine the monster's attack score */
	total_attack_mod = total_monster_attack(p, mon, att);
	if (monster_charge(mon, p)) {
		total_attack_mod += 3;
		ds += 3;
	}

	/* Determine the player's evasion score */
	total_evasion_mod = total_player_evasion(p, mon, false);

	/* Check if the player was hit */
	hit_result = hit_roll(total_attack_mod, total_evasion_mod,
						  source_monster(mon->midx), source_player(), true);

	/* Monster hits player */
	assert(effect);
	if (streq(effect->name, "NONE") || (hit_result > 0)) {
		melee_effect_handler_f effect_handler;
		int crit_bonus_dice = 0;
		int elem_bonus_dice = 0;
		int dam = 0, prt = 0;

		/* Always disturbing */
		disturb(p, true);

		/* Describe the attack method */
		act = monster_blow_method_action(method, -1);
		act_allocated = true;
		do_cut = method->cut;
		do_stun = method->stun;
		do_prt = method->prt;
		sound_msg = method->msgt;

		/* Special case */
		if (streq(method->name, "HIT") && streq(effect->name, "BATTER")) {
			if (act_allocated) {
				string_free(act);
			}
			act = (char *) "batters you";
			act_allocated = false;
		}

		/* Hack -- assume all attacks are obvious */
		obvious = true;

		/* Determine critical-hit bonus dice (if any)
		 * treats attack as weapon weighing 2 pounds per damage die */
		crit_bonus_dice = crit_bonus(p, hit_result, 20 * dd, NULL, SKILL_MELEE,
									 false);

		/* Determine elemental attack bonus dice (if any)  */
		elem_bonus_dice = elem_bonus(p, effect);

		/* Certain attacks can't do criticals */
		if (!do_prt) crit_bonus_dice = 0;

		/* Roll out the damage */
		dam = damroll(dd + crit_bonus_dice + elem_bonus_dice, ds);
	
		/* Determine the armour based damage-reduction for the player */
		prt = do_prt ? protection_roll(p, PROJ_HURT, true, RANDOMISE) : 0;

		/* Now calculate net_dam, taking protection into account */
		net_dam = MAX((dam - prt), 0);

		/* Message */
		if (act) {
			char punct[20];
			/* Determine the punctuation for the attack ("...", ".", "!" etc) */
			attack_punctuation(punct, sizeof(punct), net_dam, crit_bonus_dice);

			if (monster_charge(mon, p)) {
				/* Remember that the monster can do this */
				if (monster_is_visible(mon)) {
					rf_on(lore->flags, RF_CHARGE);
					if (act_allocated) {
						string_free(act);
					}
					act = (char *) "charges you";
					act_allocated = false;
                }
			}
                
			msgt(sound_msg, "%s %s%s", m_name, act, punct);
			if (act_allocated) {
				string_free(act);
			}
		}

		/* Perform the actual effect. */
		effect_handler = melee_handler_for_blow_effect(effect->name);
		if (effect_handler != NULL) {
			melee_effect_handler_context_t context = {
				p,
				mon,
				rlev,
				method,
				ddesc,
				obvious,
				do_stun,
				do_cut,
				dam,
				net_dam,
			};

			effect_handler(&context);

			/* Save any changes made in the handler for later use. */
			obvious = context.obvious;
			do_stun = context.stun;
			do_cut = context.cut;
			net_dam = context.damage;

			event_signal_combat_damage(EVENT_COMBAT_DAMAGE,
									   dd + crit_bonus_dice + elem_bonus_dice,
									   ds, dam, -1, -1, prt, do_prt ? 100 : 0,
									   effect->dam_type, true);
			event_signal_hit(EVENT_HIT, net_dam, effect->dam_type, p->is_dead,
							 p->grid);
		} else {
			msg("ERROR: Effect handler not found for %s.", effect->name);
		}

		/* Don't cut or stun if player is dead */
		if (p->is_dead) {
			do_cut = false;
			do_stun = false;
		}

		/* Hack -- only one of cut or stun */
		if (do_cut && do_stun) {
			if (one_in_(2)) {
				/* Cancel cut */
				do_cut = false;
			} else {
			/* Cancel stun */
				do_stun = false;
			}
		}

		/* Handle cut */
		if ((do_cut) && monster_cut_or_stun(crit_bonus_dice, net_dam, effect)) {
			/* Apply the cut */
			(void)player_inc_timed(p, TMD_CUT, dam / 2,
				 true, true, true);
		}

		/* Handle stun */
		if ((do_stun) && monster_cut_or_stun(crit_bonus_dice, net_dam, effect)){
			/* Apply the stun */
			(void)player_inc_timed(p, TMD_STUN, dam,
				true, true, true);
		}

		/* Deal with Cruel Blow */
		if (rf_has(mon->race->flags, RF_CRUEL_BLOW) && (crit_bonus_dice >= 1) &&
			(net_dam > 0)) {
			cruel_blow(mon, p, crit_bonus_dice);
		}

		/* Deal with Knock Back */
		if (rf_has(mon->race->flags, RF_KNOCK_BACK)) {
			/* Only happens on the main attack (so bites don't knock back) */
			if (blow == 0) {
				/* Determine if the player is knocked back */
				if (skill_check(source_monster(mon->midx),
								monster_stat(mon, STAT_STR) * 2,
								p->state.stat_use[STAT_CON] * 2,
								source_player()) > 0) {
					/* Do the knocking back */
					knock_back(mon->grid, p->grid);

					/* Remember that the monster can do this */
					if (monster_is_visible(mon)) {
						rf_on(lore->flags, RF_KNOCK_BACK);
                    }
                }
            }
		}

		/* Deal with cowardice */
		if ((p->state.flags[OF_COWARDICE] > 0) &&
			(net_dam >= 10 / p->state.flags[OF_COWARDICE])) {
			if (!p->timed[TMD_AFRAID]) {
				if (player_inc_timed(p, TMD_AFRAID, damroll(10, 4), true, true, true)) {
					player_inc_timed(p, TMD_FAST, damroll(5, 4), true, true, true);

					/* Give the player a chance to identify what's causing it */
					player_learn_flag(p, OF_COWARDICE);
				}
			}
		}
	} else {
		/* Visible monster missed player, so notify if appropriate. */
		if (monster_is_visible(mon) && !p->timed[TMD_CONFUSED] &&
			method->miss) {
			/* Disturbing */
			disturb(p, true);

			/* Deal with earthquakes if they miss you by 1 or 2 or 3 points */
			if (streq(effect->name, "SHATTER") && (hit_result > -3)) {
				/* Message */
				msg("%s just misses you.", m_name);

				/* Morgoth */
				if (rf_has(mon->race->flags, RF_QUESTOR)) {
					msg("You leap aside as his great hammer slams into the floor.");
					msg("The ground shakes violently with the force of the blow!");

					/* Radius 5 earthquake centered on the monster */
					effect_simple(EF_EARTHQUAKE, source_monster(mon->midx), "0",
								  0, 5, 1, NULL);
				} else {
					/* Kemenrauko */
					msg("You leap aside as its stony fist slams into the floor.");
					msg("The ground shakes violently with the force of the blow!");

					/* Radius 4 earthquake centered on the monster */
					effect_simple(EF_EARTHQUAKE, source_monster(mon->midx), "0",
								  0, 4, 1, NULL);
				}
			} else {
				/* A normal miss */
				msg("%s misses you.", m_name);

				/* Allow for ripostes */
				if (player_can_riposte(p, hit_result)) {
					msg("You riposte!");
					p->upkeep->riposte = true;
					py_attack_real(p, mon->grid, ATT_RIPOSTE);
				}
			}
		}
	}

	if (lore) {
		/* Analyze "visible" monsters only */
		if (visible) {
			/* Count "obvious" attacks (and ones that cause damage) */
			if (obvious || net_dam || (lore->blows[blow].times_seen > 10)) {
				/* Count attacks of this type */
				if (lore->blows[blow].times_seen < UCHAR_MAX)
					lore->blows[blow].times_seen++;
			}
		}

		/* Always notice cause of death */
		if (p->is_dead && (lore->deaths < SHRT_MAX))
			lore->deaths++;

		/* Learn lore */
		lore_update(mon->race, lore);
	}

	/* Assume we attacked */
	return true;
}

