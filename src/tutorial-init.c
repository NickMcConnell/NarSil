/**
 * \file tutorial-init.c
 * \brief Implement parsing of the tutorial data.
 *
 * Copyright (c) 2022 Eric Branlund
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

#include "cave.h"
#include "cmd-core.h"
#include "datafile.h"
#include "mon-move.h"
#include "mon-util.h"
#include "obj-knowledge.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "parser.h"
#include "player-abilities.h"
#include "player-timed.h"
#include "trap.h"
#include "tutorial-init.h"
#include "z-dice.h"
#include "z-form.h"
#include "z-util.h"
#include "z-virt.h"


static bool tutorial_section_sym_table_insert(
	struct tutorial_section_sym_table t,
	struct tutorial_section_sym_key *key,
	struct tutorial_section_sym_val *value);

static errr tutorial_run_parser(struct parser *p);


struct init_module tutorial_module = {
	"tutorial", NULL, tutorial_cleanup_parsed_data
};
struct tutorial_parsed_result tutorial_parsed_data = {
	{ NULL }, NULL, NULL, NULL, NULL, NULL, 0, 0
};


static struct file_parser tutorial_parser = {
	"tutorial",
	tutorial_init_parser,
	tutorial_run_parser,
	tutorial_finish_parser,
	tutorial_cleanup_parsed_data
};
static const char *obj_flags[] = {
	"NONE",
	#define OF(a, b) #a,
	#include "list-object-flags.h"
	#undef OF
	NULL
};
static const char *element_names[] = {
	"NONE",
	#define ELEM(a) #a,
	#include "list-elements.h"
	#undef ELEM
	NULL
};
static const char *obj_mods[] = {
	"NONE",
	#define STAT(a) #a,
	#include "list-stats.h"
	#undef STAT
	#define SKILL(a, b) #a,
	#include "list-skills.h"
	#undef SKILL
	#define OBJ_MOD(a) #a,
	#include "list-object-modifiers.h"
	#undef OBJ_MOD
	NULL,
};
static const char *square_flag_names[] = {
	#define SQUARE(a, b) #a,
	#include "list-square-flags.h"
	#undef SQUARE
	NULL
};


static void tutorial_item_tweaks_free(struct tutorial_item_tweak *tweaks,
		int count)
{
	int i;

	for (i = 0; i < count; ++i) {
		string_free(tweaks[i].dice);
	}
	mem_free(tweaks);
}


static uint32_t tutorial_section_sym_key_hash(const void *key)
{
	const struct tutorial_section_sym_key *sk =
		(const struct tutorial_section_sym_key*) key;
	char buf[32];

	(void) strnfmt(buf, sizeof(buf), "%d,%d,%s", sk->x, sk->y, sk->symbol);
	return djb2_hash(buf);
}


static int tutorial_section_sym_key_compare(const void *a, const void *b)
{
	const struct tutorial_section_sym_key *ska =
		(const struct tutorial_section_sym_key*) a;
	const struct tutorial_section_sym_key *skb =
		(const struct tutorial_section_sym_key*) b;

	return (streq(ska->symbol, skb->symbol) && ska->x == skb->x
		&& ska->y == skb->y) ? 0 : 1;
}


static void tutorial_section_sym_value_free(void *value)
{
	struct tutorial_section_sym_val *tv =
		(struct tutorial_section_sym_val*) value;

	switch (tv->kind) {
	case SECTION_SYM_GATE:
		string_free(tv->v.gate.dest);
		string_free(tv->v.gate.note);
		break;

	case SECTION_SYM_ITEM:
		if (!tv->v.item.is_artifact) {
			tutorial_item_tweaks_free(tv->v.item.v.details.tweaks,
				tv->v.item.v.details.tweak_count);
		}
		break;

	case SECTION_SYM_MONSTER:
		string_free(tv->v.monster.note);
		break;

	case SECTION_SYM_NOTE:
	case SECTION_SYM_START:
	case SECTION_SYM_TRIGGER:
		string_free(tv->v.name);
		break;

	default:
		/* There's nothing special to do. */
		break;
	}

	mem_free(tv);
}


/**
 * Create the dictionary for a tutorial section's symbols.  This is a thin
 * layer of type checking over the generic dictionary type.
 */
static struct tutorial_section_sym_table tutorial_section_sym_table_create(void)
{
	struct tutorial_section_sym_table result;
	struct {
		enum tutorial_section_sym_kind kind;
		const char *sym;
		int feat_idx;
	} symbol_kinds[] = {
		#define TSYM(a, b, c, d) { SECTION_SYM_##a, b, c },
		#include "list-tutorial-sym.h"
		#undef TSYM
	};
	size_t i;

	result.d = dict_create(tutorial_section_sym_key_hash,
		tutorial_section_sym_key_compare, mem_free,
		tutorial_section_sym_value_free);
	/* Insert the predefined symbols. */
	for (i = 0; i < N_ELEMENTS(symbol_kinds); ++i) {
		struct tutorial_section_sym_key *key;
		struct tutorial_section_sym_val *value;

		if (!symbol_kinds[i].sym) continue;
		key = mem_alloc(sizeof(*key));
		my_strcpy(key->symbol, symbol_kinds[i].sym,
			sizeof(key->symbol));
		key->x = -1;
		key->y = -1;
		value = mem_zalloc(sizeof(*value));
		if (symbol_kinds[i].feat_idx != FEAT_NONE) {
			value->v.feat = symbol_kinds[i].feat_idx;
		}
		value->is_predefined = true;
		value->kind = symbol_kinds[i].kind;
		if (!tutorial_section_sym_table_insert(result, key, value)) {
			quit("logic error:  duplicate symbols in list-tutorial-sym.h");
		}
	}

	return result;
}


/**
 * Destroy a tutorial section's symbol table.  This is a thin layer of type
 * checking over the generic dictionary type.
 */
static void tutorial_section_sym_table_destroy(
		struct tutorial_section_sym_table t)
{
	dict_destroy(t.d);
}


/**
 * Insert a key and value into a tutorial section's symbol table.  This is a
 * thin layer of type checking over the generic dictionary type.
 */
static bool tutorial_section_sym_table_insert(
		struct tutorial_section_sym_table t,
		struct tutorial_section_sym_key *key,
		struct tutorial_section_sym_val *value)
{
	return dict_insert(t.d, key, value);
}


/**
 * Parse a customized symbol for a tutorial section.
 */
static enum parser_error tutorial_section_parse_symbol(
		const char *symbol,
		struct tutorial_parser_priv *priv,
		struct tutorial_section_sym_table t,
		struct tutorial_section_sym_key **key)
{
	const char *lp = strchr(symbol + 1, '(');
	size_t sz = utf8_strlen(symbol);
	enum parser_error result = PARSE_ERROR_NONE;

	if (lp == NULL) {
		/* There's no coordinate specified. */
		if (sz != 1) {
			result = PARSE_ERROR_INVALID_UTF8_CODE_POINT;
		} else {
			*key = mem_alloc(sizeof(**key));
			my_strcpy((*key)->symbol, symbol,
				sizeof((*key)->symbol));
			(*key)->x = -1;
			(*key)->y = -1;
		}
	} else {
		const char *rp = strchr(lp + 1, ')');
		size_t sz1 = utf8_strlen(lp);

		assert(sz1 < sz);
		if (sz - sz1 != 1) {
			result = PARSE_ERROR_INVALID_UTF8_CODE_POINT;
		} else if (rp == NULL || *(rp + 1) != '\0') {
			result = PARSE_ERROR_MALFORMED_COORDINATE;
		} else {
			char *endx, *endy;
			long lx, ly;

			lx = strtol(lp + 1, &endx, 10);
			if (endx == lp + 1 || *endx != ',') {
				result = PARSE_ERROR_NOT_NUMBER;
			} else if (lx < 0 || lx >= z_info->dungeon_wid - 1) {
				result = PARSE_ERROR_OUT_OF_BOUNDS;
			} else {
				ly = strtol(endx + 1, &endy, 10);
				if (endy == endx + 1 || endy != rp) {
					result = PARSE_ERROR_NOT_NUMBER;
				} else if (ly < 0
						|| ly >= z_info->dungeon_hgt - 1) {
					result = PARSE_ERROR_OUT_OF_BOUNDS;
				} else {
					struct tutorial_section_sym_val *val;

					*key = mem_alloc(sizeof(**key));
					strnfmt((*key)->symbol,
						sizeof((*key)->symbol),
						"%.*s", (int) (lp - symbol),
						symbol);
					/*
					 * Verify that it doesn't match a
					 * predefined symbol.
					 */
					(*key)->x = -1;
					(*key)->y = -1;
					val = tutorial_section_sym_table_has(
						t, *key);
					if (val && val->is_predefined) {
						result = PARSE_ERROR_DUPLICATED_SYMBOL;
						mem_free(*key);
						*key = NULL;
					} else {
						(*key)->x = (int) lx;
						(*key)->y = (int) ly;
					}
				}
			}
		}
	}
	return result;
}


static uint32_t tutorial_key_hash(const void *key)
{
	const struct tutorial_dict_key_type *tk =
		(const struct tutorial_dict_key_type*) key;

	return djb2_hash(tk->name);
}


static int tutorial_key_compare(const void *a, const void *b)
{
	const struct tutorial_dict_key_type *tka =
		(const struct tutorial_dict_key_type*) a;
	const struct tutorial_dict_key_type *tkb =
		(const struct tutorial_dict_key_type*) b;

	return (streq(tka->name, tkb->name) && tka->comp == tkb->comp) ? 0 : 1;
}


static void tutorial_key_free(void *key)
{
	struct tutorial_dict_key_type *tk =
		(struct tutorial_dict_key_type*) key;

	string_free(tk->name);
	mem_free(tk);
}


static void free_trigger_compiled_ops(struct trigger_compiled_op *ops, int n)
{
	int i;

	if (!ops) {
		return;
	}
	for (i = 0; i < n; ++i) {
		mem_free(ops[i].name);
	}
	mem_free(ops);

}


static void tutorial_value_free(void* value)
{
	struct tutorial_dict_val_type *tv =
		(struct tutorial_dict_val_type*) value;
	int i;

	switch (tv->key->comp) {
	case TUTORIAL_ARCHETYPE:
		string_free(tv->v.archetype.race_name);
		string_free(tv->v.archetype.house_name);
		string_free(tv->v.archetype.sex_name);
		string_free(tv->v.archetype.character_name);
		string_free(tv->v.archetype.history);
		mem_free(tv->v.archetype.added_abilities);
		for (i = 0; i < tv->v.archetype.kit_count; ++i) {
			if (!tv->v.archetype.kit[i].item.is_artifact) {
				tutorial_item_tweaks_free(tv->v.archetype.kit[i].item.v.details.tweaks,
					tv->v.archetype.kit[i].item.v.details.tweak_count);
			}
		}
		mem_free(tv->v.archetype.kit);
		break;

	case TUTORIAL_NOTE:
		string_free(tv->v.note.text);
		break;

	case TUTORIAL_TRIGGER:
		string_free(tv->v.trigger.text);
		string_free(tv->v.trigger.death_note_name);
		free_trigger_compiled_ops(tv->v.trigger.expr.ops,
			tv->v.trigger.expr.n_op);
		break;

	case TUTORIAL_SECTION:
		string_free(tv->v.section.start_note_name);
		string_free(tv->v.section.death_note_name);
		if (tv->v.section.lines) {
			for (i = 0; i < tv->v.section.rows; ++i) {
				string_free(tv->v.section.lines[i]);
			}
			mem_free(tv->v.section.lines);
		}
		mem_free(tv->v.section.area_flags);
		tutorial_section_sym_table_destroy(tv->v.section.symt);
		break;
	}

	mem_free(tv);
}


/**
 * Create the tutorial's dictionary.  This is a thin layer of type checking
 * over the generic dictionary type.
 */
static struct tutorial_dict_type tutorial_dict_create(void)
{
	struct tutorial_dict_type result;

	result.d = dict_create(tutorial_key_hash, tutorial_key_compare,
		tutorial_key_free, tutorial_value_free);
	return result;
}


/**
 * Destroy the tutorial's dictionary.  This is a thin layer of type checking
 * over the generic dictionary type.
 */
static void tutorial_dict_destroy(struct tutorial_dict_type d)
{
	dict_destroy(d.d);
}


/**
 * Insert a key and value into the tutorial's dictionary.  This is a thin
 * layer of type checking over the generic dictionary type.  If a starting
 * note is set, hook that up to the starting location.
 */
static bool tutorial_dict_insert(struct tutorial_dict_type d,
		struct tutorial_dict_key_type *key,
		struct tutorial_dict_val_type *value)
{
	return dict_insert(d.d, key, value);
}


/**
 * Verify that a tutorial section has exactly one starting point, at least
 * one exit, and no undefined symbols in the layout.  If a starting note is
 * set, hook that up to the starting location.
 *
 * Bad layouts trigger an exit via quit().
 */
static void verify_section(struct tutorial_dict_val_type *section)
{
	int count_starts = 0;
	int count_exits = 0;
	int count_unknown = 0;
	struct loc first_start = { -1, -1 };
	struct loc first_unknown = { -1, -1 };
	struct loc grid;
	char reasons[3][80] = { "", "", "" };
	int count_reasons;

	for (grid.y = 0; grid.y < section->v.section.rows; ++grid.y) {
		char *sym = section->v.section.lines[grid.y];

		for (grid.x = 0;
				grid.x < section->v.section.columns;
				++grid.x) {
			char *next_sym = utf8_fskip(sym, 1, NULL);
			struct tutorial_section_sym_key key;
			struct tutorial_section_sym_val *val;

			if (next_sym) {
				assert((size_t) (next_sym - sym)
					< sizeof(key.symbol));
				strnfmt(key.symbol, sizeof(key.symbol),
					"%.*s", (int) (next_sym - sym), sym);
			} else {
				assert(grid.x
					== section->v.section.columns - 1);
				my_strcpy(key.symbol, sym, sizeof(key.symbol));
			}

			key.x = grid.x;
			key.y = grid.y;
			val = tutorial_section_sym_table_has(
				section->v.section.symt, &key);
			if (!val) {
				key.x = -1;
				key.y = -1;
				val = tutorial_section_sym_table_has(
					section->v.section.symt, &key);
			}
			if (val) {
				if (val->kind == SECTION_SYM_START) {
					if (!count_starts) {
						first_start = grid;
					}
					++count_starts;
					/*
					 * Associate with the assigned starting
					 * note.
					 */
					assert(!val->v.name);
					val->v.name = string_make(
						section->v.section.start_note_name);
				} else if (val->kind == SECTION_SYM_GATE) {
					++count_exits;
				}
			} else {
				if (!count_unknown) {
					first_unknown = grid;
				}
				++count_unknown;
			}

			sym = next_sym;
		}
	}

	count_reasons = 0;
	if (count_starts == 0) {
		assert(count_reasons < (int)N_ELEMENTS(reasons));
		strnfmt(reasons[count_reasons], sizeof(reasons[count_reasons]),
			"  %d) no starting location", count_reasons + 1);
		++count_reasons;
	} else if (count_starts > 1) {
		assert(count_reasons < (int)N_ELEMENTS(reasons));
		strnfmt(reasons[count_reasons], sizeof(reasons[count_reasons]),
			"  %d) %d starting locations; first at row %d and "
			"column %d", count_reasons + 1, count_starts,
			first_start.y, first_start.x);
		++count_reasons;
	}
	if (count_exits == 0) {
		assert(count_reasons < (int)N_ELEMENTS(reasons));
		strnfmt(reasons[count_reasons], sizeof(reasons[count_reasons]),
			"  %d) no exits", count_reasons + 1);
		++count_reasons;
	}
	if (count_unknown > 0) {
		assert(count_reasons < (int)N_ELEMENTS(reasons));
		strnfmt(reasons[count_reasons], sizeof(reasons[count_reasons]),
			"  %d) %d unknown symbols; first at row %d and "
			"column %d", count_reasons + 1, count_unknown,
			first_unknown.y, first_unknown.x);
		++count_reasons;
	}
	if (count_reasons > 0) {
		quit_fmt("Tutorial section, %s, has these problems:%s%s%s",
			section->key->name, reasons[0], reasons[1], reasons[2]);
	}
}


static enum parser_error tutorial_add_item_tweak(
		struct tutorial_item_tweak **tweaks,
		int *count, int *alloc, enum tutorial_item_tweak_kind kind,
		const char *dice, const random_value *rv, int idx)
{
	assert(*count >= 0 && *count <= *alloc);
	if (*count == *alloc) {
		if (!*alloc) {
			*alloc = 4;
		} else if (*alloc >= 128) {
			return PARSE_ERROR_TOO_MANY_ENTRIES;
		} else {
			*alloc += *alloc;
		}
		*tweaks = mem_realloc(*tweaks, *alloc * sizeof(**tweaks));
	}
	(*tweaks)[*count].dice = string_make(dice);
	(*tweaks)[*count].value = *rv;
	(*tweaks)[*count].kind = kind;
	(*tweaks)[*count].idx = idx;
	++*count;
	return PARSE_ERROR_NONE;
}


static enum parser_error tutorial_parse_tweaks(
		struct tutorial_item_tweak **tweaks, int *count, int *alloc,
		const char *props, const char *mods)
{
	random_value dummy_rv = { 0, 0, 0, 0 };
	char *flags, *s, *lb, *rb;
	int idx;
	enum parser_error add_error;

	*tweaks = NULL;
	*count = 0;
	*alloc = 0;

	flags = string_make(props);
	s = strtok(flags, " |");
	while (s) {
		idx = lookup_flag(obj_flags, s);
		if (idx >= 0) {
			if (idx) {
				add_error = tutorial_add_item_tweak(tweaks,
					count, alloc, TWEAK_FLAG, NULL,
					&dummy_rv, idx);
				if (add_error != PARSE_ERROR_NONE) {
					string_free(flags);
					return add_error;
				}
			}
		} else if (prefix(s, "IGNORE_")) {
			idx = lookup_flag(element_names, s + 7);
			if (idx > 0 && idx < ELEM_MAX + 1) {
				add_error = tutorial_add_item_tweak(tweaks,
					count, alloc, TWEAK_ELEM_IGNORE, NULL,
					&dummy_rv, idx - 1);
				if (add_error != PARSE_ERROR_NONE) {
					string_free(flags);
					return add_error;
				}
			} else {
				string_free(flags);
				return PARSE_ERROR_INVALID_PROPERTY;
			}
		} else if (prefix(s, "HATES_")) {
			idx = lookup_flag(element_names, s + 6);
			if (idx > 0 && idx < ELEM_MAX + 1) {
				add_error = tutorial_add_item_tweak(tweaks,
					count, alloc, TWEAK_ELEM_HATE, NULL,
					&dummy_rv, idx - 1);
				if (add_error != PARSE_ERROR_NONE) {
					string_free(flags);
					return add_error;
				}
			} else {
				string_free(flags);
				return PARSE_ERROR_INVALID_PROPERTY;
			}
		} else if ((idx = lookup_slay(s)) >= 0) {
			add_error = tutorial_add_item_tweak(tweaks,
				count, alloc, TWEAK_SLAY, NULL, &dummy_rv, idx);
			if (add_error != PARSE_ERROR_NONE) {
				string_free(flags);
				return add_error;
			}
		} else if ((idx = lookup_brand(s)) >= 0) {
			add_error = tutorial_add_item_tweak(tweaks,
				count, alloc, TWEAK_BRAND, NULL, &dummy_rv,
				idx);
			if (add_error != PARSE_ERROR_NONE) {
				string_free(flags);
				return add_error;
			}
		} else {
			string_free(flags);
			return PARSE_ERROR_INVALID_PROPERTY;
		}
		s = strtok(NULL, " |");
	}
	string_free(flags);

	flags = string_make(mods);
	s = strtok(flags, " |");
	while (s) {
		lb = strchr(s, '[');
		rb = strchr(s, ']');
		if (lb == NULL || rb == NULL || *(rb + 1) != '\0') {
			string_free(flags);
			return PARSE_ERROR_INVALID_DICE;
		}
		*lb = '\0';
		*rb = '\0';
		++lb;
		if (streq(s, "PVAL")) {
			add_error = tutorial_add_item_tweak(tweaks, count,
				alloc, TWEAK_PVAL, lb + 1, &dummy_rv, 0);
			if (add_error != PARSE_ERROR_NONE) {
				string_free(flags);
				return add_error;
			}
		} else {
			dice_t *dice = dice_new();
			random_value rv;

			if (dice_parse_string(dice, lb)) {
				dice_random_value(dice, &rv);
				dice_free(dice);
			} else {
				dice_free(dice);
				string_free(flags);
				return PARSE_ERROR_INVALID_DICE;
			}

			idx = lookup_flag(obj_mods, s);
			if (idx >= 0) {
				if (idx > 0) {
					add_error = tutorial_add_item_tweak(
						tweaks, count, alloc,
						TWEAK_MODIFIER, NULL, &rv,
						idx - 1);
					if (add_error != PARSE_ERROR_NONE) {
						string_free(flags);
						return add_error;
					}
				}
			} else if (prefix(s, "RES_")) {
				idx = lookup_flag(element_names, s + 4);
				if (idx > 0 && idx < ELEM_MAX + 1) {
					add_error = tutorial_add_item_tweak(
						tweaks, count, alloc,
						TWEAK_ELEM_RESIST, NULL, &rv,
						idx - 1);

					if (add_error != PARSE_ERROR_NONE) {
						string_free(flags);
						return add_error;
					}
				} else {
					string_free(flags);
					return PARSE_ERROR_INVALID_PROPERTY;
				}
			} else {
				string_free(flags);
				return PARSE_ERROR_INVALID_PROPERTY;
			}
		}
		s = strtok(NULL, " |");
	}
	string_free(flags);

	return PARSE_ERROR_NONE;
}


static enum parser_error tutorial_add_area_flags(struct parser *p,
		struct tutorial_dict_val_type *val, bool clear)
{
	char *flags = string_make(parser_getstr(p, "flags"));
	char *s = strtok(flags, " |");
	bitflag bits[SQUARE_SIZE];
	struct tutorial_area_flag *this_area;

	sqinfo_wipe(bits);
	while (s) {
		int idx = lookup_flag(square_flag_names, s);

		if (idx > 0) {
			sqinfo_on(bits, idx);
		} else if (!streq(s, "NONE")) {
			string_free(flags);
			return PARSE_ERROR_INVALID_FLAG;
		}
		s = strtok(NULL, " |");
	}
	string_free(flags);

	assert(val->v.section.area_flag_count >= 0
		&& val->v.section.area_flag_count
		<= val->v.section.area_flag_alloc);
	if (val->v.section.area_flag_count == val->v.section.area_flag_alloc) {
		if (!val->v.section.area_flag_alloc) {
			val->v.section.area_flag_alloc = 4;
		} else if (val->v.section.area_flag_alloc >= 1024) {
			return PARSE_ERROR_TOO_MANY_ENTRIES;
		} else {
			val->v.section.area_flag_alloc +=
				val->v.section.area_flag_alloc;
		}
		val->v.section.area_flags = mem_realloc(
			val->v.section.area_flags,
			val->v.section.area_flag_alloc
			* sizeof(*val->v.section.area_flags));
	}

	this_area = val->v.section.area_flags + val->v.section.area_flag_count;
	++val->v.section.area_flag_count;
	sqinfo_copy(this_area->flags, bits);
	this_area->ul = loc(parser_getint(p, "xul"), parser_getint(p, "yul"));
	this_area->lr = loc(parser_getint(p, "xlr"), parser_getint(p, "ylr"));
	this_area->clear = clear;

	return PARSE_ERROR_NONE;
}


static enum parser_error parse_archetype_block_start(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	struct tutorial_dict_key_type *key;
	struct tutorial_dict_val_type *value;

	if (priv->curr_value
			&& priv->curr_value->key->comp == TUTORIAL_SECTION) {
		if (priv->section_lines_parsed
				< priv->curr_value->v.section.rows) {
			return PARSE_ERROR_TOO_FEW_ENTRIES;
		}
		verify_section(priv->curr_value);
	}

	key = mem_alloc(sizeof(*key));
	key->name = string_make(parser_getstr(p, "name"));
	key->comp = TUTORIAL_ARCHETYPE;
	value = mem_zalloc(sizeof(*value));
	if (!tutorial_dict_insert(priv->r->d, key, value)) {
		tutorial_value_free(value);
		tutorial_key_free(key);
		return PARSE_ERROR_DUPLICATED_NAME;
	}

	value->key = key;
	/*
	 * Keep whatever unspent experience there is after buying skills and
	 * abilities.
	 */
	value->v.archetype.unspent_experience = -1;
	value->v.archetype.purge_kit = false;
	priv->curr_value = value;
	if (!priv->r->default_archetype) {
		priv->r->default_archetype = value;
	}

	return PARSE_ERROR_NONE;
}


static enum parser_error parse_note_block_start(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	struct tutorial_dict_key_type *key;
	struct tutorial_dict_val_type *value;

	if (priv->curr_value
			&& priv->curr_value->key->comp == TUTORIAL_SECTION) {
		if (priv->section_lines_parsed
				< priv->curr_value->v.section.rows) {
			return PARSE_ERROR_TOO_FEW_ENTRIES;
		}
		verify_section(priv->curr_value);
	}

	key = mem_alloc(sizeof(*key));
	key->name = string_make(parser_getstr(p, "name"));
	key->comp = TUTORIAL_NOTE;
	value = mem_zalloc(sizeof(*value));
	if (!tutorial_dict_insert(priv->r->d, key, value)) {
		tutorial_value_free(value);
		tutorial_key_free(key);
		return PARSE_ERROR_DUPLICATED_NAME;
	}

	assert(priv->r->note_table_n >= 0
			&& priv->r->note_table_n <= priv->r->note_table_a);
	if (priv->r->note_table_n == priv->r->note_table_a) {
		int new_a = (priv->r->note_table_a) ?
			priv->r->note_table_a + priv->r->note_table_a : 8;

		/* Limited by the number of possible pvals. */
		if (new_a > MAX_PVAL + 1) {
			return PARSE_ERROR_TOO_MANY_ENTRIES;
		}
		priv->r->note_table_a = new_a;
		priv->r->pval_to_note_table = mem_realloc(
			priv->r->pval_to_note_table,
			new_a * sizeof(*priv->r->pval_to_note_table));
	}

	value->key = key;
	value->v.note.pval = priv->r->note_table_n;
	priv->curr_value = value;
	priv->r->pval_to_note_table[priv->r->note_table_n] = value;
	++priv->r->note_table_n;

	return PARSE_ERROR_NONE;
}


static enum parser_error parse_trigger_block_start(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	struct tutorial_dict_key_type *key;
	struct tutorial_dict_val_type *value;

	if (priv->curr_value
			&& priv->curr_value->key->comp == TUTORIAL_SECTION) {
		if (priv->section_lines_parsed
				< priv->curr_value->v.section.rows) {
			return PARSE_ERROR_TOO_FEW_ENTRIES;
		}
		verify_section(priv->curr_value);
	}

	key = mem_alloc(sizeof(*key));
	key->name = string_make(parser_getstr(p, "name"));
	key->comp = TUTORIAL_TRIGGER;
	value = mem_zalloc(sizeof(*value));
	if (!tutorial_dict_insert(priv->r->d, key, value)) {
		tutorial_value_free(value);
		tutorial_key_free(key);
		return PARSE_ERROR_DUPLICATED_NAME;
	}

	value->key = key;
	priv->curr_value = value;

	return PARSE_ERROR_NONE;
}


static enum parser_error parse_section_block_start(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	const char *name = parser_getstr(p, "name");
	struct tutorial_dict_key_type *key;
	struct tutorial_dict_val_type *value;

	if (priv->curr_value
			&& priv->curr_value->key->comp == TUTORIAL_SECTION) {
		if (priv->section_lines_parsed
				< priv->curr_value->v.section.rows) {
			return PARSE_ERROR_TOO_FEW_ENTRIES;
		}
		verify_section(priv->curr_value);
	}

	key = mem_alloc(sizeof(*key));
	key->name = string_make(name);
	key->comp = TUTORIAL_SECTION;
	value = mem_zalloc(sizeof(*value));
	if (!tutorial_dict_insert(priv->r->d, key, value)) {
		tutorial_value_free(value);
		tutorial_key_free(key);
		return PARSE_ERROR_DUPLICATED_NAME;
	}

	value->key = key;
	value->v.section.symt = tutorial_section_sym_table_create();
	priv->curr_value = value;
	priv->section_lines_parsed = 0;
	if (!priv->r->default_section) {
		priv->r->default_section = value;
	}

	return PARSE_ERROR_NONE;
}


static enum parser_error parse_archetype_race(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_ARCHETYPE) {
		const char *name = parser_getstr(p, "name");

		if (priv->curr_value->v.archetype.race_name) {
			string_free(priv->curr_value->v.archetype.race_name);
		}
		priv->curr_value->v.archetype.race_name = string_make(name);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_archetype_house(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_ARCHETYPE) {
		const char *name = parser_getstr(p, "name");

		if (priv->curr_value->v.archetype.house_name) {
			string_free(priv->curr_value->v.archetype.house_name);
		}
		priv->curr_value->v.archetype.house_name = string_make(name);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_archetype_sex(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_ARCHETYPE) {
		const char *name = parser_getstr(p, "name");

		if (priv->curr_value->v.archetype.sex_name) {
			string_free(priv->curr_value->v.archetype.sex_name);
		}
		priv->curr_value->v.archetype.sex_name = string_make(name);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_archetype_character_name(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_ARCHETYPE) {
		const char *name = parser_getstr(p, "name");

		if (priv->curr_value->v.archetype.character_name) {
			string_free(priv->curr_value->v.archetype.character_name);
		}
		priv->curr_value->v.archetype.character_name =
			string_make(name);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_archetype_history(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_ARCHETYPE) {
		const char *text = parser_getstr(p, "history");

		priv->curr_value->v.archetype.history = string_append(
			priv->curr_value->v.archetype.history, text);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_archetype_experience(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_ARCHETYPE) {
		const char *text = parser_getstr(p, "value");
		char *endptr;
		long lval = strtol(text, &endptr, 10);

		if (text[0] == '\0'
				|| (*endptr != '\0'
				&& !contains_only_spaces(endptr))) {
			result = PARSE_ERROR_INVALID_VALUE;
		} else {
			priv->curr_value->v.archetype.unspent_experience =
				(int32_t)MAX(0L, MIN(lval, 0x7FFFFFFFL));
		}
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_archetype_stats(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_ARCHETYPE) {
		char *s = string_make(parser_getstr(p, "values"));
		char *t = strtok(s, " |");

		while (t) {
			int value = 0;
			int idx = 0;

			if (grab_index_and_int(&value, &idx, obj_mods, "", t)
					|| idx < 1 || idx > STAT_MAX) {
				result = PARSE_ERROR_INVALID_VALUE;
				break;
			}
			--idx;
			if (value >= 0 && priv->curr_value->v.archetype.stat_adj[idx] > INT_MAX - value) {
				result = PARSE_ERROR_INVALID_VALUE;
				break;
			}
			if (value < 0 && priv->curr_value->v.archetype.stat_adj[idx] < INT_MIN - value) {
				result = PARSE_ERROR_INVALID_VALUE;
				break;
			}
			priv->curr_value->v.archetype.stat_adj[idx] += value;
			t = strtok(NULL, " |");
		}

		string_free(s);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_archetype_skills(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_ARCHETYPE) {
		char *s = string_make(parser_getstr(p, "values"));
		char *t = strtok(s, " |");

		while (t) {
			int value = 0;
			int idx = 0;

			if (grab_index_and_int(&value, &idx, obj_mods, "", t)
					|| idx < STAT_MAX + 1
					|| idx > STAT_MAX + SKILL_MAX) {
				result = PARSE_ERROR_INVALID_VALUE;
				break;
			}
			idx -= STAT_MAX + 1;
			if (value >= 0 && priv->curr_value->v.archetype.skill_adj[idx] > INT_MAX - value) {
				result = PARSE_ERROR_INVALID_VALUE;
				break;
			}
			if (value < 0 && priv->curr_value->v.archetype.skill_adj[idx] < INT_MIN - value) {
				result = PARSE_ERROR_INVALID_VALUE;
				break;
			}
			priv->curr_value->v.archetype.skill_adj[idx] += value;
			t = strtok(NULL, " |");
		}

		string_free(s);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_archetype_abilities(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_ARCHETYPE) {
		char *s = string_make(parser_getstr(p, "values"));
		char *t = s;

		while (1) {
			char *tnext;
			bool done;
			int idx;
			char *rb;
			struct ability *ab;

			tnext = t + strcspn(t, " |");
			if (tnext == t) {
				if (*tnext) {
					t = tnext + 1;
					continue;
				}
				break;
			}
			while (*tnext == ' ' && *(tnext - 1) != ']') {
				tnext += 1 + strcspn(tnext + 1, " |");
			}
			if (*tnext) {
				*tnext = '\0';
				++tnext;
				done = false;
			} else {
				done = true;
			}
			idx = STAT_MAX + 1;
			while (1) {
				if (idx > STAT_MAX + SKILL_MAX) {
					result = PARSE_ERROR_INVALID_SKILL;
					break;
				}
				if (prefix(t, obj_mods[idx])
						&& t[strlen(obj_mods[idx])] == '[') {
					break;
				}
				++idx;
			}
			if (result != PARSE_ERROR_NONE) {
				break;
			}
			t += strlen(obj_mods[idx]) + 1;
			rb = strchr(t, ']');
			if (!rb || rb[1]) {
				result = PARSE_ERROR_INVALID_ABILITY;
				break;
			}
			*rb = '\0';
			ab = lookup_ability(idx - STAT_MAX - 1, t);
			if (!ab) {
				result = PARSE_ERROR_INVALID_ABILITY;
				break;
			}
			assert(priv->curr_value->v.archetype.ability_count >= 0
				&& priv->curr_value->v.archetype.ability_count
				<= priv->curr_value->v.archetype.ability_alloc);
			if (priv->curr_value->v.archetype.ability_count
					== priv->curr_value->v.archetype.ability_alloc) {
				if (!priv->curr_value->v.archetype.ability_alloc) {
					priv->curr_value->v.archetype.ability_alloc = 4;
				} else if (priv->curr_value->v.archetype.ability_alloc
						>= 1024) {
					/*
					 * Cap the number so allocated size
					 * won't have the potential to overflow
					 * a size_t.
					 */
					result = PARSE_ERROR_TOO_MANY_ENTRIES;
					break;
				} else {
					priv->curr_value->v.archetype.ability_alloc +=
						priv->curr_value->v.archetype.ability_alloc;
				}
				priv->curr_value->v.archetype.added_abilities =
					mem_realloc(priv->curr_value->v.archetype.added_abilities,
					priv->curr_value->v.archetype.ability_alloc
					* sizeof(*priv->curr_value->v.archetype.added_abilities));
			}
			priv->curr_value->v.archetype.added_abilities[priv->curr_value->v.archetype.ability_count] = ab;
			++priv->curr_value->v.archetype.ability_count;

			if (done) {
				break;
			}
			t = tnext;
		}

		string_free(s);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_archetype_object(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	const char *numstr = parser_getsym(p, "number");
	const char *eqstr = parser_getsym(p, "equipped");
	int tval, sval;
	dice_t *numdice;
	struct tutorial_kit_item *this_kit;

	if (priv->curr_value->key->comp != TUTORIAL_ARCHETYPE) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	tval = tval_find_idx(parser_getsym(p, "tval"));
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	sval = lookup_sval(tval, parser_getsym(p, "sval"));
	if (sval < 0) {
		return PARSE_ERROR_UNRECOGNISED_SVAL;
	}

	assert(priv->curr_value->v.archetype.kit_count >= 0
		&& priv->curr_value->v.archetype.kit_count
		<= priv->curr_value->v.archetype.kit_alloc);
	if (priv->curr_value->v.archetype.kit_count
			== priv->curr_value->v.archetype.kit_alloc) {
		if (!priv->curr_value->v.archetype.kit_count) {
			priv->curr_value->v.archetype.kit_alloc = 4;
		} else if (priv->curr_value->v.archetype.kit_alloc >= 128) {
			/*
			 * Cap the number so allocated size won't have the
			 * potential to overflow a size_t.
			 */
			return PARSE_ERROR_TOO_MANY_ENTRIES;
		} else {
			priv->curr_value->v.archetype.kit_alloc +=
				priv->curr_value->v.archetype.kit_alloc;
		}
		priv->curr_value->v.archetype.kit = mem_realloc(
			priv->curr_value->v.archetype.kit,
			priv->curr_value->v.archetype.kit_alloc *
			sizeof(*priv->curr_value->v.archetype.kit));
	}

	this_kit = priv->curr_value->v.archetype.kit
		+ priv->curr_value->v.archetype.kit_count;

	numdice = dice_new();
	if (dice_parse_string(numdice, numstr)) {
		dice_random_value(numdice, &this_kit->item.v.details.number);
		dice_free(numdice);
	} else {
		dice_free(numdice);
		return PARSE_ERROR_INVALID_DICE;
	}

	++priv->curr_value->v.archetype.kit_count;
	this_kit->item.v.details.ego = NULL;
	this_kit->item.v.details.tweaks = NULL;
	this_kit->item.v.details.tval = tval;
	this_kit->item.v.details.sval = sval;
	this_kit->item.v.details.tweak_count = 0;
	this_kit->item.is_artifact = false;
	this_kit->equipped = (my_stricmp(eqstr, "yes") == 0);

	return PARSE_ERROR_NONE;
}


static enum parser_error parse_archetype_complex_object(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	const char *numstr = parser_getsym(p, "number");
	const char *eqstr = parser_getsym(p, "equipped");
	const char *ego = parser_getsym(p, "ego");
	const char *props = parser_getsym(p, "properties");
	const char *mods = parser_getstr(p, "modifiers");
	int tval, sval;
	struct tutorial_item_tweak *tweaks;
	int tweak_count, tweak_alloc;
	enum parser_error tweak_result;
	dice_t *numdice;
	struct tutorial_kit_item *this_kit;

	if (priv->curr_value->key->comp != TUTORIAL_ARCHETYPE) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	tval = tval_find_idx(parser_getsym(p, "tval"));
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	sval = lookup_sval(tval, parser_getsym(p, "sval"));
	if (sval < 0) {
		return PARSE_ERROR_UNRECOGNISED_SVAL;
	}

	assert(priv->curr_value->v.archetype.kit_count >= 0
		&& priv->curr_value->v.archetype.kit_count
		<= priv->curr_value->v.archetype.kit_alloc);
	if (priv->curr_value->v.archetype.kit_count
			== priv->curr_value->v.archetype.kit_alloc) {
		if (!priv->curr_value->v.archetype.kit_count) {
			priv->curr_value->v.archetype.kit_alloc = 4;
		} else if (priv->curr_value->v.archetype.kit_alloc >= 128) {
			/*
			 * Cap the number so allocated size won't have the
			 * potential to overflow a size_t.
			 */
			return PARSE_ERROR_TOO_MANY_ENTRIES;
		} else {
			priv->curr_value->v.archetype.kit_alloc +=
				priv->curr_value->v.archetype.kit_alloc;
		}
		priv->curr_value->v.archetype.kit = mem_realloc(
			priv->curr_value->v.archetype.kit,
			priv->curr_value->v.archetype.kit_alloc *
			sizeof(*priv->curr_value->v.archetype.kit));
	}

	tweak_result = tutorial_parse_tweaks(&tweaks, &tweak_count,
		&tweak_alloc, props, mods);
	if (tweak_result != PARSE_ERROR_NONE) {
		tutorial_item_tweaks_free(tweaks, tweak_count);
		return tweak_result;
	}

	this_kit = priv->curr_value->v.archetype.kit
		+ priv->curr_value->v.archetype.kit_count;

	numdice = dice_new();
	if (dice_parse_string(numdice, numstr)) {
		dice_random_value(numdice, &this_kit->item.v.details.number);
		dice_free(numdice);
	} else {
		dice_free(numdice);
		tutorial_item_tweaks_free(tweaks, tweak_count);
		return PARSE_ERROR_INVALID_DICE;
	}

	++priv->curr_value->v.archetype.kit_count;
	this_kit->item.v.details.ego =
		streq(ego, "NONE") ? NULL : lookup_ego_item(ego, tval, sval);
	this_kit->item.v.details.tweaks = tweaks;
	this_kit->item.v.details.tval = tval;
	this_kit->item.v.details.sval = sval;
	this_kit->item.v.details.tweak_count = tweak_count;
	this_kit->item.is_artifact = false;
	this_kit->equipped = (my_stricmp(eqstr, "yes") == 0);

	return PARSE_ERROR_NONE;
}


static enum parser_error parse_archetype_artifact(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	const char *eqstr = parser_getsym(p, "equipped");
	const struct artifact *art;
	struct tutorial_kit_item *this_kit;

	if (priv->curr_value->key->comp != TUTORIAL_ARCHETYPE) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	art = lookup_artifact_name(parser_getsym(p, "name"));
	if (!art) {
		return PARSE_ERROR_NO_ARTIFACT_NAME;
	}

	assert(priv->curr_value->v.archetype.kit_count >= 0
		&& priv->curr_value->v.archetype.kit_count
		<= priv->curr_value->v.archetype.kit_alloc);
	if (priv->curr_value->v.archetype.kit_count
			== priv->curr_value->v.archetype.kit_alloc) {
		if (!priv->curr_value->v.archetype.kit_count) {
			priv->curr_value->v.archetype.kit_alloc = 4;
		} else if (priv->curr_value->v.archetype.kit_alloc >= 128) {
			/*
			 * Cap the number so allocated size won't have the
			 * potential to overflow a size_t.
			 */
			return PARSE_ERROR_TOO_MANY_ENTRIES;
		} else {
			priv->curr_value->v.archetype.kit_alloc +=
				priv->curr_value->v.archetype.kit_alloc;
		}
		priv->curr_value->v.archetype.kit = mem_realloc(
			priv->curr_value->v.archetype.kit,
			priv->curr_value->v.archetype.kit_alloc *
			sizeof(*priv->curr_value->v.archetype.kit));
	}

	this_kit = priv->curr_value->v.archetype.kit
		+ priv->curr_value->v.archetype.kit_count;
	++priv->curr_value->v.archetype.kit_count;
	this_kit->item.v.art = art;
	this_kit->item.is_artifact = true;
	this_kit->equipped = (my_stricmp(eqstr, "yes") == 0);

	return PARSE_ERROR_NONE;
}


static void add_trigger_op(struct trigger_compiled_op **c, int *n_c, int *a_c,
		enum trigger_op_kind kind, int tval, int sval, int idx,
		char *name)
{
	struct trigger_compiled_op *this_op;

	assert(*n_c >= 0 && *n_c <= *a_c);
	if (*n_c == *a_c) {
		*a_c = (*a_c) ? *a_c + (*a_c) : 4;
		*c = mem_realloc(*c, *a_c * sizeof(**c));
	}
	this_op = (*c) + (*n_c);
	++*n_c;
	this_op->kind = kind;
	this_op->tval = tval;
	this_op->sval = sval;
	this_op->idx = idx;
	this_op->name = name;
}


static int get_drained_index(const char *name)
{
	int result = 0;

	while (1) {
		if (result < STAT_MAX) {
			if (streq(name, obj_mods[result + 1])) {
				return result;
			} else {
				++result;
			}
		} else if (streq(name, "HEALTH")) {
			return STAT_MAX;
		} else if (streq(name, "VOICE")) {
			return STAT_MAX + 1;
		} else {
			return -1;
		}
	}
}


static enum parser_error parse_trigger_condition(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	struct trigger_nesting {
		enum trigger_op_kind pend[2]; bool expect_binary;
	} *n;
	int n_n, a_n;
	struct trigger_compiled_op *c;
	int n_c, a_c;
	const char *s_expr;
	int i, curr_stack;

	if (priv->curr_value->key->comp != TUTORIAL_TRIGGER) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	/* Don't allow for multiple condition lines. */
	if (priv->curr_value->v.trigger.expr.ops) {
		return PARSE_ERROR_TOO_MANY_ENTRIES;
	}

	a_n = 4;
	n_n = 1;
	n = mem_alloc(a_n * sizeof(*n));
	n[0].pend[0] = TRIGGER_OP_NONE;
	n[0].pend[1] = TRIGGER_OP_NONE;
	n[0].expect_binary = false;
	n_c = 0;
	a_c = 0;
	c = NULL;
	s_expr = parser_getstr(p, "expression");
	while (*s_expr) {
		const char *rb, *term, *colon;
		char *tmp, *name;
		enum trigger_op_kind kind;
		int tval, sval, idx;

		assert(n_n > 0);
		switch (*s_expr) {
		case ' ':
		case '\t':
			/* Skip whitespace. */
			++s_expr;
			break;

		case '(':
			if (n[n_n - 1].expect_binary) {
				free_trigger_compiled_ops(c, n_c);
				mem_free(n);
				return PARSE_ERROR_INVALID_EXPRESSION;
			}
			++s_expr;
			if (n_n == a_n) {
				a_n += a_n;
				n = mem_realloc(n, a_n * sizeof(*n));
			}
			n[n_n].pend[0] = TRIGGER_OP_NONE;
			n[n_n].pend[1] = TRIGGER_OP_NONE;
			n[n_n].expect_binary = false;
			++n_n;
			break;

		case ')':
			if (n_n == 1 || n[n_n - 1].expect_binary) {
				free_trigger_compiled_ops(c, n_c);
				mem_free(n);
				return PARSE_ERROR_INVALID_EXPRESSION;
			}
			++s_expr;
			if (n[n_n - 1].pend[1] != TRIGGER_OP_NONE) {
				assert(n[n_n - 1].pend[1] == TRIGGER_OP_NOT);
				add_trigger_op(&c, &n_c, &a_c,
					n[n_n - 1].pend[1], 0, 0, 0, NULL);
			}
			if (n[n_n - 1].pend[0] != TRIGGER_OP_NONE) {
				assert(n[n_n - 1].pend[0] == TRIGGER_OP_AND
					|| n[n_n - 1].pend[0] == TRIGGER_OP_NOT
					|| n[n_n - 1].pend[0] == TRIGGER_OP_OR
					|| n[n_n - 1].pend[0] == TRIGGER_OP_XOR);
				add_trigger_op(&c, &n_c, &a_c,
					n[n_n - 1].pend[0], 0, 0, 0, NULL);
			}
			--n_n;
			if (n[n_n - 1].pend[1] != TRIGGER_OP_NONE) {
				assert(n[n_n - 1].pend[1] == TRIGGER_OP_NOT);
				add_trigger_op(&c, &n_c, &a_c,
					n[n_n - 1].pend[1], 0, 0, 0, NULL);
				n[n_n - 1].pend[1] = TRIGGER_OP_NONE;
			}
			if (n[n_n - 1].pend[0] != TRIGGER_OP_NONE) {
				assert(n[n_n - 1].pend[0] == TRIGGER_OP_AND
					|| n[n_n - 1].pend[0] == TRIGGER_OP_NOT
					|| n[n_n - 1].pend[0] == TRIGGER_OP_OR
					|| n[n_n - 1].pend[0] == TRIGGER_OP_XOR);
				add_trigger_op(&c, &n_c, &a_c,
					n[n_n - 1].pend[0], 0, 0, 0, NULL);
				n[n_n - 1].pend[0] = TRIGGER_OP_NONE;
			}
			n[n_n - 1].expect_binary = true;
			break;

		case 'a':
			if (!n[n_n - 1].expect_binary || *(s_expr + 1) != 'n'
					|| *(s_expr + 2) != 'd') {
				free_trigger_compiled_ops(c, n_c);
				mem_free(n);
				return PARSE_ERROR_INVALID_EXPRESSION;
			}
			s_expr += 3;
			assert(n[n_n - 1].pend[0] == TRIGGER_OP_NONE);
			n[n_n - 1].pend[0] = TRIGGER_OP_AND;
			n[n_n - 1].expect_binary = false;
			break;

		case 'n':
			if (n[n_n - 1].expect_binary || *(s_expr + 1) != 'o'
					|| *(s_expr + 2) != 't') {
				free_trigger_compiled_ops(c, n_c);
				mem_free(n);
				return PARSE_ERROR_INVALID_EXPRESSION;
			}
			s_expr += 3;
			if (n[n_n - 1].pend[0] == TRIGGER_OP_NONE) {
				n[n_n - 1].pend[0] = TRIGGER_OP_NOT;
			} else {
				assert(n[n_n - 1].pend[1] == TRIGGER_OP_NONE);
				n[n_n - 1].pend[1] = TRIGGER_OP_NOT;
			}
			break;

		case 'o':
			if (!n[n_n - 1].expect_binary || *(s_expr + 1) != 'r') {
				free_trigger_compiled_ops(c, n_c);
				mem_free(n);
				return PARSE_ERROR_INVALID_EXPRESSION;
			}
			s_expr += 2;
			assert(n[n_n - 1].pend[0] == TRIGGER_OP_NONE);
			n[n_n - 1].pend[0] = TRIGGER_OP_OR;
			n[n_n - 1].expect_binary = false;
			break;

		case 'x':
			if (!n[n_n - 1].expect_binary || *(s_expr + 1) != 'o'
					|| *(s_expr + 2) != 'r') {
				free_trigger_compiled_ops(c, n_c);
				mem_free(n);
				return PARSE_ERROR_INVALID_EXPRESSION;
			}
			s_expr += 3;
			assert(n[n_n - 1].pend[0] == TRIGGER_OP_NONE);
			n[n_n - 1].pend[0] = TRIGGER_OP_XOR;
			n[n_n - 1].expect_binary = false;
			break;

		case '{':
			term = s_expr + 1;
			/* Find unescaped right brace. */
			rb = term;
			while (1) {
				rb = strchr(rb, '}');
				if (!rb || !tutorial_text_escaped(rb, term)) {
					break;
				}
				++rb;
			}
			if (n[n_n - 1].expect_binary || !rb) {
				free_trigger_compiled_ops(c, n_c);
				mem_free(n);
				return PARSE_ERROR_INVALID_EXPRESSION;
			}
			s_expr = rb + 1;
			idx = 0;
			tval = 0;
			sval = 0;
			name = NULL;
			if (prefix(term, "ability:")) {
				enum parser_error ec = PARSE_ERROR_NONE;

				term += 8;
				kind = TRIGGER_OP_ABILITY;

				/* Find unescaped right colon. */
				colon = term;
				while (1) {
					colon = strchr(colon, ':');
					if (!colon || !tutorial_text_escaped(colon, term)) {
						break;
					}
					++colon;
				}
				if (colon && colon < rb) {
					tmp = mem_alloc((colon - term) + 1);
					(void) tutorial_copy_strip_escapes(tmp,
						(colon - term) + 1, term,
						colon - term);
					idx = code_index_in_array(obj_mods,
						tmp);
					if (idx >= STAT_MAX + 1 &&
							idx <= STAT_MAX
							+ SKILL_MAX) {
						idx -= STAT_MAX + 1;
					} else {
						ec = PARSE_ERROR_UNRECOGNISED_SKILL;
					}
					mem_free(tmp);
				} else {
					ec = PARSE_ERROR_UNRECOGNISED_SKILL;
				}
				if (ec != PARSE_ERROR_NONE) {
					free_trigger_compiled_ops(c, n_c);
					mem_free(n);
					return ec;
				}
				term = colon + 1;
				name = mem_alloc((rb - term) + 1);
				(void) tutorial_copy_strip_escapes(
					name, (rb - term) + 1, term,
					rb - term);
			} else if (prefix(term, "carried:")) {
				enum parser_error ec = PARSE_ERROR_NONE;

				term += 8;
				kind = TRIGGER_OP_CARRIED;

				/* Find unescaped right colon. */
				colon = term;
				while (1) {
					colon = strchr(colon, ':');
					if (!colon || !tutorial_text_escaped(colon, term)) {
						break;
					}
					++colon;
				}
				if (colon && colon < rb) {
					tmp = mem_alloc((colon - term) + 1);
					(void) tutorial_copy_strip_escapes(tmp,
						(colon - term) + 1, term,
						colon - term);
					tval = tval_find_idx(tmp);
					mem_free(tmp);
					term = colon + 1;
					if (tval < 0) {
						ec = PARSE_ERROR_UNRECOGNISED_TVAL;
					}
				} else {
					ec = PARSE_ERROR_INVALID_EXPRESSION;
				}
				if (tval > 0) {
					tmp = mem_alloc((rb - term) + 1);
					(void) tutorial_copy_strip_escapes(tmp,
						(rb - term) + 1, term,
						rb - term);
					if (streq(tmp, "*")) {
						sval = -1;
					} else {
						sval = lookup_sval(tval, tmp);
						if (sval < 0) {
							ec = PARSE_ERROR_UNRECOGNISED_SVAL;
						}
					}
					mem_free(tmp);
				}
				if (ec != PARSE_ERROR_NONE) {
					free_trigger_compiled_ops(c, n_c);
					mem_free(n);
					return ec;
				}
			} else if (prefix(term, "drained:")) {
				term += 8;
				kind = TRIGGER_OP_DRAINED;

				tmp = mem_alloc((rb - term) + 1);
				(void) tutorial_copy_strip_escapes(tmp,
					(rb - term) + 1, term, rb - term);
				idx = get_drained_index(tmp);
				mem_free(tmp);
				if (idx == -1) {
					free_trigger_compiled_ops(c, n_c);
					mem_free(n);
					return PARSE_ERROR_INVALID_EXPRESSION;
				}
			} else if (prefix(term, "equipped:")) {
				enum parser_error ec = PARSE_ERROR_NONE;

				term += 9;
				kind = TRIGGER_OP_EQUIPPED;

				/* Find unescaped colon. */
				colon = term;
				while (1) {
					colon = strchr(colon, ':');
					if (!colon || !tutorial_text_escaped(colon, term)) {
						break;
					}
					++colon;
				}
				if (colon && colon < rb) {
					tmp = mem_alloc((colon - term) + 1);
					(void) tutorial_copy_strip_escapes(tmp,
						(colon - term) + 1, term,
						colon - term);
					tval = tval_find_idx(tmp);
					mem_free(tmp);
					term = colon + 1;
					if (tval < 0) {
						ec = PARSE_ERROR_UNRECOGNISED_TVAL;
					}
				} else {
					ec = PARSE_ERROR_INVALID_EXPRESSION;
				}
				if (tval > 0) {
					tmp = mem_alloc((rb - term) + 1);
					(void) tutorial_copy_strip_escapes(tmp,
						(rb - term) + 1, term,
						rb - term);
					if (streq(tmp, "*")) {
						sval = -1;
					} else {
						sval = lookup_sval(tval, tmp);
						if (sval < 0) {
							ec = PARSE_ERROR_UNRECOGNISED_SVAL;
						}
					}
					mem_free(tmp);
				}
				if (ec != PARSE_ERROR_NONE) {
					free_trigger_compiled_ops(c, n_c);
					mem_free(n);
					return ec;
				}
			} else if (prefix(term, "false}")) {
				kind = TRIGGER_OP_FALSE;
			} else if (prefix(term, "timed:")) {
				enum parser_error ec = PARSE_ERROR_NONE;

				term += 6;

				/* Find unescaped colon. */
				colon = term;
				while (1) {
					colon = strchr(colon, ':');
					if (!colon || !tutorial_text_escaped(colon, term)) {
						break;
					}
					++colon;
				}
				if (colon && colon < rb) {
					tmp = mem_alloc((colon - term) + 1);
					(void) tutorial_copy_strip_escapes(tmp,
						(colon - term) + 1, term,
						colon - term);
					idx = timed_name_to_idx(tmp);
					mem_free(tmp);
					if (idx < 0) {
						ec = PARSE_ERROR_INVALID_EXPRESSION;
					}
					term = colon + 1;
					if (prefix(term, "above:")) {
						kind = TRIGGER_OP_TIMED_ABOVE;
						term += 6;
					} else if (prefix(term, "below:")) {
						kind = TRIGGER_OP_TIMED_BELOW;
						term += 6;
					} else {
						ec = PARSE_ERROR_INVALID_EXPRESSION;
					}
					if (ec != PARSE_ERROR_INVALID_EXPRESSION) {
						name = mem_alloc((rb - term) + 1);
						(void) tutorial_copy_strip_escapes(
							name, (rb - term) + 1,
							term, rb - term);
					}
				} else {
					kind = TRIGGER_OP_TIMED;
					tmp = mem_alloc((rb - term) + 1);
					(void) tutorial_copy_strip_escapes(tmp,
						(rb - term) + 1, term,
						rb - term);
					idx = timed_name_to_idx(tmp);
					mem_free(tmp);
					if (idx < 0) {
						ec = PARSE_ERROR_INVALID_EXPRESSION;
					}
				}
				if (ec != PARSE_ERROR_NONE) {
					free_trigger_compiled_ops(c, n_c);
					mem_free(n);
					return ec;
				}
			} else if (prefix(term, "true}")) {
				kind = TRIGGER_OP_TRUE;
			} else {
				free_trigger_compiled_ops(c, n_c);
				mem_free(n);
				return PARSE_ERROR_INVALID_EXPRESSION;
			}
			add_trigger_op(&c, &n_c, &a_c, kind, tval, sval, idx,
				name);
			if (n[n_n - 1].pend[1] != TRIGGER_OP_NONE) {
				assert(n[n_n - 1].pend[1] == TRIGGER_OP_NOT);
				add_trigger_op(&c, &n_c, &a_c,
					n[n_n - 1].pend[1], 0, 0, 0, NULL);
				n[n_n - 1].pend[1] = TRIGGER_OP_NONE;
			}
			if (n[n_n - 1].pend[0] != TRIGGER_OP_NONE) {
				assert(n[n_n - 1].pend[0] == TRIGGER_OP_AND
					|| n[n_n - 1].pend[0] == TRIGGER_OP_NOT
					|| n[n_n - 1].pend[0] == TRIGGER_OP_OR
					|| n[n_n - 1].pend[0] == TRIGGER_OP_XOR);
				add_trigger_op(&c, &n_c, &a_c,
					n[n_n - 1].pend[0], 0, 0, 0, NULL);
				n[n_n - 1].pend[0] = TRIGGER_OP_NONE;
			}
			n[n_n - 1].expect_binary = true;
			break;

		default:
			free_trigger_compiled_ops(c, n_c);
			mem_free(n);
			return PARSE_ERROR_INVALID_EXPRESSION;
		}
	}

	mem_free(n);
	priv->curr_value->v.trigger.expr.ops = c;
	priv->curr_value->v.trigger.expr.n_op = n_c;
	/* Determine the stack space that'll be needed. */
	priv->curr_value->v.trigger.expr.n_stack = 0;
	for (i = 0, curr_stack = 0; i < n_c; ++i) {
		switch (c[i].kind) {
		case TRIGGER_OP_ABILITY:
		case TRIGGER_OP_CARRIED:
		case TRIGGER_OP_DRAINED:
		case TRIGGER_OP_EQUIPPED:
		case TRIGGER_OP_FALSE:
		case TRIGGER_OP_TIMED:
		case TRIGGER_OP_TIMED_ABOVE:
		case TRIGGER_OP_TIMED_BELOW:
		case TRIGGER_OP_TRUE:
			/* Push one item onto the stack. */
			++curr_stack;
			if (priv->curr_value->v.trigger.expr.n_stack
					< curr_stack) {
				priv->curr_value->v.trigger.expr.n_stack =
					curr_stack;
			}
			break;

		case TRIGGER_OP_NOT:
			/* Consumes one item and pushes one back for no change. */
			assert(curr_stack > 0);
			break;

		case TRIGGER_OP_AND:
		case TRIGGER_OP_OR:
		case TRIGGER_OP_XOR:
			/* Consume two items, push one back, for a change of -1.  */
			assert(curr_stack > 1);
			--curr_stack;
			break;

		default:
			assert(0);
			break;
		}
	}

	return PARSE_ERROR_NONE;
}


static enum parser_error parse_section_rows(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_SECTION) {
		int rows = parser_getint(p, "value");

		if (rows > 0 && rows < z_info->dungeon_hgt - 1) {
			if (!priv->curr_value->v.section.lines) {
				priv->curr_value->v.section.rows = rows;
			} else {
				result = PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
			}
		} else {
			result = PARSE_ERROR_INVALID_VALUE;
		}
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_section_columns(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_SECTION) {
		int columns = parser_getint(p, "value");

		if (columns > 0 && columns < z_info->dungeon_wid - 1) {
			if (!priv->curr_value->v.section.lines) {
				priv->curr_value->v.section.columns = columns;
			} else {
				result = PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
			}
		} else {
			result = PARSE_ERROR_INVALID_VALUE;
		}
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_section_area_flag(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_SECTION) {
		result = tutorial_add_area_flags(p, priv->curr_value, false);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_section_clear_area_flag(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_SECTION) {
		result = tutorial_add_area_flags(p, priv->curr_value, true);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_section_start_note(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_SECTION) {
		const char* name = parser_getstr(p, "name");

		if (priv->curr_value->v.section.start_note_name) {
			string_free(priv->curr_value->v.section.start_note_name);
		}
		priv->curr_value->v.section.start_note_name = string_make(name);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_trigger_or_section_death_note(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_TRIGGER) {
		const char* name = parser_getstr(p, "name");

		if (priv->curr_value->v.trigger.death_note_name) {
			string_free(priv->curr_value->v.trigger.death_note_name);
		}
		priv->curr_value->v.trigger.death_note_name =
			(name[0]) ? string_make(name) : NULL;
		priv->curr_value->v.trigger.changes_death_note = true;
	} else if (priv->curr_value->key->comp == TUTORIAL_SECTION) {
		const char* name = parser_getstr(p, "name");

		if (priv->curr_value->v.section.death_note_name) {
			string_free(priv->curr_value->v.section.death_note_name);
		}
		priv->curr_value->v.section.death_note_name = string_make(name);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_section_note(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	struct tutorial_section_sym_key *key;
	enum parser_error result;

	if (priv->curr_value->key->comp != TUTORIAL_SECTION) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	result = tutorial_section_parse_symbol(parser_getsym(p, "symbol"),
		priv, priv->curr_value->v.section.symt, &key);
	if (result == PARSE_ERROR_NONE) {
		struct tutorial_section_sym_val *value =
			mem_alloc(sizeof(*value));

		value->v.name = string_make(parser_getstr(p, "name"));
		value->is_predefined = false;
		value->kind = SECTION_SYM_NOTE;
		if (!tutorial_section_sym_table_insert(
				priv->curr_value->v.section.symt, key, value)) {
			tutorial_section_sym_value_free(value);
			mem_free(key);
			result = PARSE_ERROR_DUPLICATED_SYMBOL;
		}
	}
	return result;
}


static enum parser_error parse_section_trigger(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	struct tutorial_section_sym_key *key;
	enum parser_error result;

	if (priv->curr_value->key->comp != TUTORIAL_SECTION) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	result = tutorial_section_parse_symbol(parser_getsym(p, "symbol"),
		priv, priv->curr_value->v.section.symt, &key);
	if (result == PARSE_ERROR_NONE) {
		struct tutorial_section_sym_val *value =
			mem_alloc(sizeof(*value));

		value->v.name = string_make(parser_getstr(p, "name"));
		value->is_predefined = false;
		value->kind = SECTION_SYM_TRIGGER;
		if (!tutorial_section_sym_table_insert(
				priv->curr_value->v.section.symt, key, value)) {
			tutorial_section_sym_value_free(value);
			mem_free(key);
			result = PARSE_ERROR_DUPLICATED_SYMBOL;
		}
	}
	return result;
}


static enum parser_error parse_section_gate(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	int feat;
	struct tutorial_section_sym_key *key;
	enum parser_error result;

	if (priv->curr_value->key->comp != TUTORIAL_SECTION) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	feat = lookup_feat(parser_getsym(p, "terrain"));
	if (feat < 0) {
		return PARSE_ERROR_INVALID_TERRAIN;
	}

	result = tutorial_section_parse_symbol(parser_getsym(p, "symbol"),
		priv, priv->curr_value->v.section.symt, &key);
	if (result == PARSE_ERROR_NONE) {
		struct tutorial_section_sym_val *value =
			mem_alloc(sizeof(*value));

		value->v.gate.dest =
			string_make(parser_getsym(p, "destination"));
		value->v.gate.feat = feat;
		if (parser_hasval(p, "note")) {
			value->v.gate.note =
				string_make(parser_getstr(p, "note"));
		} else {
			value->v.gate.note = NULL;
		}
		value->is_predefined = false;
		value->kind = SECTION_SYM_GATE;
		if (!tutorial_section_sym_table_insert(
				priv->curr_value->v.section.symt, key, value)) {
			tutorial_section_sym_value_free(value);
			mem_free(key);
			result = PARSE_ERROR_DUPLICATED_SYMBOL;
		}
	}
	return result;
}


static enum parser_error parse_section_forge(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	const char *tstr = parser_getstr(p, "type");
	int feat = FEAT_NONE;
	struct tutorial_section_sym_key *key;
	enum parser_error result;

	if (priv->curr_value->key->comp != TUTORIAL_SECTION) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	if (streq(tstr, "NORMAL")) {
		feat = FEAT_FORGE;
	} else if (streq(tstr, "ENCHANTED")) {
		feat = FEAT_FORGE_GOOD;
	} else if (streq(tstr, "UNIQUE")) {
		feat = FEAT_FORGE_UNIQUE;
	} else {
		return PARSE_ERROR_INVALID_VALUE;
	}

	result = tutorial_section_parse_symbol(parser_getsym(p, "symbol"),
		priv, priv->curr_value->v.section.symt, &key);
	if (result == PARSE_ERROR_NONE) {
		struct tutorial_section_sym_val *value =
			mem_alloc(sizeof(*value));

		value->v.forge.feat = feat;
		value->v.forge.uses = parser_getint(p, "uses");
		value->is_predefined = false;
		value->kind = SECTION_SYM_FORGE;
		if (!tutorial_section_sym_table_insert(
				priv->curr_value->v.section.symt, key, value)) {
			tutorial_section_sym_value_free(value);
			mem_free(key);
			result = PARSE_ERROR_DUPLICATED_SYMBOL;
		}
	}
	return result;
}


static enum parser_error parse_section_object(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	int tval, sval;
	const char *numstr;
	dice_t *numdice;
	random_value rv;
	struct tutorial_section_sym_key *key;
	enum parser_error result;

	if (priv->curr_value->key->comp != TUTORIAL_SECTION) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	tval = tval_find_idx(parser_getsym(p, "tval"));
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	sval = lookup_sval(tval, parser_getsym(p, "sval"));
	if (sval < 0) {
		return PARSE_ERROR_UNRECOGNISED_SVAL;
	}
	numstr = parser_getstr(p, "number");
	numdice = dice_new();
	if (!dice_parse_string(numdice, numstr)) {
		dice_free(numdice);
		return PARSE_ERROR_INVALID_DICE;
	}
	dice_random_value(numdice, &rv);
	dice_free(numdice);

	result = tutorial_section_parse_symbol(parser_getsym(p, "symbol"),
		priv, priv->curr_value->v.section.symt, &key);
	if (result == PARSE_ERROR_NONE) {
		struct tutorial_section_sym_val *value =
			mem_alloc(sizeof(*value));

		value->v.item.v.details.ego = NULL;
		value->v.item.v.details.tweaks = NULL;
		value->v.item.v.details.number = rv;
		value->v.item.v.details.tval = tval;
		value->v.item.v.details.sval = sval;
		value->v.item.v.details.tweak_count = 0;
		value->v.item.is_artifact = false;
		value->is_predefined = false;
		value->kind = SECTION_SYM_ITEM;
		if (!tutorial_section_sym_table_insert(
				priv->curr_value->v.section.symt, key, value)) {
			tutorial_section_sym_value_free(value);
			mem_free(key);
			result = PARSE_ERROR_DUPLICATED_SYMBOL;
		}
	}
	return result;
}


static enum parser_error parse_section_complex_object(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	int tval, sval;
	const char *numstr;
	dice_t *numdice;
	random_value rv;
	const char *ego;
	struct tutorial_item_tweak *tweaks;
	int tweak_count, tweak_alloc;
	struct tutorial_section_sym_key *key;
	enum parser_error result;

	if (priv->curr_value->key->comp != TUTORIAL_SECTION) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	tval = tval_find_idx(parser_getsym(p, "tval"));
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	sval = lookup_sval(tval, parser_getsym(p, "sval"));
	if (sval < 0) {
		return PARSE_ERROR_UNRECOGNISED_SVAL;
	}
	numstr = parser_getsym(p, "number");
	numdice = dice_new();
	if (!dice_parse_string(numdice, numstr)) {
		dice_free(numdice);
		return PARSE_ERROR_INVALID_DICE;
	}
	dice_random_value(numdice, &rv);
	dice_free(numdice);

	ego = parser_getsym(p, "ego");
	result = tutorial_parse_tweaks(&tweaks, &tweak_count, &tweak_alloc,
		parser_getsym(p, "properties"), parser_getstr(p, "modifiers"));
	if (result != PARSE_ERROR_NONE) {
		tutorial_item_tweaks_free(tweaks, tweak_count);
		return result;
	}

	result = tutorial_section_parse_symbol(parser_getsym(p, "symbol"),
		priv, priv->curr_value->v.section.symt, &key);
	if (result == PARSE_ERROR_NONE) {
		struct tutorial_section_sym_val *value =
			mem_alloc(sizeof(*value));

		value->v.item.v.details.ego = streq(ego, "NONE") ?
			NULL : lookup_ego_item(ego, tval, sval);
		value->v.item.v.details.tweaks = tweaks;
		value->v.item.v.details.number = rv;
		value->v.item.v.details.tval = tval;
		value->v.item.v.details.sval = sval;
		value->v.item.v.details.tweak_count = tweak_count;
		value->v.item.is_artifact = false;
		value->is_predefined = false;
		value->kind = SECTION_SYM_ITEM;
		if (!tutorial_section_sym_table_insert(
				priv->curr_value->v.section.symt, key, value)) {
			tutorial_section_sym_value_free(value);
			mem_free(key);
			result = PARSE_ERROR_DUPLICATED_SYMBOL;
		}
	} else {
		tutorial_item_tweaks_free(tweaks, tweak_count);
	}
	return result;
}


static enum parser_error parse_section_artifact(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	const struct artifact *art;
	struct tutorial_section_sym_key *key;
	enum parser_error result;

	if (priv->curr_value->key->comp != TUTORIAL_SECTION) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	art = lookup_artifact_name(parser_getstr(p, "name"));
	if (!art) {
		return PARSE_ERROR_NO_ARTIFACT_NAME;
	}

	result = tutorial_section_parse_symbol(parser_getsym(p, "symbol"),
		priv, priv->curr_value->v.section.symt, &key);
	if (result == PARSE_ERROR_NONE) {
		struct tutorial_section_sym_val *value =
			mem_alloc(sizeof(*value));

		value->v.item.v.art = art;
		value->v.item.is_artifact = true;
		value->is_predefined = false;
		value->kind = SECTION_SYM_ITEM;
		if (!tutorial_section_sym_table_insert(
				priv->curr_value->v.section.symt, key, value)) {
			tutorial_section_sym_value_free(value);
			mem_free(key);
			result = PARSE_ERROR_DUPLICATED_SYMBOL;
		}
	}
	return result;
}


static enum parser_error parse_section_monster(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	struct monster_race *race;
	const char *alert_str;
	int sleepiness;
	bool sleepiness_fixed;
	struct tutorial_section_sym_key *key;
	enum parser_error result;

	if (priv->curr_value->key->comp != TUTORIAL_SECTION) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	race = lookup_monster(parser_getsym(p, "race"));
	if (race == NULL) {
		return PARSE_ERROR_INVALID_MONSTER;
	}
	alert_str = parser_getsym(p, "alertness");
	if (streq(alert_str, "ALERT")) {
		sleepiness = 0;
		sleepiness_fixed = true;
	} else if (streq(alert_str, "ASLEEP")) {
		sleepiness = race->sleep;
		sleepiness_fixed = (race->sleep <= 0);
	} else {
		char *p_end;
		long lalert = strtol(alert_str, &p_end, 10);

		if (alert_str[0] == '\0' || *p_end != '\0') {
			return PARSE_ERROR_INVALID_VALUE;
		}
		if (lalert < ALERTNESS_MIN || lalert > ALERTNESS_MAX) {
			return PARSE_ERROR_OUT_OF_BOUNDS;
		}
		sleepiness = ALERTNESS_ALERT - (int)lalert;
		sleepiness_fixed = true;
	}

	result = tutorial_section_parse_symbol(parser_getsym(p, "symbol"),
		priv, priv->curr_value->v.section.symt, &key);
	if (result == PARSE_ERROR_NONE) {
		struct tutorial_section_sym_val *value =
			mem_alloc(sizeof(*value));

		value->v.monster.race = race;
		if (parser_hasval(p, "note")) {
			value->v.monster.note =
				string_make(parser_getstr(p, "note"));
		} else {
			value->v.monster.note = NULL;
		}
		value->v.monster.sleepiness = sleepiness;
		value->v.monster.sleepiness_fixed = sleepiness_fixed;
		value->is_predefined = false;
		value->kind = SECTION_SYM_MONSTER;
		if (!tutorial_section_sym_table_insert(
				priv->curr_value->v.section.symt, key, value)) {
			tutorial_section_sym_value_free(value);
			mem_free(key);
			result = PARSE_ERROR_DUPLICATED_SYMBOL;
		}
	}
	return result;
}


static enum parser_error parse_section_trap(struct parser *p)
{
	const char *trap_flags[] = { "NONE", "VISIBLE", "INVISIBLE", NULL };
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	struct trap_kind *trap;
	bool vis, invis;
	char *flags, *s;
	struct tutorial_section_sym_key *key;
	enum parser_error result;

	if (priv->curr_value->key->comp != TUTORIAL_SECTION) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	trap = lookup_trap(parser_getsym(p, "name"));
	vis = false;
	invis = false;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		int idx = lookup_flag(trap_flags, s);

		if (idx == 1) {
			vis = true;
		} else if (idx == 2) {
			invis = true;
		} else if (idx < 0) {
			string_free(flags);
			return PARSE_ERROR_INVALID_FLAG;
		}
		s = strtok(NULL, " |");
	}
	string_free(flags);

	result = tutorial_section_parse_symbol(parser_getsym(p, "symbol"),
		priv, priv->curr_value->v.section.symt, &key);
	if (result == PARSE_ERROR_NONE) {
		struct tutorial_section_sym_val *value =
			mem_alloc(sizeof(*value));

		value->v.trap.kind = trap;
		value->v.trap.vis = vis;
		value->v.trap.invis = invis;
		value->is_predefined = false;
		value->kind = SECTION_SYM_TRAP;
		if (!tutorial_section_sym_table_insert(
				priv->curr_value->v.section.symt, key, value)) {
			tutorial_section_sym_value_free(value);
			mem_free(key);
			result = PARSE_ERROR_DUPLICATED_SYMBOL;
		}
	}
	return result;
}


static enum parser_error parse_section_door(struct parser *p)
{
	const char *door_flags[] = {
		"NONE",
		"BROKEN",
		"OPEN",
		"CLOSED",
		"SECRET",
		"LOCK_1",
		"LOCK_2",
		"LOCK_5",
		"LOCK_10",
		"LOCK_20",
		"LOCK_50",
		"STUCK_1",
		"STUCK_2",
		"STUCK_5",
		"STUCK_10",
		"STUCK_20",
		"STUCK_50",
		NULL
	};
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	int feat;
	int power;
	char *flags, *s;
	struct tutorial_section_sym_key *key;
	enum parser_error result;

	if (priv->curr_value->key->comp != TUTORIAL_SECTION) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	feat = FEAT_CLOSED;
	power = 0;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		int idx = lookup_flag(door_flags, s);

		switch (idx) {
			case 0: break;
			case 1: feat = FEAT_BROKEN; break;
			case 2: feat = FEAT_OPEN; break;
			case 3: feat = FEAT_CLOSED; break;
			case 4: feat = FEAT_SECRET; break;
			case 5: ++power; break;
			case 6: power += 2; break;
			case 7: power += 5; break;
			case 8: power += 10; break;
			case 9: power += 20; break;
			case 10: power += 50; break;
			case 11: --power; break;
			case 12: power -= 2; break;
			case 13: power -= 5; break;
			case 14: power -= 10; break;
			case 15: power -= 20; break;
			case 16: power -= 50; break;
			default:
				string_free(flags);
				return PARSE_ERROR_INVALID_FLAG;
		}
		s = strtok(NULL, " |");
	}
	string_free(flags);

	result = tutorial_section_parse_symbol(parser_getsym(p, "symbol"),
		priv, priv->curr_value->v.section.symt, &key);
	if (result == PARSE_ERROR_NONE) {
		struct tutorial_section_sym_val *value =
			mem_alloc(sizeof(*value));

		value->v.door.feat = feat;
		/*
		 * struct trap limits the lock power to a uint8_t; code in
		 * trap.c doesn't allow for secret doors to be locked or stuck.
		 */
		value->v.door.power = (feat == FEAT_CLOSED) ?
			MAX(MIN(power, 255), -255) : 0;
		value->is_predefined = false;
		value->kind = SECTION_SYM_DOOR;
		if (!tutorial_section_sym_table_insert(
				priv->curr_value->v.section.symt, key, value)) {
			tutorial_section_sym_value_free(value);
			mem_free(key);
			result = PARSE_ERROR_DUPLICATED_SYMBOL;
		}
	}
	return result;
}


static enum parser_error parse_section_line(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	const char *line;

	if (priv->curr_value->key->comp != TUTORIAL_SECTION) {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	/* Need the rows and columns lines before this. */
	if (!priv->curr_value->v.section.rows
			|| !priv->curr_value->v.section.columns) {
		return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
	}

	if (priv->section_lines_parsed == 0) {
		assert(!priv->curr_value->v.section.lines);
		priv->curr_value->v.section.lines = mem_zalloc(
			priv->curr_value->v.section.rows *
			sizeof(*(priv->curr_value->v.section.lines)));
	} else if (priv->section_lines_parsed
			>= priv->curr_value->v.section.rows) {
		return PARSE_ERROR_TOO_MANY_ENTRIES;
	}

	assert(!priv->curr_value->v.section.lines[priv->section_lines_parsed]);
	line = parser_getstr(p, "line");
	if (utf8_strlen(line) != (size_t) priv->curr_value->v.section.columns) {
		return PARSE_ERROR_VAULT_DESC_WRONG_LENGTH;
	}
	priv->curr_value->v.section.lines[priv->section_lines_parsed] =
		string_make(line);
	++priv->section_lines_parsed;

	return PARSE_ERROR_NONE;
}


static enum parser_error parse_note_or_trigger_text(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	const char *text = parser_getstr(p, "contents");
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_NOTE) {
		priv->curr_value->v.note.text = string_append(
			priv->curr_value->v.note.text, text);
	} else if (priv->curr_value->key->comp == TUTORIAL_TRIGGER) {
		priv->curr_value->v.trigger.text = string_append(
			priv->curr_value->v.trigger.text, text);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


static enum parser_error parse_archetype_or_section_flags(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	enum parser_error result = PARSE_ERROR_NONE;

	if (priv->curr_value->key->comp == TUTORIAL_ARCHETYPE
			|| priv->curr_value->key->comp == TUTORIAL_SECTION) {
		char *flags = string_make(parser_getstr(p, "flags")), *s;

		s = strtok(flags, " |");
		while (s) {
			if (streq(s, "DEFAULT")) {
				if (priv->curr_value->key->comp
						== TUTORIAL_ARCHETYPE) {
					priv->r->default_archetype =
						priv->curr_value;
				} else {
					priv->r->default_section =
						priv->curr_value;
				}
			} else if (streq(s, "PURGE_NORMAL_KIT")
					&& priv->curr_value->key->comp == TUTORIAL_ARCHETYPE) {
				priv->curr_value->v.archetype.purge_kit = true;
			} else {
				result = PARSE_ERROR_INVALID_FLAG;
			}
			s = strtok(NULL, " |");
		}
		string_free(flags);
	} else {
		result = PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return result;
}


/**
 * Run the parser for tutorial.txt.
 */
static errr tutorial_run_parser(struct parser *p)
{
	return parse_file_quit_not_found(p, "tutorial");
}


/**
 * Parse tutorial.txt.
 */
void tutorial_parse_data(void)
{
	run_parser(&tutorial_parser);
}


/**
 * Release the parsed data for the tutorial.
 */
void tutorial_cleanup_parsed_data(void)
{
	if (tutorial_parsed_data.d.d) {
		tutorial_dict_destroy(tutorial_parsed_data.d);
		tutorial_parsed_data.d.d = NULL;
	}
	tutorial_parsed_data.default_archetype = NULL;
	tutorial_parsed_data.default_section = NULL;
	mem_free(tutorial_parsed_data.pval_to_note_table);
	tutorial_parsed_data.pval_to_note_table = NULL;
	tutorial_cleanup_trigger_gate_map(
		tutorial_parsed_data.trigger_gate_map);
	tutorial_parsed_data.trigger_gate_map = NULL;
	tutorial_parsed_data.curr_death_note = NULL;
	tutorial_parsed_data.note_table_n = 0;
	tutorial_parsed_data.note_table_a = 0;
}


/**
 * Get a key's value from a tutorial section's symbol table.  This is a thin
 * layer of type checking over the generic dictionary type.
 */
struct tutorial_section_sym_val *tutorial_section_sym_table_has(
		struct tutorial_section_sym_table t,
		const struct tutorial_section_sym_key *key)
{
	return (struct tutorial_section_sym_val*) dict_has(t.d, key);
}


/**
 * Get a key's value from the tutorial's dictionary.  This is a thin
 * layer of type checking over the generic dictionary type.
 */
struct tutorial_dict_val_type *tutorial_dict_has(struct tutorial_dict_type d,
		const struct tutorial_dict_key_type *key)
{
	return (struct tutorial_dict_val_type*) dict_has(d.d, key);
}


/**
 * Return whether a character has been escaped.
 *
 * \param cursor is the location of the character to check.
 * \param limit is the bound for how far back before cursor to check.
 */
bool tutorial_text_escaped(const char *cursor, const char *limit)
{
	int count = 0;

	/*
	 * An odd number of backslashes immediately prior to it means it's
	 * escaped.
	 */
	while (cursor > limit && *(cursor - 1) == '\\') {
		++count;
		--cursor;
	}
	return (count & 1) != 0;
}


/**
 * Copy from src to dest while handling backslashes.
 *
 * \param dest is the destination for the copy.
 * \param sz is the maximum number of characters, including a terminating null,
 * to copy.
 * \param src is the source for the copy.
 * \param rd is the maximum number of characters to consume from src.
 * \return the number of characters, not including the terminating null,
 * consumed from src.
 */
size_t tutorial_copy_strip_escapes(char *dest, size_t sz, const char *src,
		size_t rd)
{
	size_t out = 0;
	const char *start = src;

	if (sz == 0) return 0;
	while (out < sz - 1 && *src && (size_t)(src - start) < rd) {
		if (*src == '\\') {
			++src;
			if (*src && (size_t)(src - start) < rd) {
				*dest = *src;
				++src;
			} else {
				*dest = '\\';
			}
		} else {
			*dest = *src;
			++src;
		}
		++dest;
		++out;
	}
	*dest = '\0';
	return src - start;
}


/**
 * Cleanup the lookup for the triggers and gates.
 */
void tutorial_cleanup_trigger_gate_map(struct tutorial_dict_val_type ***m)
{
	int i = 0;

	if (!m) return;
	/* Free each row until the sentinel is reached. */
	while (m[i]) {
		assert(i < z_info->dungeon_hgt);
		mem_free(m[i]);
		++i;
	}
	mem_free(m);
}


/**
 * Set up the parser for tutorial.txt.
 */
struct parser *tutorial_init_parser(void)
{
	struct parser *p = parser_new();
	struct tutorial_parser_priv *priv = mem_zalloc(sizeof(*priv));
	struct tutorial_dict_key_type *tutorial_exit_key;
	struct tutorial_dict_val_type *tutorial_exit;

	priv->r = &tutorial_parsed_data;
	priv->r->d = tutorial_dict_create();
	/*
	 * Insert a placeholder that looks like a tutorial section for exiting
	 * the tutorial.
	 */
	tutorial_exit_key = mem_alloc(sizeof(*tutorial_exit_key));
	tutorial_exit_key->name = string_make("EXIT");
	tutorial_exit_key->comp = TUTORIAL_SECTION;
	tutorial_exit = mem_zalloc(sizeof(*tutorial_exit));
	if (!tutorial_dict_insert(priv->r->d, tutorial_exit_key,
			tutorial_exit)) {
		mem_free(tutorial_exit);
		mem_free(tutorial_exit_key);
		mem_free(priv);
		parser_destroy(p);
		return NULL;
	}
	tutorial_exit->key = tutorial_exit_key;
	priv->r->default_archetype = NULL;
	priv->r->default_section = NULL;
	priv->r->pval_to_note_table = NULL;
	priv->r->note_kind = lookup_kind(TV_NOTE,
		lookup_sval(TV_NOTE, "tutorial note"));
	if (!priv->r->note_kind) {
		mem_free(priv);
		parser_destroy(p);
		return NULL;
	}
	priv->r->note_table_n = 0;
	priv->r->note_table_a = 0;
	parser_setpriv(p, priv);

	/* These are the lines that introduce various blocks. */
	parser_reg(p, "archetype str name", parse_archetype_block_start);
	parser_reg(p, "note str name", parse_note_block_start);
	parser_reg(p, "trigger str name", parse_trigger_block_start);
	parser_reg(p, "section str name", parse_section_block_start);

	/* These are specific to the archetype block. */
	parser_reg(p, "race str name", parse_archetype_race);
	parser_reg(p, "house str name", parse_archetype_house);
	parser_reg(p, "sex str name", parse_archetype_sex);
	parser_reg(p, "name str name", parse_archetype_character_name);
	parser_reg(p, "history str history", parse_archetype_history);
	/*
	 * Parse as a string to avoid the ambiguity of whether an int has
	 * sufficient range for an int32_t.
	 */
	parser_reg(p, "experience str value", parse_archetype_experience);
	parser_reg(p, "stats str values", parse_archetype_stats);
	parser_reg(p, "skills str values", parse_archetype_skills);
	parser_reg(p, "abilities str values", parse_archetype_abilities);
	parser_reg(p, "object sym tval sym sval sym number sym equipped",
		parse_archetype_object);
	parser_reg(p, "complex-object sym tval sym sval sym number "
		"sym equipped sym ego sym properties str modifiers",
		parse_archetype_complex_object);
	parser_reg(p, "artifact sym name sym equipped",
		parse_archetype_artifact);

	/* These are specific to the trigger block. */
	parser_reg(p, "condition str expression", parse_trigger_condition);

	/* These are specific to the section block. */
	parser_reg(p, "rows int value", parse_section_rows);
	parser_reg(p, "columns int value", parse_section_columns);
	parser_reg(p, "area-flag int xul int yul int xlr int ylr str flags",
		parse_section_area_flag);
	parser_reg(p, "clear-area-flag int xul int yul int xlr int ylr "
		"str flags", parse_section_clear_area_flag);
	parser_reg(p, "start-note str name", parse_section_start_note);
	parser_reg(p, "place-note sym symbol str name",
		parse_section_note);
	parser_reg(p, "place-trigger sym symbol str name",
		parse_section_trigger);
	parser_reg(p, "gate sym symbol sym destination sym terrain ?str note",
		parse_section_gate);
	parser_reg(p, "forge sym symbol int uses str type",
		parse_section_forge);
	parser_reg(p, "place-object sym symbol sym tval sym sval str number",
		parse_section_object);
	parser_reg(p, "place-complex-object sym symbol sym tval sym sval "
		"sym number sym ego sym properties str modifiers",
		parse_section_complex_object);
	parser_reg(p, "place-artifact sym symbol str name",
		parse_section_artifact);
	parser_reg(p, "monster sym symbol sym race sym alertness ?str note",
		parse_section_monster);
	parser_reg(p, "trap sym symbol sym name str flags", parse_section_trap);
	parser_reg(p, "door sym symbol str flags", parse_section_door);
	parser_reg(p, "D str line", parse_section_line);

	/* These are shared by the note and trigger blocks. */
	parser_reg(p, "text str contents", parse_note_or_trigger_text);

	/* These are shared by archetype and section blocks. */
	parser_reg(p, "flags str flags", parse_archetype_or_section_flags);

	/* These are shared by the trigger and section blocks. */
	parser_reg(p, "death-note str name",
		parse_trigger_or_section_death_note);

	return p;
}


/**
 * Cleanup the parser for tutorial.txt; handle any post-processing of the
 * parsed results.
 */
errr tutorial_finish_parser(struct parser *p)
{
	struct tutorial_parser_priv *priv = (struct tutorial_parser_priv*)
		parser_priv(p);
	struct tutorial_dict_val_type *last_val = priv->curr_value;
	int n_lines = priv->section_lines_parsed;

	mem_free(priv);
	parser_destroy(p);

	if (last_val && last_val->key->comp == TUTORIAL_SECTION) {
		if (n_lines < last_val->v.section.rows) {
			return PARSE_ERROR_TOO_FEW_ENTRIES;
		}
		verify_section(last_val);
	}
	return 0;
}
