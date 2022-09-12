/**
 * \file effects.c
 * \brief Public effect and auxiliary functions for every effect in the game
 *
 * Copyright (c) 2007 Andi Sidwell
 * Copyright (c) 2016 Ben Semmler, Nick McConnell
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

#include "effects.h"
#include "effect-handler.h"
#include "game-input.h"
#include "init.h"
#include "mon-summon.h"
#include "obj-gear.h"
#include "player-abilities.h"
#include "player-history.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "trap.h"


/**
 * ------------------------------------------------------------------------
 * Properties of effects
 * ------------------------------------------------------------------------ */
/**
 * Useful things about effects.
 */
static const struct effect_kind effects[] =
{
	{ EF_NONE, false, NULL, NULL, NULL, NULL },
	#define F(x) effect_handler_##x
	#define EFFECT(x, a, b, c, d, e, f)	{ EF_##x, a, b, F(x), e, f },
	#include "list-effects.h"
	#undef EFFECT
	#undef F
	{ EF_MAX, false, NULL, NULL, NULL, NULL }
};


static const char *effect_names[] = {
	NULL,
	#define EFFECT(x, a, b, c, d, e, f)	#x,
	#include "list-effects.h"
	#undef EFFECT
};

/*
 * Utility functions
 */

/**
 * Free all the effects in a structure
 *
 * \param source the effects being freed
 */
void free_effect(struct effect *source)
{
	struct effect *e = source, *e_next;
	while (e) {
		e_next = e->next;
		dice_free(e->dice);
		if (e->msg) {
			string_free(e->msg);
		}
		mem_free(e);
		e = e_next;
	}
}

bool effect_valid(const struct effect *effect)
{
	if (!effect) return false;
	return effect->index > EF_NONE && effect->index < EF_MAX;
}

bool effect_aim(const struct effect *effect)
{
	const struct effect *e = effect;

	if (!effect_valid(effect))
		return false;

	while (e) {
		if (effects[e->index].aim) return true;
		e = e->next;
	}

	return false;
}

const char *effect_info(const struct effect *effect)
{
	if (!effect_valid(effect))
		return NULL;

	return effects[effect->index].info;
}

const char *effect_desc(const struct effect *effect)
{
	if (!effect_valid(effect))
		return NULL;

	return effects[effect->index].desc;
}

effect_index effect_lookup(const char *name)
{
	size_t i;

	for (i = 0; i < N_ELEMENTS(effect_names); i++) {
		const char *effect_name = effect_names[i];

		/* Test for equality */
		if (effect_name != NULL && streq(name, effect_name))
			return i;
	}

	return EF_MAX;
}

/**
 * Translate a string to an effect parameter subtype index
 */
int effect_subtype(int index, const char *type)
{
	int val = -1;

	/* If not a numerical value, assign according to effect index */
	if (sscanf(type, "%d", &val) != 1) {
		switch (index) {
				/* Projection name */
			case EF_PROJECT_LOS:
			case EF_PROJECT_LOS_GRIDS:
			case EF_LIGHT_AREA:
			case EF_EXPLOSION:
			case EF_SPOT:
			case EF_SPHERE:
			case EF_BREATH:
			case EF_BOLT:
			case EF_BEAM:
			case EF_TERRAIN_BEAM: {
				val = proj_name_to_idx(type);
				break;
			}

				/* Timed effect name */
			case EF_CURE:
			case EF_TIMED_SET:
			case EF_TIMED_INC:
			case EF_TIMED_INC_CHECK:
			case EF_TIMED_INC_NO_RES: {
				val = timed_name_to_idx(type);
				break;
			}

				/* Nourishment types */
			case EF_NOURISH: {
				if (streq(type, "INC_BY"))
					val = 0;
				else if (streq(type, "DEC_BY"))
					val = 1;
				break;
			}

				/* Summon name */
			case EF_SUMMON: {
				val = summon_name_to_idx(type);
				break;
			}

				/* Stat name */
			case EF_RESTORE_STAT:
			case EF_DRAIN_STAT:
			case EF_DART: {
				val = stat_name_to_idx(type);
				break;
			}

				/* Inscribe a glyph */
			case EF_GLYPH: {
				if (streq(type, "WARDING"))
					val = GLYPH_WARDING;
				break;
			}

				/* Allow monster teleport toward */
			case EF_TELEPORT_TO: {
				if (streq(type, "SELF"))
					val = 1;
				break;
			}

				/* Pit types */
			case EF_PIT: {
				if (streq(type, "SPIKED"))
					val = 1;
				else if (streq(type, "NORMAL"))
					val = 0;
				break;
			}

				/* Monster listen types */
			case EF_NOISE: {
				if (streq(type, "PLAYER"))
					val = 1;
				else if (streq(type, "MONSTER"))
					val = 0;
				break;
			}

				/* Some effects only want a radius, so this is a dummy */
			default: {
				if (streq(type, "NONE"))
					val = 0;
			}
		}
	}

	return val;
}

static int32_t effect_value_base_zero(void)
{
	return 0;
}

static int32_t effect_value_base_spell_power(void)
{
	int power = 0;

	/* Check the reference race first */
	if (ref_race)
	   power = ref_race->spell_power;
	/* Otherwise the current monster if there is one */
	else if (cave->mon_current > 0)
		power = cave_monster(cave, cave->mon_current)->race->spell_power;

	return power;
}

static int32_t effect_value_base_dungeon_level(void)
{
	return cave->depth;
}

static int32_t effect_value_base_max_sight(void)
{
	return z_info->max_sight;
}

static int32_t effect_value_base_player_hp(void)
{
	return player->chp;
}

static int32_t effect_value_base_player_max_hp(void)
{
	return player->mhp;
}

static int32_t effect_value_base_player_will(void)
{
	int will = player->state.skill_use[SKILL_WILL];
	if (player_active_ability(player, "Channeling")) {
		will += 5;
	}
	return will;
}

static int32_t effect_value_base_player_cut(void)
{
	return player->timed[TMD_CUT];
}

static int32_t effect_value_base_player_pois(void)
{
	return player->timed[TMD_POISONED];
}

expression_base_value_f effect_value_base_by_name(const char *name)
{
	static const struct value_base_s {
		const char *name;
		expression_base_value_f function;
	} value_bases[] = {
		{ "ZERO", effect_value_base_zero },
		{ "SPELL_POWER", effect_value_base_spell_power },
		{ "DUNGEON_LEVEL", effect_value_base_dungeon_level },
		{ "MAX_SIGHT", effect_value_base_max_sight },
		{ "PLAYER_HP", effect_value_base_player_hp },
		{ "PLAYER_MAX_HP", effect_value_base_player_max_hp },
		{ "PLAYER_WILL", effect_value_base_player_will },
		{ "PLAYER_CUT", effect_value_base_player_cut },
		{ "PLAYER_POIS", effect_value_base_player_pois },
		{ NULL, NULL },
	};
	const struct value_base_s *current = value_bases;

	while (current->name != NULL && current->function != NULL) {
		if (my_stricmp(name, current->name) == 0)
			return current->function;

		current++;
	}

	return NULL;
}

/**
 * ------------------------------------------------------------------------
 * Execution of effects
 * ------------------------------------------------------------------------ */
/**
 * Execute an effect chain.
 *
 * \param effect is the effect chain
 * \param origin is the origin of the effect (player, monster etc.)
 * \param obj    is the object making the effect happen (or NULL)
 * \param ident  will be updated if the effect is identifiable
 *               (NB: no effect ever sets *ident to false)
 * \param aware  indicates whether the player is aware of the effect already
 * \param dir    is the direction the effect will go in
 * \param cmd    If the effect is invoked as part of a command, this is the
 *               the command structure - used primarily so repeating the
 *               command can use the same information without prompting the
 *               player again.  Use NULL for this if not invoked as part of
 *               a command.
 */
bool effect_do(struct effect *effect,
		struct source origin,
		struct object *obj,
		bool *ident,
		bool aware,
		int dir,
		struct command *cmd)
{
	bool completed = false;
	bool first = true;
	effect_handler_f handler;
	random_value value = { 0, 0, 0, 0 };

	do {
		int leftover = 1;

		if (!effect_valid(effect)) {
			msg("Bad effect passed to effect_do(). Please report this bug.");
			return false;
		}

		if (effect->dice != NULL)
			(void) dice_roll(effect->dice, &value);

		/* Handle the effect */
		handler = effects[effect->index].handler;
		if (handler != NULL) {
			effect_handler_context_t context = {
				effect->index,
				origin,
				obj,
				aware,
				dir,
				value,
				effect->subtype,
				effect->radius,
				effect->other,
				effect->msg,
				*ident,
				cmd
			};

			completed = handler(&context) || completed;

			/* Don't identify by NOURISH unless it's the only effect */
			if ((effect->index != EF_NOURISH) || (!effect->next && first)) {
				*ident = context.ident;
			}
			first = false;
		}

		/* Get the next effect, if there is one */
		while (leftover-- && effect)
			effect = effect->next;
	} while (effect);

	return completed;
}

/**
 * Perform a single effect with a simple dice string and parameters
 * Calling with ident a valid pointer will (depending on effect) give success
 * information; ident = NULL will ignore this
 */
void effect_simple(int index,
				   struct source origin,
				   const char *dice_string,
				   int subtype,
				   int radius,
				   int other,
				   bool *ident)
{
	struct effect effect;
	int dir = DIR_TARGET;
	bool dummy_ident = false;

	/* Set all the values */
	memset(&effect, 0, sizeof(effect));
	effect.index = index;
	effect.dice = dice_new();
	dice_parse_string(effect.dice, dice_string);
	effect.subtype = subtype;
	effect.radius = radius;
	effect.other = other;

	/* Direction if needed */
	if (effect_aim(&effect))
		get_aim_dir(&dir, z_info->max_range);

	/* Do the effect */
	if (!ident) {
		ident = &dummy_ident;
	}

	effect_do(&effect, origin, NULL, ident, true, dir, NULL);
	dice_free(effect.dice);
}
