/**
 * \file player-timed.c
 * \brief Timed effects handling
 *
 * Copyright (c) 1997 Ben Harrison
 * Copyright (c) 2007 Andi Sidwell
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
#include "datafile.h"
#include "effects.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-util.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-properties.h"
#include "obj-slays.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "songs.h"

int PY_FOOD_MAX;
int PY_FOOD_FULL;
int PY_FOOD_ALERT;
int PY_FOOD_WEAK;
int PY_FOOD_STARVE;

/**
 * ------------------------------------------------------------------------
 * Parsing functions for player_timed.txt
 * ------------------------------------------------------------------------ */

struct timed_effect_data timed_effects[TMD_MAX] = {
#define TMD(a, b, c)	{ #a, b, c, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, NULL, { 0 }, 0, false },
	#include "list-player-timed.h"
	#undef TMD
};

const char *obj_flags[] = {
	"NONE",
	#define OF(a, b) #a,
	#include "list-object-flags.h"
	#undef OF
	NULL
};

int timed_name_to_idx(const char *name)
{
    for (size_t i = 0; i < N_ELEMENTS(timed_effects); i++) {
        if (my_stricmp(name, timed_effects[i].name) == 0) {
            return i;
        }
    }

    return -1;
}

/**
 * List of timed effect names
 */
static const char *list_timed_effect_names[] = {
	#define TMD(a, b, c) #a,
	#include "list-player-timed.h"
	#undef TMD
	"MAX",
	NULL
};

static enum parser_error parse_player_timed_name(struct parser *p)
{
	const char *name = parser_getstr(p, "name");
	int index;

	if (grab_name("timed effect",
			name,
			list_timed_effect_names,
			N_ELEMENTS(list_timed_effect_names),
			&index)) {
		/* XXX not a desctiptive error */
		return PARSE_ERROR_INVALID_SPELL_NAME;
	}

	struct timed_effect_data *t = &timed_effects[index];

	t->index = index;
	t->fail = -1;
	parser_setpriv(p, t);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_desc(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);
	assert(t);

	t->desc = string_append(t->desc, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_end_message(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);
	assert(t);

	t->on_end = string_append(t->on_end, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_increase_message(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);
	assert(t);

	t->on_increase = string_append(t->on_increase, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_decrease_message(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);
	assert(t);

	t->on_decrease = string_append(t->on_decrease, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_change_increase(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);
	struct timed_change *current = t->increase;
	assert(t);

	/* Make a zero change structure if there isn't one */
	if (!current) {
		t->increase = mem_zalloc(sizeof(struct timed_change));
		current = t->increase;
	} else {
		/* Move to the highest change so far */
		while (current->next) {
			current = current->next;
		}
		current->next = mem_zalloc(sizeof(struct timed_change));
		current = current->next;
	}

	current->max = parser_getint(p, "max");
	current->msg = string_append(current->msg, parser_getsym(p, "msg"));
	if (parser_hasval(p, "inc_msg")) {
		current->inc_msg = string_append(current->inc_msg,
										 parser_getsym(p, "inc_msg"));
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_change_decrease(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);
	assert(t);

	t->decrease.max = parser_getint(p, "max");
	t->decrease.msg = string_make(parser_getsym(p, "msg"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_message_type(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);
	assert(t);

	t->msgt = message_lookup_by_name(parser_getsym(p, "type"));

	return t->msgt < 0 ?
				PARSE_ERROR_INVALID_MESSAGE :
				PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_fail(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);
	const char *name = parser_getstr(p, "flag");
	int flag = lookup_flag(obj_flags, name);
	assert(t);

	if (flag == FLAG_END) {
		return PARSE_ERROR_INVALID_FLAG;
	} else {
		t->fail = flag;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_grade(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);
	struct timed_grade *current = t->grade;
	struct timed_grade *l = mem_zalloc(sizeof(*l));
    const char *color = parser_getsym(p, "color");
    int attr = 0;
	assert(t);

	/* Make a zero grade structure if there isn't one */
	if (!current) {
		t->grade = mem_zalloc(sizeof(struct timed_grade));
		current = t->grade;
	}

	/* Move to the highest grade so far */
	while (current->next) {
		current = current->next;
	}

	/* Add the new one */
	current->next = l;
	l->grade = current->grade + 1;

    if (strlen(color) > 1) {
		attr = color_text_to_attr(color);
    } else {
		attr = color_char_to_attr(color[0]);
	}
    if (attr < 0)
		return PARSE_ERROR_INVALID_COLOR;
    l->color = attr;

	l->max = parser_getint(p, "max");
	l->name = string_make(parser_getsym(p, "name"));

	/* Name may be a dummy (eg hunger)*/
	if (strlen(l->name) == 1) {
		string_free(l->name);
		l->name = NULL;
	}

	l->up_msg = string_make(parser_getsym(p, "up_msg"));

	/* Message may be a dummy */
	if (strlen(l->up_msg) == 1) {
		string_free(l->up_msg);
		l->up_msg = NULL;
	}

	if (parser_hasval(p, "down_msg")) {
		l->down_msg = string_make(parser_getsym(p, "down_msg"));
	}

	/* Set food constants and deal with percentages */
	if (streq(t->name, "FOOD")) {
		if (streq(l->name, "Starving")) {
			PY_FOOD_STARVE = l->max;
		} else if (streq(l->name, "Weak")) {
			PY_FOOD_WEAK = l->max;
		} else if (streq(l->name, "Hungry")) {
			PY_FOOD_ALERT = l->max;
		} else if (streq(l->name, "Fed")) {
			PY_FOOD_FULL = l->max;
		} else if (streq(l->name, "Full")) {
			PY_FOOD_MAX = l->max;
		}
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_change_grade(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);
	struct timed_change_grade *current = t->c_grade;
	struct timed_change_grade *l = mem_zalloc(sizeof(*l));
    const char *color = parser_getsym(p, "color");
    int attr = 0;
	assert(t);

	/* Make a zero grade structure if there isn't one */
	if (!current) {
		t->c_grade = mem_zalloc(sizeof(struct timed_change_grade));
		current = t->c_grade;
	}

	/* Move to the highest grade so far */
	while (current->next) {
		current = current->next;
	}

	/* Add the new one */
	current->next = l;
	l->c_grade = current->c_grade + 1;

    if (strlen(color) > 1) {
		attr = color_text_to_attr(color);
    } else {
		attr = color_char_to_attr(color[0]);
	}
    if (attr < 0)
		return PARSE_ERROR_INVALID_COLOR;
    l->color = attr;

	l->max = parser_getint(p, "max");
	l->name = string_make(parser_getsym(p, "name"));

	/* Name may be a dummy (eg hunger)*/
	if (strlen(l->name) == 1) {
		string_free(l->name);
		l->name = NULL;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_resist(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);
	const char *name = parser_getsym(p, "elem");
	int idx = (name) ? proj_name_to_idx(name) : -1;

	assert(t);
	if (idx < 0 || idx >= ELEM_MAX) return PARSE_ERROR_INVALID_VALUE;
	t->temp_resist = idx;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_timed_este(struct parser *p)
{
	struct timed_effect_data *t = parser_priv(p);

	assert(t);
	t->este = parser_getuint(p, "value") ? true : false;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_player_timed(void)
{
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_player_timed_name);
	parser_reg(p, "desc str text", parse_player_timed_desc);
	parser_reg(p, "on-end str text", parse_player_timed_end_message);
	parser_reg(p, "on-increase str text", parse_player_timed_increase_message);
	parser_reg(p, "on-decrease str text", parse_player_timed_decrease_message);
	parser_reg(p, "change-inc int max sym msg ?sym inc_msg",
			   parse_player_timed_change_increase);
	parser_reg(p, "change-dec int max sym msg",
			   parse_player_timed_change_decrease);
	parser_reg(p, "msgt sym type", parse_player_timed_message_type);
	parser_reg(p, "fail str flag", parse_player_timed_fail);
	parser_reg(p, "grade sym color int max sym name sym up_msg ?sym down_msg",
			   parse_player_timed_grade);
	parser_reg(p, "change-grade sym color int max sym name",
			   parse_player_timed_change_grade);
	parser_reg(p, "resist sym elem", parse_player_timed_resist);
	parser_reg(p, "este uint value", parse_player_timed_este);
	return p;
}

static errr run_parse_player_timed(struct parser *p)
{
	return parse_file_quit_not_found(p, "player_timed");
}

static errr finish_parse_player_timed(struct parser *p)
{
	parser_destroy(p);
	return 0;
}

static void cleanup_player_timed(void)
{
	for (size_t i = 0; i < TMD_MAX; i++) {
		struct timed_effect_data *effect = &timed_effects[i];
		struct timed_grade *grade = effect->grade;
		struct timed_change_grade *c_grade = effect->c_grade;
		struct timed_change *increase = effect->increase;

		while (grade) {
			struct timed_grade *next = grade->next;
			string_free(grade->name);
			if (grade->up_msg) string_free(grade->up_msg);
			if (grade->down_msg) string_free(grade->down_msg);
			mem_free(grade);
			grade = next;
		}
		effect->grade = NULL;
		while (c_grade) {
			struct timed_change_grade *next = c_grade->next;
			string_free(c_grade->name);
			mem_free(c_grade);
			c_grade = next;
		}
		effect->c_grade = NULL;
		while (increase) {
			struct timed_change *next = increase->next;
			string_free(increase->msg);
			if (increase->inc_msg) string_free(increase->inc_msg);
			mem_free(increase);
			increase = next;
		}
		effect->increase = NULL;
		string_free(effect->decrease.msg);

		string_free(effect->desc);

		if (effect->on_end)
			string_free(effect->on_end);
		if (effect->on_increase)
			string_free(effect->on_increase);
		if (effect->on_decrease)
			string_free(effect->on_decrease);

		effect->desc        = NULL;
		effect->on_end      = NULL;
		effect->on_increase = NULL;
		effect->on_decrease = NULL;
	}
}

struct file_parser player_timed_parser = {
	"player timed effects",
	init_parse_player_timed,
	run_parse_player_timed,
	finish_parse_player_timed,
	cleanup_player_timed
};


/**
 * ------------------------------------------------------------------------
 * Utilities for more complex or anomolous effects
 * ------------------------------------------------------------------------ */
/**
 * Amount to decrement over time
 */
int player_timed_decrement_amount(struct player *p, int idx)
{
	struct song *este = lookup_song("Este");
	struct song *freedom = lookup_song("Freedom");
	int bonus_este = song_bonus(p, p->state.skill_use[SKILL_SONG], este);
	int bonus_freedom = song_bonus(p, p->state.skill_use[SKILL_SONG], freedom);
	int amount = 1;

	/* Adjust for songs */
	if (timed_effects[idx].este && player_is_singing(p, este)) {
		amount = bonus_este;
	}
	if ((idx == TMD_SLOW) && player_is_singing(p, freedom)) {
		amount = bonus_freedom;
	}

	/* Special cases */
	if ((idx == TMD_CUT) || (idx == TMD_POISONED)) {
		amount *= ((p->timed[idx] + 4) / 5);
		p->upkeep->redraw |= (PR_STATUS);
	}

	return amount;
}

/**
 * Effects on end of temporary boost
 */
static void player_timed_end_effect(int idx)
{
	bool fake;
	switch (idx) {
		case TMD_ENTRANCED: {
			player->upkeep->was_entranced = true;
			break;
		}
		case TMD_RAGE: {
			player->upkeep->redraw |= PR_MAP;
			break;
		}
		case TMD_STR: {
			effect_simple(EF_DRAIN_STAT, source_none(), "3", STAT_STR, 0, 0,
						  &fake);
			break;
		}
		case TMD_DEX: {
			effect_simple(EF_DRAIN_STAT, source_none(), "3", STAT_DEX, 0, 0,
						  &fake);
			break;
		}
		case TMD_CON: {
			effect_simple(EF_DRAIN_STAT, source_none(), "3", STAT_CON, 0, 0,
						  &fake);
			break;
		}
		case TMD_GRA: {
			effect_simple(EF_DRAIN_STAT, source_none(), "3", STAT_GRA, 0, 0,
						  &fake);
			break;
		}
		default: break;
	}
}

/**
 * Return true if the player timed effect matches the given string
 */
bool player_timed_grade_eq(const struct player *p, int idx, const char *match)
{
	if (p->timed[idx]) {
		const struct timed_grade *grade = timed_effects[idx].grade;
		while (p->timed[idx] > grade->max) {
			grade = grade->next;
		}
		if (grade->name && streq(grade->name, match)) return true;
	}

	return false;
}

/**
 * Return true if the player timed effect is at a gradation above the given
 * string.
 */
bool player_timed_grade_gt(const struct player *p, int idx, const char *match)
{
	if (p->timed[idx]) {
		const struct timed_grade *grade = timed_effects[idx].grade;
		while (1) {
			if (!grade) {
				break;
			}
			if (grade->name && streq(grade->name, match)) {
				return p->timed[idx] > grade->max;
			}
			grade = grade->next;
		}
	}
	return false;
}

/**
 * Return true if the player timed effect is not set or is at a gradation below
 * the given string.
 */
bool player_timed_grade_lt(const struct player *p, int idx, const char *match)
{
	if (p->timed[idx]) {
		const struct timed_grade *grade = timed_effects[idx].grade;
		const struct timed_grade *prev_grade = NULL;
		while (1) {
			if (!grade) {
				return false;
			}
			if (grade->name && streq(grade->name, match)) {
				return prev_grade
					&& p->timed[idx] <= prev_grade->max;
			}
			prev_grade = grade;
			grade = grade->next;
		}
	}
	return true;
}

/**
 * ------------------------------------------------------------------------
 * Setting, increasing, decreasing and clearing timed effects
 * ------------------------------------------------------------------------ */
/**
 * Set a timed effect.
 */
bool player_set_timed(struct player *p, int idx, int v, bool notify)
{
	assert(idx >= 0);
	assert(idx < TMD_MAX);

	struct timed_effect_data *effect = &timed_effects[idx];
	struct timed_grade *new_grade = effect->grade;
	struct timed_grade *current_grade = effect->grade;
	struct timed_change_grade *change_grade = effect->c_grade;
	struct object *weapon = equipped_item_by_slot_name(p, "weapon");
	struct timed_grade *blackout_grade = timed_effects[TMD_STUN].grade;

	/* Lower bound */
	v = MAX(v, (idx == TMD_FOOD) ? 1 : 0);

	/* No change */
	if (p->timed[idx] == v) {
		return false;
	}

	/* Don't increase stunning if stunning value is greater than 100.
	 * this is an effort to eliminate the "knocked out" instadeath. */
	if (blackout_grade->grade) {
		while (!streq(blackout_grade->name, "Heavy Stun")) {
			blackout_grade = blackout_grade->next;
		}
		if ((idx == TMD_STUN) && (p->timed[idx] > blackout_grade->max) &&
			(v > p->timed[idx])) {
			return false;
		}
	}

	/* Deal with graded effects */
	if (new_grade) {
		/* Find the grade we will be going to, and the current one */
		while (v > new_grade->max) {
			new_grade = new_grade->next;
			if (!new_grade->next) break;
		}
		while (p->timed[idx] > current_grade->max) {
			current_grade = current_grade->next;
			if (!current_grade->next) break;
		}

		/* Upper bound */
		v = MIN(v, new_grade->max);

		/* Knocked out */
		if ((idx == TMD_STUN) && v > 100) {
			p->timed[TMD_BLIND] = MAX(p->timed[TMD_BLIND], 2);
		}

		/* Always mention going up a grade, otherwise on request */
		if (new_grade->grade > current_grade->grade) {
			if ((timed_effects[idx].temp_resist != -1) &&
				player_resists(p, timed_effects[idx].temp_resist)) {
				/* Special message for temporary + permanent resist */
				print_custom_message(weapon, effect->on_increase, effect->msgt,
									 p);
			} else {			
				print_custom_message(weapon, new_grade->up_msg, effect->msgt,
									 p);
			}
			notify = true;
		} else if ((new_grade->grade < current_grade->grade) &&
				   (new_grade->down_msg)) {
			print_custom_message(weapon, new_grade->down_msg, effect->msgt, p);

			/* Special cases */
			if (idx == TMD_FOOD) ident_hunger(p);
			if ((idx == TMD_STUN) && (v < blackout_grade->max)) {
				msg("You wake up.");
				p->timed[TMD_BLIND] = MAX(p->timed[TMD_BLIND] - 1, 0);
			}

			notify = true;
		} else if (notify) {
			if (v == 0) {
				/* Finishing */
				print_custom_message(weapon, effect->on_end, MSG_RECOVER, p);
				player_timed_end_effect(idx);
			} else if (p->timed[idx] > v && effect->on_decrease) {
				/* Decrementing */
				print_custom_message(weapon, effect->on_decrease, effect->msgt,
									 p);
			}
		}
	} else {
		int change;

		/* There had better be a change grade */
		assert(change_grade);

		/* Find the change we will be using */
		change = v - p->timed[idx];

		/* Increase */
		if (change > 0) {
			struct timed_change *inc = effect->increase;
			while (change >= inc->max) {
				inc = inc->next;
			}
			if (p->timed[idx] && inc->inc_msg) {
				/* Increasing from existing effect, and increase message */
				msgt(effect->msgt, inc->inc_msg);
			} else {
				/* Effect starting, or no special increase message */
				msgt(effect->msgt, inc->msg);
			}
		} else {
			/* Decrease */
			int div = effect->decrease.max;
			if (-change > (p->timed[idx] + div - 1) / div) {
				msgt(effect->msgt, effect->decrease.msg);
			}
			/* Finishing */
			if (v == 0) {
				msgt(effect->msgt, effect->on_end);
			}
		}
	}

	/* Use the value */
	p->timed[idx] = v;

	if (notify) {
		/* Disturb */
		disturb(p, false);

		/* Update the visuals, as appropriate. */
		p->upkeep->update |= effect->flag_update;
		p->upkeep->redraw |= (PR_STATUS | effect->flag_redraw);

		/* Handle stuff */
		handle_stuff(p);
	}

	return notify;
}

/**
 * The saving throw is a will skill check.
 *
 * Note that the player is resisting and thus wins ties.
 */
bool player_saving_throw(struct player *p, struct monster *mon, int resistance)
{
	int player_skill = p->state.skill_use[SKILL_WILL];
	int difficulty = mon ? monster_skill(mon, SKILL_WILL) : 10;

	/* Adjust difficulty for resistance */
	difficulty -= 10 * resistance;

	if (mon) {
		return skill_check(source_monster(mon->midx), difficulty, player_skill,
						   source_player()) <= 0;
	}
	return skill_check(source_none(), difficulty, player_skill,
					   source_player()) <= 0;
}

/**
 * Check whether a timed effect will affect the player
 */
bool player_inc_check(struct player *p, int idx, bool lore)
{
	struct timed_effect_data *effect = &timed_effects[idx];
	int resistance = (effect->fail != -1) ? p->state.flags[effect->fail]: 0;
	struct monster *mon = cave->mon_current > 0 ?
		cave_monster(cave, cave->mon_current) : NULL;

	/* If we're only doing this for monster lore purposes */
	if (lore) {
		return (resistance == 0);
	}

	/* Check whether @ has resistance to this effect */
	if (resistance) {
		/* Possibly identify relevant items */
		ident_flag(p, effect->fail);
	}

	/* Determine whether the player saves */
	if (player_saving_throw(p, mon, resistance)) {
		return false;
	}

	return true;
}

/**
 * Increase the timed effect `idx` by `v`.  Mention this if `notify` is true.
 * Check for resistance to the effect if `check` is true.
 */
bool player_inc_timed(struct player *p, int idx, int v, bool notify, bool check)
{
	assert(idx >= 0);
	assert(idx < TMD_MAX);

	if (check == false || player_inc_check(p, idx, false) == true) {
		/* Entrancement should be non-cumulative */
		if (idx == TMD_ENTRANCED && p->timed[TMD_ENTRANCED] > 0) {
			return false;
		} else {
			return player_set_timed(p, idx, p->timed[idx] + v, notify);
		}
	}

	return false;
}

/**
 * Decrease the timed effect `idx` by `v`.  Mention this if `notify` is true.
 */
bool player_dec_timed(struct player *p, int idx, int v, bool notify)
{
	int new_value;
	assert(idx >= 0);
	assert(idx < TMD_MAX);
	new_value = p->timed[idx] - v;

	/* Obey `notify` if not finishing; if finishing, always notify */
	if (new_value > 0) {
		return player_set_timed(p, idx, new_value, notify);
	}
	return player_set_timed(p, idx, new_value, true);
}

/**
 * Clear the timed effect `idx`.  Mention this if `notify` is true.
 */
bool player_clear_timed(struct player *p, int idx, bool notify)
{
	assert(idx >= 0);
	assert(idx < TMD_MAX);

	return player_set_timed(p, idx, 0, notify);
}

/**
 * Hack to check whether a timed effect happened.
 */
bool player_timed_inc_happened(struct player *p, int old[], int len)
{
	int idx;
	assert(len == TMD_MAX);
	for (idx = 0; idx < len; idx++) {
		if (player->timed[idx] > old[idx]) return true;
	}

	return false;
}

