/**
 * \file combat.c
 * \brief All forms of combat
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
#include "mon-calcs.h"
#include "mon-lore.h"
#include "mon-move.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-gear.h"
#include "obj-tval.h"
#include "object.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "songs.h"
#include "trap.h"

/**
 * Knock monster or player backwards
 */
bool knock_back(struct loc grid1, struct loc grid2)
{
    bool knocked = false;
    int mod, d, i;

    /* Default knocking back a monster */
    struct monster *mon = square_monster(cave, grid2);
    
	/* The location to get knocked to */
    struct loc grid3;
    
    /* Determine the main direction from the source to the target */
    int dir = rough_direction(grid1, grid2);
    
    /* Extract the next grid in the direction */
    struct loc next = loc_sum(grid2, ddgrid[dir]);
    
    /* First try to knock it straight back */
    if (!square_iswall(cave, next) && (square_monster(cave, next) == NULL)) {
        grid3 = next;
        knocked = true;
    } else {
		/* Then try the adjacent directions */
        /* Randomize clockwise or anticlockwise */
        mod = one_in_(2) ? -1 : +1;

        /* Try both directions */
        for (i = 0; i < 2; i++) {
            d = cycle[chome[dir_from_delta(ddy[dir], ddx[dir])] + mod];
            grid3 = loc_sum(grid2, ddgrid[d]);
            if (!square_iswall(cave, grid3) &&
				(square_monster(cave, next) == NULL)) {
                knocked = true;
                break;
            }
            
            /* switch direction */
            mod *= -1;
        }
    }

    /* Make the target skip a turn */
    if (knocked) {
        if (mon) {
            mon->skip_next_turn = true;
            
            /* Actually move the monster */
            monster_swap(grid2, grid3);
        } else {
            msg("You are knocked back.");

            player->upkeep->knocked_back = true;

            /* Actually move the player */
            monster_swap(grid2, grid3);

            /* Cannot stay in the air */
            player->upkeep->leaping = false;

            /* Make some noise when landing */
			player->stealth_score -= 5;

            /* Set off traps */
			if (square_issecrettrap(cave, grid3) ||
				square_isdisarmabletrap(cave, grid3)) {
				disturb(player, false);
				square_reveal_trap(cave, grid3, true);
				hit_trap(grid3);
			} else if (square_ischasm(cave, grid3)) {
				player_fall_in_chasm(player);
			}
        }
    }

    return knocked;
}

/**
 * Determine the result of a skill check.
 * (1d10 + skill) - (1d10 + difficulty)
 * Results <= 0 count as fails.
 * Results > 0 are successes.
 *
 * There is a fake skill check in monsters_hear (where player roll is
 * used once for all monsters) so if something changes here, remember to change
 * it there.
 */
int skill_check(struct source attacker, int skill, int difficulty,
				struct source defender)
{
	int skill_total;
	int difficulty_total;

	/* Bonuses against your enemy of choice */
	if ((attacker.what == SRC_PLAYER) && (defender.what == SRC_MONSTER)) {
		struct monster *mon = cave_monster(cave, defender.which.monster);
		skill += player_bane_bonus(player, mon);
	}
	if ((defender.what == SRC_PLAYER) && (attacker.what == SRC_MONSTER)) {
		struct monster *mon = cave_monster(cave, attacker.which.monster);
		difficulty += player_bane_bonus(player, mon);
    }

    /* Elf-bane bonus against you */
	if ((attacker.what == SRC_PLAYER) && (defender.what == SRC_MONSTER)) {
		struct monster *mon = cave_monster(cave, defender.which.monster);
		difficulty += monster_elf_bane_bonus(mon, player);
	}
	if ((defender.what == SRC_PLAYER) && (attacker.what == SRC_MONSTER)) {
		struct monster *mon = cave_monster(cave, attacker.which.monster);
		skill += monster_elf_bane_bonus(mon, player);
    }

	/* The basic rolls */
	skill_total = randint1(10) + skill;
	difficulty_total = randint1(10) + difficulty;

	/* Alternate rolls for dealing with the player curse */
	if (player->cursed) { 
		int alt_total;

		if (attacker.what == SRC_PLAYER) {
			alt_total = randint1(10) + skill;
			skill_total = MIN(skill_total, alt_total);
		}
		if (defender.what == SRC_PLAYER) {
			alt_total = randint1(10) + difficulty;
			difficulty_total = MIN(difficulty_total, alt_total);
		}
	}

	/* Debugging message */
	if (OPT(player, cheat_skill_rolls)) {
		msg("{%d+%d v %d+%d = %d}.", skill_total - skill, skill, 
			difficulty_total - difficulty, difficulty,
			skill_total - difficulty_total);
	}

	return skill_total - difficulty_total;
}

/**
 * Determine the result of an attempt to hit an opponent.
 * Results <= 0 count as misses.
 * Results > 0 are hits and, if high enough, are criticals.
 *
 * The monster is the creature doing the attacking. 
 * This is used in displaying the attack roll details.
 * attacker_vis is whether the attacker is visible.
 * this is used in displaying the attack roll details.
 */
int hit_roll(int att, int evn, struct source attacker, struct source defender,
			 bool display_roll)
{
	int attack_score, attack_score_alt;
	int evasion_score, evasion_score_alt;
	bool non_player_visible;
	
	/* Determine the visibility for the combat roll window */
	if (attacker.what == SRC_PLAYER) {
		if ((defender.what == SRC_NONE) || (defender.what == SRC_TRAP)) {
			non_player_visible = true;
		} else {
			struct monster *mon = cave_monster(cave, defender.which.monster);
			assert(mon);
			non_player_visible = monster_is_visible(mon);
		}
	} else {
		if ((attacker.what == SRC_NONE) || (attacker.what == SRC_TRAP)) {
			non_player_visible = true;
		} else {
			struct monster *mon = cave_monster(cave, attacker.which.monster);
			assert(mon);
			non_player_visible = monster_is_visible(mon);
		}
	}

	/* Roll the dice... */
	attack_score = randint1(20) + att;
	attack_score_alt = randint1(20) + att;
	evasion_score = randint1(20) + evn;
	evasion_score_alt = randint1(20) + evn;

	/* Take the worst of two rolls for cursed players */
	if (player && player->cursed) {
		if (attacker.what == SRC_PLAYER) {
			attack_score = MIN(attack_score, attack_score_alt);
		} else {
			evasion_score = MIN(evasion_score, evasion_score_alt);
		}
	}
	
	/* Set the information for the combat roll window */
	if (display_roll) {
		event_signal_combat_attack(EVENT_COMBAT_ATTACK, attacker, defender,
								   non_player_visible, att, attack_score - att,
								   evn, evasion_score - evn, true);
	}
	
	return (attack_score - evasion_score);
}

/**
 * Determines the bonus for the ability 'concentration' and updates some
 * related variables.
 */

static int concentration_bonus(struct player *p, struct loc grid)
{
	int bonus = 0;
	int midx = square_monster(cave, grid) ? square_monster(cave, grid)->midx :0;
	
	/* Deal with 'concentration' ability */
	if (player_active_ability(p, "Concentration") &&
		(p->last_attack_m_idx == midx)) {
		bonus = MIN(p->consecutive_attacks,
					p->state.skill_use[SKILL_PERCEPTION] / 2);
	}

	/* If the player is not engaged with this monster, reset the attack count
	 * and monster */
	if (p->last_attack_m_idx != midx) {
		p->consecutive_attacks = 0;
		p->last_attack_m_idx = midx;
	}

	return bonus;
}

/**
 * Determines the bonus for the ability 'focused attack'.
 */
static int focused_attack_bonus(struct player *p)
{
	/* Focused attack */
	if (p->focused) {
		p->focused = false;
		
		if (player_active_ability(p, "Focused Attack")) {
			return (p->state.skill_use[SKILL_PERCEPTION] / 2);
		}
	}

	return 0;
}


/**
 * Determines the bonus for the ability 'master hunter'.
 */
static int master_hunter_bonus(struct player *p, struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);

	/* Master hunter bonus */
	if (player_active_ability(p, "Master Hunter")) {
		return MIN(lore->pkills, p->state.skill_use[SKILL_PERCEPTION] / 4);
	}
	return 0;
}


/**
 * Determines the player's attack based on all the relevant attributes and
 * modifiers.
 */
int total_player_attack(struct player *p, struct monster *mon, int base)
{
	int att = base;

	/* Reward concentration ability (if applicable) */
	att += concentration_bonus(p, mon->grid);

	/* Reward focused attack ability (if applicable) */
	att += focused_attack_bonus(p);
	
	/* reward bane ability (if applicable) */
	att += player_bane_bonus(p, mon);

	/* Reward master hunter ability (if applicable) */
	att += master_hunter_bonus(p, mon);
	
	/* Penalise distance -- note that this penalty will equal 0 in melee */
	att -= distance(p->grid, mon->grid) / 5;
		
	/* Halve attack score for certain situations (and only halve positive
	 * scores!) */
	if (att > 0) {
		/* Penalise the player if (s)he can't see the monster */
		if (!monster_is_visible(mon)) att /= 2;
		
		/* Penalise the player if (s)he is in a pit or web */
		if (square_ispit(cave, p->grid) || square_iswebbed(cave, p->grid)) {
			att /= 2;
		}
	}
	
	return att;
}

/**
 * Determines the player's evasion based on all the relevant attributes and
 * modifiers.
 */
int total_player_evasion(struct player *p, struct monster *mon, bool archery)
{ 
	int evn = p->state.skill_use[SKILL_EVASION];
	
	/* Reward successful use of the dodging ability  */
	evn += player_dodging_bonus(p);
	
	/* Reward successful use of the bane ability */
	evn += player_bane_bonus(p, mon);

	/* Halve evasion for certain situations (and only positive evasion!) */
	if (evn > 0) {
		/* Penalise the player if (s)he can't see the monster */
		if (!monster_is_visible(mon)) {
			evn /= 2;
		}

		/* Penalise targets of archery attacks */
		if (archery) {
			evn /= 2;
		}

		/* Penalise the player if (s)he is in a pit or web */
		if (square_ispit(cave, p->grid) || square_iswebbed(cave, p->grid)) {
			evn /= 2;
		}
	}

	return evn;
}

/**
 * Light hating monsters get a penalty to hit/evn if the player's
 * square is too bright.
 */
static int light_penalty(const struct monster *mon)
{
	int penalty = 0;

	if (rf_has(mon->race->flags, RF_HURT_LIGHT)) {
		penalty = square_light(cave, mon->grid) - 2;
		if (penalty < 0) penalty = 0;
	}

	return penalty;
}

/**
 * Determines a monster's attack score based on all the relevant attributes
 * and modifiers.
 */

int total_monster_attack(struct player *p, struct monster *mon, int base)
{
	int att = base;

	/* Penalise stunning  */
	if (mon->m_timed[MON_TMD_STUN]) {
		att -= 2;
	}

	/* Penalise being in bright light for light-averse monsters */
	att -= light_penalty(mon);

	/* Reward surrounding the player */
	att += overwhelming_att_mod(p, mon);

	/* Penalise distance */
	att -= distance(p->grid, mon->grid) / 5;

    /* Elf-bane bonus */
    att += monster_elf_bane_bonus(mon, p);
	
	/* Halve attack score for certain situations (and only positive scores!) */
	if (att > 0) {
		/* Penalise monsters who can't see the player */
		if ((mon->race->light > 0) && strchr("@G", mon->race->d_char) &&
			(square_light(cave, p->grid) <= 0)) {
			att /= 2;
		}
	}

	return att;
}


/**
 * Determines a monster's evasion based on all the relevant attributes and
 * modifiers.
 */
int total_monster_evasion(struct player *p, struct monster *mon, bool archery)
{
	struct monster_race *race = mon->race;
	int evn = race->evn;
	bool unseen = false;
	
	/* All sleeping monsters have -5 total evasion */
	if (mon->alertness < ALERTNESS_UNWARY) return -5;
	
	/* Penalise stunning */
	if (mon->m_timed[MON_TMD_STUN]) {
		evn -= 2;
	}
	
	/* Penalise being in bright light for light-averse monsters */
	evn -= light_penalty(mon);
	
    /* Elf-bane bonus */
    evn += monster_elf_bane_bonus(mon, p);
	
    /* Halve evasion for certain situations (and only halve positive evasion!)*/
	if (evn > 0) {
		/* Check if player is unseen */
		if ((race->light > 0) && strchr("@G", race->d_char) &&
			(square_light(cave, p->grid) <= 0)) {
			unseen = true;
		}
		
		/* Penalise unwary monsters, or those who can't see the player */
		if (unseen || (mon->alertness < ALERTNESS_ALERT)) {
			evn /= 2;
		}
		
		/* Penalise targets of archery attacks */
		if (archery) {
			evn /= 2;
		}
	}
	
	return evn;
}

/**
 * Monsters are already given a large set penalty for being asleep
 * (total evasion mod of -5) and unwary (evasion score / 2),
 * but we also give a bonus for high stealth characters who have ASSASSINATION.
 */

int stealth_melee_bonus(const struct monster *mon)
{
	int stealth_bonus = 0;
		
	if (player_active_ability(player, "Assassination")) {
		if ((mon->alertness < ALERTNESS_ALERT) && monster_is_visible(mon) &&
			!player->timed[TMD_CONFUSED]) {
			stealth_bonus = player->state.skill_use[SKILL_STEALTH];
		}
	}
	return stealth_bonus;
}

/**
 * Give a bonus to attack the player depending on the number of adjacent
 * monsters.
 * This is +1 for monsters near the attacker or to the sides,
 * and +2 for monsters in the three positions behind the player:
 * 
 * 1M1  M11
 * 1@1  1@2
 * 222  122
 *
 * We should lessen this with the crowd fighting ability
 */
int overwhelming_att_mod(struct player *p, struct monster *mon)
{
	int mod = 0;
    int dir;
	int dy, dx;
	int py = p->grid.y;
	int px = p->grid.x;
	
    /* Determine the main direction from the player to the monster */
    dir = rough_direction(p->grid, mon->grid);

    /* Extract the deltas from the direction */
    dy = ddy[dir];
    dx = ddx[dir];
	
	/* If monster in an orthogonal direction   753 */
	/*                                         8@M */
	/*                                         642 */
	if (dy * dx == 0) {
		/* Increase modifier for monsters engaged with the player... */
		if (square_monster(cave, loc(px - dy + dx, py + dx + dy))) mod++;/* 2 */
		if (square_monster(cave, loc(px + dy + dx, py - dx + dy))) mod++;/* 3 */
		if (square_monster(cave, loc(px - dy     , py + dx     ))) mod++;/* 4 */
		if (square_monster(cave, loc(px + dy     , py - dx     ))) mod++;/* 5 */
		
		/* ...especially if they are behind the player */
		if (square_monster(cave, loc(px - dy - dx, py + dx - dy))) mod++;/* 6 */
		if (square_monster(cave, loc(px + dy - dx, py - dx - dy))) mod++;/* 7 */
		if (square_monster(cave, loc(px      - dx, py      - dy))) mod++;/* 8 */
	} else {
		/* If monster in a diagonal direction   875 */
		/*                                      6@3 */
		/*                                      42M */
		/* Increase modifier for monsters engaged with the player... */
		if (square_monster(cave, loc(px     , py + dy))) mod++; /* 2 */
		if (square_monster(cave, loc(px + dx, py     ))) mod++; /* 3 */
		if (square_monster(cave, loc(px - dy, py + dx))) mod++; /* 4 */
		if (square_monster(cave, loc(px + dy, py - dx))) mod++; /* 5 */
		
		/* ...especially if they are behind the player */
		if (square_monster(cave, loc(px     , py - dy))) mod++; /* 6 */
		if (square_monster(cave, loc(px - dx, py     ))) mod++; /* 7 */
		if (square_monster(cave, loc(px - dx, py - dy))) mod++; /* 8 */
	}
	
	/* Adjust for crowd fighting ability */
	if (player_active_ability(p, "Crowd Fighting")) {
		mod /= 2;
	}
	
	return (mod);
}



/**
 * Determines the number of bonus dice from a (potentially) critical hit
 *
 * bonus of 1 die for every (6 + weight_in_pounds) over what is needed.
 * (using rounding at 0.5 instead of always rounding up)
 *
 * Thus for a Dagger (0.8lb):         7, 14, 20, 27...  (6+weight)
 *            Short Sword (1.5lb):    8, 15, 23, 30...
 *            Long Sword (3lb):       9, 18, 27, 35...
 *            Bastard Sword (4lb):   10, 20, 30, 40...
 *            Great Sword (7lb):     13, 26, 39, 52...
 *            Shortbow (2lb):         8, 16, 24, 32...
 *            Longbow (3lb):          9, 18, 27, 36...
 *            m 1dX (2lb):            8, 16, 24, 32...
 *            m 2dX (4lb):           10, 20, 30, 40...
 *            m 3dX (6lb):           12, 24, 36, 48...
 *
 * (old versions)
 * Thus for a Dagger (0.8lb):         9, 13, 17, 21...  5 then (3+weight)
 *            Short Sword (1.5lb):   10, 14, 19, 23...
 *            Long Sword (3lb):      11, 17, 23, 29...
 *            Bastard Sword (4lb):   12, 19, 26, 33...
 *            Great Sword (7lb):     15, 25, 35, 45...
 *            Shortbow (2lb):        10, 15, 20, 25...
 *            Longbow (3lb):         11, 17, 23, 29...
 *            m 1dX (2lb):           10, 15, 20, 25...
 *            m 2dX (4lb):           12, 19, 26, 33...
 *            m 3dX (6lb):           14, 23, 32, 41...
 * Thus for a Dagger (0.8lb):        11, 12, 13, 14...  (10 then weightx)
 *            Short Sword (1.5lb):   12, 13, 15, 16...
 *            Long Sword (3lb):      13, 16, 19, 22...
 *            Bastard Sword (4lb):   14, 18, 22, 26...
 *            Great Sword (7lb):     17, 24, 31, 38...
 *            Shortbow (2lb):        12, 14, 16, 18...
 *            Longbow (3lb):         13, 16, 19, 22...
 * Thus for a Dagger (0.8lb):         6, 12, 18, 24...  (5+weight)
 *            Short Sword (1.5lb):    7, 13, 20, 26...
 *            Long Sword (3lb):       8, 16, 24, 32...
 *            Bastard Sword (4lb):    9, 18, 27, 36...
 *            Great Sword (7lb):     12, 24, 36, 48...
 *            Shortbow (2lb):         7, 14, 21, 28...
 *            Longbow (3lb):          8, 16, 24, 32...
 * Thus for a Dagger (0.8lb):         4,  8, 12, 16...  (3+weight)
 *            Short Sword (1.5lb):    5,  9, 14, 18...
 *            Long Sword (3lb):       6, 12, 18, 25...
 *            Bastard Sword (4lb):    7, 14, 21, 28...
 *            Great Sword (7lb):     10, 20, 30, 40...
 *            Shortbow (2lb):         5, 10, 15, 20...
 *            Longbow (3lb):          6, 12, 18, 24...
 * Thus for a Dagger (0.8lb):         8, 12, 15, 18...  (old1)
 *            Short Sword (1.5lb):    9, 14, 18, 23...
 *            Long Sword (3lb):      11, 17, 23, 29...
 *            Bastard Sword (3.5lb): 11, 18, 24, 31...
 *            Great Sword (7lb):     15, 25, 35, 45...
 * Thus for a Dagger (0.8lb):         7, 10, 12, 14...  (old2)
 *            Short Sword (1.5lb):    8, 12, 15, 19...
 *            Long Sword (3lb):      10, 15, 20, 25...
 *            Bastard Sword (3.5lb): 10, 16, 21, 27...
 *            Great Sword (7lb):     14, 23, 32, 41...
 */
int crit_bonus(struct player *p, int hit_result, int weight,
			   const struct monster_race *race, int skill_type, bool thrown)
{
	int crit_bonus_dice;
	int crit_separation = 70;

	/* When attacking a monster... */
	if (race) {
		/* Changes to melee criticals */
		if (skill_type == SKILL_MELEE) {
			/* Can have improved criticals for melee */
			if (player_active_ability(p, "Finesse")) {
				crit_separation -= 10;
			}

			/* Can have improved criticals for melee with one handed weapons */
			if (player_active_ability(p, "Subtlety") && !thrown &&
				!two_handed_melee(p) &&
				!equipped_item_by_slot_name(p, "arm")) {
				crit_separation -= 20;
			}

			/* Can have inferior criticals for melee */
			if (player_active_ability(p, "Power")) {
				crit_separation += 10;
			}
		}

		/* Can have improved criticals for archery */
		if ((skill_type == SKILL_ARCHERY) &&
			player_active_ability(p, "Precision")) {
			crit_separation -= 10;
		}
	} else {
		/* When attacking the player... */
		/* Resistance to criticals increases what they need for each bonus die*/
		if (player_active_ability(p, "Critical Resistance")) {
			crit_separation += (p->state.skill_use[SKILL_WILL] / 5) * 10;	
		}
	}

	/* Note: the +4 in this calculation is for rounding purposes */
	crit_bonus_dice = (hit_result * 10 + 4) / (crit_separation + weight);

	/* When attacking a monster... */
	if (race) {
		/* Resistance to criticals doubles what you need for each bonus die */
		if (rf_has(race->flags, RF_RES_CRIT)) {
			crit_bonus_dice /= 2;
		}

		/* Certain creatures cannot suffer crits as they have no vulnerable
		 * areas */
		if (rf_has(race->flags, RF_NO_CRIT)) {
			crit_bonus_dice = 0;
		}
	}

	/* Can't have fewer than zero dice */
	return MAX(crit_bonus_dice, 0);
}

/**
 * Roll the protection dice for all parts of the player's armour
 */
int protection_roll(struct player *p, int typ, bool melee, aspect prot_aspect)
{
	int i;
	int prt = 0;
	int mult = 1;
	int armour_weight = 0;
	struct song *staying = lookup_song("Staying");
	
	/* Things that always count: */
	if (player_is_singing(p, staying)) {
		int bonus = song_bonus(p, p->state.skill_use[SKILL_SONG], staying);
		prt += damcalc(1, MAX(1, bonus), prot_aspect);
	}
	
	if (player_active_ability(p, "Hardiness")) {
		prt += damcalc(1, MIN(1, p->state.skill_use[SKILL_WILL] / 6),
					   prot_aspect);
	}
	
	/* Armour: */
	for (i = 0; i < p->body.count; i++) {
		struct object *obj = p->body.slots[i].obj;
		if (!obj) continue;
        
		/* Skip off-hand weapons */
		if (slot_type_is(p, i, EQUIP_SHIELD) && tval_is_weapon(obj)) continue;

		/* Count weight of armour */
		if (tval_is_armor(obj)) {
			armour_weight += obj->weight;
		}

		/* Fire and cold and generic 'hurt' all check the shield */
		if (slot_type_is(p, i, EQUIP_SHIELD)) {
			if ((typ == PROJ_HURT) || (typ == PROJ_FIRE) || (typ == PROJ_COLD)){
				if (player_active_ability(p, "Blocking") &&
					(!melee || ((p->previous_action[0] == ACTION_STAND) ||
								((p->previous_action[0] == ACTION_NOTHING) &&
								 (p->previous_action[1] == ACTION_STAND))))) {
					mult = 2;
				}
				if (obj->pd > 0) {
					prt += damcalc(obj->pd * mult, obj->ps, prot_aspect);
				}
			}
		} else if ((typ == PROJ_HURT) || (tval_is_jewelry(obj)))	{
			/* Also add protection if damage is generic 'hurt' or it is
			 * a ring or amulet slot */
			if (obj->ps > 0) {
				prt += damcalc(obj->pd, obj->ps, prot_aspect);
			}
		}
	}

	/* Heavy armour bonus */
	if (player_active_ability(p, "Heavy Armour") && (typ == PROJ_HURT)) {
		prt += damcalc(1, MIN(1, armour_weight / 150), prot_aspect);
	}

	return prt;
}
