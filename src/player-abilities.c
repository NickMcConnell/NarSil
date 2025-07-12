/**
 * \file player-abilities.c 
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

#include "angband.h"
#include "datafile.h"
#include "game-input.h"
#include "init.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-util.h"

struct ability *abilities;

/**
 * ------------------------------------------------------------------------
 * Initialize abilities
 * ------------------------------------------------------------------------ */
/* Temporary list to store prerequisite details; 199 should be enough */
static struct {
	uint8_t skill;
	const char *name;
} prereq_list[100];
static unsigned int prereq_num = 1;

static unsigned int skill_index;

static enum parser_error parse_ability_skill(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	int index = lookup_skill(name);
	if (index < 0)
		return PARSE_ERROR_UNRECOGNISED_SKILL;

	skill_index = (unsigned int) index;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ability_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct ability *last = parser_priv(p);
	struct ability *a = mem_zalloc(sizeof *a);

	if (last) {
		last->next = a;
	} else {
		abilities = a;
	}
	parser_setpriv(p, a);
	a->name = string_make(name);
	a->skill = skill_index;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ability_level(struct parser *p) {
	struct ability *a = parser_priv(p);
	if (!a)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	a->level = parser_getint(p, "level");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ability_prereq(struct parser *p) {
	int skill = lookup_skill(parser_getsym(p, "skill"));
	const char *name = parser_getsym(p, "ability");
	struct ability *a = parser_priv(p);
	int i;

	if (!a)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (skill < 0)
		return PARSE_ERROR_INVALID_SKILL;

	/* Store the prereq details locally */
	prereq_list[prereq_num].skill = skill;
	prereq_list[prereq_num].name = string_make(name);

	/* Store the index in the struct */
	for (i = 0; i < MAX_PREREQS; i++) {
		if (!a->prereq_index[i]) break;
	}
	if (i == MAX_PREREQS)
		return PARSE_ERROR_TOO_MANY_ABILITY_PREREQS;
	a->prereq_index[i] = prereq_num;
	prereq_num++;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ability_type(struct parser *p) {
	struct poss_item *poss;
	int i;
	int tval = tval_find_idx(parser_getstr(p, "tval"));
	bool found_one_kind = false;

	struct ability *a = parser_priv(p);
	if (!a)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (tval < 0)
		return PARSE_ERROR_UNRECOGNISED_TVAL;

	/* Find all the right object kinds */
	for (i = 0; i < z_info->k_max; i++) {
		if (k_info[i].tval != tval) continue;
		poss = mem_zalloc(sizeof(struct poss_item));
		poss->kidx = i;
		poss->next = a->poss_items;
		a->poss_items = poss;
		found_one_kind = true;
	}

	if (!found_one_kind)
		return PARSE_ERROR_NO_KIND_FOR_ABILITY;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ability_item(struct parser *p) {
	struct poss_item *poss;
	int tval = tval_find_idx(parser_getsym(p, "tval"));
	int sval = lookup_sval(tval, parser_getsym(p, "sval"));

	struct ability *a = parser_priv(p);
	if (!a)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (tval < 0)
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	if (sval < 0)
		return PARSE_ERROR_UNRECOGNISED_SVAL;

	poss = mem_zalloc(sizeof(struct poss_item));
	poss->kidx = lookup_kind(tval, sval)->kidx;
	poss->next = a->poss_items;
	a->poss_items = poss;

	if (poss->kidx <= 0)
		return PARSE_ERROR_INVALID_ITEM_NUMBER;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ability_desc(struct parser *p) {
	struct ability *a = parser_priv(p);
	if (!a)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	a->desc = string_append(a->desc, parser_getstr(p, "desc"));
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_ability(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	prereq_num = 1;
	parser_reg(p, "skill str name", parse_ability_skill);
	parser_reg(p, "name str name", parse_ability_name);
	parser_reg(p, "level int level", parse_ability_level);
	parser_reg(p, "prerequisite sym skill sym ability", parse_ability_prereq);
	parser_reg(p, "type str tval", parse_ability_type);
	parser_reg(p, "item sym tval sym sval", parse_ability_item);
	parser_reg(p, "desc str desc", parse_ability_desc);
	return p;
}

static errr run_parse_ability(struct parser *p) {
	return parse_file_quit_not_found(p, "ability");
}

static errr finish_parse_ability(struct parser *p) {
	struct ability *a;

	/* Fill in prerequisite abilities */
	for (a = abilities; a; a = a->next) {
		int i = 0;
		while (a->prereq_index[i]) {
			int idx = a->prereq_index[i];
			struct ability *pre = mem_zalloc(sizeof *pre);
			struct ability *stored = lookup_ability(prereq_list[idx].skill,
													prereq_list[idx].name);
			if (!stored)
				return PARSE_ERROR_INVALID_ABILITY;
			memcpy(pre, stored, sizeof(*pre));
			pre->next = a->prerequisites;
			a->prerequisites = pre;
			i++;
		}
	}

	/* Done with prereq_list so release the resources allocated for it. */
	while (prereq_num > 1) {
		--prereq_num;
		string_free((char*)prereq_list[prereq_num].name);
		prereq_list[prereq_num].name = NULL;
	}

	parser_destroy(p);
	return 0;
}

static void cleanup_ability(void)
{
	struct ability *a = abilities;
	while (a) {
		struct ability *a_next = a->next;
		struct ability *pre = a->prerequisites;
		struct poss_item *poss = a->poss_items;
		while (poss) {
			struct poss_item *poss_next = poss->next;
			mem_free(poss);
			poss = poss_next;
		}
		while (pre) {
			struct ability *pre_next = pre->next;
			mem_free(pre);
			pre = pre_next;
		}
		string_free(a->name);
		string_free(a->desc);
		mem_free(a);
		a = a_next;
	}
}

struct file_parser ability_parser = {
	"ability",
	init_parse_ability,
	run_parse_ability,
	finish_parse_ability,
	cleanup_ability
};

/**
 * ------------------------------------------------------------------------
 * Ability utilities
 * ------------------------------------------------------------------------ */
/**
 * Find an ability given its name and skill
 */
struct ability *lookup_ability(int skill, const char *name)
{
	struct ability *ability = abilities;
	if (skill < 0) {
		msg("Invalid skill index passed to lookup_ability()!");
		return NULL;
	}
	while (ability) {
		if ((ability->skill == skill) && streq(ability->name, name)) {
			return ability;
		}
		ability = ability->next;
	}
	return NULL;
}

/**
 * Counts the abilities for a given skill in a set.
 * If the skill is SKILL_MAX, count all abilities.
 */
static int count_abilities(struct ability *ability, int skill)
{
	int count = 0;
	assert(0 <= skill && skill <= SKILL_MAX);
	while (ability) {
		if ((skill == SKILL_MAX) || (skill == ability->skill)) {
			count++;
		}
		ability = ability->next;
	}
	return count;
}

static bool ability_is_active(const struct ability *ability)
{
	return ability->active;
}

static int test_ability(const char *name, struct ability *test,
						ability_predicate pred)
{
	int skill, count = 0;
	bool found = false;

	if (!test) return 0;

	/* Look in every skill for an ability with the right name */
	for (skill = 0; skill < SKILL_MAX; skill++) {
		struct ability *ability = lookup_ability(skill, name);
		if (ability) {
			struct ability *thisa = test;

			/* Note that we have found an ability of that name */
			found = true;

			/* See if the provided ability list contains the named one... */
			while (thisa) {
				if (streq(thisa->name, name) && (thisa->skill == skill)) {
					/* ...and if so, if it satisfies any required condition */
					if (!pred || (pred && pred(thisa))) {
						count++;
					}
				}
				thisa = thisa->next;
			}
		}
	}
	if (!found) {
		assert(0);
	}
	return count;
}

/**
 * Does the given object type support the given ability type?
 */
bool applicable_ability(struct ability *ability, struct object *obj)
{
	struct poss_item *poss = ability->poss_items;

	for (poss = ability->poss_items; poss; poss = poss->next) {
		if (poss->kidx == obj->kind->kidx) return true;
	}		

	/* Throwing Mastery is OK for throwing items */
	if (of_has(obj->flags, OF_THROWING) && (ability->skill == SKILL_MELEE) &&
		streq(ability->name, "Throwing Mastery")) {
		return true;
	}

	return false;
}

/**
 * Reports if a given ability is already in a set of abilities.
 */
struct ability *locate_ability(struct ability *ability, struct ability *test)
{
	/* Look for the right one */
	while (ability) {
		if ((ability->skill == test->skill) &&
			streq(ability->name, test->name)) break;
		ability = ability->next;
	}
	return ability;
}

/**
 * Adds a given ability to a set of abilities.
 */
void add_ability(struct ability **set, struct ability *add)
{	
	struct ability *new;

	/* Check if we have it already */
	new = *set;
	if (locate_ability(new, add)) return;

	/* Not found, add the new one */
	new = mem_zalloc(sizeof(*new));
	memcpy(new, add, sizeof(*new));
	new->next = *set;
	*set = new;
}

/**
 * Activates a given ability in a set of abilities.
 */
void activate_ability(struct ability **set, struct ability *activate)
{
	struct ability *ability;
	for (ability = *set; ability; ability = ability->next) {
		if (streq(ability->name, activate->name)) {
			ability->active = true;
			break;
		}
	}
}

/**
 * Removes a given ability from a set of abilities.
 */
void remove_ability(struct ability **ability, struct ability *remove)
{
	struct ability *current = *ability, *prev = NULL, *next = NULL;

	/* Look for the ability to remove */
	while (current) {
		next = current->next;
		if ((current->skill == remove->skill) &&
			streq(current->name, remove->name)) {
			break;
		}
		prev = current;
		current = next;
	}

	/* Excise the ability if we have it */
	if (current) {
		if (prev) {
			/* We're removing an ability from the middle or end of the list */
			prev->next = next;
		} else {
			/* We're removing the head ability and replacing it with the next */
			*ability = next;
		}
		mem_free(current);
	}
}

bool player_has_ability(struct player *p, struct ability *ability)
{
	if (!ability) return false;
	if (locate_ability(p->abilities, ability)) return true;
	if (locate_ability(p->item_abilities, ability)) return true;
	return false;
}

int player_active_ability(struct player *p, const char *name)
{
	int count;
	if (!p) return 0;
	count = test_ability(name, p->abilities, ability_is_active);
	count += test_ability(name, p->item_abilities, ability_is_active);
	return count;
}

bool player_has_prereq_abilities(struct player *p, struct ability *ability)
{
	struct ability *prereqs = ability->prerequisites;
	if (prereqs) {
		while (prereqs) {
			struct ability *possessed = p->abilities;
			while (possessed) {
				if (streq(possessed->name, prereqs->name) &&
					(possessed->skill == prereqs->skill)) {
					return true;
				}
				possessed = possessed->next;
			}
			prereqs = prereqs->next;
		}
		return false;
	}
	return true;
}

/**
 * Ability cost is based on race and class affinity for the relevant skill,
 * the number of abilities already gained from that skill.
 */
int player_ability_cost(struct player *p, struct ability *ability)
{
	int skill = ability->skill;
	int num = count_abilities(p->abilities, skill);
	int cost = (num + 1) * z_info->ability_cost;
	int affinity = p->race->skill_adj[skill] + p->house->skill_adj[skill];
	cost -= affinity * z_info->ability_cost;
	return MAX(0, cost);
}

bool player_can_gain_ability(struct player *p, struct ability *ability)
{
	return player_ability_cost(p, ability) <= p->new_exp;
}

bool player_gain_ability(struct player *p, struct ability *ability)
{
	struct ability *new;
	int cost = player_ability_cost(p, ability);
	if (cost > p->new_exp) {
		msg("You do not have enough experience to acquire this ability.");
		return false;
	}
	if (!get_check("Are you sure you wish to gain this ability? ")) {
		return false;
	}
	p->new_exp -= cost;
	add_ability(&p->abilities, ability);
	new = locate_ability(p->abilities, ability);
	new->active = true;
	/*
	 * For some abilities, updating the bonuses is necessary; for some it
	 * is not.  Having that indicated in ability.txt seems like overkill
	 * to avoid an update_bonuses() call.
	 */
	p->upkeep->update |= (PU_BONUS);
	p->upkeep->redraw |= (PR_EXP);
	return true;
}

/**
 * Release a linked list of abilities where each entry in the list is a
 * shallow copy, except for the next field, of an entry in the global abilities
 * list.
 *
 * \param head points to the first entry in the list.
 */
void release_ability_list(struct ability *head)
{
	while (head) {
		struct ability *tgt = head;

		head = head->next;
		mem_free(tgt);
	}
}

/**
 * Copy a linked list of abilities where each entry in the list is a
 * shallow copy, except for the next field, of an entry in the global abilities
 * list.
 *
 * \param head points to the first entry in the list.
 * \return the pointer to the first entry in the copied list.
 */
struct ability *copy_ability_list(const struct ability *head)
{
	struct ability *dest_head, *dest_tail;

	if (!head) {
		return NULL;
	}

	dest_head = mem_alloc(sizeof(*dest_head));
	memcpy(dest_head, head, sizeof(*dest_head));
	dest_tail = dest_head;
	while (head->next) {
		head = head->next;
		dest_tail->next = mem_alloc(sizeof(*(dest_tail->next)));
		dest_tail = dest_tail->next;
		memcpy(dest_tail, head, sizeof(*dest_tail));
	}
	dest_tail->next = NULL;
	return dest_head;
}
