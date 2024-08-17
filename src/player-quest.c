/**
 * \file player-quest.c
 * \brief All throne room-related code
 *
 * Copyright (c) 2013 Angband developers
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
#include "datafile.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-move.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "project.h"

/**
 * Makes Morgoth drop his Iron Crown with an appropriate message.
 */
void drop_iron_crown(struct monster *mon, const char *message)
{
	int i;
	struct loc grid;
	struct monster_race *race = mon->race;
	const struct artifact *crown = lookup_artifact_name("of Morgoth");
	bool note;

	if (!is_artifact_created(crown)) {
		struct object *obj;
		struct object_kind *kind;
		msg(message);

		/* Choose a nearby location, but not his own square */
		find_nearby_grid(cave, &grid, mon->grid, 1, 1);
		for (i = 0; i < 1000; i++) {
			find_nearby_grid(cave, &grid, mon->grid, 1, 1);
			if (!loc_eq(grid, mon->grid) && square_isfloor(cave, grid)) break;
		}

		/* Allocate by hand, prep, apply magic */
		obj = mem_zalloc(sizeof(*obj));
		kind = lookup_kind(crown->tval, crown->sval);
		object_prep(obj, kind, z_info->dun_depth, RANDOMISE);
		obj->artifact = crown;
		copy_artifact_data(obj, obj->artifact);
		mark_artifact_created(crown, true);

		/* Set origin details */
		obj->origin = ORIGIN_DROP;
		obj->origin_depth = convert_depth_to_origin(cave->depth);
		obj->origin_race = race;
		obj->number = 1;

		/* Drop it there */
		floor_carry(cave, grid, obj, &note);

		/* Lower Morgoth's protection, remove his light source, increase his
		 * will and perception */
		race->pd -= 1;
		race->light = 0;
		race->wil += 5;
		race->per += 5;
	}
}

/**
 * Shatter the player's wielded weapon.
 */
void shatter_weapon(struct player *p, int silnum)
{
	int i;
	struct object *weapon = equipped_item_by_slot_name(p, "weapon");
	struct object *destroyed;
	char w_name[80];
	bool dummy = false;

	p->crown_shatter = true;

	/* Get the basic name of the object */
	object_desc(w_name, sizeof(w_name), weapon, false, 0);
	
	if (silnum == 2) {
		msg("You strive to free a second Silmaril, but it is not fated to be.");
	} else {
		msg("You strive to free a third Silmaril, but it is not fated to be.");
	}	
	msg("As you strike the crown, your %s shatters into innumerable pieces.",
		w_name);

	/* Make more noise */
	p->stealth_score -= 5;

	destroyed = gear_object_for_use(player, weapon, 1, false, &dummy);
	object_delete(p->cave, NULL, &destroyed->known);
	object_delete(cave, p->cave, &destroyed);

	/* Process monsters */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* If Morgoth, then anger him */
		if (rf_has(mon->race->flags, RF_QUESTOR)) {
			if ((mon->cdis <= 5) && los(cave, p->grid, mon->grid)) {
				msg("A shard strikes Morgoth upon his cheek.");
				set_alertness(mon, ALERTNESS_VERY_ALERT);
			}
		}
	}
}

/**
 * Break the truce in Morgoth's throne room
 */
void break_truce(struct player *p, bool obvious)
{
	int i;
	struct monster *mon = NULL;
	char m_name[80];
	
	if (p->truce) {
		/* Scan all other monsters */
		for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
			/* Access the monster */
			mon = cave_monster(cave, i);
			
			/* Ignore dead monsters */
			if (!mon->race) continue;
			
			/* Ignore monsters out of line of sight */
			if (!los(cave, mon->grid, p->grid)) continue;
			
			/* Ignore unalert monsters */
			if (mon->alertness < ALERTNESS_ALERT) continue;
			
			/* Get the monster name (using 'something' for hidden creatures) */
			monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);
			
			p->truce = false;
		}
		
		if (obvious) p->truce = false;
		
		if (!p->truce) {
			if (!obvious) {
				msg("%s lets out a cry! The tension is broken.", m_name);

				/* Make a lot of noise */
				cave->monster_noise.centre = mon->grid;
				update_flow(cave, &cave->monster_noise, NULL);
				monsters_hear(false, false, -10);
			} else {
				msg("The tension is broken.");
			}
			
			/* Scan all other monsters */
			for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
				/* Access the monster */
				mon = cave_monster(cave, i);
				
				/* Ignore dead monsters */
				if (!mon->race) continue;
				
				/* Mark minimum desired range for recalculation */
				mon->min_range = 0;
			}
		}
	}
}

/**
 * Check whether to break the truce in Morgoth's throne room
 */
void check_truce(struct player *p)
{
	int d;

	/* Check around the character */
	for (d = 0; d < 8; d++) {
		struct loc grid = loc_sum(p->grid, ddgrid_ddd[d]);
		struct monster *mon = square_monster(cave, grid);

		if (mon && (mon->race == lookup_monster("Morgoth, Lord of Darkness"))
			&& (mon->alertness >= ALERTNESS_ALERT)) {
			msg("With a voice as of rolling thunder, Morgoth, Lord of Darkness, speaks:");
			msg("'You dare challenge me in mine own hall? Now is your death upon you!'");

			/* Break the truce (always) */
			break_truce(p, true);
			return;
		}
	}
}

/**
 * Wake up all monsters, and speed up "los" monsters.
 */
void wake_all_monsters(struct player *p)
{
	int i;

	/* Aggravate everyone */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);
		/* Paranoia -- Skip dead monsters */
		if (!mon->race) continue;

		/* Alert it */
		set_alertness(mon, MAX(mon->alertness, ALERTNESS_VERY_ALERT));

		/* Possibly update the monster health bar*/
		if (p->upkeep->health_who == mon) p->upkeep->redraw |= (PR_HEALTH);
	}
}

void prise_silmaril(struct player *p)
{
	struct object *obj, *weapon;
	const char *freed_msg = NULL;
	bool freed = false;
	int net_dam = 0;
	int hit_result = 0;
	int pd = 0;
	int noise = 0;
	int mds = p->state.mds;
	int attack_mod = p->state.skill_use[SKILL_MELEE];
	struct monster_race *race = lookup_monster("Morgoth, Lord of Darkness");

	/* The Crown is on the ground */
	obj = square_object(cave, p->grid);

	switch (obj->pval) {
		case 3: {
			pd = 15;
			noise = 5;
			freed_msg = "You have freed a Silmaril!";
			break;
		}
		case 2: {
			pd = 25;
			noise = 10;
			if (p->crown_shatter) {
				freed_msg = "The fates be damned! You free a second Silmaril.";
			} else {
				freed_msg = "You free a second Silmaril.";
			}
			break;
		}
		case 1: {
			pd = 30;
			noise = 15;
			freed_msg = "You free the final Silmaril. You have a very bad feeling about this.";
			msg("Looking into the hallowed light of the final Silmaril, you are filled with a strange dread.");
			if (!get_check("Are you sure you wish to proceed? ")) return;
			
			break;
		}
	}

	/* Get the weapon */
	weapon = equipped_item_by_slot_name(p, "weapon");

	/* Undo rapid attack penalties */
	if (player_active_ability(p, "Rapid Attack")) {
		/* Undo strength adjustment to the attack */
		mds = total_mds(p, &p->state, weapon, 0);
		
		/* Undo the dexterity adjustment to the attack */
		attack_mod += 3;
	}
	
	/* Test for hit */
	hit_result = hit_roll(attack_mod, 0, source_player(), source_none(), true);
	
	/* Make some noise */
	p->stealth_score -= noise;
	
	/* Determine damage */
	if (hit_result > 0) {
		int dummy;
		int crit_bonus_dice = crit_bonus(p, hit_result, weapon->weight, race,
										 SKILL_MELEE, false);
		int dam = damroll(p->state.mdd + crit_bonus_dice, mds);
		int prt = damroll(pd, 4);
		int prt_percent = prt_after_sharpness(p, weapon, &dummy);
		prt = (prt * prt_percent) / 100;
		net_dam = MAX(dam - prt, 0);
		
		event_signal_combat_damage(EVENT_COMBAT_DAMAGE,
								   p->state.mdd + crit_bonus_dice, mds, dam,
								   pd, 4, prt, prt_percent, PROJ_HURT, true);
	}


	/* If you succeed in prising out a Silmaril... */
	if (net_dam > 0) {
		freed = true;
		
		switch (obj->pval) {
			case 3: {
				break;
			}
			case 2: {
				if (!p->crown_shatter && one_in_(2)) {
					shatter_weapon(p, 2);
					freed = false;
				}
				break;
			}
			case 1: {
				if (!p->crown_shatter) {
					shatter_weapon(p, 3);
					freed = false;
				} else {
					p->cursed = true;
				}
				break;
			}
		}
		
		if (freed) {
			struct object *sil = object_new();
			struct object_kind *kind = lookup_kind(TV_LIGHT,
												   lookup_sval(TV_LIGHT, "Silmaril"));

			/* Crown has one less silmaril */
			obj->pval--;

			/* Report success */
			msg(freed_msg);

			/* Make Silmaril */
			object_prep(sil, kind, z_info->dun_depth, RANDOMISE);

			/* Get it */
			inven_carry(p, sil, true, true);

			/* Break the truce (always) */
			break_truce(p, true);

			/* Add a note to the notes file */
			history_add(p, "Cut a Silmaril from Morgoth's crown.",
						HIST_SILMARIL);
		}
	} else {
		/* If you fail to prise out a Silmaril... */
		msg("Try though you might, you were unable to free a Silmaril.");
		msg("Perhaps you should try again or use a different weapon.");

		if (pd == 15) {
			msg("(The combat rolls window shows what is happening.)");
		}

		/* Break the truce if creatures see */
		break_truce(p, false);
	}

	/* Check for taking of final Silmaril */
	if ((pd == 30) && freed) {
		msg("Until you escape you must now roll twice for every skill check, taking the worse result each time.");
		msg("You hear a cry of veangance echo through the iron hells.");
		wake_all_monsters(p);
	}
}

/**
 * Counts the player's silmarils
 */
int silmarils_possessed(struct player *p)
{
	int silmarils = 0;
	struct object *obj;

	for (obj = p->gear; obj; obj = obj->next) {
		if (tval_is_light(obj) && of_has(obj->flags, OF_NO_FUEL) &&
			(obj->pval == 7)) {
			silmarils += obj->number;
		}
		if (obj->artifact && streq(obj->artifact->name, "of Morgoth")) {
			silmarils += obj->pval;
		}
	}

	return silmarils;
}
