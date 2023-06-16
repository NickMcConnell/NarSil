/**
 * \file player-calcs.c
 * \brief Player status calculation, signalling ui events based on 
 *	status changes.
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2014 Nick McConnell
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
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-msg.h"
#include "mon-util.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "songs.h"



/**
 * ------------------------------------------------------------------------
 * Melee calculations
 * ------------------------------------------------------------------------ */
/**
 * Determines the total melee damage dice (before criticals and slays)
 */
 
static uint8_t total_mdd(struct player *p, const struct object *obj)
{
	uint8_t dd;
	
	/* If no weapon is wielded, use 1d1 */
	if (!obj) {
		dd = 1;
	} else {
		/* Otherwise use the weapon dice */
		dd = obj->dd;
	}
	/* Add the modifiers */
	dd += p->state.to_mdd;
	
	return dd;
}

/**
 * Determines the total melee damage sides (from strength and to_mds)
 * Does include strength and weight modifiers
 *
 * Includes factors for strength and weight, but not bonuses from ring of
 * damage etc
 */
uint8_t total_mds(struct player *p, struct player_state *state,
				  const struct object *obj, int str_adjustment)
{
	uint8_t mds;
	int int_mds; /* to allow negative values in the intermediate stages */
	int str_to_mds;
	int divisor;
	
	str_to_mds = state->stat_use[STAT_STR] + str_adjustment;
		
	/* If no weapon, use 1d1 and don't limit strength bonus */
	if (!obj) {
		int_mds = 1;
		int_mds += str_to_mds;
	} else {
		/* If a weapon is being assessed, use its dice and limit bonus */
		int_mds = obj->ds;
		
		if (two_handed_melee(p)) {
			divisor = 10;

			/* Bonus for 'hand and a half' weapons like the bastard sword
			 * when used with two hands */
			int_mds += hand_and_a_half_bonus(p, obj);
		} else {
			divisor = 10;
		}
		
		/* Apply the Momentum ability */
		if (player_active_ability(p, "Momentum")) {
			divisor /= 2;
		}

		/* Limit the strength sides bonus by weapon weight */
		if ((str_to_mds > 0) && (str_to_mds > (obj->weight / divisor))) {
			int_mds += obj->weight / divisor;
		} else if ((str_to_mds < 0) && (str_to_mds < -(obj->weight / divisor))){
			int_mds += -(obj->weight / divisor);
		} else {
			int_mds += str_to_mds;
		}
	}

	/* Add generic damage bonus */
	int_mds += state->to_mds;

	/* Bonus for users of 'mighty blows' ability */
	if (player_active_ability(p, "Power")) {
		int_mds += 1;
	}

	/* Make sure the total is non-negative */
	mds = (int_mds < 0) ? 0 : int_mds;

	return mds;
}

/**
 * Bonus for 'hand and a half' weapons like the bastard sword when wielded
 * with two hands
 */
int hand_and_a_half_bonus(struct player *p, const struct object *obj)
{
	if (p && obj && obj->kind && of_has(obj->kind->flags, OF_HAND_AND_A_HALF) &&
		(equipped_item_by_slot_name(p, "weapon") == obj) &&
	    (equipped_item_by_slot_name(p, "arm") == NULL)) {
		return 2;
	}
	return 0;
}

/**
 * Two handed melee weapon (including bastard sword used two handed)
 */
bool two_handed_melee(struct player *p)
{
	struct object *obj = equipped_item_by_slot_name(p, "weapon");

	if (!obj) return false;
	if (of_has(obj->kind->flags, OF_TWO_HANDED) ||
		hand_and_a_half_bonus(p, obj)) {
		return true;
	}
	return false;
}

/**
 * Bonus for certain races/houses (elves) using blades
 */
int blade_bonus(struct player *p, const struct object *obj)
{
	if (player_has(p, PF_BLADE_PROFICIENCY) && (tval_is_sword(obj))) {
		return 1;
	}
	return 0;
}

/**
 * Bonus for certain races/houses (dwarves) using axes
 */
int axe_bonus(struct player *p, const struct object *obj)
{
	if (player_has(p, PF_AXE_PROFICIENCY) && of_has(obj->kind->flags, OF_AXE)) {
		return 1;
	}
	return 0;
}


/**
 * Bonus for people with polearm affinity
 */
int polearm_bonus(struct player *p, const struct object *obj)
{
	if (player_active_ability(p, "Polearm Mastery") &&
		of_has(obj->kind->flags, OF_POLEARM)) {
		return 1;
	}
	return 0;
}

/**
 * Determines the total damage side for archery
 * based on the weight of the bow, strength, and the sides of the bow
 */
 
uint8_t total_ads(struct player *p, struct player_state *state,
				  const struct object *obj, bool single_shot)
{
	uint8_t ads;
	int int_ads; /* to allow negative values in the intermediate stages */
	int str_to_ads;
	
	str_to_ads = state->stat_use[STAT_STR];

	if (player_active_ability(p, "Rapid Fire") && !single_shot) {
		str_to_ads -= 3;
	}

	int_ads = obj->ds;

	/* Limit the strength sides bonus by bow weight */
	if ((str_to_ads > 0) && (str_to_ads > (obj->weight / 10))) {
		int_ads += obj->weight / 10;
	} else if ((str_to_ads < 0) && (str_to_ads < -(obj->weight / 10))) {
		int_ads += -(obj->weight / 10);
	} else {
		int_ads += str_to_ads;
	}

	/* Add archery damage bonus */
	int_ads += state->to_ads;

	/* Make sure the total is non-negative */
	ads = (int_ads < 0) ? 0 : int_ads;

	return ads;
}

/**
 * ------------------------------------------------------------------------
 * 
 * ------------------------------------------------------------------------ */
/**
 * Decide which object comes earlier in the standard inventory listing,
 * defaulting to the first if nothing separates them.
 *
 * \return whether to replace the original object with the new one
 */
static bool earlier_object(struct object *orig, struct object *new)
{
	/* Check we have actual objects */
	if (!new) return false;
	if (!orig) return true;

	/* Usable ammo is before other ammo */
	if (tval_is_ammo(orig) && tval_is_ammo(new)) {
		/* First favour usable ammo */
		if ((player->state.ammo_tval == orig->tval) &&
			(player->state.ammo_tval != new->tval))
			return false;
		if ((player->state.ammo_tval != orig->tval) &&
			(player->state.ammo_tval == new->tval))
			return true;
	}

	/* Objects sort by decreasing type */
	if (orig->tval > new->tval) return false;
	if (orig->tval < new->tval) return true;

	/* Non-aware (flavored) items always come last (default to orig) */
	if (!object_flavor_is_aware(new)) return false;
	if (!object_flavor_is_aware(orig)) return true;

	/* Objects sort by increasing sval */
	if (orig->sval < new->sval) return false;
	if (orig->sval > new->sval) return true;

	/* Unaware objects always come last (default to orig) */
	if (new->kind->flavor && !object_flavor_is_aware(new)) return false;
	if (orig->kind->flavor && !object_flavor_is_aware(orig)) return true;

	/* Lights sort by decreasing fuel */
	if (tval_is_light(orig)) {
		if (orig->pval > new->pval) return false;
		if (orig->pval < new->pval) return true;
	}

	/* Objects sort by decreasing value, except ammo */
	if (tval_is_ammo(orig)) {
		if (object_value(orig) < object_value(new))
			return false;
		if (object_value(orig) > object_value(new))
			return true;
	} else {
		if (object_value(orig) > object_value(new))
			return false;
		if (object_value(orig) < object_value(new))
			return true;
	}

	/* No preference */
	return false;
}

int equipped_item_slot(struct player_body body, struct object *item)
{
	int i;

	if (item == NULL) return body.count;

	/* Look for an equipment slot with this item */
	for (i = 0; i < body.count; i++)
		if (item == body.slots[i].obj) break;

	/* Correct slot, or body.count if not equipped */
	return i;
}

/**
 * Put the player's inventory and quiver into easily accessible arrays.  The
 * pack may be overfull by one item
 */
void calc_inventory(struct player *p)//TODO make two quivers (= quiver slots?)
{
	int old_inven_cnt = p->upkeep->inven_cnt;
	int n_max = 1 + z_info->pack_size + z_info->quiver_size
		+ p->body.count;
	struct object **old_pack = mem_zalloc(z_info->pack_size
		* sizeof(*old_pack));
	bool *assigned = mem_alloc(n_max * sizeof(*assigned));
	struct object *current;
	int i, j;

	/*
	 * Equipped items are already taken care of.  Only the others need
	 * to be tested for assignment to the quiver or pack.
	 */
	for (current = p->gear, j = 0; current; current = current->next, ++j) {
		assert(j < n_max);
		assigned[j] = object_is_equipped(p->body, current)
			|| object_is_in_quiver(p, current);
	}
	for (; j < n_max; ++j) {
		assigned[j] = false;
	}

	/* Copy the current pack */
	for (i = 0; i < z_info->pack_size; i++) {
		old_pack[i] = p->upkeep->inven[i];
	}

	/* Prepare to fill the inventory */
	p->upkeep->inven_cnt = 0;

	for (i = 0; i <= z_info->pack_size; i++) {
		struct object *first = NULL;
		int jfirst = -1;

		/* Find the object that should go there. */
		j = 0;
		current = p->gear;
		while (1) {
			if (!current) break;
			assert(j < n_max);

			/* Consider it if it hasn't already been handled. */
			if (!assigned[j]) {
				/* Choose the first in order. */
				if (earlier_object(first, current)) {
					first = current;
					jfirst = j;
				}
			}

			current = current->next;
			++j;
		}

		/* Allocate */
		p->upkeep->inven[i] = first;
		if (first) {
			++p->upkeep->inven_cnt;
			assigned[jfirst] = true;
		}
	}

	/* Note reordering */
	if (character_dungeon && p->upkeep->inven_cnt == old_inven_cnt) {
		for (i = 0; i < z_info->pack_size; i++) {
			if (old_pack[i] && p->upkeep->inven[i] != old_pack[i]
				&& !object_is_equipped(p->body, old_pack[i])
				&& !object_is_in_quiver(p, old_pack[i])) {
				msg("You re-arrange your pack.");
				break;
			}
		}
	}

	mem_free(assigned);
	mem_free(old_pack);
}



/**
 * Calculate maximum voice.  You do not need to know any songs.
 *
 * This function induces status messages.
 */
void calc_voice(struct player *p, bool update)
{
	int i, msp, tmp; 

	/* Get voice value -  20 + a compounding 20% bonus per point of gra */
	tmp = 20 * 100;
	if (p->state.stat_use[STAT_GRA] >= 0) {
		for (i = 0; i < p->state.stat_use[STAT_GRA]; i++) {
			tmp = tmp * 12 / 10;
		}
	} else {
		for (i = 0; i < -(p->state.stat_use[STAT_GRA]); i++) {
			tmp = tmp * 10 / 12;
		}
	}
	msp = tmp / 100;

	/* Return if no updates */
	if (!update) return;

	/* Maximum voice has changed */
	if (p->msp != msp) {
		i = 100;

		/* Get percentage of maximum sp */
		if (p->msp) i = ((100 * p->csp) / p->msp);

		/* Save new limit */
		p->msp = msp;

		/* Update current sp */
		p->csp = ((i * p->msp) / 100) + (((i * p->msp) % 100 >= 50) ? 1 : 0);

		/* Enforce new limit */
		if (p->csp >= msp) {
			p->csp = msp;
		}

		/* Display mana later */
		p->upkeep->redraw |= (PR_MANA);
	}
}


/**
 * Calculate the players (maximal) hit points
 *
 * Adjust current hitpoints if necessary
 */
static void calc_hitpoints(struct player *p)
{
	int i, mhp, tmp; 

	/* Get voice value -  20 + a compounding 20% bonus per point of con */
	tmp = 20 * 100;
	if (p->state.stat_use[STAT_CON] >= 0) {
		for (i = 0; i < p->state.stat_use[STAT_CON]; i++) {
			tmp = tmp * 12 / 10;
		}
	} else {
		for (i = 0; i < -(p->state.stat_use[STAT_CON]); i++) {
			tmp = tmp * 10 / 12;
		}
	}
	mhp = tmp / 100;

	/* Maximum hitpoints has changed */
	if (p->mhp != mhp) {
		i = 100;

		/* Get percentage of maximum hp */
		if (p->mhp) i = ((100 * p->chp) / p->mhp);

		/* Save new limit */
		p->mhp = mhp;

		/* Update current hp */
		p->chp = ((i * p->mhp) / 100) + (((i * p->mhp) % 100 >= 50) ? 1 : 0);

		/* Enforce new limit */
		if (p->chp >= mhp) {
			p->chp = mhp;
		}

		/* Display mana later */
		p->upkeep->redraw |= (PR_HP);
	}
}


/**
 * Determine the radius of possibly flickering lights
 */
static int light_up_to(struct object *obj)
{ 
	int radius = obj->pval;
	
	/* Some lights flicker */
	if (of_has(obj->flags, OF_DARKNESS)) {
		while ((radius > -2) && one_in_(3)) {
			radius--;
		}
	} else if (obj->timeout < 100) {
		while ((radius > 0) && one_in_(3)) {
			radius--;
		}
	}

	return radius;
}

/**
 * Determines how much an enemy in a given location should make the sword glow
 */
static int hate_level(struct loc grid, int multiplier)
{
	int dist;

	/* Check distance of monster from player (by noise) */
	dist = MAX(flow_dist(cave->monster_noise, grid), 1);

	/* Determine the danger level */
	return (50 * multiplier) / dist;
}

/**
 * Determine whether a melee weapon is glowing in response to nearby enemies
 */
bool weapon_glows(struct object *obj)
{
	int i, total_hate = 0;
	struct loc grid, obj_grid;
	bool viewable = false;

	if (!character_dungeon) return false;

	/* Must be a melee weapon */
	if (!tval_is_melee_weapon(obj)) return false;

	/* Use the player's position where needed */
	obj_grid = loc_is_zero(obj->grid) ? player->grid : obj->grid;
	
	/* Out of LOS objects don't glow (or it can't be seen) */
	for (grid.y = obj_grid.y - 1; grid.y <= obj_grid.y + 1; grid.y++) {
		for (grid.x = obj_grid.x - 1; grid.x <= obj_grid.x + 1; grid.x++) {
			if (square_isview(cave, grid)) viewable = true;
		}
	}
	if (!viewable) return false;

	/* Create a 'flow' around the object */
	cave->monster_noise.centre = obj_grid;
	update_flow(cave, &cave->monster_noise, NULL);

	/* Add up the total of creatures vulnerable to the weapon's slays */
	for (i = 1; i < cave_monster_max(cave); i++) {
		bool target = false;
		int j, multiplier = 1;
		struct monster *mon = cave_monster(cave, i);
		struct monster_race *race = mon->race;

		/* Paranoia -- Skip dead monsters */
		if (!race) continue;

		/* Determine if a slay is applicable */
		for (j = 0; j < z_info->slay_max; j++) {
			if (obj->slays && obj->slays[j] &&
				rf_has(race->flags, slays[j].race_flag)) {
				target = true;
				break;
			}
		}

		/* Skip inapplicable monsters */
		if (!target) continue;

		/* Increase the effect for uniques */
		if (rf_has(race->flags, RF_UNIQUE)) multiplier *= 2;

		/* Increase the effect for individually occuring creatures */
		if (!monster_has_friends(mon))	multiplier *= 2;

		/* Add up the 'hate' */
		total_hate += hate_level(mon->grid, multiplier);
	}

	/* Add a similar effect for very nearby webs for spider slaying weapons */
	for (i = 0; i < z_info->slay_max; i++) {
		if (slays[i].race_flag == RF_SPIDER) break;
	}
	if (obj->slays && obj->slays[i]) {
		for (grid.y = obj_grid.y - 2; grid.y <= obj_grid.y + 2; grid.y++) {
			for (grid.x = obj_grid.x - 2; grid.x <= obj_grid.x + 2; grid.x++) {
				if (square_in_bounds(cave, grid) &&
					square_iswebbed(cave, grid)) {
					/* Add up the 'hate' */
					total_hate += hate_level(grid, 1);
				}
			}
		}
	}

	return total_hate >= 15;
}

/**
 * Calculate and set the current light radius.
 *
 * The light radius will be the total of all lights carried.
 */
void calc_light(struct player *p)
{
	int i;
	int new_light = 0;
	struct object *main_weapon = equipped_item_by_slot_name(player, "weapon");
	struct object *second_weapon = equipped_item_by_slot_name(player, "arm");
	struct song *trees = lookup_song("the Trees");

	/* Assume no light */
	new_light = 0;

	/* Examine all wielded objects */
	for (i = 0; i < p->body.count; i++) {
		struct object *obj = slot_object(p, i);

		/* Skip empty slots */
		if (!obj) continue;

		/* Does this item glow? */
		if (of_has(obj->flags, OF_LIGHT)) new_light++;

		/* Does this item create darkness? */
		if (of_has(obj->flags, OF_DARKNESS)) new_light--;

		/* Examine actual lights */
		if (tval_is_light(obj)) {
			/* Some items provide permanent, bright, light */
			if (of_has(obj->flags, OF_NO_FUEL)) {
				new_light += obj->pval;
			} else if (obj->timeout > 0) {
				/* Torches or lanterns (with fuel) provide some light */
				new_light += light_up_to(obj);
			}
		}
	}

	/* Increase radius when the player's weapon glows */
	if (main_weapon && weapon_glows(main_weapon)) new_light++;
	if (second_weapon && weapon_glows(second_weapon)) new_light++;

	/* Player is darkened */
	if (p->timed[TMD_DARKENED] && (new_light > 0)) new_light--;

	/* Smithing brightens the room a bit */
	if (p->upkeep->smithing) new_light += 2;

	/* Song of the Trees */
	if (player_is_singing(p, trees)) {
		new_light += song_bonus(p, p->state.skill_use[SKILL_SONG], trees);
	}

	/* Update the light radius and visuals if necessary */
	if (p->upkeep->cur_light != new_light) {
		p->upkeep->cur_light = new_light;
		p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	}

	return;
}

/**
 * Computes current weight limit in tenths of pounds.
 *
 * 100 pounds + a compounding 20% bonus per point of str
 */
int weight_limit(struct player_state state)
{
	int i;
	int limit = 1000;
	int str = state.stat_use[STAT_STR];
	
	if (str >= 0) {
		for (i = 0; i < str; i++) {
			limit = limit * 12 / 10;
		}
	} else {
		for (i = 0; i < -str; i++) {
			limit = limit * 10 / 12;
		}
	}

	/* Return the result */
	return limit;
}


/**
 * Computes weight remaining before burdened.
 */
int weight_remaining(struct player *p)
{
	return weight_limit(p->state) - p->upkeep->total_weight;
}


/**
 * Calculate the players current "state", taking into account
 * not only race/class intrinsics, but also objects being worn
 * and temporary spell effects.
 *
 * See also calc_mana() and calc_hitpoints().
 *
 * Take note of the new "speed code", in particular, a very strong
 * player will start slowing down as soon as he reaches 150 pounds,
 * but not until he reaches 450 pounds will he be half as fast as
 * a normal kobold.  This both hurts and helps the player, hurts
 * because in the old days a player could just avoid 300 pounds,
 * and helps because now carrying 300 pounds is not very painful.
 *
 * The "weapon" and "bow" do *not* add to the bonuses to hit or to
 * damage, since that would affect non-combat things.  These values
 * are actually added in later, at the appropriate place.
 *
 * If known_only is true, calc_bonuses() will only use the known
 * information of objects; thus it returns what the player _knows_
 * the character state to be.
 */
void calc_bonuses(struct player *p, struct player_state *state, bool known_only,
				  bool update)
{
	int i, j;
	struct object *launcher = equipped_item_by_slot_name(p, "shooting");
	struct object *weapon = equipped_item_by_slot_name(p, "weapon");
	struct object *off = equipped_item_by_slot_name(p, "arm");
	bitflag f[OF_SIZE];
	int armour_weight = 0;
	struct song *song;

	/* Remove off-hand weapons if you cannot wield them */
	if (!player_active_ability(p, "Two Weapon Fighting") &&
		off && tval_is_weapon(off)) {
		msg("You can no longer wield both weapons.");
		inven_takeoff(off);
	}

	/* Reset */
	memset(state, 0, sizeof *state);

	/* Set various defaults */
	state->speed = 2;
	state->el_info[ELEM_FIRE].res_level = 1;
	state->el_info[ELEM_COLD].res_level = 1;
	state->el_info[ELEM_POIS].res_level = 1;

	/* Extract race/house skill info */
	for (i = 0; i < SKILL_MAX; i++) {
		state->skill_misc_mod[i] = p->race->skill_adj[i]
			+ p->house->skill_adj[i];
	}

	/* Base pflags */
	pf_copy(state->pflags, p->race->pflags);

	/* Analyze equipment */
	for (i = 0; i < p->body.count; i++) {
		struct object *obj = slot_object(p, i);

		if (obj) {
			/* Extract the item flags */
			if (known_only) {
				object_flags_known(obj, f);
			} else {
				object_flags(obj, f);
			}

			/* Apply the item flags */
			for (j = 0; j < OF_MAX; j++) {
				if (of_has(f, j)) {
					state->flags[j]++;
				}
			}			

			/* Apply modifiers */
			for (j = 0; j < STAT_MAX; j++) {
				state->stat_equip_mod[j] +=	obj->modifiers[j];
			}
			for (j = 0; j < SKILL_MAX; j++) {
				state->skill_equip_mod[j] += obj->modifiers[STAT_MAX + j];
				if (j == SKILL_EVASION) {
					state->skill_equip_mod[j] += obj->evn;
				}
			}
			if (obj->modifiers[OBJ_MOD_DAMAGE_SIDES]) {
				state->to_mds += obj->modifiers[OBJ_MOD_DAMAGE_SIDES];
				state->to_ads += obj->modifiers[OBJ_MOD_DAMAGE_SIDES];
			}

			/* Apply element info, noting vulnerabilites for later processing */
			for (j = 0; j < ELEM_MAX; j++) {
				if (object_is_known(obj) || !known_only) {
					state->el_info[j].res_level += obj->el_info[j].res_level;
				}
			}

			/* Add up the armour weight */
			if (tval_is_armor(obj)) {
				armour_weight += obj->weight;
			}

			/* Do not apply weapon to-hit bonuses yet */
			if (tval_is_weapon(obj)) continue;

			/* Apply the bonus to hit */
			state->skill_equip_mod[SKILL_MELEE] += obj->att;
			state->skill_equip_mod[SKILL_ARCHERY] += obj->att;		
		}
	}

	/* Parrying grants extra bonus for weapon evasion */
	if (weapon && player_active_ability(p, "Parry")) {
		state->skill_equip_mod[SKILL_EVASION] += weapon->evn;
	}

	/* Deal with vulnerabilities and dark resistance */
	for (i = 0; i < ELEM_MAX; i++) {
		/* Represent overall vulnerabilities as negatives of the normal range */
		if (state->el_info[i].res_level < 1) {
			state->el_info[i].res_level -= 2;
		}

		/* Dark resistance depends only on the brightness of the player grid */
		if ((i == ELEM_DARK) && character_dungeon) {
			state->el_info[i].res_level = square_light(cave, p->grid);
		}
	}

	/* Ability stat boosts */
	state->stat_misc_mod[STAT_STR] += player_active_ability(p, "Strength");
	state->stat_misc_mod[STAT_DEX] += player_active_ability(p, "Dexterity");
	state->stat_misc_mod[STAT_CON] += player_active_ability(p, "Constitution");
	state->stat_misc_mod[STAT_GRA] += player_active_ability(p, "Grace");

	if (player_active_ability(p, "Strength in Adversity")) {
		/* If <= 50% health, give a bonus to strength and grace */
		if (health_level(p->chp, p->mhp) <= HEALTH_BADLY_WOUNDED) {
			state->stat_misc_mod[STAT_STR]++;
			state->stat_misc_mod[STAT_GRA]++;
		}

		/* If <= 25% health, give an extra bonus */
		if (health_level(p->chp, p->mhp) <= HEALTH_ALMOST_DEAD) {
			state->stat_misc_mod[STAT_STR]++;
			state->stat_misc_mod[STAT_GRA]++;
		}
	}

	/* Ability skill modifications */
	if (player_active_ability(p, "Rapid Attack")) {
		state->skill_misc_mod[SKILL_MELEE] -= 3;
	}
	if (player_active_ability(p, "Rapid Fire")) {
		state->skill_misc_mod[SKILL_ARCHERY] -= 3;
	}
	if (player_active_ability(p, "Poison Resistance")) {
		state->el_info[ELEM_POIS].res_level += 1;
	}

	/* Timed effects */
	if (player_timed_grade_eq(p, TMD_STUN, "Heavy Stun")) {
		for (i = 0; i < SKILL_MAX; i++) {
			state->skill_misc_mod[i] -= 4;
		}
	} else if (player_timed_grade_eq(p, TMD_STUN, "Stun")) {
		for (i = 0; i < SKILL_MAX; i++) {
			state->skill_misc_mod[i] -= 2;
		}
	}
	if (player_timed_grade_eq(p, TMD_FOOD, "Weak")) {
		state->stat_misc_mod[STAT_STR] -= 1;
	}
	if (p->timed[TMD_RAGE]) {
		state->stat_misc_mod[STAT_STR] += 1;
		state->stat_misc_mod[STAT_DEX] -= 1;
		state->stat_misc_mod[STAT_CON] += 1;
		state->stat_misc_mod[STAT_GRA] -= 1;
	}
	if (p->timed[TMD_STR]) {
		state->stat_misc_mod[STAT_STR] += 3;
		state->flags[OF_SUST_STR] += 1;
	}
	if (p->timed[TMD_DEX]) {
		state->stat_misc_mod[STAT_DEX] += 3;
		state->flags[OF_SUST_DEX] += 1;
	}
	if (p->timed[TMD_CON]) {
		state->stat_misc_mod[STAT_CON] += 3;
		state->flags[OF_SUST_CON] += 1;
	}
	if (p->timed[TMD_GRA]) {
		state->stat_misc_mod[STAT_GRA] += 3;
		state->flags[OF_SUST_GRA] += 1;
	}
	if (p->timed[TMD_FAST]) {
		state->speed += 1;
	}
	if (p->timed[TMD_SLOW]) {
		state->speed -= 1;
	}
	if (p->timed[TMD_SINVIS]) {
		state->flags[OF_SEE_INVIS] += 1;
		state->flags[OF_PROT_BLIND] += 1;
		state->flags[OF_PROT_HALLU] += 1;
	}

	/* Decrease food consumption with 'mind over body' ability */
	if (player_active_ability(p, "Mind Over Body")) {
		state->flags[OF_HUNGER] -= 1;
	}

	/* Protect from confusion, stunning, hallucinaton with 'clarity' ability */
	if (player_active_ability(p, "Clarity")) {
		state->flags[OF_PROT_CONF] += 1;
		state->flags[OF_PROT_STUN] += 1;
		state->flags[OF_PROT_HALLU] += 1;
	}

	/* Calculate stats */
	for (i = 0; i < STAT_MAX; i++) {
		state->stat_use[i] = p->stat_base[i] + state->stat_equip_mod[i]
			+ p->stat_drain[i] + state->stat_misc_mod[i];
		/* Hack for correct calculations during pre-birth */
		if (!p->body.name) {
			state->stat_use[i] += p->race->stat_adj[i]
				+ p->house->stat_adj[i];
		}

		/* Cap to -9 and 20 */
		state->stat_use[i] = MIN(state->stat_use[i], BASE_STAT_MAX);
		state->stat_use[i] = MAX(state->stat_use[i], BASE_STAT_MIN);
	}

	/* Analyze weight */
	j = p->upkeep->total_weight;
	i = weight_limit(*state);
	if (j > i) state->speed -= 1;

	state->speed += state->flags[OF_SPEED];

	/* Stealth slows the player down (unless they are passing) */
	if (p->stealth_mode) {
		if (p->previous_action[0] != ACTION_STAND) state->speed -= 1;
 	    state->skill_misc_mod[SKILL_STEALTH] += z_info->stealth_bonus;
	}

    /* Sprinting speeds the player up */
	if (player_is_sprinting(p)) {
		state->speed += 1;
	}

	/* Speed must lie between 1 and 3 */
	state->speed = MIN(3, MAX(1, state->speed));

	/* Increase food consumption if regenerating */
	if (state->flags[OF_REGEN]) state->flags[OF_HUNGER] += 1;

	/* Armour weight (not inventory weight) reduces stealth
	 * by 1 point per 10 pounds (rounding down) */
	state->skill_equip_mod[SKILL_STEALTH] -= armour_weight / 100;

	/* Penalise stealth based on song(s) being sung */
	state->skill_misc_mod[SKILL_STEALTH] -= player_song_noise(p);

	/*** Modify skills by ability scores ***/
	state->skill_stat_mod[SKILL_MELEE] = state->stat_use[STAT_DEX];
	state->skill_stat_mod[SKILL_ARCHERY] = state->stat_use[STAT_DEX];
	state->skill_stat_mod[SKILL_EVASION] = state->stat_use[STAT_DEX];
	state->skill_stat_mod[SKILL_STEALTH] = state->stat_use[STAT_DEX];
	state->skill_stat_mod[SKILL_PERCEPTION] = state->stat_use[STAT_GRA];
	state->skill_stat_mod[SKILL_WILL] = state->stat_use[STAT_GRA];
	state->skill_stat_mod[SKILL_SMITHING] = state->stat_use[STAT_GRA];
	state->skill_stat_mod[SKILL_SONG] = state->stat_use[STAT_GRA];

	/* Finalise song skill first as it modifies some other skills... */
	state->skill_use[SKILL_SONG] = p->skill_base[SKILL_SONG]
		+ state->skill_equip_mod[SKILL_SONG]
		+ state->skill_stat_mod[SKILL_SONG]
		+ state->skill_misc_mod[SKILL_SONG];

	/* Apply song effects that modify skills */
	song = lookup_song("Slaying");
	if (player_is_singing(p, song)) {
		int pskill = state->skill_use[SKILL_SONG];
		state->skill_misc_mod[SKILL_MELEE] += song_bonus(p, pskill, song);
		state->skill_misc_mod[SKILL_ARCHERY] += song_bonus(p, pskill, song);
	}
	song = lookup_song("Aule");
	if (player_is_singing(p, song)) {
		int pskill = state->skill_use[SKILL_SONG];
		state->skill_misc_mod[SKILL_SMITHING] += song_bonus(p, pskill, song);
	}
	song = lookup_song("Staying");
	if (player_is_singing(p, song)) {
		int pskill = state->skill_use[SKILL_SONG];
		state->skill_misc_mod[SKILL_WILL] += song_bonus(p, pskill, song);
	}
	song = lookup_song("Freedom");
	if (player_is_singing(p, song)) {
		state->flags[OF_FREE_ACT] += 1;
	}

	/* Analyze launcher */
	if (launcher) {
		state->skill_equip_mod[SKILL_ARCHERY] += launcher->att;
		state->ammo_tval = TV_ARROW;
		state->add = launcher->dd;
		state->ads = total_ads(p, state, launcher, false);
	}

	/* Analyze weapon */
	if (weapon) {
		/* Add the weapon's attack mod */
		state->skill_equip_mod[SKILL_MELEE] += weapon->att;

		/* Attack bonuses for matched weapon types */
		state->skill_misc_mod[SKILL_MELEE] += blade_bonus(p, weapon)
			+ axe_bonus(p, weapon) + polearm_bonus(p, weapon);
	}

	/* Deal with the 'Versatility' ability */
	if (player_active_ability(p, "Versatility") &&
		(p->skill_base[SKILL_ARCHERY] > p->skill_base[SKILL_MELEE])) {
		state->skill_misc_mod[SKILL_MELEE] +=
			(p->skill_base[SKILL_ARCHERY] - p->skill_base[SKILL_MELEE]) / 2;
	}

	/* Generate melee dice/sides from weapon, to_mdd, to_mds, strength */
	state->mdd = total_mdd(p, weapon);
	state->mds = total_mds(p, state, weapon,
						   player_active_ability(p, "Rapid Attack") ? -3 : 0);

	/* Determine the off-hand melee score, damage and sides */
	if (player_active_ability(p, "Two Weapon Fighting") && 
		off && tval_is_weapon(off)) {
		/* Remove main-hand specific bonuses */
		state->offhand_mel_mod -= weapon->att + blade_bonus(p, weapon)
			+ axe_bonus(p, weapon) + polearm_bonus(p, weapon);
		if (player_active_ability(p, "Rapid Attack")) {
			state->offhand_mel_mod += 3;
		}

		/* Add off-hand specific bonuses */
		state->offhand_mel_mod += off->att + blade_bonus(p, off)
			+ axe_bonus(p, off) + polearm_bonus(p, off) - 3;

		state->mdd2 = total_mdd(p, off);
		state->mds2 = total_mds(p, state, off, -3);
	}

	/* Entrancement or being knocked out sets total evasion score to -5 */
	if (p->timed[TMD_ENTRANCED] ||
		player_timed_grade_eq(p, TMD_STUN, "Knocked Out")) {
		state->skill_misc_mod[SKILL_EVASION] = -5
			- (p->skill_base[SKILL_EVASION]
			   + state->skill_equip_mod[SKILL_EVASION]
			   + state->skill_stat_mod[SKILL_EVASION]);
	}

	/* Finalise the non-song skills */
	for (i = 0; i < SKILL_SONG; i++) {
		state->skill_use[i] = p->skill_base[i] + state->skill_equip_mod[i]
			+ state->skill_stat_mod[i] + state->skill_misc_mod[i];
	}

	return;
}

/**
 * Calculate bonuses, and print various things on changes.
 */
static void update_bonuses(struct player *p)
{
	int i;

	struct player_state state = p->state;
	struct player_state known_state = p->known_state;


	/* ------------------------------------
	 * Calculate bonuses
	 * ------------------------------------ */

	calc_bonuses(p, &state, false, true);
	calc_bonuses(p, &known_state, true, true);


	/* ------------------------------------
	 * Notice changes
	 * ------------------------------------ */

	/* Analyze stats */
	for (i = 0; i < STAT_MAX; i++) {
		/* Notice changes */
		if (state.stat_use[i] != p->state.stat_use[i]) {
			/* Redisplay the stats later */
			p->upkeep->redraw |= (PR_STATS);

			/* Change in CON affects Hitpoints */
			if (i == STAT_CON)
				p->upkeep->update |= (PU_HP);

			/* Change in GRA affects voice */
			if (i == STAT_GRA)
				p->upkeep->update |= (PU_MANA);
		}
	}

	/* Hack -- See Invis Change */
	if (state.flags[OF_SEE_INVIS] != p->state.flags[OF_SEE_INVIS])
		/* Update monster visibility */
		p->upkeep->update |= (PU_MONSTERS);

	/* Redraw speed (if needed) */
	if (state.speed != p->state.speed)
		p->upkeep->redraw |= (PR_SPEED);

    /* Always redraw terrain */
    p->upkeep->redraw |= (PR_TERRAIN);

	/* Redraw melee (if needed) */
	if ((state.skill_use[SKILL_MELEE] != p->state.skill_use[SKILL_MELEE]) ||
		(state.mdd != p->state.mdd) || (state.mds != p->state.mds) ||
		(state.mdd2 != p->state.mdd2) || (state.mds2 != p->state.mds2)) {
		/* Redraw */
		p->upkeep->redraw |= (PR_MELEE);
	}

	/* Redraw archery (if needed) */
	if ((state.skill_use[SKILL_ARCHERY] != p->state.skill_use[SKILL_ARCHERY])
		|| (state.add != p->state.add) || (state.ads != p->state.ads)) {
		/* Redraw */
		p->upkeep->redraw |= (PR_ARC);
	}

	/* Redraw armor (if needed) */
	if (state.skill_use[SKILL_EVASION] != p->state.skill_use[SKILL_EVASION]) {
		/* Redraw */
		p->upkeep->redraw |= (PR_ARMOR);
	}

	memcpy(&p->state, &state, sizeof(state));
	memcpy(&p->known_state, &known_state, sizeof(known_state));

	/* Propagate knowledge */
	update_player_object_knowledge(p);
	if (player_active_ability(p, "Lore-Keeper")) {
		pseudo_id_everything();
	}

	/* Identify {special} items when the type has been seen before */
	if (character_dungeon) {
		id_known_specials();
	}
}




/**
 * ------------------------------------------------------------------------
 * Monster and object tracking functions
 * ------------------------------------------------------------------------ */

/**
 * Track the given monster
 */
void health_track(struct player_upkeep *upkeep, struct monster *mon)
{
	upkeep->health_who = mon;
	upkeep->redraw |= PR_HEALTH;
}

/**
 * Track the given monster race
 */
void monster_race_track(struct player_upkeep *upkeep, struct monster_race *race)
{
	/* Don't track when hallucinating or raging */
	if (player->timed[TMD_IMAGE] || player->timed[TMD_RAGE]) return;

	/* Save this monster ID */
	upkeep->monster_race = race;

	/* Window stuff */
	upkeep->redraw |= (PR_MONSTER);
}

/**
 * Track the given object
 */
void track_object(struct player_upkeep *upkeep, struct object *obj)
{
	upkeep->object = obj;
	upkeep->object_kind = NULL;
	upkeep->redraw |= (PR_OBJECT);
}

/**
 * Track the given object kind
 */
void track_object_kind(struct player_upkeep *upkeep, struct object_kind *kind)
{
	upkeep->object = NULL;
	upkeep->object_kind = kind;
	upkeep->redraw |= (PR_OBJECT);
}

/**
 * Cancel all object tracking
 */
void track_object_cancel(struct player_upkeep *upkeep)
{
	upkeep->object = NULL;
	upkeep->object_kind = NULL;
	upkeep->redraw |= (PR_OBJECT);
}

/**
 * Is the given item tracked?
 */
bool tracked_object_is(struct player_upkeep *upkeep, struct object *obj)
{
	return (upkeep->object == obj);
}



/**
 * ------------------------------------------------------------------------
 * Generic "deal with" functions
 * ------------------------------------------------------------------------ */

/**
 * Handle "player->upkeep->notice"
 */
void notice_stuff(struct player *p)
{
	/* Notice stuff */
	if (!p->upkeep->notice) return;

	/* Deal with ignore stuff */
	if (p->upkeep->notice & PN_IGNORE) {
		p->upkeep->notice &= ~(PN_IGNORE);
		ignore_drop(p);
	}

	/* Combine the pack */
	if (p->upkeep->notice & PN_COMBINE) {
		p->upkeep->notice &= ~(PN_COMBINE);
		combine_pack(p);
	}

	/* Dump the monster messages */
	if (p->upkeep->notice & PN_MON_MESSAGE) {
		p->upkeep->notice &= ~(PN_MON_MESSAGE);

		/* Make sure this comes after all of the monster messages */
		show_monster_messages();
	}
}

/**
 * Handle "player->upkeep->update"
 */
void update_stuff(struct player *p)
{
	/* Update stuff */
	if (!p->upkeep->update) return;


	if (p->upkeep->update & (PU_INVEN)) {
		p->upkeep->update &= ~(PU_INVEN);
		calc_inventory(p);
	}

	if (p->upkeep->update & (PU_BONUS)) {
		p->upkeep->update &= ~(PU_BONUS);
		update_bonuses(p);
	}

	if (p->upkeep->update & (PU_TORCH)) {
		p->upkeep->update &= ~(PU_TORCH);
		calc_light(p);
	}

	if (p->upkeep->update & (PU_HP)) {
		p->upkeep->update &= ~(PU_HP);
		calc_hitpoints(p);
	}

	if (p->upkeep->update & (PU_MANA)) {
		p->upkeep->update &= ~(PU_MANA);
		calc_voice(p, true);
	}

	/* Character is not ready yet, no map updates */
	if (!character_generated) return;

	/* Map is not shown, no map updates */
	if (!map_is_visible()) return;

	if (p->upkeep->update & (PU_UPDATE_VIEW)) {
		p->upkeep->update &= ~(PU_UPDATE_VIEW);
		update_view(cave, p);
	}

	if (p->upkeep->update & (PU_DISTANCE)) {
		p->upkeep->update &= ~(PU_DISTANCE);
		p->upkeep->update &= ~(PU_MONSTERS);
		update_monsters(true);
	}

	if (p->upkeep->update & (PU_MONSTERS)) {
		p->upkeep->update &= ~(PU_MONSTERS);
		update_monsters(false);
	}


	if (p->upkeep->update & (PU_PANEL)) {
		p->upkeep->update &= ~(PU_PANEL);
		event_signal(EVENT_PLAYERMOVED);
	}
}



struct flag_event_trigger
{
	uint32_t flag;
	game_event_type event;
};



/**
 * Events triggered by the various flags.
 */
static const struct flag_event_trigger redraw_events[] =
{
	{ PR_MISC,    EVENT_NAME },
	{ PR_EXP,     EVENT_EXPERIENCE },
	{ PR_STATS,   EVENT_STATS },
	{ PR_ARMOR,   EVENT_ARMOR },
	{ PR_HP,      EVENT_HP },
	{ PR_MANA,    EVENT_MANA },
	{ PR_SONG,    EVENT_SONG },
	{ PR_MELEE,   EVENT_MELEE },
	{ PR_ARC,     EVENT_ARCHERY },
	{ PR_HEALTH,  EVENT_MONSTERHEALTH },
	{ PR_DEPTH,   EVENT_DUNGEONLEVEL },
	{ PR_SPEED,   EVENT_PLAYERSPEED },
	{ PR_STATE,   EVENT_STATE },
	{ PR_STATUS,  EVENT_STATUS },
	{ PR_LIGHT,   EVENT_LIGHT },

	{ PR_INVEN,   EVENT_INVENTORY },
	{ PR_EQUIP,   EVENT_EQUIPMENT },
	{ PR_MONLIST, EVENT_MONSTERLIST },
	{ PR_ITEMLIST, EVENT_ITEMLIST },
	{ PR_MONSTER, EVENT_MONSTERTARGET },
	{ PR_OBJECT, EVENT_OBJECTTARGET },
	{ PR_MESSAGE, EVENT_MESSAGE },
	{ PR_COMBAT, EVENT_COMBAT_DISPLAY },
};

/**
 * Handle "player->upkeep->redraw"
 */
void redraw_stuff(struct player *p)
{
	size_t i;
	uint32_t redraw = p->upkeep->redraw;

	/* Redraw stuff */
	if (!redraw) return;

	/* Character is not ready yet, no screen updates */
	if (!character_generated) return;

	/* Map is not shown, subwindow updates only */
	if (!map_is_visible()) 
		redraw &= PR_SUBWINDOW;

	/* Hack - rarely update while resting or running, makes it over quicker */
	if (((player_resting_count(p) % 100) || (p->upkeep->running % 100))
		&& !(redraw & (PR_MESSAGE | PR_MAP)))
		return;

	/* For each listed flag, send the appropriate signal to the UI */
	for (i = 0; i < N_ELEMENTS(redraw_events); i++) {
		const struct flag_event_trigger *hnd = &redraw_events[i];

		if (redraw & hnd->flag)
			event_signal(hnd->event);
	}

	/* Then the ones that require parameters to be supplied. */
	if (redraw & PR_MAP) {
		/* Mark the whole map to be redrawn */
		event_signal_point(EVENT_MAP, -1, -1);
	}

	p->upkeep->redraw &= ~redraw;

	/* Map is not shown, subwindow updates only */
	if (!map_is_visible()) return;

	/*
	 * Do any plotting, etc. delayed from earlier - this set of updates
	 * is over.
	 */
	event_signal(EVENT_END);
}


/**
 * Handle "player->upkeep->update" and "player->upkeep->redraw"
 */
void handle_stuff(struct player *p)
{
	if (p->upkeep->update) update_stuff(p);
	if (p->upkeep->redraw) redraw_stuff(p);
}

