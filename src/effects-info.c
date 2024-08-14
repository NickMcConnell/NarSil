/**
 * \file effects-info.c
 * \brief Implement interfaces for displaying information about effects
 *
 * Copyright (c) 2020 Eric Branlund
 * Copyright (c) 2010 Andi Sidwell
 * Copyright (c) 2004 Robert Ruehlmann
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

#include "effects-info.h"
#include "effects.h"
#include "init.h"
#include "message.h"
#include "mon-summon.h"
#include "obj-info.h"
#include "player-timed.h"
#include "project.h"
#include "z-color.h"
#include "z-form.h"
#include "z-util.h"


static struct {
        int index;
        int args;
        int efinfo_flag;
        const char *desc;
	const char *menu_name;
} base_descs[] = {
        { EF_NONE, 0, EFINFO_NONE, "", "" },
        #define EFFECT(x, a, b, c, d, e, f) { EF_##x, c, d, e, f },
        #include "list-effects.h"
        #undef EFFECT
};


/**
 * Get the possible dice strings.
 */
static void format_dice_string(const random_value *v, int multiplier,
	size_t len, char* dice_string)
{
	if (v->dice && v->base) {
		if (multiplier == 1) {
			strnfmt(dice_string, len, "%d+%dd%d", v->base, v->dice,
				v->sides);
		} else {
			strnfmt(dice_string, len, "%d+%d*(%dd%d)",
				multiplier * v->base, multiplier, v->dice,
				v->sides);
		}
	} else if (v->dice) {
		if (multiplier == 1) {
			strnfmt(dice_string, len, "%dd%d", v->dice, v->sides);
		} else {
			strnfmt(dice_string, len, "%d*(%dd%d)", multiplier,
				v->dice, v->sides);
		}
	} else {
		strnfmt(dice_string, len, "%d", multiplier * v->base);
	}
}


/**
 * Appends a message describing the magical device skill bonus and the average
 * damage. Average damage is only displayed if there is variance or a magical
 * device bonus.
 */
static void append_damage(char *buffer, size_t buffer_size, random_value value)
{
	if (randcalc_varies(value)) {
		// Ten times the average damage, for 1 digit of precision
		int dam = (100 * randcalc(value, 0, AVERAGE)) / 10;
		my_strcat(buffer, format(" for an average of %d.%d damage", dam / 10,
			dam % 10), buffer_size);
	}
}


static void copy_to_textblock_with_coloring(textblock *tb, const char *s)
{
	while (*s) {
		if (isdigit((unsigned char) *s)) {
			textblock_append_c(tb, COLOUR_L_GREEN, "%c", *s);
		} else {
			textblock_append(tb, "%c", *s);
		}
		++s;
	}
}


/**
 * Creates a new textblock which has a description of the effect in *e and all
 * the subsequent effects.  If none of the effects has a description, will
 * return NULL.  If there is at least one effect with a description and prefix
 * is not NULL, the string pointed to by prefix will be added to the textblock
 * before the descriptions.
 */
textblock *effect_describe(const struct effect *e, const char *prefix)
{
	textblock *tb = NULL;
	char desc[250];
	random_value value = { 0, 0, 0, 0 };
	bool value_set = false;

	while (e) {
		const char* edesc = effect_desc(e);
		char dice_string[20];

		if ((e->dice != NULL) && !value_set) {
			(void) dice_roll(e->dice, &value);
		}

		if (!edesc) {
			e = e->next;
			continue;
		}

		format_dice_string(&value, 1, sizeof(dice_string), dice_string);

		/* Check all the possible types of description format. */
		switch (base_descs[e->index].efinfo_flag) {
		case EFINFO_DICE:
			strnfmt(desc, sizeof(desc), edesc, dice_string);
			break;

		case EFINFO_HEAL:
			/* Healing sometimes has a minimum percentage. */
			{
				char min_string[50];

				if (value.m_bonus) {
					strnfmt(min_string, sizeof(min_string),
						" (or %d%%, whichever is greater)",
						value.m_bonus);
				} else {
					strnfmt(min_string, sizeof(min_string),
						"");
				}
				strnfmt(desc, sizeof(desc), edesc, dice_string,
					min_string);
			}
			break;

		case EFINFO_FOOD:
			{
				const char *fed = e->subtype ?
					(e->subtype == 1 ? "uses enough food value" : 
					 "leaves you nourished") : "feeds you";
				char turn_dice_string[20];

				format_dice_string(&value, 10,
					sizeof(turn_dice_string),
					turn_dice_string);

				strnfmt(desc, sizeof(desc), edesc, fed,
					turn_dice_string, dice_string);
			}
			break;

		case EFINFO_CURE:
			strnfmt(desc, sizeof(desc), edesc, timed_effects[e->subtype].desc);
			break;

		case EFINFO_TIMED:
			strnfmt(desc, sizeof(desc), edesc, timed_effects[e->subtype].desc,
					dice_string);
			break;

		case EFINFO_TERROR:
			strnfmt(desc, sizeof(desc), edesc, dice_string);
			break;

		case EFINFO_STAT:
			{
				int stat = e->subtype;

				strnfmt(desc, sizeof(desc), edesc,
					lookup_obj_property(OBJ_PROPERTY_STAT, stat)->name);
			}
			break;

		case EFINFO_PROJ:
			strnfmt(desc, sizeof(desc), edesc, projections[e->subtype].desc);
			break;

		case EFINFO_SUMM:
			strnfmt(desc, sizeof(desc), edesc, summon_desc(e->subtype));
			break;

		case EFINFO_QUAKE:
			strnfmt(desc, sizeof(desc), edesc, e->radius);
			break;

		case EFINFO_SPOT:
			{
				int i_radius = e->other ? e->other : e->radius;

				strnfmt(desc, sizeof(desc), edesc,
					projections[e->subtype].player_desc,
					e->radius, i_radius, dice_string);
				append_damage(desc, sizeof(desc), value);
			}
			break;

		case EFINFO_BREATH:
			strnfmt(desc, sizeof(desc), edesc,
				projections[e->subtype].player_desc, e->other,
				dice_string);
			append_damage(desc, sizeof(desc), value);
			break;

		case EFINFO_BOLT:
			/* Bolts and beams that damage */
			strnfmt(desc, sizeof(desc), edesc,
				projections[e->subtype].desc, dice_string);
			append_damage(desc, sizeof(desc), value);
			break;

		case EFINFO_NONE:
			strnfmt(desc, sizeof(desc), "%s", edesc);
			break;

		default:
			strnfmt(desc, sizeof(desc), "");
			msg("Bad effect description passed to effect_info().  Please report this bug.");
			break;
		}

		e = e->next;

		if (desc[0] != '\0') {
			if (tb) {
				if (e) {
					textblock_append(tb, ", ");
				} else {
					textblock_append(tb, " and ");
				}
			} else {
				tb = textblock_new();
				if (prefix) {
					textblock_append(tb, "%s", prefix);
				}
			}
			copy_to_textblock_with_coloring(tb, desc);
		}
	}

	return tb;
}

/**
 * Fills a buffer with a short description, suitable for use a menu entry, of
 * an effect.
 * \param buf is the buffer to fill.
 * \param max is the maximum number of characters the buffer can hold.
 * \param e is the effect to describe.
 * \return the number of characters written to the buffer; will be zero if
 * the effect is invalid
 */
size_t effect_get_menu_name(char *buf, size_t max, const struct effect *e)
{
	const char *fmt;
	size_t len;

	if (!e || e->index <= EF_NONE || e->index >= EF_MAX) {
		return 0;
	}

	fmt = base_descs[e->index].menu_name;
	switch (base_descs[e->index].efinfo_flag) {
	case EFINFO_DICE:
	case EFINFO_HEAL:
	case EFINFO_QUAKE:
	case EFINFO_TERROR:
	case EFINFO_NONE:
		len = strnfmt(buf, max, "%s", fmt);
		break;

	case EFINFO_FOOD:
		{
			const char *actstr;
			const char *actarg;
			int avg;

			switch (e->subtype) {
			case 0: /* INC_BY */
				actstr = "feed";
				actarg = "yourself";
				break;
			case 1: /* DEC_BY */
				actstr = "increase";
				actarg = "hunger";
				break;
			case 2: /* SET_TO */
				avg = (e->dice) ?
					dice_evaluate(e->dice, 1, AVERAGE, NULL) : 0;
				actstr = "become";
				if (avg > PY_FOOD_FULL) {
					actarg = "bloated";
				} else {
					actarg = "hungry";
				}
				break;
			case 3: /* INC_TO */
				avg = (e->dice) ?
					dice_evaluate(e->dice, 1, AVERAGE, NULL): 0;
				actstr = "leave";
				if (avg > PY_FOOD_FULL) {
					actarg = "bloated";
				} else {
					actarg = "hungry";
				}
				break;
			default:
				actstr = NULL;
				actarg = NULL;
				break;
			}
			if (actstr && actarg) {
				len = strnfmt(buf, max, fmt, actstr, actarg);
			} else {
				len = strnfmt(buf, max, "");
			}
		}
		break;

	case EFINFO_CURE:
	case EFINFO_TIMED:
		len = strnfmt(buf, max, fmt, timed_effects[e->subtype].desc);
		break;

	case EFINFO_STAT:
		len = strnfmt(buf, max, fmt,
			lookup_obj_property(OBJ_PROPERTY_STAT, e->subtype)->name);
		break;

	case EFINFO_PROJ:
	case EFINFO_BOLT:
		len = strnfmt(buf, max, fmt, projections[e->subtype].desc);
		break;

	case EFINFO_SUMM:
		len = strnfmt(buf, max, fmt, summon_desc(e->subtype));
		break;

	case EFINFO_SPOT:
	case EFINFO_BREATH:
		len = strnfmt(buf, max, fmt, projections[e->subtype].player_desc);
		break;

	default:
		len = strnfmt(buf, max, "");
		msg("Bad effect description passed to effect_get_menu_name().  Please report this bug.");
		break;
	}

	return len;
}

/**
 * Returns a pointer to the next effect in the effect stack
 */
struct effect *effect_next(struct effect *effect)
{
	return effect->next;
}

/**
 * Checks if the effect deals damage, by checking the effect's info string.
 * Random or select effects are considered to deal damage if any sub-effect
 * deals damage.
 */
bool effect_damages(const struct effect *effect)
{
	return effect_info(effect) != NULL && streq(effect_info(effect), "dam");
}

/**
 * Calculates the average damage of the effect. Random effects and select
 * effects return an average of all sub-effect averages.
 *
 * \param effect is the effect to evaluate.
 */
int effect_avg_damage(const struct effect *effect)
{
	return dice_evaluate(effect->dice, 0, AVERAGE, NULL);
}

/**
 * Returns the projection of the effect, or an empty string if it has none.
 * Random or select effects only return a projection if all sub-effects have
 * the same projection.
 */
const char *effect_projection(const struct effect *effect)
{
	if (projections[effect->subtype].player_desc != NULL) {
		switch (base_descs[effect->index].efinfo_flag) {
			case EFINFO_PROJ:
			case EFINFO_BOLT:
			case EFINFO_BREATH:
			case EFINFO_SPOT:
				return projections[effect->subtype].player_desc;
		}
	}

	return "";
}

