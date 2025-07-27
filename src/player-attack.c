/**
 * \file player-attack.c
 * \brief Attacks (both throwing and melee) by the player
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
#include "cmds.h"
#include "effects.h"
#include "game-event.h"
#include "game-input.h"
#include "generate.h"
#include "init.h"
#include "mon-attack.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-msg.h"
#include "mon-predicate.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "songs.h"
#include "target.h"

/**
 * ------------------------------------------------------------------------
 * Ability-based attack functions
 * ------------------------------------------------------------------------ */
/**
 *  Determines whether an attack is a charge attack
 */
static bool valid_charge(struct player *p, struct loc grid, int attack_type)
{
	int d, i;
	
	int delta_y = grid.y - p->grid.y;
	int delta_x = grid.x - p->grid.x;
	
	if (player_active_ability(p, "Charge") && (p->state.speed > 1) &&
	    ((attack_type == ATT_MAIN) || (attack_type == ATT_FLANKING) ||
		 (attack_type == ATT_CONTROLLED_RETREAT))) { 
		/* Try all three directions */
		for (i = -1; i <= 1; i++) {
			d = cycle[chome[dir_from_delta(delta_y, delta_x)] + i];
						
			if (p->previous_action[1] == d) {
				return true;
			}		
		}
	}
	
	return false;
}

/**
 * Attacks a new monster with 'follow through' if applicable
 */
static void possible_follow_through(struct player *p, struct loc grid,
									int attack_type)
{
	int d, i;
	struct loc new_grid;
	int delta_y = grid.y - p->grid.y;
	int delta_x = grid.x - p->grid.x;
	
	if (player_active_ability(p, "Follow-Through") && !p->timed[TMD_CONFUSED] &&
		((attack_type == ATT_MAIN) || (attack_type == ATT_FLANKING) || 
		 (attack_type == ATT_CONTROLLED_RETREAT) ||
		 (attack_type == ATT_FOLLOW_THROUGH))) {
        /* Look through adjacent squares in an anticlockwise direction */
        for (i = 1; i < 8; i++) {
			struct monster *mon;
            d = cycle[chome[dir_from_delta(delta_y, delta_x)] + i];
            new_grid = loc_sum(p->grid, ddgrid[d]);
			mon = square_monster(cave, new_grid);
            
            if (mon && monster_is_visible(mon) &&
				(!OPT(p, forgo_attacking_unwary) ||
				 (mon->alertness >= ALERTNESS_ALERT))) {
                    msg("You continue your attack!");
                    py_attack_real(p, new_grid, ATT_FOLLOW_THROUGH);
                    return;
			}
		}
	}
}


/**
 * Cruel blow ability
 */
static void cruel_blow(int crit_bonus_dice, struct monster *mon)
{
	char m_name[80];

	if (player_active_ability(player, "Cruel Blow")) {
		/* Must be a damaging critical hit */
		if (crit_bonus_dice <= 0) return;

		/* Monster must not resist */
		if (rf_has(mon->race->flags, RF_RES_CRIT)) return;

		monster_desc(m_name, sizeof(m_name), mon, MDESC_TARG);
		if (skill_check(source_player(), crit_bonus_dice * 4,
						monster_skill(mon, SKILL_WILL),
						source_monster(mon->midx)) > 0) {
			msg("%s reels in pain!", m_name);

			/* Confuse the monster (if possible)
			 * The +1 is needed as a turn of this wears off immediately */
			mon_inc_timed(mon, MON_TMD_CONF, crit_bonus_dice + 1, 0);

			/* Cause a temporary morale penalty */
			scare_onlooking_friends(mon, -20);
		}
	}
}

/**
 * ------------------------------------------------------------------------
 * Attack calculations
 * ------------------------------------------------------------------------ */
/**
 * Determines the protection percentage
 */
int prt_after_sharpness(struct player *p, const struct object *obj, int *flag)
{
	int protection = 100;
	struct song *sharp = lookup_song("Sharpness");

	if (!obj) return 0;

	/* Sharpness */
	if (of_has(obj->flags, OF_SHARPNESS)) {
		*flag = OF_SHARPNESS;
		protection = 50;
	}

	/* Sharpness 2 */
	if (of_has(obj->flags, OF_SHARPNESS2)) {
		*flag = OF_SHARPNESS2;
		protection = 0;
	}

	/* Song of sharpness */
	if (player_is_singing(p, sharp)) {
		if (tval_is_sharp(obj)) {
			protection -= song_bonus(p, p->state.skill_use[SKILL_SONG], sharp);
		}
	}

	return MAX(protection, 0);
}

void attack_punctuation(char *punctuation, size_t len, int net_dam,
						int crit_bonus_dice)
{
	if (net_dam == 0) {
		my_strcpy(punctuation, "...", len);
	} else if (crit_bonus_dice <= 0) {
		my_strcpy(punctuation, ".", len);
	} else {
		size_t i;
		for (i = 0; (i < (size_t) crit_bonus_dice) && (i < len - 1); i++) {
			punctuation[i] = '!';
		}
		punctuation[i] = '\0';
	}
}
/**
 * ------------------------------------------------------------------------
 * Melee attack
 * ------------------------------------------------------------------------ */
/**
 * A whirlwind attack is possible
 */
static bool whirlwind_possible(struct player *p)
{
	int d, dir;
	struct loc grid;

	if (p->timed[TMD_RAGE]) return true;
	
	if (!player_active_ability(p, "Whirlwind Attack")) {
		return false;
	}

	/* Check adjacent squares for impassable squares */
	 for (d = 0; d < 8; d++) {
		 dir = cycle[d];
		 grid = loc_sum(p->grid, ddgrid[dir]);

		 if (square_iswall(cave, grid)) {
			 return false;
		 }
	 }
	 
	return true;
}

/**
 * A whirlwind attack
 */
static void whirlwind(struct player *p, struct loc grid)
{
	int i, dir, dir0;
	bool clockwise = one_in_(2);

	/* Message only for rage (too annoying otherwise) */
	if (p->timed[TMD_RAGE]) {
		msg("You strike out at everything around you!");
	}
	
	dir = dir_from_delta(grid.y - p->grid.y, grid.x - p->grid.x);

	/* Extract cycle index */
	dir0 = chome[dir];

	/* Attack the adjacent squares in sequence */
	for (i = 0; i < 8; i++) {
		struct loc adj_grid;
		struct monster *mon;
		if (clockwise) {
			dir = cycle[dir0 + i];
		} else {
			dir = cycle[dir0 - i];
		}

		adj_grid = loc_sum(p->grid, ddgrid[dir]);
		mon = square_monster(cave, adj_grid);

		if (mon) {
			if (p->timed[TMD_RAGE]) {
				py_attack_real(p, adj_grid, ATT_RAGE);
			} else if ((i == 0) || !OPT(p, forgo_attacking_unwary) ||
					   (mon->alertness >= ALERTNESS_ALERT)) {
				py_attack_real(p, adj_grid, ATT_WHIRLWIND);
			}
		}
	}
}

/**
 * Attack the monster at the given location with a single blow.
 */
void py_attack_real(struct player *p, struct loc grid, int attack_type)
{
	/* Information about the target of the attack */
	struct monster *mon = square_monster(cave, grid);
	struct monster_race *race = mon ? mon->race : NULL;
	char m_name[80];
	char name[80];

	/* The weapon used */
	struct object *obj = equipped_item_by_slot_name(p, "weapon");

	/* Information about the attack */
	int blows = 1;
	int num = 0;
	int attack_mod = 0, total_attack_mod = 0, total_evasion_mod = 0;
	int hit_result = 0;
	int dam = 0, prt = 0;
	int net_dam = 0;
	int prt_percent = 100;
	int stealth_bonus = 0;
	int mdd, mds;
    bool monster_riposte = false;
    bool abort_attack = false;
	bool charge = false;
	bool rapid_attack = false;

	char verb[20];
	char punct[20];
	int weight;
	const struct artifact *crown = lookup_artifact_name("of Morgoth");

	/* Default to punching */
	my_strcpy(verb, "punch", sizeof(verb));

	/* Extract monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_TARG);

	/* Auto-Recall and track if possible and visible */
	if (monster_is_visible(mon)) {
		monster_race_track(p->upkeep, mon->race);
		health_track(p->upkeep, mon);
	}

	/* Handle player fear (only for invisible monsters) */
	if (p->timed[TMD_AFRAID]) {
		msgt(MSG_AFRAID, "You are too afraid to attack %s!", m_name);
		return;
	}

    /* Inscribing an object with "!a" produces prompts to confirm that you wish
	 * to attack with it; idea from MarvinPA */
    if (obj && check_for_inscrip(obj, "!a") && !p->truce) {
		if (!get_check("Are you sure you wish to attack? ")) {
			abort_attack = true;
		}
    }

 	/* Warning about breaking the truce */
	if (p->truce && !get_check("Are you sure you wish to attack? ")) {
        abort_attack = true;
	}

    /* Warn about fighting with fists */
    if (!obj &&	!get_check("Are you sure you wish to attack with no weapon? ")){
        abort_attack = true;
    }

    /* Warn about fighting with shovel */
	if (obj) { 
		object_short_name(name, sizeof(name), obj->kind->name);
		if (tval_is_digger(obj) && streq(name, "Shovel") &&
			!get_check("Are you sure you wish to attack with your shovel? ")) {
			abort_attack = true;
		}
    }

    /* Cancel the attack if needed */
    if (abort_attack) {
        if (!p->attacked) {
            /* Reset the action type */
            p->previous_action[0] = ACTION_NOTHING;

            /* Don't take a turn */
            p->upkeep->energy_use = 0;
        }

        /* Done */
        return;
    }

	if (obj) {
		/* Handle normal weapon */
		weight = obj->weight;
		my_strcpy(verb, "hit", sizeof(verb));
	} else {
		/* Fighting with fists is equivalent to a 4 lb weapon for the purpose
		 * of criticals */
		weight = 0;
	}

	mdd = p->state.mdd;
	mds = p->state.mds;

	/* Determine the base for the attack_mod */
	attack_mod = p->state.skill_use[SKILL_MELEE];
		
	/* Monsters might notice */
	p->attacked = true;
		
	/* Determine the number of attacks */
	if (player_active_ability(p, "Rapid Attack")) {
		blows++;
		rapid_attack = true;
	}
	if (p->state.mds2 > 0) {
		blows++;
	}

	/* Attack types that take place in the opponents' turns only allow a
	 * single attack */
	if ((attack_type != ATT_MAIN) && (attack_type != ATT_FLANKING) &&
		(attack_type != ATT_CONTROLLED_RETREAT)) {
		blows = 1;
		
		/* Undo strength adjustment to the attack (if any) */
		mds = total_mds(p, &p->state, obj, 0);
		
		/* Undo the dexterity adjustment to the attack (if any) */
		if (rapid_attack) { 
			rapid_attack = false;
			attack_mod += 3;
		}
	}

	/* Attack once for each legal blow */
	while (num++ < blows) {
		bool do_knock_back = false;
		bool knocked = false;
		bool off_hand_blow = false;

		/* If the previous blow was a charge, undo the charge effects for
		 * later blows */
		if (charge) {
			charge = false;
			attack_mod -= 3;
			mds = p->state.mds;
		}

		/* Adjust for off-hand weapon if it is being used */
		if ((num == blows) && (num != 1) && (p->state.mds2 > 0)) {
			off_hand_blow = true;
			rapid_attack = false;
			
			attack_mod += p->state.offhand_mel_mod;
			mdd = p->state.mdd2;
			mds = p->state.mds2;
			obj = equipped_item_by_slot_name(p, "arm");
			weight = obj->weight;
		}

		/* +3 Str/Dex on first blow when charging */
		if ((num == 1) && valid_charge(p, grid, attack_type)) {
			int str_adjustment = 3;
			
			if (rapid_attack) str_adjustment -= 3;
			
			charge = true;
			attack_mod += 3;

			/* Undo strength adjustment to the attack (if any) */
			mds = total_mds(p, &p->state, obj, str_adjustment);
		}

		/* Reward melee attacks on sleeping monsters by characters with the
		 * asssassination ability (only when a main, flanking, or controlled
		 * retreat attack, and not charging) */
		if (((attack_type == ATT_MAIN) || (attack_type == ATT_FLANKING) ||
			 (attack_type == ATT_CONTROLLED_RETREAT)) && !charge) {
			stealth_bonus = stealth_melee_bonus(mon);
		}

		/* Determine the player's attack score after all modifiers */
		total_attack_mod = total_player_attack(p, mon,
											   attack_mod + stealth_bonus);

		/* Determine the monster's evasion score after all modifiers */
		total_evasion_mod = total_monster_evasion(p, mon, false);

		/* Test for hit */
		hit_result = hit_roll(total_attack_mod, total_evasion_mod,
							  source_player(), source_monster(mon->midx), true);

		/* If the attack connects... */
		if (hit_result > 0) {
			int crit_bonus_dice = 0, slay_bonus_dice = 0, total_dice = 0;
			int effective_strength = p->state.stat_use[STAT_STR];
			bool fatal_blow = false;
			bool living = monster_is_living(mon);
			int slay = 0, brand = 0, flag = 0;

			/* Mark the monster as attacked */
			mflag_on(mon->mflag, MFLAG_HIT_BY_MELEE);

			/* Mark the monster as charged */
			if (charge) mflag_on(mon->mflag, MFLAG_CHARGED);

			/* Calculate the damage */
			crit_bonus_dice = crit_bonus(p, hit_result, weight, race,
										 SKILL_MELEE, false);
			slay_bonus_dice = slay_bonus(p, obj, mon, &slay, &brand);
			total_dice = mdd + slay_bonus_dice + crit_bonus_dice;

			dam = damroll(total_dice, mds);
			prt = damroll(race->pd, race->ps);

			prt_percent = prt_after_sharpness(p, obj, &flag);
			prt = (prt * prt_percent) / 100;

			/* No negative damage */
			net_dam = MAX(dam - prt, 0);

			/* Determine the punctuation for the attack ("...", ".", "!" etc) */
			attack_punctuation(punct, sizeof(punct), net_dam, crit_bonus_dice);

			/* Special message for visible unalert creatures */
			if (stealth_bonus) {
				msgt(MSG_HIT, "You stealthily attack %s%s", m_name,
					 punct);
			} else if (charge) {
					msgt(MSG_HIT, "You charge %s%s", m_name, punct);
			} else {
				msgt(MSG_HIT, "You hit %s%s", m_name, punct);
			}

			event_signal_combat_damage(EVENT_COMBAT_DAMAGE, total_dice, mds,
									   dam, race->pd, race->ps, prt,
									   prt_percent, PROJ_HURT, true);

			/* Determine the player's score for knocking an opponent backwards
			 * if they have the ability */
			/* First calculate their strength including modifiers for this
			 * attack */
			effective_strength = p->state.stat_use[STAT_STR];
			if (charge) effective_strength += 3;
			if (rapid_attack) effective_strength -= 3;
			if (off_hand_blow) effective_strength -= 3;

			/* Cap the value by the weapon weight */
			if (effective_strength > weight / 10) {
				effective_strength = weight / 10;
			} else if ((effective_strength < 0) &&
					   (-effective_strength > weight / 10)) {
				effective_strength = -(weight / 10);
			}

			/* Give an extra +2 bonus for using a weapon two-handed */
			if (two_handed_melee(p)) {
				effective_strength += 2;
			}

			/* Check whether the effect triggers */
			if (player_active_ability(p, "Knock Back") &&
				(attack_type != ATT_OPPORTUNIST) &&
				!rf_has(race->flags, RF_NEVER_MOVE) &&
			    (skill_check(source_player(), effective_strength * 2,
							 monster_stat(mon, STAT_CON) * 2,
							 source_monster(mon->midx)) > 0)) {
				do_knock_back = true;
			}

			/* If a slay, brand or flag was noticed, learn it */
			if (slay || brand) {
				learn_brand_slay_from_melee(p, obj, mon);
			}
			if (flag && !player_knows_flag(p, flag)) {
				char o_name[80];
				char desc[80];
				object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, p);
				if (flag_slay_message(flag, m_name, desc, sizeof(desc))) {
					msg("Your %s %s.", o_name, desc);
				}
				player_learn_flag(p, flag);
			}

			/* Damage, check for death */
			fatal_blow = mon_take_hit(mon, p, net_dam, NULL);

			/* Display depending on whether knock back triggered */
			if (do_knock_back) {
				event_signal_hit(EVENT_HIT, net_dam, PROJ_SOUND, fatal_blow,
								 grid);
			} else {
				event_signal_hit(EVENT_HIT, net_dam, PROJ_HURT, fatal_blow,
								 grid);
			}

			/* Deal with killing blows */
			if (fatal_blow) {
				/* Heal with a vampiric weapon */
				if (obj && of_has(obj->flags, OF_VAMPIRIC) && living) {
					if (p->chp < p->mhp) {
						effect_simple(EF_HEAL_HP, source_player(), "m7", 0, 0,
									  0, NULL);
						if (!player_knows_flag(p, OF_VAMPIRIC)) {
							char o_name[80];
							char desc[80];
							object_desc(o_name, sizeof(o_name), obj,
										ODESC_BASE, p);
							if (flag_slay_message(OF_VAMPIRIC, m_name, desc,
												  sizeof(desc))) {
								msg("Your %s %s.", o_name, desc);
							}
							player_learn_flag(p, OF_VAMPIRIC);
						}
					}
				}

				/* Gain wrath if singing song of slaying */
				if (player_is_singing(p, lookup_song("Slaying"))) {
					p->wrath += 100;
					p->upkeep->update |= PU_BONUS;
					p->upkeep->redraw |= PR_SONG;
				}

				/* Deal with 'follow_through' ability */
				possible_follow_through(p, grid, attack_type);
				
				/* Stop attacking */
				break;
			} else {
				/* deal with knock back ability if it triggered */
				if (do_knock_back) {
					knocked = knock_back(p->grid, grid);
 				}

				/* Morgoth drops his iron crown if he is hit for 10 or more
				 * net damage twice */
				if (rf_has(mon->race->flags, RF_QUESTOR) &&
					!is_artifact_created(crown)) {
					if (net_dam >= 10) {
						if (p->morgoth_hits == 0) {
							msg("The force of your blow knocks the Iron Crown off balance.");
							p->morgoth_hits++;
						} else if (p->morgoth_hits == 1) {
							drop_iron_crown(mon, "You knock his crown from off his brow, and it falls to the ground nearby.");
							p->morgoth_hits++;
						}
					}
				}

				if (net_dam) {
					cruel_blow(crit_bonus_dice, mon);
				}
			}
		} else {
			/* Player misses */
			msgt(MSG_MISS, "You miss %s.", m_name);

			/* Occasional warning about fighting from within a pit */
			if (square_ispit(cave, p->grid) && one_in_(3)) {
				msg("(It is very hard to dodge or attack from within a pit.)");
			}

			/* Occasional warning about fighting from within a web */
			if (square_iswebbed(cave, p->grid) && one_in_(3)) {
				msg("(It is very hard to dodge or attack from within a web.)");
			}

			/*
			 * Allow for ripostes - treats attack as a weapon
			 * weighing 2 pounds per damage die
			 */
			if (rf_has(race->flags, RF_RIPOSTE) &&
					!monster_riposte &&
					!mon->m_timed[MON_TMD_CONF] &&
					(mon->stance != STANCE_FLEEING) &&
					!mon->skip_this_turn &&
					!mon->skip_next_turn &&
					(hit_result <= -10 - (2 * race->blow[0].dice.dice))) {
				/* Remember that the monster can do this */
				if (monster_is_visible(mon)) {
					struct monster_lore *lore =
						get_lore(mon->race);

					rf_on(lore->flags, RF_RIPOSTE);
				}
				msg("%s ripostes!", m_name);
				make_attack_normal(mon, p);
				monster_riposte = true;
			}
		}

		/* Alert the monster, even if no damage was done or the player missed */
		make_alert(mon, 0);

		/* Stop attacking if you displace the creature */
		if (knocked) break;
	}

	/* Break the truce if creatures see */
	break_truce(p, false);
}


/**
 * Attack the monster at the given location
 */
void py_attack(struct player *p, struct loc grid, int attack_type)
{
	/* Store the action type */
	p->previous_action[0] = ACTION_MISC;

	if (whirlwind_possible(p) && (adj_mon_count(p->grid) > 1) &&
		!p->timed[TMD_AFRAID]) {
		whirlwind(p, grid);
	} else {
		py_attack_real(p, grid, attack_type);
	}
}

/**
 * ------------------------------------------------------------------------
 * Ranged attacks
 * ------------------------------------------------------------------------ */
/**
 * Returns percent chance of an object breaking after throwing or shooting.
 *
 * Artifacts will never break.
 *
 * Beyond that, each item kind has a percent chance to break (0-100). When the
 * object hits its target this chance is used.
 *
 * When an object misses it also has a chance to break. This is determined by
 * squaring the normaly breakage probability. So an item that breaks 100% of
 * the time on hit will also break 100% of the time on a miss, whereas a 50%
 * hit-breakage chance gives a 25% miss-breakage chance, and a 10% hit breakage
 * chance gives a 1% miss-breakage chance.
 */
int breakage_chance(const struct object *obj, bool hit_wall) {
	int perc = obj->kind->base->break_perc;

	if (obj->artifact) return 0;
	if (tval_is_light(obj)) {
		/* Jewels don't break */
		if (of_has(obj->flags, OF_NO_FUEL)) {
			if (obj->pval == 1) {
				/* Lesser Jewel */
				perc = 0;
			} else if (obj->pval == 7) {
				/* Silmaril */
				perc = 0;
			}
		}
	} else if (tval_is_ammo(obj)) {
		if (player_active_ability(player, "Careful Shot")) perc /= 2;
		if (player_active_ability(player, "Flaming Arrows")) perc = 100;
	} else if ((perc != 100) &&
			   player_active_ability(player, "Throwing Mastery")) {
		perc = 0;
	}

	/* Double breakage chance if it hit a wall */
	if (hit_wall) {
		perc *= 2;
		perc = MIN(perc, 100);
	}

	/* Unless they hit a wall, items designed for throwing won't break */
	if (of_has(obj->flags, OF_THROWING)) {
		if (hit_wall) {
			perc /= 4;
		} else {
			perc = 0;
		}
	}

	return perc;
}

/**
 * Maximum shooting range with a given bow
 */
int archery_range(const struct object *bow)
{
	int range;

	range = (bow->dd * total_ads(player, &player->state, bow, false) * 3) / 2;
	return MIN(range, z_info->max_range);
}


/**
 * Maximum throwing range with a given object
 */
int throwing_range(const struct object *obj)
{
	/* The divisor is the weight + 2lb */
	int div = obj->weight + 20;
	int range = (weight_limit(player->state) / 5) / div;

	/* Min distance of 1 */
	if (range < 1) range = 1;

	return MIN(range, z_info->max_range);
}

/**
 * Determines if a bow shoots radiant arrows and lights the current grid if so
 */
static bool do_radiance(struct player *p, struct loc grid) {
	/* Nothing to do */
	if (square_isglow(cave, grid)) return false;

	/* Give it light */
	sqinfo_on(square(cave, grid)->info, SQUARE_GLOW);
	
	/* Remember the grid */
	sqinfo_on(square(cave, grid)->info, SQUARE_MARK);

	/* Fully update the visuals */
	p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Update stuff */
	update_stuff(p);
	
	return true;
}

/**
 * Handle special effects of throwing certain potions
 */
static bool thrown_potion_effects(struct player *p, struct object *obj,
		bool *is_dead, struct monster *mon)
{
	struct loc grid = mon->grid;

	bool ident = false;
	bool used = true;
	bool aware = object_flavor_is_aware(obj);

	/* Hold the monster name */
	char m_name[80];
	char m_poss[80];

	/* Get the monster name*/
	monster_desc(m_name, sizeof(m_name), mon, MDESC_DEFAULT);

	/* Get the monster possessive ("his"/"her"/"its") */
	monster_desc(m_poss, sizeof(m_poss), mon, MDESC_PRO_VIS | MDESC_POSS);

	/* Do the effect, if any */
	if (obj->kind->thrown_effect) {
		used = effect_do(obj->kind->thrown_effect,
						 source_monster(mon->midx),
						 obj,
						 &ident,
						 aware,
						 DIR_NONE,
						 NULL);
	} else {
		used = false;
	}

	/* Monster is now dead, skip messages below*/
	if (!square_monster(cave, grid)) {
		*is_dead = true;
	}

	/* Inform them of the potion, mark it as known */
	if (ident && !aware) {
		char o_name[80];

		/* Identify it fully */
		object_flavor_aware(p, obj);

		/* Description */
		object_desc(o_name, sizeof(o_name), obj,
			ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM | (1 << 16),
			p);

		/* Describe the potion */
		msg("You threw %s.", o_name);

		/* Combine / Reorder the pack (later) */
		p->upkeep->notice |= (PN_COMBINE);

		/* Window stuff */
		p->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
	}

	/* Redraw if necessary*/
	if (used) p->upkeep->redraw |= (PR_HEALTH);

	/* Handle stuff */
	handle_stuff(p);

	return used;

}

/**
 * Give all adjacent, alert, non-mindless opponents (except one whose
 * coordinates are supplied) a free attack on the player.
 */
void attacks_of_opportunity(struct player *p, struct loc safe)
{
    int i;
    int start = randint0(8);
	int opportunity_attacks = 0;
	
    /* Look for adjacent monsters */
    for (i = start; i < 8 + start; i++) {
        struct loc grid = loc_sum(p->grid, ddgrid_ddd[i % 8]);
		struct monster *mon = square_monster(cave, grid);

        /* Check Bounds */
        if (!square_in_bounds(cave, grid)) continue;

        /* 'Point blank archery' avoids attacks of opportunity from the monster
		 * shot at */
        if (player_active_ability(p, "Point Blank Archery") &&
			loc_eq(safe, grid)) {
			continue;
        }

        /* If it is occupied by a monster */
        if (mon) {
            /* The monster must be alert, not confused, and not mindless */
            if ((mon->alertness >= ALERTNESS_ALERT) &&
				!mon->m_timed[MON_TMD_CONF] &&
				(mon->stance != STANCE_FLEEING) &&
				!rf_has(mon->race->flags, RF_MINDLESS) &&
				!mon->skip_next_turn && !mon->skip_this_turn) {
                opportunity_attacks++;

                if (opportunity_attacks == 1) {
                    msg("You provoke attacks of opportunity from adjacent enemies!");
                }
                make_attack_normal(mon, p);
            }
        }
    }

    return;
}

/**
 * Helper function used with ranged_helper by do_cmd_fire.
 */
static struct attack_result make_ranged_shot(struct player *p,
											 struct object *ammo,
											 struct monster *mon,
											 bool undo_rapid,
											 bool attack_penalty, bool one_shot)
{
	struct attack_result result = {0, 0, 0, false};
	struct object *bow = equipped_item_by_slot_name(p, "shooting");
	struct monster_race *race = mon->race;
	int attack_mod = p->state.skill_use[SKILL_ARCHERY] + ammo->att;
	int total_attack_mod, total_evasion_mod;
	int prt_percent;
	int slay_bonus_dice;
	int total_dd, total_ds;
	int dam, prt;
	int arrow_slay = 0, arrow_brand = 0, arrow_flag = 0;
	int bow_slay = 0, bow_brand = 0;
	char m_name[80];

	/* Remove the rapid fire penalty to attack if necessary */
	if (undo_rapid) {
		attack_mod += 3;
	}

	/* Determine the player's attack score after all modifiers */
	total_attack_mod = total_player_attack(p, mon, attack_mod);
	if (attack_penalty) {
		total_attack_mod = 0;
	}

	/* Determine the monster's evasion after all modifiers */
	total_evasion_mod = total_monster_evasion(p, mon, true);

	/* Did we hit it */
	result.hit = hit_roll(total_attack_mod, total_evasion_mod,
						  source_player(), source_monster(mon->midx), true);
	if (result.hit <= 0) {
		return result;
	}

	/* Handle sharpness (which can change 'hit' message) */
	prt_percent = prt_after_sharpness(p, ammo, &arrow_flag);
	if (percent_chance(100 - prt_percent)) {
		result.pierce = true;
	}

	/* Add 'critical hit' dice based on bow weight */
	result.crit_dice = crit_bonus(p, result.hit, bow->weight, race,
								  SKILL_ARCHERY, false);

	/* Add slay (or brand) dice based on both arrow and bow */
	slay_bonus_dice = slay_bonus(p, ammo, mon, &arrow_slay, &arrow_brand);
	slay_bonus_dice += slay_bonus(p, bow, mon, &bow_slay, &bow_brand);

	/* Bonus for flaming arrows */
	if (player_active_ability(p, "Flaming Arrows")) {
		struct monster_lore *lore = get_lore(race);

		/* Notice immunity */
		if (rf_has(race->flags, RF_RES_FIRE)) {
			if (monster_is_visible(mon)) {
				rf_on(lore->flags, RF_RES_FIRE);
			}
		} else {
			/* Otherwise, take the damage */
			slay_bonus_dice += 1;

			/* Extra bonus against vulnerable creatures */
			if (rf_has(race->flags, RF_HURT_FIRE)) {
				slay_bonus_dice += 1;

				/* Memorize the effects */
				rf_on(lore->flags, RF_RES_FIRE);

				/* Cause a temporary morale penalty */
				scare_onlooking_friends(mon, -20);
			}
		}
	}

	/* Calculate the damage done */
	total_dd = bow->dd + result.crit_dice + slay_bonus_dice;

	/* Note that this is recalculated in case the player has rapid shots but
	 * only one arrow */
	total_ds = MAX(total_ads(p, &p->state, bow, one_shot), 0);

	/* Calculate damage */
	dam = damroll(total_dd, total_ds);
	prt = damroll(race->pd, race->ps);
	prt = (prt * prt_percent) / 100;
	result.dmg = MAX(0, dam - prt);

	/* Monster description */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_DEFAULT);

	/* If a slay, brand or flag was noticed, then identify the weapon */
	if (bow_slay || bow_brand || arrow_slay || arrow_brand) {
		learn_brand_slay_from_launch(p, ammo, bow, mon);
	}
	if (arrow_flag) {
		char o_name[80];
		char desc[80];
		object_desc(o_name, sizeof(o_name), ammo, ODESC_BASE, p);
		if (flag_slay_message(arrow_flag, m_name, desc, sizeof(desc))) {
			msg("Your %s %s.", o_name, desc);
		}
		player_learn_flag(p, arrow_flag);
	}

	event_signal_combat_damage(EVENT_COMBAT_DAMAGE, total_dd, total_ds,
							   result.dmg, race->pd, race->ps, prt, prt_percent,
							   PROJ_HURT, false);
	return result;
}


/**
 * Helper function used with ranged_helper by do_cmd_throw.
 */
static struct attack_result make_ranged_throw(struct player *p,
											  struct object *obj,
											  struct monster *mon,
											  bool undo_rapid,
											  bool attack_penalty,
											  bool one_shot)
{
	struct attack_result result = {0, 0, 0, false};
	struct object *weapon = equipped_item_by_slot_name(p, "weapon");
	struct monster_race *race = mon->race;
	int attack_mod = p->state.skill_use[SKILL_MELEE] + obj->att;
	int total_attack_mod, total_evasion_mod;
	int prt_percent;
	int slay_bonus_dice;
	int total_dd, total_ds;
	int dam, prt;
	int slay = 0, brand = 0, flag = 0;

	/* Subtract the melee weapon's bonus (as we had already accounted for it) */
	if (weapon) {
		attack_mod -= weapon->att;
		attack_mod -= blade_bonus(p, weapon);
		attack_mod -= axe_bonus(p, weapon);
		attack_mod -= polearm_bonus(p, weapon);
	}

	/* Weapons that are not good for throwing are much less accurate */
	if (!of_has(obj->flags, OF_THROWING)) {
		attack_mod -= 5;
	}

	/* Give people their weapon affinity bonuses if the weapon is thrown */
	attack_mod += blade_bonus(p, obj);
	attack_mod += axe_bonus(p, obj);
	attack_mod += polearm_bonus(p, obj);

	/* Bonus for throwing proficiency ability */
	if (player_active_ability(p, "Throwing Mastery")) attack_mod += 5;

	/* Determine the player's attack score after all modifiers */
	total_attack_mod = total_player_attack(p, mon, attack_mod);
	if (attack_penalty) {
		total_attack_mod = 0;
	}

	/* Determine the monster's evasion after all modifiers */
	total_evasion_mod = total_monster_evasion(p, mon, false);

	/* Did we hit it */
	result.hit = hit_roll(total_attack_mod, total_evasion_mod,
						  source_player(), source_monster(mon->midx), true);
	if (result.hit <= 0) {
		return result;
	}

	/* Handle sharpness */
	prt_percent = prt_after_sharpness(p, obj, &flag);

	/* Add 'critical hit' dice based on bow weight */
	result.crit_dice = crit_bonus(p, result.hit, obj->weight, mon->race,
								  SKILL_MELEE, false);

	/* Add slay (or brand) dice based on both arrow and bow */
	slay_bonus_dice = slay_bonus(p, obj, mon, &slay, &brand);

	/* Calculate the damage done */
	total_dd = obj->dd + result.crit_dice + slay_bonus_dice;
	total_ds = MAX(total_mds(p, &p->state, obj, 0), 0);

	/* Penalise items that aren't made to be thrown */
	if (!of_has(obj->flags, OF_THROWING)) total_ds /= 2;

	/* Calculate damage */
	dam = damroll(total_dd, total_ds);
	prt = damroll(race->pd, race->ps);
	prt = (prt * prt_percent) / 100;
	result.dmg = MAX(0, dam - prt);

	/* If a slay, brand or flag was noticed, then identify the weapon */
	if (slay || brand) {
		learn_brand_slay_from_throw(p, obj, mon);
	}
	if (flag) {
		char m_name[80];
		char o_name[80];
		char desc[80];

		monster_desc(m_name, sizeof(m_name), mon, MDESC_DEFAULT);
		object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, p);
		if (flag_slay_message(flag, m_name, desc, sizeof(desc))) {
			msg("Your %s %s.", o_name, desc);
		}
		player_learn_flag(p, flag);
	}

	event_signal_combat_damage(EVENT_COMBAT_DAMAGE, total_dd, total_ds,
							   result.dmg, race->pd, race->ps, prt, prt_percent,
							   PROJ_HURT, false);
	return result;
}


/**
 * This is a helper function used by do_cmd_throw and do_cmd_fire.
 *
 * It abstracts out the projectile path, display code, identify and clean up
 * logic, while using the 'attack' parameter to do work particular to each
 * kind of attack.
 */
static void ranged_helper(struct player *p,	struct object *obj, int dir,
						  int range, int shots, bool archery, bool radiance)
{
	int i;
	ranged_attack attack;
	int path_n;
	struct loc path_g[256];

	/* Start at the player */
	struct loc grid = p->grid;

	/* Predict the "target" location */
	struct loc target = loc_sum(grid, loc(99 * ddx[dir], 99 * ddy[dir]));
	struct loc first = loc(0, 0);

	bool none_left = false;
	bool noticed_radiance = false;
	bool targets_remaining = false;
	bool rapid_fire = player_active_ability(p, "Rapid Fire");
	bool hit_body = false;
	bool is_potion;

	struct object *bow = equipped_item_by_slot_name(p, "shooting");
	struct object *missile;
	int shot;
	const struct artifact *crown = lookup_artifact_name("of Morgoth");

	/* Check for target validity */
	if ((dir == DIR_TARGET) && target_okay(range)) {
		target_get(&target);
	}

	/* Handle player fear */
	if (p->timed[TMD_AFRAID]) {
		/* Message */
		msg("You are too afraid to aim properly!");
		
		/* Done */
		return;
	}

	/* Sound */
	sound(MSG_SHOOT);

	/* Set the attack type and other specifics */
	if (archery) {
		attack = make_ranged_shot;
		if (rapid_fire && obj->number > 1) {
			shots = 2;
		}
	} else {
		attack = make_ranged_throw;
	}
	/*
	 * Remember if the missile is a potion (need that after the missile
	 * may have been destroyed).
	 */
	is_potion = tval_is_potion(obj);

	/* Actually "fire" the object */
	p->upkeep->energy_use = z_info->move_energy;

	/* Store the action type */
	p->previous_action[0] = ACTION_MISC;

	/* Calculate the path */
	path_n = project_path(cave, path_g, range, grid, &target, 0);

	/* Hack -- Handle stuff */
	handle_stuff(p);

	/* If the bow has 'radiance', then light the starting square */
	noticed_radiance = radiance && do_radiance(p, grid);

	for (shot = 0; shot < shots; shot++) {
		bool hit_wall = false;
		bool ghost_arrow = false;
		int missed_monsters = 0;
		struct loc final_grid = (path_n > 0) ?
			path_g[path_n - 1] : p->grid;

		/* Abort any later shot(s) if there is no target on the trajectory */
		if ((shot > 0) && !targets_remaining) break;
		targets_remaining = false;

		/* Project along the path */
		for (i = 0; i < path_n; ++i) {
			struct monster *mon = NULL;
			bool see = square_isseen(cave, path_g[i]);

			/* Stop before hitting walls */
			if (!square_isprojectable(cave, path_g[i])) {
				/* If the arrow hasn't already stopped, do some things... */
				if (!ghost_arrow) {
					hit_wall = true;
					final_grid = grid;

					/* Only do visuals if the player can "see" the missile */
					if (panel_contains(grid.y, grid.x)) {
						bool sees[1] = { square_isview(cave, grid) };
						int dist[1] = { 0 };
						struct loc blast_grid[1] = { grid };
						event_signal_blast(EVENT_EXPLOSION, PROJ_ARROW, 1, dist,
										   true, sees, blast_grid, grid);
					}
				}
				break;
			}

			/* Advance */
			grid = path_g[i];

			/* Check for monster */
			mon = square_monster(cave, grid);

			/* After an arrow has stopped, keep looking along the path, but
			 * don't attempt to hit creatures, or display graphics etc */
			if (ghost_arrow) {
				if (mon && (!OPT(p, forgo_attacking_unwary) ||
							(mon->alertness >= ALERTNESS_ALERT))) {
					targets_remaining = true;
				}
				continue;
			}

			/* If the bow has 'radiance', light the square being passed over */
			noticed_radiance = radiance && do_radiance(p, grid);

			/* Tell the UI to display the missile */
			event_signal_missile(EVENT_MISSILE, obj, see, grid.y, grid.x);

			/* Try the attack on the monster if any */
			if (mon) {
				bool potion_effect = false;
				bool attack_penalty = false;
				int visible = monster_is_visible(mon);
				const char *note_dies = monster_is_nonliving(mon) ? 
					" is destroyed." : " dies.";
				struct attack_result result;
				int pdam = 0;

                /* Record the grid of the first monster in line of fire */
				first = grid;

				/* Monsters might notice */
				p->attacked = true;

				/* Modifications for shots that go past the target or strike
				 * things before the target... */
				if ((dir == DIR_TARGET) && target_okay(range)) {
					/* If there is a specific target and this is not it, then
					 * massively penalise */
					if (!loc_eq(grid, target)) {
						attack_penalty = true;
					}
				} else if (missed_monsters > 0) {
					/* If it is just a shot in a direction and has already
					 * missed something, then massively penalise */
					attack_penalty = true;
				} else {
					/* If it is a shot in a direction and this is the first
					 * monster */
					if (monster_is_visible(mon)) {
						monster_race_track(p->upkeep, mon->race);
						health_track(p->upkeep, mon);
						target_set_monster(mon);
					}
				}

				/* Perform the attack */
				result = attack(p, obj, mon, rapid_fire, attack_penalty,
								shots == 1);
				if (result.hit > 0) {
					char o_name[80];
					bool fatal_blow = false;

					/* Note the collision */
					hit_body = true;

					/* Mark the monster as attacked by the player */
					mflag_on(mon->mflag, MFLAG_HIT_BY_RANGED);

					/* Describe the object (have up-to-date knowledge now) */
					object_desc(o_name, sizeof(o_name), obj,
								ODESC_FULL | ODESC_SINGULAR, p);

					if (!visible) {
						/* Invisible monster */
						msgt(MSG_SHOOT_HIT, "The %s finds a mark.", o_name);
					} else {
						char m_name[80];
						char punct[20];

						/* Determine the punctuation for the attack
						 * ("...", ".", "!" etc) */
						attack_punctuation(punct, sizeof(punct), result.dmg,
										   result.crit_dice);

						monster_desc(m_name, sizeof(m_name), mon, MDESC_OBJE);

						if (result.pierce) {
							msgt(MSG_SHOOT_HIT, "The %s pierces %s%s", o_name,
								 m_name, punct);
						} else {
							msgt(MSG_SHOOT_HIT, "The %s hits %s%s", o_name,
								 m_name, punct);
						}
					}

					/* Special effects sometimes reveal the kind of potion*/
					if (is_potion) {
						/* Record monster hit points*/
						pdam = mon->hp;

						msg("The bottle breaks.");
					
						/* Returns true if damage has already been handled */
						potion_effect = thrown_potion_effects(
							p, obj, &fatal_blow, mon);

						/* Check the change in monster hp*/
						pdam -= mon->hp;

						/* Monster could have been healed*/
						if (pdam < 0) pdam = 0;
					}

					/* Hit the monster, unless there's a potion effect */
					if (!potion_effect) {
						fatal_blow = mon_take_hit(mon, p, result.dmg,
												  note_dies);

						event_signal_hit(EVENT_HIT, result.dmg, PROJ_HURT,
										 fatal_blow, grid);

						/* If this was the killing shot */
						if (fatal_blow) {
							/* Gain wrath if singing song of slaying */
							if (player_is_singing(p, lookup_song("Slaying"))) {
								p->wrath += 100;
								p->upkeep->update |= PU_BONUS;
								p->upkeep->redraw |= PR_SONG;
							}
						}
					}

					if (!fatal_blow) {
						/* If it is still alive, then there is at least
						 * one target left on the trajectory*/
						targets_remaining = true;

						/* Alert the monster, even if no damage was done
						 * (if damage was done, then it was alerted by
						 * mon_take_hit() ) */
						if (result.dmg == 0) {
							make_alert(mon, 0);
						}
						
						/* Morgoth drops his iron crown if he is hit for 10 or
						 * more net damage twice */
						if (rf_has(mon->race->flags, RF_QUESTOR) &&
							!is_artifact_created(crown)) {
							if (result.dmg >= 10) {
								if (p->morgoth_hits == 0) {
									msg("The force of your %s knocks the Iron Crown off balance.", archery ? "shot" : "blow");
									p->morgoth_hits++;
								} else if (player->morgoth_hits == 1) {
									drop_iron_crown(mon, "You knock his crown from off his brow, and it falls to the ground nearby.");
									p->morgoth_hits++;
								}
							}
						}

						/* Message if applicable */
						if ((!potion_effect || (pdam > 0)) && !monster_is_visible(mon)) {
							message_pain(mon, pdam ? pdam : result.dmg);
						}

						/* Deal with crippling shot ability */
						if (archery
							&& player_active_ability(p, "Crippling Shot")
							&& (result.crit_dice >= 1) && (result.dmg > 0)
							&& !rf_has(mon->race->flags, RF_RES_CRIT)) {
							if (skill_check(source_player(),
											result.crit_dice * 4,
											monster_skill(mon, SKILL_WILL),
											source_monster(mon->midx)) > 0) {
								msg("Your shot cripples %^s!", mon);
								
								/* Slow the monster - the +1 is needed as a
								 * turn of this wears off immediately */
								mon_inc_timed(mon, result.crit_dice + 1,
											  MON_TMD_SLOW, false);
							}							
						}
					}
					/* Stop looking if a monster was hit but not pierced */
					if (!result.pierce) {
						/* Continue checking trajectory, but without effect */
						ghost_arrow = true;

						/* Record resting place of arrow */
						final_grid = grid;
					}
				} else {
					/* There is at least one target left on the trajectory */
					targets_remaining = true;
				}

				/* We have missed a target, but could still hit something
				 * (with a penalty) */
				missed_monsters++;
			}
		}

		if (bow && !of_has(bow->known->flags, OF_RADIANCE) && noticed_radiance){
			char o_full_name[80];
			char o_short_name[80];
			object_desc(o_short_name, sizeof(o_short_name), obj, ODESC_BASE, p);
			player_learn_flag(p, OF_RADIANCE);
			object_desc(o_full_name, sizeof(o_full_name), obj,
				ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
				(1 << 16), p);
			msg("The arrow leaves behind a trail of light!");
			msg("You recognize your %s to be %s", o_short_name, o_full_name);
		}

		/* Break the truce if creatures see */
		break_truce(p, false);

		/* Get the missile */
		if (object_is_carried(p, obj)) {
			missile = gear_object_for_use(p, obj, 1, true, &none_left);
		} else {
			missile = floor_object_for_use(p, obj, 1, true, &none_left);
		}

		/* Set to auto-pickup */
		missile->notice |= OBJ_NOTICE_PICKUP;

		/* Drop (or break) near that location */
		drop_near(cave, &missile, breakage_chance(missile, hit_wall),
				  final_grid, true, false);
	}

	/* Need to print this message even if the potion missed */
	if (!hit_body && is_potion) {
		msg("The bottle breaks.");
	}

	/* Have to set this here as well, just in case... */
	p->attacked = true;

    /* Provoke attacks of opportunity */
	if (archery) {
		if (player_active_ability(p, "Point Blank Archery")) {
			attacks_of_opportunity(p, first);
		} else {
			attacks_of_opportunity(p, loc(0, 0));
		}
	}
}


/**
 * Fire an object from the quiver, pack or floor at a target.
 */
void do_cmd_fire(struct command *cmd) {
	int dir, range;
	int shots = 1;

	struct object *bow = equipped_item_by_slot_name(player, "shooting");
	struct object *obj;
	bool radiance;

	/* Require a usable launcher */
	if (!bow || !player->state.ammo_tval) {
		msg("You have nothing to fire with.");
		return;
	}

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Fire which ammunition?",
			/* Error  */ "You have no suitable ammunition to fire.",
			/* Filter */ obj_can_fire,
			/* Choice */ USE_EQUIP)
		!= CMD_OK)
		return;

	/* Check the item being fired is usable by the player. */
	if (!item_is_available(obj)) {
		msg("That item is not within your reach.");
		return;
	}

	/* Check the ammo can be used with the launcher */
	if (obj->tval != player->state.ammo_tval) {
		msg("That ammo cannot be fired by your current weapon.");
		return;
	}

	range = archery_range(bow);
	if (cmd_get_target(cmd, "target", &dir, range, false) == CMD_OK) {
		player_confuse_dir(player, &dir, false);
		if (player->timed[TMD_AFRAID]) {
			msgt(MSG_AFRAID, "You are too afraid to aim properly!");
			return;
		}
	} else {
		return;
	}

	/* Determine if the bow has 'radiance' */
	radiance = of_has(bow->flags, OF_RADIANCE);

	ranged_helper(player, obj, dir, range, shots, true, radiance);
}


/**
 * Throw an object from the quiver, pack, floor, or, in limited circumstances,
 * the equipment.
 */
void do_cmd_throw(struct command *cmd) {
	int dir;
	int shots = 1;
	int range;
	struct object *obj;

	/*
	 * Get arguments.  Never default to showing the equipment as the first
	 * list (since throwing the equipped weapon leaves that slot empty will
	 * have to choose another source anyways).
	 */
	if (player->upkeep->command_wrk == USE_EQUIP)
		player->upkeep->command_wrk = USE_INVEN;
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Throw which item?",
			/* Error  */ "You have nothing to throw.",
			/* Filter */ obj_can_throw,
			/* Choice */ USE_EQUIP | USE_QUIVER | USE_INVEN | USE_FLOOR | SHOW_THROWING)
		!= CMD_OK)
		return;

	range = throwing_range(obj);

	if (cmd_get_target(cmd, "target", &dir, range, false) == CMD_OK) {
		player_confuse_dir(player, &dir, false);
		if (player->timed[TMD_AFRAID]) {
			msgt(MSG_AFRAID, "You are too afraid to aim properly!");
			return;
		}
	} else {
		return;
	}

	if (object_is_equipped(player->body, obj)) {
		assert(obj_can_takeoff(obj) && tval_is_melee_weapon(obj));
		if (handle_stickied_removal(player, obj)) {
			return;
		}
		inven_takeoff(obj);
	}

	ranged_helper(player, obj, dir, range, shots, false, false);
}

/**
 * Front-end command which fires from the first quiver.
 */
void do_cmd_fire_quiver1(void) {
	struct object *bow = equipped_item_by_slot_name(player, "shooting");
	struct object *ammo = equipped_item_by_slot_name(player, "first quiver");

	/* Require a usable launcher */
	if (!bow || !player->state.ammo_tval) {
		msg("You have nothing to fire with.");
		return;
	}

	/* Require usable ammo */
	if (!ammo) {
		msg("You have no ammunition in the first quiver to fire.");
		return;
	}
	if (ammo->tval != player->state.ammo_tval) {
		msg("The ammunition in the first quiver is not compatible with your launcher.");
		return;
	}

	/* Fire! */
	cmdq_push(CMD_FIRE);
	cmd_set_arg_item(cmdq_peek(), "item", ammo);
}

/**
 * Front-end command which fires from the second quiver.
 */
void do_cmd_fire_quiver2(void) {
	struct object *bow = equipped_item_by_slot_name(player, "shooting");
	struct object *ammo = equipped_item_by_slot_name(player, "second quiver");

	/* Require a usable launcher */
	if (!bow || !player->state.ammo_tval) {
		msg("You have nothing to fire with.");
		return;
	}

	/* Require usable ammo */
	if (!ammo) {
		msg("You have no ammunition in the second quiver to fire.");
		return;
	}
	if (ammo->tval != player->state.ammo_tval) {
		msg("The ammunition in the second quiver is not compatible with your launcher.");
		return;
	}

	/* Fire! */
	cmdq_push(CMD_FIRE);
	cmd_set_arg_item(cmdq_peek(), "item", ammo);
}

/**
 * Front-end command which fires at the nearest target with default ammo.
 */
void do_cmd_fire_at_nearest(void) {
	int dir = DIR_TARGET;
	struct object *ammo = NULL;
	struct object *bow = equipped_item_by_slot_name(player, "shooting");
	struct object *ammo1 = equipped_item_by_slot_name(player, "first quiver");
	struct object *ammo2 = equipped_item_by_slot_name(player, "second quiver");

	/* Require a usable launcher */
	if (!bow || !player->state.ammo_tval) {
		msg("You have nothing to fire with.");
		return;
	}

	/* Find first eligible ammo in the quiver */
	if (ammo1) {
		ammo = ammo1;
	} else if (ammo2) {
		ammo = ammo2;
	}

	/* Require usable ammo */
	if (!ammo) {
		msg("You have no ammunition in the quiver to fire.");
		return;
	}

	/* Require foe */
	if (!target_set_closest((TARGET_KILL | TARGET_QUIET), NULL)) return;

	/* Fire! */
	cmdq_push(CMD_FIRE);
	cmd_set_arg_item(cmdq_peek(), "item", ammo);
	cmd_set_arg_target(cmdq_peek(), "target", dir);
}

/**
 * Front-end command for "automatic" throwing
 *
 * Throws the first item in the inventory that is designed for throwing at the
 * current target, if set and in range, or the nearest monster that is in
 * range.
 */
void do_cmd_automatic_throw(void) {
	struct object *thrown;
	int nthrow = scan_items(&thrown, 1, player, USE_INVEN, obj_is_throwing);
	int range;

	if (nthrow <= 0) {
		msg("You don't have anything designed for throwing in your inventory.");
		return;
	}

	range = throwing_range(thrown);
	assert(range > 0);
	if (!target_okay(range)) {
		/*
		 * Get the nearest monster in range.  Could use
		 * target_set_closest(), but that would have the drawback of
		 * clearing the current target if there is nothing in range.
		 */
		struct point_set *targets = target_get_monsters(
			TARGET_KILL | TARGET_QUIET, NULL, false);
		struct monster *target = NULL;
		int target_range = range + 1;
		int ntgt = point_set_size(targets), i = 0;

		while (1) {
			if (i >= ntgt) {
				point_set_dispose(targets);
				if (!target) {
					msg("No clear target for automatic throwing.");
					return;
				}
				target_set_monster(target);
				health_track(player->upkeep, target);
				break;
			}
			if (distance(player->grid, targets->pts[i])
					< target_range) {
				target = square_monster(cave, targets->pts[i]);
				assert(target);
			}
			++i;
		}
	}

	/* Throw! */
	cmdq_push(CMD_THROW);
	cmd_set_arg_item(cmdq_peek(), "item", thrown);
	cmd_set_arg_target(cmdq_peek(), "target", DIR_TARGET);
}
