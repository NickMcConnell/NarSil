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
 * Count a set of brands
 * \param brands The brands to count.
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
 * Count a set of slays
 * \param slays The slays to count.
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
 * \param player is the player performing the attack
 * \param obj is the object being used to attack
 * \param mon is the monster being attacked
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
 * \param name is the monster name 
 */
bool brand_message(int brand, char *name, char *message, int len)
{
	char buf[1024] = "\0";

	/* See if we have a message */
	if (!brands[brand].desc) return false;

	/* Insert */
	insert_name(buf, 1024, brands[brand].desc, name);
	my_strcpy(message, buf, len);
	return true;
}

