/**
 * \file mon-calcs.c
 * \brief Monster status calculation 
 *	status changes.
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2022 Nick McConnell
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
#include "game-world.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-group.h"
#include "mon-lore.h"
#include "mon-move.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-knowledge.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "player.h"
#include "project.h"
#include "songs.h"

/**
 * ------------------------------------------------------------------------
 * Morale
 * ------------------------------------------------------------------------ */
/**
 * Bonus for elf bane against elves.
 */
int monster_elf_bane_bonus(struct monster *mon, struct player *p)
{
	bool elf = (streq(p->race->name, "Noldor") ||
				streq(p->race->name, "Sindar"));

    if (!mon) return 0;

	/* Dagohir must have killed between 32 and 63 elves */
    return (rf_has(mon->race->flags, RF_ELFBANE) && elf) ? 5 : 0;
}

/**
 * Calculate the number of monsters of the same type within LOS of a given
 * monster.
 */
static int morale_from_friends(struct monster *mon)
{
	int i;
	int morale_bonus = 0;
	int morale_penalty = 0;

	/* Scan monsters */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon1 = cave_monster(cave, i);

		/* Skip dead monsters */
		if (!mon1->race) continue;

		/* Skip self! */
		if (mon == mon1) continue;

		/* Skip monsters not in LoS */
		if (!los(cave, mon->grid, mon1->grid)) continue;

		/* Skip dissimilar monsters */
		if (!similar_monsters(mon, mon1)) continue;
		
		/* Only consider alert monsters */
		if (mon1->alertness >= ALERTNESS_ALERT) {
			int multiplier = 1;

			if (rf_has(mon1->race->flags, RF_ESCORT) ||
				rf_has(mon1->race->flags, RF_ESCORTS)) {
				multiplier = 4;
			}

			/* Add bonus or penalty to morale */
			if (mon1->stance == STANCE_FLEEING)	{
				morale_penalty += 10 * multiplier;
			} else {
				morale_bonus += 10 * multiplier;
			}
		}
	}

	return (morale_bonus - morale_penalty);
}

/**
 * Calculate the morale for a monster.
 */
void calc_morale(struct monster *mon)
{
	int morale;
    int difference;
	struct monster_race *race = mon->race;

	/* Starting morale is 60 */
	morale = 60;

	/* Monsters have boosted morale during the endgame */
	if (player->on_the_run) {
		morale += 20;
	} else {
		/* Monsters have higher morale if they are usually found deeper than
		 * this and vice versa */
		morale += (race->level - player->depth) * 10;
		
		/* Make sure orcs etc in throne room don't have too low morale */
		if (player->depth == z_info->dun_depth) {
			morale = MAX(morale, 20);
		}
	}
		
	/* Take player's conditions into account */
	if (player->timed[TMD_IMAGE]) {
		morale += 20;
	}
	if (player->timed[TMD_BLIND]) {
		morale += 20;
	}
	if (player->timed[TMD_CONFUSED]) {
		morale += 40;
	}
	if (player->timed[TMD_SLOW]) {
		morale += 40;
	}
	if (player->timed[TMD_AFRAID]) {
		morale += 40;
	}
	if (player->timed[TMD_ENTRANCED]) {
		morale += 80;
	} else if (player->timed[TMD_STUN] > 100) {
		morale += 80;
	} else if (player->timed[TMD_STUN] > 50) {
		morale += 40;
	} else if (player->timed[TMD_STUN] > 0) {
		morale += 20;
	}

	/* Take player's health into account */
	switch (health_level(player->chp, player->mhp)) {
		case  HEALTH_WOUNDED:		morale += 20;	break;  /* <= 75% health */
		case  HEALTH_BADLY_WOUNDED:	morale += 40;	break;  /* <= 50% health */
		case  HEALTH_ALMOST_DEAD:	morale += 80;	break;  /* <= 25% health */
	}

	/* Take monster's conditions into account */
	if (mon->m_timed[MON_TMD_STUN]) {
		morale -= 20;
	}
	/* Skip confusion as it is less good if confused monsters flee */
	if (mon->m_timed[MON_TMD_FAST]) {
		morale += 40;
	}

	/* Take monster's health into account */
	switch (health_level(mon->hp, mon->maxhp))
	{
		case  HEALTH_WOUNDED:		morale -= 20;	break;  /* <= 75% health */
		case  HEALTH_BADLY_WOUNDED:	morale -= 40;	break;  /* <= 50% health */
		case  HEALTH_ALMOST_DEAD:	morale -= 80;	break;  /* <= 25% health */
	}

	/* Extra penalty if <=75% health and already fleeing
	 * helps avoid them coming back too quickly */
	if ((mon->stance == STANCE_FLEEING) &&
		(health_level(mon->hp, mon->maxhp) <= HEALTH_WOUNDED)) {
		morale -= 20;
	}

	/* Get a bonus for non-fleeing friends and a penalty for fleeing ones */
	morale += morale_from_friends(mon);

	/* Reduce morale for light averse monsters facing a brightly lit player */
	if (rf_has(race->flags, RF_HURT_LIGHT) &&
		(square_light(cave, player->grid) >= 4)) {
		morale -= (square_light(cave, player->grid) - 3) * 10;
	}

	/* Reduce morale for each carried object for non-uniques, so thieves avoid
	 * the player */
	if (!rf_has(race->flags, RF_UNIQUE)) {
		struct object *obj = mon->held_obj;
		while (obj) {
			/* Lower morale */
			morale -= 20;

			/* Get the next object */
			obj = obj->next;
		}
	}
	
	/* Reduce morale for the Majesty ability */
    difference = MAX(player->state.skill_use[SKILL_WILL]
					 - monster_skill(mon, SKILL_WILL), 0);
	if (player_active_ability(player, "Majesty")) {
		morale -= difference / 2 * 10;
	}

	/* Reduce morale for the Bane ability */
	if (player_active_ability(player, "Bane")) {
		morale -= player_bane_bonus(player, mon) * 10;
	}

    /* Increase morale for the Elf-Bane ability */
	morale += monster_elf_bane_bonus(mon, player) * 10;

	/* Add temporary morale modifiers */
	morale += mon->tmp_morale;

	/* Update the morale */
	mon->morale = morale;
}	

/**
 * ------------------------------------------------------------------------
 * Stance
 * ------------------------------------------------------------------------ */
/**
 * Calculate the stance for a monster.
 *
 * Based on the monster's morale, type, and other effects.
 *
 * Can be:
 *    STANCE_FLEEING
 *    STANCE_CONFIDENT
 *    STANCE_AGGRESSIVE
 */
void calc_stance(struct monster *mon)
{
	struct monster_race *race = mon->race;
	int stance;
	int stances[3];

	/* Set the default stances */
	stances[0] = STANCE_FLEEING;
	stances[1] = STANCE_CONFIDENT;
	stances[2] = STANCE_AGGRESSIVE;

	/* Some monsters are immune to (non-magical) fear */
	if (rf_has(race->flags, RF_NO_FEAR) && (mon->tmp_morale >= 0)) {
		stances[0] = STANCE_CONFIDENT;
	}

	/* Mindless monsters just attack */
	if (rf_has(race->flags, RF_MINDLESS)) {
		stances[0] = STANCE_AGGRESSIVE;
		stances[1] = STANCE_AGGRESSIVE;
	}

	/* Trolls are aggressive rather than confident */
	if (rf_has(race->flags, RF_TROLL)) {
		stances[1] = STANCE_AGGRESSIVE;
	}

	/* Aggravation makes non-mindless things much more hostile */
	if (player->state.flags[OF_AGGRAVATE] && !rf_has(race->flags, RF_MINDLESS)){
		stances[1] = STANCE_AGGRESSIVE;
		if (monster_is_in_view(mon)) {
			equip_learn_flag(player, OF_AGGRAVATE);
		}
	}

	/* Monsters that have been angered have confident turned into aggressive */
	if (mflag_has(mon->mflag, MFLAG_AGGRESSIVE)) {
		stances[1] = STANCE_AGGRESSIVE;
	}

	/* Determine the stance */
	if (mon->morale > 200) {
		stance = stances[2];
	} else if (mon->morale > 0) {
		stance = stances[1];
	} else {
		stance = stances[0];
	}

	/* Override this for unwary/sleeping monsters */
	if (mon->alertness < ALERTNESS_ALERT) {
		stance = stances[1];
	}

	/* React to changes in stance */
	if (stance != mon->stance) {
		enum mon_messages stance_msg = MON_MSG_NONE;
		switch (mon->stance) {
			case STANCE_FLEEING: {
				/* Give the monster a temporary 'rally' bonus to its morale */
				mon->tmp_morale += 60;
				calc_morale(mon);
				if (!player->truce) {
					stance_msg = MON_MSG_TURN_TO_FIGHT;
				} else {
					stance_msg = MON_MSG_RECOVER_COMPOSURE;
				}
				break;
			}
			case STANCE_CONFIDENT:
			case STANCE_AGGRESSIVE: {
				if (stance == STANCE_FLEEING) {
					/* Give the monster a temporary 'break' penalty to morale */
					mon->tmp_morale -= 60;
					calc_morale(mon);
					stance_msg = MON_MSG_FLEE_IN_TERROR;
				}
				break;
			}
		}

		/* Inform player of visible changes */
		if (stance_msg && monster_is_visible(mon) &&
			!rf_has(race->flags, RF_NEVER_MOVE)) {
			add_monster_message(mon, stance_msg, true);
		}

		/* Force recalculation of range if stance changes */
		mon->min_range = 0;
		
 	}

	/* Update the monster's stance */
	mon->stance = stance;
}

/**
 * ------------------------------------------------------------------------
 * Alertness
 * ------------------------------------------------------------------------ */
/**
 * Changes a monster's alertness value and displays any appropriate messages
 */
void make_alert(struct monster *mon, int dam)
{
	int random_level = rand_range(ALERTNESS_ALERT, ALERTNESS_QUITE_ALERT);
	set_alertness(mon, MAX(mon->alertness + dam, random_level + dam));
}

/**
 * Changes a monster's alertness value and displays any appropriate messages
 */
void set_alertness(struct monster *mon, int alertness)
{
	bool redisplay = false;
	enum mon_messages alert_msg = MON_MSG_NONE;
	
	/* Nothing to be done... */
	if (mon->alertness == alertness) return;
	
	/* Bound the alertness value */
	alertness = (MAX(MIN(alertness, ALERTNESS_MAX), ALERTNESS_MIN));
	
	/* First deal with cases where the monster becomes more alert */
	if (mon->alertness < alertness) {
		if (mon->alertness < ALERTNESS_UNWARY) {
			if (alertness >= ALERTNESS_ALERT) {
				/* Monster must spend its next turn noticing you */
				mon->skip_next_turn = true;

				/* Notice the "waking up and noticing" */
				if (monster_is_visible(mon)) {
					/* Dump a message */
					alert_msg = MON_MSG_WAKE_AND_NOTICE;
				}

				/* Disturb the player */
				disturb(player, true);

				/* Redisplay the monster */
				redisplay = true;
			} else if (alertness >= ALERTNESS_UNWARY) {
				/* Notice the "waking up" */
				if (monster_is_visible(mon)) {
					/* Dump a message */
					alert_msg = MON_MSG_WAKES_UP;
				}

				/* Disturb the player */
				disturb(player, true);

				/* Redisplay the monster */
				redisplay = true;
			}
		} else if ((mon->alertness < ALERTNESS_ALERT) &&
				   (alertness >= ALERTNESS_ALERT)) {
			/* Monster must spend its next turn noticing you */
			mon->skip_next_turn = true;
            
			/* Notice the "noticing" (!) */
			if (monster_is_visible(mon)) {
				/* Dump a message */
				alert_msg = MON_MSG_NOTICE;

				/* Disturb the player */
				disturb(player, true);

				/* Redisplay the monster */
				redisplay = true;
			}
		} else if ((mon->alertness < ALERTNESS_UNWARY) &&
				   (alertness < ALERTNESS_UNWARY) &&
				   (alertness >= ALERTNESS_UNWARY - 2)) {
			/* Notice the "stirring" */
			if (monster_is_visible(mon)) {
				/* Dump a message */
				alert_msg = MON_MSG_STIR;
			}
		} else if ((mon->alertness < ALERTNESS_ALERT) &&
				   (alertness < ALERTNESS_ALERT) &&
				   (alertness >= ALERTNESS_ALERT - 2)) {
			/* Notice the "looking around" */
			if (monster_is_visible(mon)) {
				/* Dump a message */
				alert_msg = MON_MSG_LOOK_AROUND;
			}
		}
	} else {
		/* Deal with cases where the monster becomes less alert */
		if ((mon->alertness >= ALERTNESS_UNWARY) &&
			(alertness < ALERTNESS_UNWARY)) {
			/* Notice the falling asleep */
			if (monster_is_visible(mon)) {
				/* Dump a message */
				alert_msg = MON_MSG_FALL_ASLEEP;

				/* Morgoth drops his iron crown if he falls asleep */
				if (rf_has(mon->race->flags, RF_QUESTOR)) {
					drop_iron_crown(mon, "His crown slips from off his brow and falls to the ground nearby.");
				}

				/* Redisplay the monster */
				redisplay = true;
			}
		} else if ((mon->alertness >= ALERTNESS_ALERT) &&
				   (alertness < ALERTNESS_ALERT)) {
			/* Notice the becoming unwary */
			if (monster_is_visible(mon)) {
				/* Dump a message */
				alert_msg = MON_MSG_BECOME_UNWARY;

				/* Redisplay the monster */
				redisplay = true;

				/* Give the monster a new place to wander towards */
				if (!rf_has(mon->race->flags, RF_TERRITORIAL)) {
					monster_group_new_wandering_flow(cave, mon, player->grid);
				}
			}
		}
	}
	
	/* Add the message */
	if (alert_msg) {
		add_monster_message(mon, alert_msg, true);
	}

	/* Do the actual alerting */
	mon->alertness = alertness;
	
	/* Redisplay the monster */
	if (redisplay) {
		square_light_spot(cave, mon->grid);
	}
}

/**
 * ------------------------------------------------------------------------
 * Monster updates
 * ------------------------------------------------------------------------ */
/**
 * Try to locate a monster by the noise it is making
 */
static void listen(struct chunk *c, struct player *p, struct monster *mon)
{
	int result;
	int difficulty = flow_dist(c->player_noise, mon->grid) - mon->noise;
	struct song *silence = lookup_song("Silence");

	/* Reset the monster noise */
	mon->noise = 0;

	/* Must have the listen skill */
	if (!player_active_ability(p, "Listen")) return;

	/* Must not be visible */
	if (monster_is_visible(mon)) return;

	/* Monster must be able to move */
	if (rf_has(mon->race->flags, RF_NEVER_MOVE)) return;

	/* Use monster stealth */
	difficulty += monster_skill(mon, SKILL_STEALTH);

	/* Bonus for awake but unwary monsters (to simulate their lack of care) */
	if ((mon->alertness >= ALERTNESS_UNWARY) &&
		(mon->alertness < ALERTNESS_ALERT)) {
		difficulty -= 3;
	}

	/* Penalty for song of silence */
	if (player_is_singing(p, silence)) {
		difficulty += song_bonus(p, p->state.skill_use[SKILL_SONG], silence);
	}

	/* Make the check */
	result = skill_check(source_player(), p->state.skill_use[SKILL_PERCEPTION],
						 difficulty, source_monster(mon->midx));

	/* Give up if it is a failure */
	if (result <= 0) {
		square_light_spot(c, mon->grid);
		return;
	}

	/* Make the monster completely visible if a dramatic success */
	if (result > 10) {
		mflag_on(mon->mflag, MFLAG_VISIBLE);
		square_light_spot(c, mon->grid);
		return;
	}

	/* Let's see if this works... */
	mflag_on(mon->mflag, MFLAG_LISTENED);
}

/**
 * Analyse the path from player to infravision-seen monster and forget any
 * grids which would have blocked line of sight
 */
static void path_analyse(struct chunk *c, struct loc grid)
{
	int path_n, i;
	struct loc path_g[256];

	if (c != cave) {
		return;
	}

	/* Plot the path. */
	path_n = project_path(c, path_g, z_info->max_range, player->grid,
		&grid, PROJECT_NONE);

	/* Project along the path */
	for (i = 0; i < path_n - 1; ++i) {
		/* Forget grids which would block los */
		if (!square_allowslos(player->cave, path_g[i])) {
			sqinfo_off(square(c, path_g[i])->info, SQUARE_SEEN);
			square_forget(c, path_g[i]);
			square_light_spot(c, path_g[i]);
		}
	}
}

/**
 * This function updates the monster record of the given monster
 *
 * This involves extracting the distance to the player (if requested),
 * and then checking for visibility (natural, infravision, see-invis,
 * telepathy), updating the monster visibility flag, redrawing (or
 * erasing) the monster when its visibility changes, and taking note
 * of any interesting monster flags (cold-blooded, invisible, etc).
 *
 * Note the new "mflag" field which encodes several monster state flags,
 * including "view" for when the monster is currently in line of sight,
 * and "mark" for when the monster is currently visible via detection.
 *
 * The only monster fields that are changed here are "cdis" (the
 * distance from the player), "ml" (visible to the player), and
 * "mflag" (to maintain the "MFLAG_VIEW" flag).
 *
 * Note the special "update_monsters()" function which can be used to
 * call this function once for every monster.
 *
 * Note the "full" flag which requests that the "cdis" field be updated;
 * this is only needed when the monster (or the player) has moved.
 *
 * Every time a monster moves, we must call this function for that
 * monster, and update the distance, and the visibility.  Every time
 * the player moves, we must call this function for every monster, and
 * update the distance, and the visibility.  Whenever the player "state"
 * changes in certain ways ("blindness", "infravision", "telepathy",
 * and "see invisible"), we must call this function for every monster,
 * and update the visibility.
 *
 * Routines that change the "illumination" of a grid must also call this
 * function for any monster in that grid, since the "visibility" of some
 * monsters may be based on the illumination of their grid.
 *
 * Note that this function is called once per monster every time the
 * player moves.  When the player is running, this function is one
 * of the primary bottlenecks, along with "update_view()" and the
 * "process_monsters()" code, so efficiency is important.
 *
 * Note the optimized "inline" version of the "distance()" function.
 *
 * A monster is "visible" to the player if (1) it has been detected
 * by the player, (2) it is close to the player and the player has
 * telepathy, or (3) it is close to the player, and in line of sight
 * of the player, and it is "illuminated" by some combination of
 * infravision, torch light, or permanent light (invisible monsters
 * are only affected by "light" if the player can see invisible).
 *
 * Monsters which are not on the current panel may be "visible" to
 * the player, and their descriptions will include an "offscreen"
 * reference.  Currently, offscreen monsters cannot be targeted
 * or viewed directly, but old targets will remain set.  XXX XXX
 *
 * The player can choose to be disturbed by several things, including
 * "OPT(player, disturb_near)" (monster which is "easily" viewable moves in some
 * way).  Note that "moves" includes "appears" and "disappears".
 */
void update_mon(struct monster *mon, struct chunk *c, bool full)
{
	struct monster_lore *lore;
	struct monster_race *race;

	int d;

	/* Seen at all */
	bool flag = false;

	/* Seen by vision */
	bool easy = false;

    /* Known because immobile */
    bool immobile_seen = false;

	assert(mon != NULL);

	/* Return if this is not the current level */
	if (c != cave) {
		return;
	}

	lore = get_lore(mon->race);
	race = mon->race;
	
    /* Unmoving mindless monsters can be seen once encountered */
    if (rf_has(race->flags, RF_NEVER_MOVE) &&
		rf_has(race->flags, RF_MINDLESS) && mon->encountered) {
		immobile_seen = true;
	}
    
	/* Compute distance, or just use the current one */
	if (full) {
		/* Distance components */
		int dy = ABS(player->grid.y - mon->grid.y);
		int dx = ABS(player->grid.x - mon->grid.x);

		/* Approximate distance */
		d = (dy > dx) ? (dy + (dx >>  1)) : (dx + (dy >> 1));

		/* Restrict distance */
		if (d > 255) d = 255;

		/* Save the distance */
		mon->cdis = d;
	} else {
		/* Extract the distance */
		d = mon->cdis;
	}

	/* Detected */
	if (mflag_has(mon->mflag, MFLAG_MARK)) flag = true;

	/* Clear the listen flag */
	mflag_off(mon->mflag, MFLAG_LISTENED);

	/* Nearby */
	if (d <= z_info->max_sight) {
		/* Normal line of sight and player is not blind */
		if (square_isview(c, mon->grid) && !player->timed[TMD_BLIND]) {
			bool do_invisible = false;

			/* Use illumination */
			if (square_isseen(c, mon->grid)) {
				/* Handle invisibility */
				if (monster_is_invisible(mon)) {
					int difficulty = monster_skill(mon, SKILL_WILL) +
						(2 * distance(player->grid, mon->grid)) -
						10 * player->state.flags[OF_SEE_INVIS];

					/* Take note */
					do_invisible = true;

					/* Keen senses */
					if (player_active_ability(player, "Keen Senses")) {
						/* Makes things a bit easier */
						difficulty -= 5;
					}

					/* See invisible through perception skill */
					if (skill_check(source_player(),
									player->state.skill_use[SKILL_PERCEPTION],
									difficulty, source_monster(mon->midx)) > 0){
						/* Easy to see */
						easy = flag = true;
					}
				} else {
					/* Easy to see */
					easy = flag = true;
				}
			} else if (square_seen_by_keen_senses(c, mon->grid)) {
				/* Easy to see */
				easy = flag = true;
			}

			/* Visible */
			if (flag && do_invisible) {
				/* Learn about invisibility */
				rf_on(lore->flags, RF_INVISIBLE);
			}

			/* Learn about intervening squares */
			path_analyse(c, mon->grid);
		}
	}

	/* Is the monster now visible? */
	if (flag || immobile_seen) {
        /* Untarget if this is an out-of-LOS stationary monster */
        if (immobile_seen && !flag) {
            if (target_get_monster() == mon) {
				target_set_monster(NULL);
			}
            if (player->upkeep->health_who == mon) {
				health_track(player->upkeep, NULL);
			}
		}

		/* It was previously unseen */
		if (!monster_is_visible(mon)) {
			/* Mark as visible */
			mflag_on(mon->mflag, MFLAG_VISIBLE);

			/* Draw the monster */
			square_light_spot(c, mon->grid);

			/* Update health bar as needed */
			if (player->upkeep->health_who == mon)
				player->upkeep->redraw |= (PR_HEALTH);

			/* Window stuff */
			player->upkeep->redraw |= PR_MONLIST;

			/* Identify see invisible items */
			if (rf_has(race->flags, RF_INVISIBLE) &&
				(player->state.flags[OF_SEE_INVIS] > 0)) {
				player_learn_flag(player, OF_SEE_INVIS);
			}
		}
	} else if (monster_is_visible(mon)) {
		/* Not visible but was previously seen */
		mflag_off(mon->mflag, MFLAG_VISIBLE);

		/* Erase the monster */
		square_light_spot(c, mon->grid);

		/* Update health bar as needed */
		if (player->upkeep->health_who == mon)
			player->upkeep->redraw |= (PR_HEALTH);

		/* Window stuff */
		player->upkeep->redraw |= PR_MONLIST;
	}


	/* Is the monster is now easily visible? */
	if (easy) {
		/* Change */
		if (!monster_is_in_view(mon)) {
			/* Mark as easily visible */
			mflag_on(mon->mflag, MFLAG_VIEW);

			/* Disturb on appearance */
			disturb(player, false);

			/* Re-draw monster window */
			player->upkeep->redraw |= PR_MONLIST;
		}
	} else {
		/* Change */
		if (monster_is_in_view(mon)) {
			/* Mark as not easily visible */
			mflag_off(mon->mflag, MFLAG_VIEW);

			/* Re-draw monster list window */
			player->upkeep->redraw |= PR_MONLIST;
		}
	}

	listen(c, player, mon);

	/* Check encounters with monsters (must be visible and in line of sight) */
	if (monster_is_visible(mon) && !mon->encountered &&
		square_isseen(c, mon->grid) && (lore->psights < SHRT_MAX)) {
		int new_exp = adjusted_mon_exp(race, false);

		/* Gain experience for encounter */
		player_exp_gain(player, new_exp);	
		player->encounter_exp += new_exp;	

		/* Update stats */
		mon->encountered = true;
		lore->psights++;
		if (lore->tsights < SHRT_MAX) lore->tsights++;

		/* If the player encounters a Unique for the first time, write a note */
		if (rf_has(race->flags, RF_UNIQUE)) {
			char note[120];

			/* Write note */
			my_strcpy(note, format("Encountered %s", race->name), sizeof(note));
			history_add(player, note, HIST_MEET_UNIQUE);
		}

		/* If it was a wraith, possibly realise you are haunted */
		if (rf_has(race->flags, RF_UNDEAD) &&
			!rf_has(race->flags, RF_TERRITORIAL)) {
			player_learn_flag(player, OF_HAUNTED);
		}
	}
}

/**
 * Updates all the (non-dead) monsters via update_mon().
 */
void update_monsters(bool full)
{
	int i;

	/* Update each (live) monster */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Update the monster if alive */
		if (mon->race)
			update_mon(mon, cave, full);
	}
}

/**
 * ------------------------------------------------------------------------
 * Skills and stats
 * ------------------------------------------------------------------------ */
/**
 * Calculates a skill score for a monster
 */
int monster_skill(struct monster *mon, int skill_type)
{
	struct monster_race *race = mon->race;
	int skill = 0;
	
    switch (skill_type) {
        case SKILL_MELEE:
            msg("Can't determine the monster's Melee score.");
            break;
        case SKILL_ARCHERY:
            msg("Can't determine the monster's Archery score.");
            break;
        case SKILL_EVASION:
            msg("Can't determine the monster's Evasion score.");
            break;
        case SKILL_STEALTH:
            skill = race->stl;
            break;
        case SKILL_PERCEPTION:
            skill = race->per;
            break;
        case SKILL_WILL:
            skill = race->wil;
            break;
        case SKILL_SMITHING:
            msg("Can't determine the monster's Smithing score.");
            break;
        case SKILL_SONG:
            msg("Can't determine the monster's Song score.");
            break;
            
        default:
            msg("Asked for an invalid monster skill.");
            break;
    }
    
	/* Penalise stunning */
	if (mon->m_timed[MON_TMD_STUN]) {
		skill -= 2;
	}

	return skill;
}

/**
 * Calculates a Stat score for a monster
 */
int monster_stat(struct monster *mon, int stat_type)
{
	struct monster_race *race = mon->race;
	int stat = 0;
	int mhp = mon->maxhp;
	int base = 20;

    switch (stat_type) {
        case STAT_STR:
            stat = (race->blow[0].dice.dice * 2) + (race->hdice / 10) - 4;
            break;
        case STAT_DEX:
            msg("Can't determine the monster's Dex score.");
            break;
        case STAT_CON:
            if (mhp < base) {
                while (mhp < base) {
                    stat--;
                    base = (base * 10) / 12;
                }
            } else if (mhp >= base) {
                stat--;
                while (mhp >= base) {
                    stat++;
                    base = (base * 12) / 10;
                }
            }
            break;
        case STAT_GRA:
            msg("Can't determine the monster's Gra score.");
            break;
            
        default:
            msg("Asked for an invalid monster stat.");
            break;
    }
    	
	return stat;
}

/**
 * ------------------------------------------------------------------------
 * Speed
 * ------------------------------------------------------------------------ */
/**
 * Calculate the speed of a given monster
 */
void calc_monster_speed(struct monster *mon)
{
	int speed;

	/* Paranoia */
	if (!mon) return;

	/* Get the monster base speed */
	speed = mon->race->speed;

	/* Factor in the hasting and slowing counters */
	if (mon->m_timed[MON_TMD_FAST]) speed += 1;
	if (mon->m_timed[MON_TMD_SLOW]) speed -= 1;

	if (speed < 1) speed = 1;

	/* Set the speed and return */
	mon->mspeed = speed;

	return;
}
