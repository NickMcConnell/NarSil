/**
 * \file player-abilities.h
 * \brief Player abilities
 *
 * Copyright (c) 1997-2020 Ben Harrison, James E. Wilson, Robert A. Koeneke,
 * Nick McConnell
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

#ifndef INCLUDED_PLAYER_ABILITIES_H
#define INCLUDED_PLAYER_ABILITIES_H

/**
 * Ability
 */
struct ability {
	struct ability *next;
	char *name;
	char *desc;
	uint8_t skill;
	uint8_t level;
	bool active;
	bool last;
	struct ability *prerequisites;
	int prereq_index[MAX_PREREQS];		/* Temporary field for parsing */
	struct poss_item *poss_items;
};

extern struct ability *abilities;

/**
 * ability_predicate is a function pointer which tests a given ability to
 * see if the predicate in question is true.
 */
typedef bool (*ability_predicate)(const struct ability *test);

struct ability *lookup_ability(int skill, const char *name);
bool applicable_ability(struct ability *ability, struct object *obj);
struct ability *locate_ability(struct ability *ability, struct ability *test);
void add_ability(struct ability **set, struct ability *add);
void activate_ability(struct ability **set, struct ability *activate);
void remove_ability(struct ability **ability, struct ability *remove);
bool player_has_ability(struct player *p, const char *name);
int player_active_ability(struct player *p, const char *name);
bool player_has_prereq_abilities(struct player *p, struct ability *ability);
int player_ability_cost(struct player *p, struct ability *ability);
bool player_can_gain_ability(struct player *p, struct ability *ability);
bool player_gain_ability(struct player *p, struct ability *ability);
void release_ability_list(struct ability *head);
struct ability *copy_ability_list(const struct ability *head);

extern struct file_parser ability_parser;

#endif /* !INCLUDED_PLAYER_ABILITIES_H */
