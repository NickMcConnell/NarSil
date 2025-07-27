/**
 * \file obj-slays.c
 * \brief Functions for manipulating slays/brands
 *
 * Copyright (c) 2010 Chris Carr and Peter Denison
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
#include "init.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-init.h"
#include "obj-knowledge.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-timed.h"


struct slay *slays;
struct brand *brands;

int lookup_slay(const char *code)
{
	int i;
	for (i = 1; i < z_info->slay_max; i++) {
		if (streq(slays[i].code, code)) return i;
	}
	return -1;
}

int lookup_brand(const char *code)
{
	int i;
	for (i = 1; i < z_info->brand_max; i++) {
		if (streq(brands[i].code, code)) return i;
	}
	return -1;
}

/**
 * Check if two slays affect the same set of monsters
 */
bool same_monsters_slain(int slay1, int slay2)
{
	if (slays[slay1].race_flag != slays[slay2].race_flag) return false;
	return true;
}

/**
 * Add all the slays from one structure to another
 *
 * \param dest the address the slays are going to
 * \param source the slays being copied
 */
void copy_slays(bool **dest, bool *source)
{
	int i, j;

	/* Check structures */
	if (!source) return;
	if (!(*dest)) {
		*dest = mem_zalloc(z_info->slay_max * sizeof(bool));
	}

	/* Copy */
	for (i = 0; i < z_info->slay_max; i++) {
		(*dest)[i] |= source[i];
	}

	/* Check for duplicates */
	for (i = 0; i < z_info->slay_max; i++) {
		for (j = 0; j < i; j++) {
			if ((*dest)[i] && (*dest)[j] && same_monsters_slain(i, j)) {
				(*dest)[j] = false;
			}
		}
	}
}

/**
 * Add all the brands from one structure to another
 *
 * \param dest the address the brands are going to
 * \param source the brands being copied
 */
void copy_brands(bool **dest, bool *source)
{
	int i, j;

	/* Check structures */
	if (!source) return;
	if (!(*dest))
		*dest = mem_zalloc(z_info->brand_max * sizeof(bool));

	/* Copy */
	for (i = 0; i < z_info->brand_max; i++)
		(*dest)[i] |= source[i];

	/* Check for duplicates */
	for (i = 0; i < z_info->brand_max; i++) {
		for (j = 0; j < i; j++) {
			if ((*dest)[i] && (*dest)[j] &&
				streq(brands[i].name, brands[j].name)) {
				(*dest)[j] = false;
			}
		}
	}
}

/**
 * Return the number of brands present
 *
 * \param brands_on is an array of z_info->brand_max booleans indicating
 * whether each brand is present
 */
int brand_count(const bool *brands_on)
{
	int i, count = 0;

	/* Count the brands */
	for (i = 0; i < z_info->brand_max; i++) {
		if (brands_on[i]) {
			count++;
		}
	}

	return count;
}


/**
 * Return the number of slays present
 *
 * \param slays_on is an array of z_info->slay_max booleans indicating whether
 * each slay is present
 */
int slay_count(const bool *slays_on)
{
	int i, count = 0;

	/* Count the slays */
	for (i = 0; i < z_info->slay_max; i++) {
		if (slays_on[i]) {
			count++;
		}
	}

	return count;
}

/**
 * React to slays which hurt a monster
 * 
 * \param slay is the slay we're testing for effectiveness
 * \param mon is the monster we're testing for being slain
 */
bool react_to_slay(struct slay *slay, const struct monster *mon)
{
	if (!slay->name) return false;
	if (!mon->race) return false;

	/* Check the race flag */
	if (rf_has(mon->race->flags, slay->race_flag))
		return true;

	return false;
}


/**
 * Extract the bonus dice from a given object hitting a given monster.
 *
 * \param p is the player performing the attack
 * \param obj is the object being used to attack
 * \param mon is the monster being attacked
 * \param slay is, if any slay affects the result, dereferenced and set to the
 * index of the last slay affecting the result
 * \param brand is, if any brand affects the result, dereferenced and set to
 * index of the last brand affecting the result
 */
int slay_bonus(struct player *p, struct object *obj, const struct monster *mon,
			   int *slay, int *brand)
{
	int i, dice = 0;
	bool scare = false;
	struct monster_lore *lore = get_lore(mon->race);

	if (!obj) return dice;

	/* Brands */
	for (i = 1; i < z_info->brand_max; i++) {
		struct brand *b = &brands[i];

		/* Is the object branded? */
		if (!obj->brands || !obj->brands[i]) continue;
 
		/* Is the monster vulnerable? */
		if (!rf_has(mon->race->flags, b->resist_flag)) {
			dice += b->dice;
			if (b->vuln_flag && rf_has(mon->race->flags, b->vuln_flag)) {
				dice += b->vuln_dice;
				scare = true;
			}
			*brand = i;
		} else {
			rf_on(lore->flags, b->resist_flag);
		}
	}

	/* Slays */
	for (i = 1; i < z_info->slay_max; i++) {
		struct slay *s = &slays[i];

		/* Does the object slay? */
		if (!obj->slays || !obj->slays[i]) continue;
 
		/* Is the monster vulnerable? */
		if (react_to_slay(s, mon)) {
			dice += s->dice;
			scare = true;
			*slay = i;
		} else {
			rf_on(lore->flags, s->race_flag);
		}
	}

	if (scare) {
		scare_onlooking_friends(mon, -20);
	}

	return dice;
}


/**
 * Print a message when a brand is identified by use.
 *
 * \param brand is the brand being noticed
 * \param mon is the monster being attacked
 * \return true if a message was printed; otherwise, return false
 */
static bool brand_message(struct brand *brand, const struct monster *mon)
{
	char buf[1024] = "\0";
	char m_name[80];

	/* Extract monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_TARG);

	/* See if we have a message */
	if (!brand->desc) return false;

	/* Insert */
	insert_name(buf, 1024, brand->desc, m_name);
	msg("%s", buf);
	return true;
}

/**
 * Help learn_brand_slay_{melee,launch,throw}().
 *
 * \param p is the player learning from the experience.
 * \param obj1 is an object directly involved in the attack.
 * \param obj2 is an auxiliary object (i.e. a launcher) involved in the attack.
 * \param mon is the monster being attacked.
 */
static void learn_brand_slay_helper(struct player *p, struct object *obj1,
		struct object *obj2, const struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);
	int i;

	/* Handle brands. */
	for (i = 1; i < z_info->brand_max; i++) {
		struct brand *b;
		bool learn = false;

		/* Check the objects directly involved. */
		if (obj1 && obj1->brands && obj1->brands[i]) {
			learn = true;
		}
		if (obj2 && obj2->brands && obj2->brands[i]) {
			learn = true;
		}
		if (!learn) continue;

		b = &brands[i];
		if (!b->resist_flag || !rf_has(mon->race->flags, b->resist_flag)) {
			/* Learn the brand */
			if (!player_knows_brand(p, i)) {
				player_learn_brand(p, i);
				brand_message(b, mon);
			}

			/* Learn about the monster. */
			if (b->resist_flag) {
				lore_learn_flag_if_visible(lore, mon,
					b->resist_flag);
			}
			if (b->vuln_flag) {
				lore_learn_flag_if_visible(lore, mon,
					b->vuln_flag);
			}
		} else if (player_knows_brand(p, i)) {
			/* Learn about the monster. */
			lore_learn_flag_if_visible(lore, mon, b->resist_flag);
		}
	}

	/* Handle slays. */
	for (i = 1; i < z_info->slay_max; ++i) {
		struct slay *s;
		bool learn = false;

		/* Check the objects directly involved. */
		if (obj1 && obj1->slays && obj1->slays[i]) {
			learn = true;
		}
		if (obj2 && obj2->slays && obj2->slays[i]) {
			learn = true;
		}
		if (!learn) {
			continue;
		}

		s = &slays[i];
		if (react_to_slay(s, mon)) {
			/* Learn about the monster. */
			lore_learn_flag_if_visible(lore, mon, s->race_flag);
			if (monster_is_visible(mon)) {
				/* Learn the slay */
				if (!player_knows_slay(p, i)) {
					char o_name[80];
					object_desc(o_name, sizeof(o_name), obj1, ODESC_BASE, p);
					msg("Your %s strikes truly.", o_name);
					player_learn_slay(p, i);
				}
			}
		} else if (player_knows_slay(p, i)) {
			/* Learn about unaffected monsters. */
			lore_learn_flag_if_visible(lore, mon, s->race_flag);
		}
	}
}


/**
 * Learn about object and monster properties related to slays and brands from
 * a melee attack.
 *
 * \param p is the player learning from the experience.
 * \param weapon is the equipped weapon used in the attack; this is a parameter
 * to allow for the possibility of dual-wielding or body types with multiple
 * equipped weapons.  May be NULL for an unarmed attack.
 * \param mon is the monster being attacked.
 */
void learn_brand_slay_from_melee(struct player *p, struct object *weapon,
		const struct monster *mon)
{
	learn_brand_slay_helper(p, weapon, NULL, mon);
}


/**
 * Learn about object and monster properties related to slays and brands
 * from a ranged attack with a missile launcher.
 *
 * \param p is the player learning from the experience.
 * \param missile is the missile used in the attack.  Must not be NULL.
 * \param launcher is the launcher used in the attack; this is a parameter
 * to allow for body types with multiple equipped launchers.  Must not be NULL.
 * \param mon is the monster being attacked.
 */
void learn_brand_slay_from_launch(struct player *p, struct object *missile,
		struct object *launcher, const struct monster *mon)
{
	assert(missile && launcher);
	learn_brand_slay_helper(p, missile, launcher, mon);
}


/**
 * Learn about object and monster properties related to slays and brands
 * from a ranged attack with a thrown object.
 *
 * \param p is the player learning from the experience.
 * \param missile is the thrown object used in the attack.  Must not be NULL.
 * \param mon is the monster being attacked.
 */
void learn_brand_slay_from_throw(struct player *p, struct object *missile,
		const struct monster *mon)
{
	assert(missile);
	learn_brand_slay_helper(p, missile, NULL, mon);
}
