/**
 * \file obj-init.c
 * \brief Various game initialization routines
 *
 * Copyright (c) 1997 Ben Harrison
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
 *
 * This file is used to initialize various variables and arrays for objects
 * in the Angband game.
 *
 * Several of the arrays for Angband are built from data files in the
 * "lib/gamedata" directory.
 */


#include "angband.h"
#include "buildid.h"
#include "datafile.h"
#include "effects.h"
#include "init.h"
#include "mon-util.h"
#include "obj-ignore.h"
#include "obj-list.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-smith.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "option.h"
#include "player-abilities.h"
#include "player-util.h"
#include "project.h"

static const char *mon_race_flags[] =
{
	#define RF(a, b, c) #a,
	#include "list-mon-race-flags.h"
	#undef RF
	NULL
};

static const char *obj_flags[] = {
	"NONE",
	#define OF(a, b) #a,
	#include "list-object-flags.h"
	#undef OF
	NULL
};

static const char *obj_mods[] = {
	#define STAT(a) #a,
	#include "list-stats.h"
	#undef STAT
#define SKILL(a, b) #a,
	#include "list-skills.h"
	#undef SKILL
	#define OBJ_MOD(a) #a,
	#include "list-object-modifiers.h"
	#undef OBJ_MOD
	NULL
};

static const char *kind_flags[] = {
	#define KF(a, b) #a,
	#include "list-kind-flags.h"
	#undef KF
	NULL
};

static const char *element_names[] = {
	#define ELEM(a) #a,
	#include "list-elements.h"
	#undef ELEM
	NULL
};

static bool grab_element_flag(struct element_info *info, const char *flag_name)
{
	char prefix[20];
	char suffix[20];
	size_t i;

	if (2 != sscanf(flag_name, "%[^_]_%s", prefix, suffix)) {
		return false;
	}

	/* Ignore or hate */
	for (i = 0; i < ELEM_MAX; i++) {
		if (streq(suffix, element_names[i])) {
			if (streq(prefix, "IGNORE")) {
				info[i].flags |= EL_INFO_IGNORE;
				return true;
			}
			if (streq(prefix, "HATES")) {
				info[i].flags |= EL_INFO_HATES;
				return true;
			}
		}
	}
	return false;
}

static enum parser_error write_dummy_object_record(struct artifact *art, const char *name)
{
	struct object_kind *temp, *dummy;
	int i;
	char mod_name[100];

	/* Extend by 1 and realloc */
	z_info->k_max += 1;
	temp = mem_realloc(k_info, (z_info->k_max + 1) * sizeof(*temp));

	/* Copy if no errors */
	if (!temp) {
		return PARSE_ERROR_INTERNAL;
	}
	k_info = temp;
	/* Use the (second) last entry for the dummy */
	dummy = &k_info[z_info->k_max - 1];
	memset(dummy, 0, sizeof(*dummy));

	/* Copy the tval, base and level */
	dummy->tval = art->tval;
	dummy->base = &kb_info[dummy->tval];

	/* Make the name and index */
	strnfmt(mod_name, sizeof(mod_name), "& %s~", name);
	dummy->name = string_make(mod_name);
	dummy->kidx = z_info->k_max - 1;
	dummy->level = art->level;

	/* Increase the sval count for this tval, set the new one to the max */
	for (i = 0; i < TV_MAX; i++) {
		if (kb_info[i].tval == dummy->tval) {
			kb_info[i].num_svals++;
			dummy->sval = kb_info[i].num_svals;
			break;
		}
	}
	if (i == TV_MAX) return PARSE_ERROR_INTERNAL;

	/* Copy the sval to the artifact info */
	art->sval = dummy->sval;

	/* Give the object default colours (these should be overwritten) */
	dummy->d_char = '*';
	dummy->d_attr = COLOUR_RED;

	/* Inherit the flags and element information of the tval */
	of_copy(dummy->flags, kb_info[i].flags);
	kf_copy(dummy->kind_flags, kb_info[i].kind_flags);
	(void)memcpy(dummy->el_info, kb_info[i].el_info,
		sizeof(dummy->el_info[0]) * ELEM_MAX);

	/* Register this as an INSTA_ART object */
	kf_on(dummy->kind_flags, KF_INSTA_ART);

	return PARSE_ERROR_NONE;
}

/**
 * ------------------------------------------------------------------------
 * Initialize projections
 * ------------------------------------------------------------------------ */

static enum parser_error parse_projection_code(struct parser *p) {
	const char *code = parser_getstr(p, "code");
	struct projection *h = parser_priv(p);
	int index = h ? h->index + 1 : 0;
	struct projection *projection = mem_zalloc(sizeof *projection);

	parser_setpriv(p, projection);
	projection->next = h;
	projection->index = index;
	if ((index < ELEM_MAX) && !streq(code, element_names[index])) {
		return PARSE_ERROR_ELEMENT_NAME_MISMATCH;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_projection_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct projection *projection = parser_priv(p);

	if (!projection) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(projection->name);
	projection->name = string_make(name);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_projection_type(struct parser *p) {
	const char *type = parser_getstr(p, "type");
	struct projection *projection = parser_priv(p);

	if (!projection) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(projection->type);
	projection->type = string_make(type);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_projection_desc(struct parser *p) {
	const char *desc = parser_getstr(p, "desc");
	struct projection *projection = parser_priv(p);

	if (!projection) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(projection->desc);
	projection->desc = string_make(desc);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_projection_player_desc(struct parser *p) {
	const char *desc = parser_getstr(p, "desc");
	struct projection *projection = parser_priv(p);

	if (!projection) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(projection->player_desc);
	projection->player_desc = string_make(desc);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_projection_blind_desc(struct parser *p) {
	const char *desc = parser_getstr(p, "desc");
	struct projection *projection = parser_priv(p);

	if (!projection) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(projection->blind_desc);
	projection->blind_desc = string_make(desc);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_projection_message_type(struct parser *p)
{
	int msg_index;
	const char *type;
	struct projection *projection = parser_priv(p);

	if (!projection) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	type = parser_getsym(p, "type");
	msg_index = message_lookup_by_name(type);
	if (msg_index < 0) {
		return PARSE_ERROR_INVALID_MESSAGE;
	}
	projection->msgt = msg_index;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_projection_damaging(struct parser *p) {
	int damaging = parser_getuint(p, "answer");
	struct projection *projection = parser_priv(p);

	if (!projection) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	projection->damaging = (damaging == 1) ? true : false;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_projection_evade(struct parser *p) {
	int evade = parser_getuint(p, "answer");
	struct projection *projection = parser_priv(p);

	if (!projection) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	projection->evade = (evade == 1) ? true : false;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_projection_obvious(struct parser *p) {
	int obvious = parser_getuint(p, "answer");
	struct projection *projection = parser_priv(p);

	if (!projection) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	projection->obvious = (obvious == 1) ? true : false;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_projection_wake(struct parser *p) {
	int wake = parser_getuint(p, "answer");
	struct projection *projection = parser_priv(p);

	if (!projection) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	projection->wake = (wake == 1) ? true : false;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_projection_color(struct parser *p) {
	struct projection *projection = parser_priv(p);
	const char *color;

	if (!projection) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	color = parser_getsym(p, "color");
	if (strlen(color) > 1) {
		projection->color = color_text_to_attr(color);
	} else {
		projection->color = color_char_to_attr(color[0]);
	}
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_projection(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "code str code", parse_projection_code);
	parser_reg(p, "name str name", parse_projection_name);
	parser_reg(p, "type str type", parse_projection_type);
	parser_reg(p, "desc str desc", parse_projection_desc);
	parser_reg(p, "player-desc str desc", parse_projection_player_desc);
	parser_reg(p, "blind-desc str desc", parse_projection_blind_desc);
	parser_reg(p, "msgt sym type", parse_projection_message_type);
	parser_reg(p, "damaging uint answer", parse_projection_damaging);
	parser_reg(p, "evade uint answer", parse_projection_evade);
	parser_reg(p, "obvious uint answer", parse_projection_obvious);
	parser_reg(p, "wake uint answer", parse_projection_wake);
	parser_reg(p, "color sym color", parse_projection_color);
	return p;
}

static errr run_parse_projection(struct parser *p) {
	return parse_file_quit_not_found(p, "projection");
}

static errr finish_parse_projection(struct parser *p) {
	struct projection *projection, *next = NULL;
	int element_count = 0, count = 0;

	/* Count the entries */
	z_info->projection_max = 0;
	projection = parser_priv(p);
	while (projection) {
		z_info->projection_max++;
		if (projection->type && streq(projection->type, "element")) {
			element_count++;
		}
		projection = projection->next;
	}

	if (element_count + 1 < (int) N_ELEMENTS(element_names)) {
		quit_fmt("Too few elements in projection.txt!");
	} else if (element_count + 1 > (int) N_ELEMENTS(element_names)) {
		quit_fmt("Too many elements in projection.txt!");
	}

	/* Allocate the direct access list and copy the data to it */
	projections = mem_zalloc((z_info->projection_max) * sizeof(*projection));
	count = z_info->projection_max - 1;
	for (projection = parser_priv(p); projection; projection = next, count--) {
		memcpy(&projections[count], projection, sizeof(*projection));
		next = projection->next;
		mem_free(projection);
	}

	parser_destroy(p);
	return 0;
}

static void cleanup_projection(void)
{
	int idx;
	for (idx = 0; idx < z_info->projection_max; idx++) {
		string_free(projections[idx].name);
		string_free(projections[idx].type);
		string_free(projections[idx].desc);
		string_free(projections[idx].player_desc);
		string_free(projections[idx].blind_desc);
	}
	mem_free(projections);
}

struct file_parser projection_parser = {
	"projection",
	init_parse_projection,
	run_parse_projection,
	finish_parse_projection,
	cleanup_projection
};

/**
 * ------------------------------------------------------------------------
 * Initialize object bases
 * ------------------------------------------------------------------------ */

struct kb_parsedata {
	struct object_base defaults;
	struct object_base *kb;
};

static enum parser_error parse_object_base_defaults(struct parser *p) {
	const char *label;
	int value;
	struct kb_parsedata *d = parser_priv(p);

	assert(d);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");
	if (streq(label, "break-chance")) {
		d->defaults.break_perc = value;
	} else if (streq(label, "max-stack")) {
		d->defaults.max_stack = value;
	} else {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_base_name(struct parser *p) {
	struct object_base *kb;
	struct kb_parsedata *d = parser_priv(p);

	assert(d);
	kb = mem_alloc(sizeof *kb);
	memcpy(kb, &d->defaults, sizeof(*kb));
	kb->next = d->kb;
	d->kb = kb;

	kb->tval = tval_find_idx(parser_getsym(p, "tval"));
	if (kb->tval == -1) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	if (parser_hasval(p, "name")) {
		kb->name = string_make(parser_getstr(p, "name"));
	}
	kb->num_svals = 0;

	kb->smith_slays = mem_zalloc(z_info->slay_max * sizeof(*(kb->smith_slays)));
	kb->smith_brands = mem_zalloc(z_info->brand_max *
								  sizeof(*(kb->smith_brands)));

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_base_graphics(struct parser *p) {
	struct object_base *kb;
	const char *color;
	struct kb_parsedata *d = parser_priv(p);

	assert(d);
	kb = d->kb;

	if (!kb) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	color = parser_getsym(p, "color");
	if (strlen(color) > 1) {
		kb->attr = color_text_to_attr(color);
	} else {
		kb->attr = color_char_to_attr(color[0]);
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_base_break(struct parser *p) {
	struct object_base *kb;
	struct kb_parsedata *d = parser_priv(p);

	assert(d);
	kb = d->kb;
	if (!kb) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	kb->break_perc = parser_getint(p, "breakage");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_base_max_stack(struct parser *p) {
	struct kb_parsedata *d = parser_priv(p);
	struct object_base *kb;

	assert(d);
	kb = d->kb;
	if (!kb) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	kb->max_stack = parser_getint(p, "size");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_base_smith_attack(struct parser *p) {
	struct object_base *kb;
	struct kb_parsedata *d = parser_priv(p);

	assert(d);
	kb = d->kb;
	if (!kb) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	kb->smith_attack_valid = true;
	kb->smith_attack_artistry = parser_getint(p, "artistry");
	kb->smith_attack_artefact = parser_getint(p, "artefact");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_base_flags(struct parser *p) {
	struct object_base *kb;
	char *s, *t;
	struct kb_parsedata *d = parser_priv(p);

	assert(d);
	kb = d->kb;
	if (!kb) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	s = string_make(parser_getstr(p, "flags"));
	t = strtok(s, " |");
	while (t) {
		bool found = false;
		if (!grab_flag(kb->flags, OF_SIZE, obj_flags, t)) {
			found = true;
		}
		if (!grab_flag(kb->kind_flags, KF_SIZE, kind_flags, t)) {
			found = true;
		}
		if (grab_element_flag(kb->el_info, t)) {
			found = true;
		}
		if (!found) {
			break;
		}
		t = strtok(NULL, " |");
	}
	string_free(s);
	return t ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_object_base_smith_values(struct parser *p) {
	struct object_base *kb;
	char *s, *t;
	struct kb_parsedata *d = parser_priv(p);

	assert(d);
	kb = d->kb;
	if (!kb) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	s = string_make(parser_getstr(p, "values"));
	t = strtok(s, " |");

	while (t) {
		int value = 0;
		int index = 0;
		bool found = false;
		if (!grab_index_and_int(&value, &index, obj_mods, "", t)) {
			found = true;
			kb->smith_modifiers[index] = value;
		}
		if (!grab_index_and_int(&value, &index, element_names, "RES_", t)) {
			found = true;
			/* Both resistance and vulnerability allowed is stored as 2 */
			if (kb->smith_el_info[index].res_level == 0) {
				kb->smith_el_info[index].res_level = value;
			} else {
				kb->smith_el_info[index].res_level = 2;
			}
		}
		if (!found) {
			break;
		}
		t = strtok(NULL, " |");
	}

	string_free(s);
	return t ? PARSE_ERROR_INVALID_VALUE : PARSE_ERROR_NONE;
}

static enum parser_error parse_object_base_smith_flags(struct parser *p) {
	struct object_base *kb;
	char *s, *t;
	struct kb_parsedata *d = parser_priv(p);

	assert(d);
	kb = d->kb;
	if (!kb) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	s = string_make(parser_getstr(p, "flags"));
	t = strtok(s, " |");
	while (t) {
		bool found = false;
		if (!grab_flag(kb->smith_flags, OF_SIZE, obj_flags, t)) {
			found = true;
		}
		if (grab_element_flag(kb->smith_el_info, t)) {
			found = true;
		}
		if (!found) {
			break;
		}
		t = strtok(NULL, " |");
	}
	string_free(s);

	return t ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_object_base_smith_slay(struct parser *p) {
	struct object_base *kb;
	const char *s;
	int i;
	struct kb_parsedata *d = parser_priv(p);

	assert(d);
	kb = d->kb;
	if (!kb) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	s = parser_getstr(p, "code");

	for (i = 1; i < z_info->slay_max; i++) {
		if (streq(s, slays[i].code)) break;
	}
	if (i == z_info->slay_max) {
		return PARSE_ERROR_UNRECOGNISED_SLAY;
	}
	if (!kb->smith_slays) {
		kb->smith_slays = mem_zalloc(z_info->slay_max * sizeof(bool));
	}
	kb->smith_slays[i] = true;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_base_smith_brand(struct parser *p) {
	struct object_base *kb;
	const char *s;
	int i;
	struct kb_parsedata *d = parser_priv(p);

	assert(d);
	kb = d->kb;
	if (!kb) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	s = parser_getstr(p, "code");

	for (i = 1; i < z_info->brand_max; i++) {
		if (streq(s, brands[i].code)) break;
	}
	if (i == z_info->brand_max) {
		return PARSE_ERROR_UNRECOGNISED_BRAND;
	}
	if (!kb->smith_brands) {
		kb->smith_brands = mem_zalloc(z_info->brand_max * sizeof(bool));
	}
	kb->smith_brands[i] = true;
	return PARSE_ERROR_NONE;
}

struct parser *init_parse_object_base(void) {
	struct parser *p = parser_new();

	struct kb_parsedata *d = mem_zalloc(sizeof(*d));
	parser_setpriv(p, d);

	parser_reg(p, "default sym label int value", parse_object_base_defaults);
	parser_reg(p, "name sym tval ?str name", parse_object_base_name);
	parser_reg(p, "graphics sym color", parse_object_base_graphics);
	parser_reg(p, "break int breakage", parse_object_base_break);
	parser_reg(p, "max-stack int size", parse_object_base_max_stack);
	parser_reg(p, "smith-attack int artistry int artefact",
			   parse_object_base_smith_attack);
	parser_reg(p, "flags str flags", parse_object_base_flags);
	parser_reg(p, "smith-values str values", parse_object_base_smith_values);
	parser_reg(p, "smith-flags str flags", parse_object_base_smith_flags);
	parser_reg(p, "slay str code", parse_object_base_smith_slay);
	parser_reg(p, "brand str code", parse_object_base_smith_brand);
	return p;
}

static errr run_parse_object_base(struct parser *p) {
	return parse_file_quit_not_found(p, "object_base");
}

static errr finish_parse_object_base(struct parser *p) {
	struct object_base *kb;
	struct object_base *next = NULL;
	struct kb_parsedata *d = parser_priv(p);

	assert(d);

	kb_info = mem_zalloc(TV_MAX * sizeof(*kb_info));

	for (kb = d->kb; kb; kb = next) {
		if (kb->tval < TV_MAX && kb->tval >= 0) {
			memcpy(&kb_info[kb->tval], kb, sizeof(*kb));
		} else {
			string_free(kb->name);
		}
		next = kb->next;
		mem_free(kb);
	}

	mem_free(d);
	parser_destroy(p);
	return 0;
}

static void cleanup_object_base(void)
{
	int idx;
	for (idx = 0; idx < TV_MAX; idx++) {
		string_free(kb_info[idx].name);
		mem_free(kb_info[idx].smith_slays);
		mem_free(kb_info[idx].smith_brands);
	}
	mem_free(kb_info);
}

struct file_parser object_base_parser = {
	"object_base",
	init_parse_object_base,
	run_parse_object_base,
	finish_parse_object_base,
	cleanup_object_base
};



/**
 * ------------------------------------------------------------------------
 * Initialize object slays
 * ------------------------------------------------------------------------ */

static enum parser_error parse_slay_code(struct parser *p) {
	const char *code = parser_getstr(p, "code");
	struct slay *h = parser_priv(p);
	struct slay *slay = mem_zalloc(sizeof *slay);

	slay->next = h;
	parser_setpriv(p, slay);
	slay->code = string_make(code);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_slay_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct slay *slay = parser_priv(p);

	if (!slay) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(slay->name);
	slay->name = string_make(name);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_slay_race_flag(struct parser *p) {
	int flag;
	struct slay *slay = parser_priv(p);

	if (!slay) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	flag = lookup_flag(mon_race_flags, parser_getsym(p, "flag"));
	if (flag == FLAG_END) {
		return PARSE_ERROR_INVALID_FLAG;
	}
	slay->race_flag = flag;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_slay_dice(struct parser *p) {
	struct slay *slay = parser_priv(p);

	if (!slay) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	slay->dice = parser_getuint(p, "dice");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_slay_smith_difficulty(struct parser *p) {
	struct slay *slay = parser_priv(p);

	if (!slay) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	slay->smith_difficulty = parser_getuint(p, "diff");
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_slay(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "code str code", parse_slay_code);
	parser_reg(p, "name str name", parse_slay_name);
	parser_reg(p, "race-flag sym flag", parse_slay_race_flag);
	parser_reg(p, "dice uint dice", parse_slay_dice);
	parser_reg(p, "smith-difficulty uint diff", parse_slay_smith_difficulty);
	return p;
}

static errr run_parse_slay(struct parser *p) {
	return parse_file_quit_not_found(p, "slay");
}

static errr finish_parse_slay(struct parser *p) {
	struct slay *slay, *next = NULL;
	int count = 1;
	errr result = PARSE_ERROR_NONE;

	/* Count the entries */
	z_info->slay_max = 0;
	slay = parser_priv(p);
	while (slay) {
		if (z_info->slay_max >= 254) {
			result = PARSE_ERROR_TOO_MANY_ENTRIES;
			break;
		}
		z_info->slay_max++;
		slay = slay->next;
	}

	/* Allocate the direct access list and copy the data to it */
	slays = mem_zalloc((z_info->slay_max + 1) * sizeof(*slay));
	for (slay = parser_priv(p); slay; slay = next, count++) {
		next = slay->next;
		if (count <= z_info->slay_max) {
			memcpy(&slays[count], slay, sizeof(*slay));
			slays[count].next = NULL;
		}
		mem_free(slay);
	}
	z_info->slay_max += 1;

	parser_destroy(p);
	return result;
}

static void cleanup_slay(void)
{
	int idx;
	for (idx = 0; idx < z_info->slay_max; idx++) {
		string_free(slays[idx].code);
		string_free(slays[idx].name);
	}
	mem_free(slays);
}

struct file_parser slay_parser = {
	"slay",
	init_parse_slay,
	run_parse_slay,
	finish_parse_slay,
	cleanup_slay
};

/**
 * ------------------------------------------------------------------------
 * Initialize object brands
 * ------------------------------------------------------------------------ */

static enum parser_error parse_brand_code(struct parser *p) {
	const char *code = parser_getstr(p, "code");
	struct brand *h = parser_priv(p);
	struct brand *brand = mem_zalloc(sizeof *brand);

	brand->next = h;
	parser_setpriv(p, brand);
	brand->code = string_make(code);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_brand_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct brand *brand = parser_priv(p);

	if (!brand) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(brand->name);
	brand->name = string_make(name);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_brand_desc(struct parser *p) {
	const char *desc = parser_getstr(p, "desc");
	struct brand *brand = parser_priv(p);

	if (!brand) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(brand->desc);
	brand->desc = string_make(desc);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_brand_dice(struct parser *p) {
	struct brand *brand = parser_priv(p);

	if (!brand) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	brand->dice = parser_getuint(p, "dice");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_brand_vuln_dice(struct parser *p) {
	struct brand *brand = parser_priv(p);

	if (!brand) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	brand->vuln_dice = parser_getuint(p, "dice");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_brand_smith_difficulty(struct parser *p) {
	struct brand *brand = parser_priv(p);

	if (!brand) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	brand->smith_difficulty = parser_getuint(p, "diff");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_brand_resist_flag(struct parser *p) {
	int flag;
	struct brand *brand = parser_priv(p);

	if (!brand) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	flag = lookup_flag(mon_race_flags, parser_getsym(p, "flag"));
	if (flag == FLAG_END) {
		return PARSE_ERROR_INVALID_FLAG;
	}
	brand->resist_flag = flag;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_brand_vuln_flag(struct parser *p) {
	int flag;
	struct brand *brand = parser_priv(p);

	if (!brand) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	flag = lookup_flag(mon_race_flags, parser_getsym(p, "flag"));
	if (flag == FLAG_END) {
		return PARSE_ERROR_INVALID_FLAG;
	}
	brand->vuln_flag = flag;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_brand(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "code str code", parse_brand_code);
	parser_reg(p, "name str name", parse_brand_name);
	parser_reg(p, "desc str desc", parse_brand_desc);
	parser_reg(p, "dice uint dice", parse_brand_dice);
	parser_reg(p, "vuln-dice uint dice", parse_brand_vuln_dice);
	parser_reg(p, "smith-difficulty uint diff", parse_brand_smith_difficulty);
	parser_reg(p, "resist-flag sym flag", parse_brand_resist_flag);
	parser_reg(p, "vuln-flag sym flag", parse_brand_vuln_flag);
	return p;
}

static errr run_parse_brand(struct parser *p) {
	return parse_file_quit_not_found(p, "brand");
}

static errr finish_parse_brand(struct parser *p) {
	struct brand *brand, *next = NULL;
	int count = 1;
	errr result = PARSE_ERROR_NONE;

	/* Count the entries */
	z_info->brand_max = 0;
	brand = parser_priv(p);
	while (brand) {
		if (z_info->brand_max >= 254) {
			result = PARSE_ERROR_TOO_MANY_ENTRIES;
			break;
		}
		z_info->brand_max++;
		brand = brand->next;
	}

	/* Allocate the direct access list and copy the data to it */
	brands = mem_zalloc((z_info->brand_max + 1) * sizeof(*brand));
	for (brand = parser_priv(p); brand; brand = next, count++) {
		next = brand->next;
		if (count <= z_info->brand_max) {
			memcpy(&brands[count], brand, sizeof(*brand));
			brands[count].next = NULL;
		}
		mem_free(brand);
	}
	z_info->brand_max += 1;

	parser_destroy(p);
	return result;
}

static void cleanup_brand(void)
{
	int idx;
	for (idx = 0; idx < z_info->brand_max; idx++) {
		string_free(brands[idx].code);
		string_free(brands[idx].name);
		string_free(brands[idx].desc);
	}
	mem_free(brands);
}

struct file_parser brand_parser = {
	"brand",
	init_parse_brand,
	run_parse_brand,
	finish_parse_brand,
	cleanup_brand
};


/**
 * ------------------------------------------------------------------------
 * Initialize objects
 * ------------------------------------------------------------------------ */

/* Generic object kinds */
struct object_kind *unknown_item_kind;
struct object_kind *pile_kind;

static enum parser_error parse_object_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct object_kind *h = parser_priv(p);

	struct object_kind *k = mem_zalloc(sizeof *k);
	k->next = h;
	parser_setpriv(p, k);
	k->name = string_make(name);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_graphics(struct parser *p) {
	wchar_t glyph = parser_getchar(p, "glyph");
	const char *color = parser_getsym(p, "color");
	struct object_kind *k = parser_priv(p);

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k->d_char = glyph;
	if (strlen(color) > 1) {
		k->d_attr = color_text_to_attr(color);
	} else {
		k->d_attr = color_char_to_attr(color[0]);
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_type(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	int tval;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	tval = tval_find_idx(parser_getsym(p, "tval"));
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	k->tval = tval;
	k->base = &kb_info[k->tval];
	k->base->num_svals++;
	k->sval = k->base->num_svals;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_pval(struct parser *p) {
	struct object_kind *k = parser_priv(p);

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k->pval = parser_getint(p, "pval");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_level(struct parser *p) {
	struct object_kind *k = parser_priv(p);

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k->level = parser_getint(p, "level");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_weight(struct parser *p) {
	struct object_kind *k = parser_priv(p);

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k->weight = parser_getint(p, "weight");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_cost(struct parser *p) {
	struct object_kind *k = parser_priv(p);

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k->cost = parser_getint(p, "cost");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_attack(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	struct random hd;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k->att = parser_getint(p, "att");
	hd = parser_getrand(p, "hd");
	k->dd = hd.dice;
	k->ds = hd.sides;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_defence(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	struct random hd;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k->evn = parser_getint(p, "evn");
	hd = parser_getrand(p, "hd");
	k->pd = hd.dice;
	k->ps = hd.sides;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_alloc(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	struct allocation *a;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the last valid allocation, then allocate a new one */
	a = k->alloc;
	if (!a) {
		k->alloc = mem_zalloc(sizeof(struct allocation));
		a = k->alloc;
	} else {
		while (a->next)
			a = a->next;
		a->next = mem_zalloc(sizeof(struct allocation));
		a = a->next;
	}

	/* Now read the data */
	a->locale = parser_getuint(p, "locale");
	a->chance = parser_getuint(p, "chance");
	return PARSE_ERROR_NONE;
}


static enum parser_error parse_object_flags(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	char *s, *t;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	s = string_make(parser_getstr(p, "flags"));
	t = strtok(s, " |");
	while (t) {
		bool found = false;
		if (!grab_flag(k->flags, OF_SIZE, obj_flags, t)) {
			found = true;
		}
		if (!grab_flag(k->kind_flags, KF_SIZE, kind_flags, t)) {
			found = true;
		}
		if (grab_element_flag(k->el_info, t)) {
			found = true;
		}
		if (!found) {
			break;
		}
		t = strtok(NULL, " |");
	}
	string_free(s);
	return t ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_object_charges(struct parser *p) {
	struct object_kind *k = parser_priv(p);

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k->charge = parser_getrand(p, "charges");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_effect(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	struct effect *effect, *new_effect;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the next vacant effect and set it to the new one  */
	new_effect = mem_zalloc(sizeof(*new_effect));
	if (k->effect) {
		effect = k->effect;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		k->effect = new_effect;
	}
	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_object_dice(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	dice_t *dice;
	struct effect *effect;
	const char *string;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = k->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	dice = dice_new();
	if (dice == NULL) {
		return PARSE_ERROR_INVALID_DICE;
	}
	string = parser_getstr(p, "dice");
	if (dice_parse_string(dice, string)) {
		dice_free(effect->dice);
		effect->dice = dice;
	} else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_expr(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	struct effect *effect;
	expression_t *expression;
	expression_base_value_f function;
	const char *name;
	const char *base;
	const char *expr;
	enum parser_error result;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = k->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL) {
		return PARSE_ERROR_NONE;
	}
	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();
	if (expression == NULL) {
		return PARSE_ERROR_INVALID_EXPRESSION;
	}
	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0) {
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	} else if (dice_bind_expression(effect->dice, name, expression) < 0) {
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	} else {
		result = PARSE_ERROR_NONE;
	}
	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);
	return result;
}

static enum parser_error parse_object_thrown_effect(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	struct effect *effect, *new_effect;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the next vacant effect and set it to the new one  */
	new_effect = mem_zalloc(sizeof(*new_effect));
	if (k->thrown_effect) {
		effect = k->thrown_effect;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		k->thrown_effect = new_effect;
	}
	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_object_thrown_effect_dice(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	dice_t *dice;
	struct effect *effect;
	const char *string;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = k->thrown_effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	dice = dice_new();
	if (dice == NULL) {
		return PARSE_ERROR_INVALID_DICE;
	}
	string = parser_getstr(p, "dice");
	if (dice_parse_string(dice, string)) {
		dice_free(effect->dice);
		effect->dice = dice;
	} else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_thrown_effect_expr(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	struct effect *effect;
	expression_t *expression;
	expression_base_value_f function;
	const char *name;
	const char *base;
	const char *expr;
	enum parser_error result;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = k->thrown_effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL) {
		return PARSE_ERROR_NONE;
	}
	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();
	if (expression == NULL) {
		return PARSE_ERROR_INVALID_EXPRESSION;
	}
	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0) {
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	} else if (dice_bind_expression(effect->dice, name, expression) < 0) {
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	} else {
		result = PARSE_ERROR_NONE;
	}
	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);
	return result;
}

static enum parser_error parse_object_msg(struct parser *p) {
	struct object_kind *k = parser_priv(p);

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k->effect_msg = string_append(k->effect_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_desc(struct parser *p) {
	struct object_kind *k = parser_priv(p);

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k->text = string_append(k->text, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_values(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	char *s, *t;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	s = string_make(parser_getstr(p, "values"));
	t = strtok(s, " |");

	while (t) {
		int value = 0;
		int index = 0;
		bool found = false;
		if (!grab_rand_value(k->modifiers, obj_mods, t)) {
			found = true;
		}
		if (!grab_index_and_int(&value, &index, element_names, "RES_", t)) {
			found = true;
			k->el_info[index].res_level = value;
		}
		if (!found) {
			break;
		}
		t = strtok(NULL, " |");
	}

	string_free(s);
	return t ? PARSE_ERROR_INVALID_VALUE : PARSE_ERROR_NONE;
}

static enum parser_error parse_object_slay(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	const char *s = parser_getstr(p, "code");
	int i;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	for (i = 1; i < z_info->slay_max; i++) {
		if (streq(s, slays[i].code)) break;
	}
	if (i == z_info->slay_max) {
		return PARSE_ERROR_UNRECOGNISED_SLAY;
	}
	if (!k->slays) {
		k->slays = mem_zalloc(z_info->slay_max * sizeof(bool));
	}
	k->slays[i] = true;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_brand(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	const char *s = parser_getstr(p, "code");
	int i;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	for (i = 1; i < z_info->brand_max; i++) {
		if (streq(s, brands[i].code)) break;
	}
	if (i == z_info->brand_max) {
		return PARSE_ERROR_UNRECOGNISED_BRAND;
	}
	if (!k->brands) {
		k->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
	}
	k->brands[i] = true;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_special(struct parser *p) {
	struct object_kind *k = parser_priv(p);
	const char *dice_string = parser_getsym(p, "value");
	dice_t *dice;

	if (!k) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	dice = dice_new();
	if (!dice_parse_string(dice, dice_string)) {
		dice_free(dice);
		return PARSE_ERROR_NOT_RANDOM;
	}
	dice_random_value(dice, &k->special1);
	if (parser_hasval(p, "min")) {
		k->special2 = parser_getint(p, "min");
	}
	dice_free(dice);
	return PARSE_ERROR_NONE;
}

struct parser *init_parse_object(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_object_name);
	parser_reg(p, "type sym tval", parse_object_type);
	parser_reg(p, "pval int pval", parse_object_pval);
	parser_reg(p, "graphics char glyph sym color", parse_object_graphics);
	parser_reg(p, "depth int level", parse_object_level);
	parser_reg(p, "weight int weight", parse_object_weight);
	parser_reg(p, "cost int cost", parse_object_cost);
	parser_reg(p, "alloc uint locale uint chance", parse_object_alloc);
	parser_reg(p, "attack int att rand hd", parse_object_attack);
	parser_reg(p, "defence int evn rand hd", parse_object_defence);
	parser_reg(p, "flags str flags", parse_object_flags);
	parser_reg(p, "charges rand charges", parse_object_charges);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_object_effect);
	parser_reg(p, "dice str dice", parse_object_dice);
	parser_reg(p, "expr sym name sym base str expr", parse_object_expr);
	parser_reg(p, "thrown-effect sym eff ?sym type ?int radius ?int other",
			   parse_object_thrown_effect);
	parser_reg(p, "thrown-dice str dice", parse_object_thrown_effect_dice);
	parser_reg(p, "thrown-expr sym name sym base str expr",
			   parse_object_thrown_effect_expr);
	parser_reg(p, "msg str text", parse_object_msg);
	parser_reg(p, "values str values", parse_object_values);
	parser_reg(p, "desc str text", parse_object_desc);
	parser_reg(p, "slay str code", parse_object_slay);
	parser_reg(p, "brand str code", parse_object_brand);
	parser_reg(p, "special sym value ?int min", parse_object_special);
	return p;
}

static errr run_parse_object(struct parser *p) {
	return parse_file_quit_not_found(p, "object");
}

static errr finish_parse_object(struct parser *p) {
	struct object_kind *k, *next = NULL;
	int kidx;

	/* scan the list for the max id and max number of allocations */
	z_info->k_max = 0;
	z_info->obj_alloc_max = 0;
	k = parser_priv(p);
	while (k) {
		int max_alloc = 0;
		struct allocation *a = k->alloc;
		z_info->k_max++;
		while (a) {
			a = a->next;
			max_alloc++;
		}
		if (max_alloc > z_info->obj_alloc_max)
			z_info->obj_alloc_max = max_alloc;
		k = k->next;
	}

	/* allocate the direct access list and copy the data to it */
	k_info = mem_zalloc((z_info->k_max + 1) * sizeof(*k));
	kidx = z_info->k_max - 1;
	for (k = parser_priv(p); k; k = next, kidx--) {
		struct allocation *a_new;
		assert(kidx >= 0);

		memcpy(&k_info[kidx], k, sizeof(*k));
		k_info[kidx].kidx = kidx;

		/* Add base kind flags to kind kind flags */
		kf_union(k_info[kidx].kind_flags, kb_info[k->tval].kind_flags);

		next = k->next;
		if (kidx < z_info->k_max - 1) {
			k_info[kidx].next = &k_info[kidx + 1];
		} else {
			k_info[kidx].next = NULL;
		}

		/* Allocation */
		a_new = mem_zalloc(z_info->obj_alloc_max * sizeof(*a_new));
		if (k->alloc) {
			struct allocation *a_temp, *a_old = k->alloc;
			int i;

			/* Allocate space and copy */
			for (i = 0; i < z_info->obj_alloc_max; i++) {
				memcpy(&a_new[i], a_old, sizeof(*a_old));
				a_old = a_old->next;
				if (!a_old) break;
			}

			/* Make next point correctly */
			for (i = 0; i < z_info->obj_alloc_max; i++)
				if (a_new[i].next)
					a_new[i].next = &a_new[i + 1];

			/* Tidy up */
			a_old = k->alloc;
			a_temp = a_old;
			while (a_temp) {
				a_temp = a_old->next;
				mem_free(a_old);
				a_old = a_temp;
			}
		}
		k_info[kidx].alloc = a_new;

		mem_free(k);
	}
	z_info->k_max += 1;
	z_info->ordinary_kind_max = z_info->k_max;

	parser_destroy(p);
	return 0;
}

static void cleanup_object(void)
{
	int idx;
	for (idx = 0; idx < z_info->k_max; idx++) {
		struct object_kind *kind = &k_info[idx];
		string_free(kind->name);
		string_free(kind->text);
		string_free(kind->effect_msg);
		mem_free(kind->brands);
		mem_free(kind->slays);
		release_ability_list(kind->abilities);
		free_effect(kind->effect);
		free_effect(kind->thrown_effect);
		mem_free(kind->alloc);
	}
	mem_free(k_info);
}

struct file_parser object_parser = {
	"object",
	init_parse_object,
	run_parse_object,
	finish_parse_object,
	cleanup_object
};

/**
 * ------------------------------------------------------------------------
 * Initialize drop types
 * ------------------------------------------------------------------------ */

static enum parser_error parse_drop_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct drop *h = parser_priv(p);

	struct drop *d = mem_zalloc(sizeof *d);
	d->next = h;
	parser_setpriv(p, d);
	d->name = string_make(name);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_drop_chest(struct parser *p) {
	struct drop *d = parser_priv(p);
	int chest;

	if (!d) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	chest = parser_getuint(p, "chest");
	d->chest = (chest == 1) ? true : false;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_drop_base(struct parser *p) {
	struct poss_item *poss;
	int i;
	int tval = tval_find_idx(parser_getsym(p, "tval"));
	bool found_one_kind = false;

	struct drop *d = parser_priv(p);
	if (!d) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	/* Find all the right object kinds */
	for (i = 0; i < z_info->k_max; i++) {
		if (k_info[i].tval != tval) continue;
		poss = mem_zalloc(sizeof(struct poss_item));
		poss->kidx = i;
		poss->next = d->poss;
		d->poss = poss;
		found_one_kind = true;
	}

	return (!found_one_kind) ?
		PARSE_ERROR_NO_KIND_FOR_DROP_TYPE : PARSE_ERROR_NONE;
}

static enum parser_error parse_drop_not_base(struct parser *p) {
	struct poss_item *imposs;
	int i;
	int tval = tval_find_idx(parser_getsym(p, "tval"));
	bool found_one_kind = false;
	struct drop *d = parser_priv(p);

	if (!d) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	/* Find all the right object kinds */
	for (i = 0; i < z_info->k_max; i++) {
		if (k_info[i].tval != tval) continue;
		imposs = mem_zalloc(sizeof(struct poss_item));
		imposs->kidx = i;
		imposs->next = d->imposs;
		d->imposs = imposs;
		found_one_kind = true;
	}
	return (!found_one_kind) ?
		PARSE_ERROR_NO_KIND_FOR_DROP_TYPE : PARSE_ERROR_NONE;
}

static enum parser_error parse_drop_item(struct parser *p) {
	struct poss_item *poss;
	int tval = tval_find_idx(parser_getsym(p, "tval"));
	int sval = lookup_sval(tval, parser_getsym(p, "sval"));
	int kidx;
	struct drop *d = parser_priv(p);

	if (!d) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	if (sval < 0) {
		return PARSE_ERROR_UNRECOGNISED_SVAL;
	}
	kidx = lookup_kind(tval, sval)->kidx;
	if (kidx < 0) {
		return PARSE_ERROR_INVALID_ITEM_NUMBER;
	}
	poss = mem_zalloc(sizeof(struct poss_item));
	poss->kidx = kidx;
	poss->next = d->poss;
	d->poss = poss;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_drop(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_drop_name);
	parser_reg(p, "chest uint chest", parse_drop_chest);
	parser_reg(p, "base sym tval", parse_drop_base);
	parser_reg(p, "not-base sym tval", parse_drop_not_base);
	parser_reg(p, "item sym tval sym sval", parse_drop_item);
	return p;
}

static errr run_parse_drop(struct parser *p) {
	return parse_file_quit_not_found(p, "drop");
}

static errr finish_parse_drop(struct parser *p) {
	struct drop *d, *n;
	int idx;

	/* Scan the list for the max id */
	z_info->drop_max = 0;
	d = parser_priv(p);
	while (d) {
		z_info->drop_max++;
		d = d->next;
	}

	/* Allocate the direct access list and copy the data to it */
	drops = mem_zalloc((z_info->drop_max + 1) * sizeof(*d));
	idx = z_info->drop_max - 1;
	for (d = parser_priv(p); d; d = n, idx--) {
		assert(idx >= 0);

		memcpy(&drops[idx], d, sizeof(*d));
		drops[idx].idx = idx;
		n = d->next;
		if (idx < z_info->drop_max - 1) {
			drops[idx].next = &drops[idx + 1];
		} else {
			drops[idx].next = NULL;
		}
		mem_free(d);
	}
	z_info->drop_max += 1;

	parser_destroy(p);
	return 0;
}

static void cleanup_drop(void)
{
	int idx;
	for (idx = 0; idx < z_info->drop_max; idx++) {
		struct drop *drop = &drops[idx];
		struct poss_item *poss;

		string_free(drop->name);
		poss = drop->poss;
		while (poss) {
			struct poss_item *next = poss->next;
			mem_free(poss);
			poss = next;
		}
		poss = drop->imposs;
		while (poss) {
			struct poss_item *next = poss->next;
			mem_free(poss);
			poss = next;
		}
	}
	mem_free(drops);
}

struct file_parser drop_parser = {
	"drop",
	init_parse_drop,
	run_parse_drop,
	finish_parse_drop,
	cleanup_drop
};

/**
 * ------------------------------------------------------------------------
 * Initialize ego items
 * ------------------------------------------------------------------------ */

static enum parser_error parse_ego_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct ego_item *h = parser_priv(p);

	struct ego_item *e = mem_zalloc(sizeof *e);
	e->next = h;
	parser_setpriv(p, e);
	e->name = string_make(name);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_alloc(struct parser *p) {
	struct ego_item *e = parser_priv(p);
	const char *tmp = parser_getstr(p, "minmax");
	int amin, amax;

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	e->rarity = parser_getint(p, "common");
	if (sscanf(tmp, "%d to %d", &amin, &amax) != 2) {
		return PARSE_ERROR_INVALID_ALLOCATION;
	}
	if (amin > 255 || amax > 255 || amin < 0 || amax < 0) {
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	e->level = amin;
	e->alloc_max = amax;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_cost(struct parser *p) {
	struct ego_item *e = parser_priv(p);

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	e->cost = parser_getint(p, "cost");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_max_attack(struct parser *p) {
	struct ego_item *e = parser_priv(p);

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	e->att = parser_getuint(p, "att");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_dam_dice(struct parser *p) {
	struct ego_item *e = parser_priv(p);

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	e->dd = parser_getuint(p, "dice");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_dam_sides(struct parser *p) {
	struct ego_item *e = parser_priv(p);

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	e->ds = parser_getuint(p, "sides");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_max_evasion(struct parser *p) {
	struct ego_item *e = parser_priv(p);

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	e->evn = parser_getuint(p, "evn");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_prot_dice(struct parser *p) {
	struct ego_item *e = parser_priv(p);

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	e->pd = parser_getuint(p, "dice");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_prot_sides(struct parser *p) {
	struct ego_item *e = parser_priv(p);

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	e->ps = parser_getuint(p, "sides");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_max_pval(struct parser *p) {
	struct ego_item *e = parser_priv(p);

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	e->pval = parser_getuint(p, "pval");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_type(struct parser *p) {
	struct poss_item *poss;
	int i;
	int tval = tval_find_idx(parser_getsym(p, "tval"));
	bool found_one_kind = false;
	struct ego_item *e = parser_priv(p);

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	/* Find all the right object kinds */
	for (i = 0; i < z_info->k_max; i++) {
		if (k_info[i].tval != tval) continue;
		poss = mem_zalloc(sizeof(struct poss_item));
		poss->kidx = i;
		poss->next = e->poss_items;
		e->poss_items = poss;
		found_one_kind = true;
	}

	if (!found_one_kind) {
		return PARSE_ERROR_NO_KIND_FOR_EGO_TYPE;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_item(struct parser *p) {
	struct poss_item *poss;
	int tval = tval_find_idx(parser_getsym(p, "tval"));
	int sval = lookup_sval(tval, parser_getsym(p, "sval"));
	struct ego_item *e = parser_priv(p);

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	if (sval < 0) {
		return PARSE_ERROR_UNRECOGNISED_SVAL;
	}

	poss = mem_zalloc(sizeof(struct poss_item));
	poss->kidx = lookup_kind(tval, sval)->kidx;
	poss->next = e->poss_items;
	e->poss_items = poss;
	return (poss->kidx <= 0) ?
		PARSE_ERROR_INVALID_ITEM_NUMBER : PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_flags(struct parser *p) {
	struct ego_item *e = parser_priv(p);
	char *flags;
	char *t;

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (!parser_hasval(p, "flags")) {
		return PARSE_ERROR_NONE;
	}
	flags = string_make(parser_getstr(p, "flags"));
	t = strtok(flags, " |");
	while (t) {
		bool found = false;
		if (!grab_flag(e->flags, OF_SIZE, obj_flags, t)) {
			found = true;
		}
		if (!grab_flag(e->kind_flags, KF_SIZE, kind_flags, t)) {
			found = true;
		}
		if (grab_element_flag(e->el_info, t)) {
			found = true;
		}
		if (!found) {
			break;
		}
		t = strtok(NULL, " |");
	}
	string_free(flags);
	return t ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_values(struct parser *p) {
	struct ego_item *e = parser_priv(p);
	char *s, *t;

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (!parser_hasval(p, "values")) {
		return PARSE_ERROR_MISSING_FIELD;
	}
	s = string_make(parser_getstr(p, "values"));
	t = strtok(s, " |");

	while (t) {
		bool found = false;
		int value = 0;
		int index = 0;
		if (!grab_int_value(e->modifiers, obj_mods, t)) {
			found = true;
		}
		if (!grab_index_and_int(&value, &index, element_names, "RES_", t)) {
			found = true;
			e->el_info[index].res_level = value;
		}
		if (!found) {
			break;
		}
		t = strtok(NULL, " |");
	}

	string_free(s);
	return t ? PARSE_ERROR_INVALID_VALUE : PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_slay(struct parser *p) {
	struct ego_item *e = parser_priv(p);
	const char *s = parser_getstr(p, "code");
	int i;

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	for (i = 1; i < z_info->slay_max; i++) {
		if (streq(s, slays[i].code)) break;
	}
	if (i == z_info->slay_max) {
		return PARSE_ERROR_UNRECOGNISED_SLAY;
	}
	if (!e->slays) {
		e->slays = mem_zalloc(z_info->slay_max * sizeof(bool));
	}
	e->slays[i] = true;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_brand(struct parser *p) {
	struct ego_item *e = parser_priv(p);
	const char *s = parser_getstr(p, "code");
	int i;

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	for (i = 1; i < z_info->brand_max; i++) {
		if (streq(s, brands[i].code)) break;
	}
	if (i == z_info->brand_max)
		return PARSE_ERROR_UNRECOGNISED_BRAND;

	if (!e->brands) {
		e->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
	}
	e->brands[i] = true;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_ego_ability(struct parser *p) {
	struct ego_item *e = parser_priv(p);
	int skill = lookup_skill(parser_getsym(p, "skill"));
	struct ability *a, *n;

	if (!e) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (skill < 0) {
		return PARSE_ERROR_INVALID_SKILL;
	}
	a = lookup_ability(skill, parser_getsym(p, "ability"));
	if (!a) {
		return PARSE_ERROR_INVALID_ABILITY;
	}
	n = mem_zalloc(sizeof(*n));
	memcpy(n, a, sizeof(*n));
	n->next = e->abilities;
	e->abilities = n;

	return PARSE_ERROR_NONE;
}

struct parser *init_parse_ego(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_ego_name);
	parser_reg(p, "alloc int common str minmax", parse_ego_alloc);
	parser_reg(p, "cost int cost", parse_ego_cost);
	parser_reg(p, "max-attack uint att", parse_ego_max_attack);
	parser_reg(p, "dam-dice uint dice", parse_ego_dam_dice);
	parser_reg(p, "dam-sides uint sides", parse_ego_dam_sides);
	parser_reg(p, "max-evasion uint evn", parse_ego_max_evasion);
	parser_reg(p, "prot-dice uint dice", parse_ego_prot_dice);
	parser_reg(p, "prot-sides uint sides", parse_ego_prot_sides);
	parser_reg(p, "max-pval uint pval", parse_ego_max_pval);
	parser_reg(p, "type sym tval", parse_ego_type);
	parser_reg(p, "item sym tval sym sval", parse_ego_item);
	parser_reg(p, "flags ?str flags", parse_ego_flags);
	parser_reg(p, "values str values", parse_ego_values);
	parser_reg(p, "slay str code", parse_ego_slay);
	parser_reg(p, "brand str code", parse_ego_brand);
	parser_reg(p, "ability sym skill sym ability", parse_ego_ability);
	return p;
}

static errr run_parse_ego(struct parser *p) {
	return parse_file_quit_not_found(p, "special");
}

static errr finish_parse_ego(struct parser *p) {
	struct ego_item *e, *n;
	int eidx;

	/* Scan the list for the max id */
	z_info->e_max = 0;
	e = parser_priv(p);
	while (e) {
		z_info->e_max++;
		e = e->next;
	}

	/* Allocate the direct access list and copy the data to it */
	e_info = mem_zalloc((z_info->e_max + 1) * sizeof(*e));
	eidx = z_info->e_max - 1;
	for (e = parser_priv(p); e; e = n, eidx--) {
		assert(eidx >= 0);

		memcpy(&e_info[eidx], e, sizeof(*e));
		e_info[eidx].eidx = eidx;
		n = e->next;
		if (eidx < z_info->e_max - 1) {
			e_info[eidx].next = &e_info[eidx + 1];
		} else {
			e_info[eidx].next = NULL;
		}
		mem_free(e);
	}
	z_info->e_max += 1;

	parser_destroy(p);
	return 0;
}

static void cleanup_ego(void)
{
	int idx;
	for (idx = 0; idx < z_info->e_max; idx++) {
		struct ego_item *ego = &e_info[idx];
		struct poss_item *poss;

		string_free(ego->name);
		mem_free(ego->brands);
		mem_free(ego->slays);
		release_ability_list(ego->abilities);

		poss = ego->poss_items;
		while (poss) {
			struct poss_item *next = poss->next;
			mem_free(poss);
			poss = next;
		}
	}
	mem_free(e_info);
}

struct file_parser ego_parser = {
	"ego_item",
	init_parse_ego,
	run_parse_ego,
	finish_parse_ego,
	cleanup_ego
};

/**
 * ------------------------------------------------------------------------
 * Initialize artifacts
 * ------------------------------------------------------------------------ */
static enum parser_error parse_artifact_name(struct parser *p) {
	size_t i;
	const char *name = parser_getstr(p, "name");
	struct artifact *h = parser_priv(p);

	struct artifact *a = mem_zalloc(sizeof *a);
	a->next = h;
	parser_setpriv(p, a);
	a->name = string_make(name);

	/* Ignore all base elements */
	for (i = ELEM_BASE_MIN; i < ELEM_HIGH_MIN; i++) {
		a->el_info[i].flags |= EL_INFO_IGNORE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_base_object(struct parser *p) {
	struct artifact *a = parser_priv(p);
	int tval, sval;
	const char *sval_name;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	tval = tval_find_idx(parser_getsym(p, "tval"));
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	a->tval = tval;

	sval_name = parser_getsym(p, "sval");
	sval = lookup_sval(a->tval, sval_name);
	if (sval < 0) {
		return write_dummy_object_record(a, sval_name);
	}
	a->sval = sval;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_color(struct parser *p) {
	const char *color = parser_getsym(p, "color");
	struct artifact *a = parser_priv(p);
	struct object_kind *k;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k = lookup_kind(a->tval, a->sval);
	assert(k);
	if (strlen(color) > 1) {
		k->d_attr = color_text_to_attr(color);
	} else {
		k->d_attr = color_char_to_attr(color[0]);
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_graphics(struct parser *p) {
	wchar_t glyph = parser_getchar(p, "glyph");
	const char *color = parser_getsym(p, "color");
	struct artifact *a = parser_priv(p);
	struct object_kind *k;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k = lookup_kind(a->tval, a->sval);
	assert(k);
	if (!kf_has(k->kind_flags, KF_INSTA_ART)) {
		return PARSE_ERROR_NOT_SPECIAL_ARTIFACT;
	}
	k->d_char = glyph;
	if (strlen(color) > 1) {
		k->d_attr = color_text_to_attr(color);
	} else {
		k->d_attr = color_char_to_attr(color[0]);
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_pval(struct parser *p) {
	struct artifact *a = parser_priv(p);

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	a->pval = parser_getint(p, "pval");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_level(struct parser *p) {
	struct artifact *a = parser_priv(p);

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	a->level = parser_getint(p, "level");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_rarity(struct parser *p) {
	struct artifact *a = parser_priv(p);

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	a->rarity = parser_getint(p, "rarity");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_weight(struct parser *p) {
	struct artifact *a = parser_priv(p);
	struct object_kind *k;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k = lookup_kind(a->tval, a->sval);
	assert(k);
	a->weight = parser_getint(p, "weight");
	/* Set kind weight for special artifacts */
	if (k->kidx >= z_info->ordinary_kind_max) {
		k->weight = a->weight;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_cost(struct parser *p) {
	struct artifact *a = parser_priv(p);
	struct object_kind *k;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	k = lookup_kind(a->tval, a->sval);
	assert(k);
	a->cost = parser_getint(p, "cost");
	/* Set kind cost for special artifacts */
	if (k->kidx >= z_info->ordinary_kind_max) {
		k->cost = a->cost;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_attack(struct parser *p) {
	struct artifact *a = parser_priv(p);
	struct random d;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	a->att = parser_getint(p, "att");
	d = parser_getrand(p, "dice");
	a->dd = d.dice;
	a->ds = d.sides;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_defence(struct parser *p) {
	struct artifact *a = parser_priv(p);
	struct random d;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	a->evn = parser_getint(p, "evn");
	d = parser_getrand(p, "dice");
	a->pd = d.dice;
	a->ps = d.sides;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_flags(struct parser *p) {
	struct artifact *a = parser_priv(p);
	char *s, *t;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (!parser_hasval(p, "flags")) {
		return PARSE_ERROR_NONE;
	}
	s = string_make(parser_getstr(p, "flags"));
	t = strtok(s, " |");
	while (t) {
		bool found = false;
		if (!grab_flag(a->flags, OF_SIZE, obj_flags, t)) {
			found = true;
		}
		if (grab_element_flag(a->el_info, t)) {
			found = true;
		}
		if (!found) {
			break;
		}
		t = strtok(NULL, " |");
	}
	string_free(s);
	return t ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_values(struct parser *p) {
	struct artifact *a = parser_priv(p);
	char *s, *t;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	s = string_make(parser_getstr(p, "values"));
	t = strtok(s, " |");
	while (t) {
		bool found = false;
		int value = 0;
		int index = 0;
		if (!grab_int_value(a->modifiers, obj_mods, t)) {
			found = true;
		}
		if (!grab_index_and_int(&value, &index, element_names, "RES_", t)) {
			found = true;
			a->el_info[index].res_level = value;
		}
		if (!found) {
			break;
		}

		t = strtok(NULL, " |");
	}

	string_free(s);
	return t ? PARSE_ERROR_INVALID_VALUE : PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_desc(struct parser *p) {
	struct artifact *a = parser_priv(p);

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	a->text = string_append(a->text, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_slay(struct parser *p) {
	struct artifact *a = parser_priv(p);
	const char *s = parser_getstr(p, "code");
	int i;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	for (i = 1; i < z_info->slay_max; i++) {
		if (streq(s, slays[i].code)) break;
	}
	if (i == z_info->slay_max) {
		return PARSE_ERROR_UNRECOGNISED_SLAY;
	}
	if (!a->slays) {
		a->slays = mem_zalloc(z_info->slay_max * sizeof(bool));
	}
	a->slays[i] = true;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_brand(struct parser *p) {
	struct artifact *a = parser_priv(p);
	const char *s = parser_getstr(p, "code");
	int i;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	for (i = 1; i < z_info->brand_max; i++) {
		if (streq(s, brands[i].code)) break;
	}
	if (i == z_info->brand_max) {
		return PARSE_ERROR_UNRECOGNISED_BRAND;
	}
	if (!a->brands) {
		a->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
	}
	a->brands[i] = true;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_artifact_ability(struct parser *p) {
	struct artifact *a = parser_priv(p);
	int skill;
	struct ability *b, *n;

	if (!a) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	skill = lookup_skill(parser_getsym(p, "skill"));
	if (skill < 0) {
		return PARSE_ERROR_INVALID_SKILL;
	}
	b = lookup_ability(skill, parser_getsym(p, "ability"));
	if (!b) {
		return PARSE_ERROR_INVALID_ABILITY;
	}
	n = mem_zalloc(sizeof(*n));
	memcpy(n, b, sizeof(*n));
	n->next = a->abilities;
	a->abilities = n;
	return PARSE_ERROR_NONE;
}

struct parser *init_parse_artifact(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_artifact_name);
	parser_reg(p, "base-object sym tval sym sval", parse_artifact_base_object);
	parser_reg(p, "color sym color", parse_artifact_color);
	parser_reg(p, "graphics char glyph sym color", parse_artifact_graphics);
	parser_reg(p, "pval int pval", parse_artifact_pval);
	parser_reg(p, "depth int level", parse_artifact_level);
	parser_reg(p, "rarity int rarity", parse_artifact_rarity);
	parser_reg(p, "weight int weight", parse_artifact_weight);
	parser_reg(p, "cost int cost", parse_artifact_cost);
	parser_reg(p, "attack int att rand dice", parse_artifact_attack);
	parser_reg(p, "defence int evn rand dice", parse_artifact_defence);
	parser_reg(p, "flags ?str flags", parse_artifact_flags);
	parser_reg(p, "values str values", parse_artifact_values);
	parser_reg(p, "desc str text", parse_artifact_desc);
	parser_reg(p, "slay str code", parse_artifact_slay);
	parser_reg(p, "brand str code", parse_artifact_brand);
	parser_reg(p, "ability sym skill sym ability", parse_artifact_ability);
	return p;
}

static errr run_parse_artifact(struct parser *p) {
	return parse_file_quit_not_found(p, "artefact");
}

static errr finish_parse_artifact(struct parser *p) {
	struct artifact *a, *n;
	int none, aidx;

	/* Scan the list for the max id */
	z_info->a_max = 0;
	a = parser_priv(p);
	while (a) {
		z_info->a_max++;
		a = a->next;
	}

	/* Allocate the direct access list and copy the data to it */
	a_info = mem_zalloc((z_info->a_max + 1) * sizeof(*a));
	aup_info = mem_zalloc((z_info->a_max + 1) * sizeof(*aup_info));
	aidx = z_info->a_max;
	for (a = parser_priv(p); a; a = n, aidx--) {
		assert(aidx > 0);

		memcpy(&a_info[aidx], a, sizeof(*a));
		a_info[aidx].aidx = aidx;
		n = a->next;
		a_info[aidx].next = (aidx < z_info->a_max) ?
			&a_info[aidx + 1] : NULL;
		mem_free(a);

		aup_info[aidx].aidx = aidx;
	}
	z_info->a_max += 1;

	/* Now we're done with object kinds, deal with object-like things */
	none = tval_find_idx("none");
	pile_kind = lookup_kind(none, lookup_sval(none, "<pile>"));
	parser_destroy(p);
	return 0;
}

static void cleanup_artifact(void)
{
	int idx;
	for (idx = 0; idx < z_info->a_max; idx++) {
		struct artifact *art = &a_info[idx];
		string_free(art->name);
		string_free(art->text);
		mem_free(art->brands);
		mem_free(art->slays);
		release_ability_list(art->abilities);
	}
	mem_free(a_info);
	mem_free(aup_info);
}

struct file_parser artifact_parser = {
	"artefact",
	init_parse_artifact,
	run_parse_artifact,
	finish_parse_artifact,
	cleanup_artifact
};

/**
 * ------------------------------------------------------------------------
 * Initialize self-made artifacts
 * This mostly uses the artifact functions
 * ------------------------------------------------------------------------ */
static errr run_parse_randart(struct parser *p) {
	return parse_file_quit_not_found(p, "randart");
}

static errr finish_parse_randart(struct parser *p) {
	struct artifact *a, *n;
	int aidx;
	int old_max = z_info->a_max, new_max = z_info->a_max;

	/* Scan the list for the max id */
	a = parser_priv(p);
	while (a) {
		++new_max;
		a = a->next;
	}

	/* Skip using an artifact index of zero. */
	if (!old_max && new_max) {
		++new_max;
	}
	/* Artifact indices have to fit in a uint16_t. */
	if (new_max > 65535) {
		plog_fmt("Too many artifacts (%d) after reading the "
			"randart file!", new_max);
		return PARSE_ERROR_TOO_MANY_ENTRIES;
	}

	/* Re-allocate the direct access list and copy the data to it */
	a_info = mem_realloc(a_info, new_max * sizeof(*a));
	aup_info = mem_realloc(aup_info, new_max * sizeof(*aup_info));
	if (!old_max && new_max) {
		memset(&a_info[0], 0, sizeof(a_info[0]));
		memset(&aup_info[0], 0, sizeof(aup_info[0]));
	}
	aidx = new_max - 1;
	for (a = parser_priv(p); a; a = n, aidx--) {
		assert(aidx >= old_max);

		memcpy(&a_info[aidx], a, sizeof(*a));
		a_info[aidx].aidx = aidx;
		n = a->next;
		a_info[aidx].next = (aidx < z_info->a_max) ?
			&a_info[aidx + 1] : NULL;
		mem_free(a);

		aup_info[aidx].aidx = aidx;
		aup_info[aidx].created = false;
		aup_info[aidx].seen = false;
		aup_info[aidx].everseen = false;
	}
	z_info->a_max = new_max;

	parser_destroy(p);
	return 0;
}

struct file_parser randart_parser = {
	"randart",
	init_parse_artifact,
	run_parse_randart,
	finish_parse_randart,
	cleanup_artifact
};

/**
 * ------------------------------------------------------------------------
 * Initialize object properties
 * ------------------------------------------------------------------------ */

static enum parser_error parse_object_property_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct obj_property *h = parser_priv(p);
	struct obj_property *prop = mem_zalloc(sizeof *prop);

	prop->next = h;
	parser_setpriv(p, prop);
	prop->name = string_make(name);
	prop->smith_cat = SMITH_CAT_MAX;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_type(struct parser *p) {
	struct obj_property *prop = parser_priv(p);
	const char *name = parser_getstr(p, "type");

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (streq(name, "stat")) {
		prop->type = OBJ_PROPERTY_STAT;
	} else if (streq(name, "skill")) {
		prop->type = OBJ_PROPERTY_SKILL;
	} else if (streq(name, "mod")) {
		prop->type = OBJ_PROPERTY_MOD;
	} else if (streq(name, "flag")) {
		prop->type = OBJ_PROPERTY_FLAG;
	} else if (streq(name, "slay")) {
		prop->type = OBJ_PROPERTY_SLAY;
	} else if (streq(name, "brand")) {
		prop->type = OBJ_PROPERTY_BRAND;
	} else if (streq(name, "ignore")) {
		prop->type = OBJ_PROPERTY_IGNORE;
	} else if (streq(name, "resistance")) {
		prop->type = OBJ_PROPERTY_RESIST;
	} else if (streq(name, "vulnerability")) {
		prop->type = OBJ_PROPERTY_VULN;
	} else {
		return PARSE_ERROR_INVALID_PROPERTY;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_subtype(struct parser *p) {
	struct obj_property *prop = parser_priv(p);
	const char *name = parser_getstr(p, "subtype");

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (streq(name, "sustain")) {
		prop->subtype = OFT_SUST;
	} else if (streq(name, "protection")) {
		prop->subtype = OFT_PROT;
	} else if (streq(name, "misc ability")) {
		prop->subtype = OFT_MISC;
	} else if (streq(name, "light")) {
		prop->subtype = OFT_LIGHT;
	} else if (streq(name, "melee")) {
		prop->subtype = OFT_MELEE;
	} else if (streq(name, "bad")) {
		prop->subtype = OFT_BAD;
	} else if (streq(name, "dig")) {
		prop->subtype = OFT_DIG;
	} else if (streq(name, "throw")) {
		prop->subtype = OFT_THROW;
	} else {
		return PARSE_ERROR_INVALID_SUBTYPE;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_id_type(struct parser *p) {
	struct obj_property *prop = parser_priv(p);
	const char *name = parser_getstr(p, "id");

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (streq(name, "on effect")) {
		prop->id_type = OFID_NORMAL;
	} else if (streq(name, "timed")) {
		prop->id_type = OFID_TIMED;
	} else if (streq(name, "on wield")) {
		prop->id_type = OFID_WIELD;
	} else {
		return PARSE_ERROR_INVALID_ID_TYPE;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_code(struct parser *p) {
	struct obj_property *prop = parser_priv(p);
	const char *code = parser_getstr(p, "code");
	int index = -1;

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (!prop->type) {
		return PARSE_ERROR_MISSING_OBJ_PROP_TYPE;
	}
	if (prop->type == OBJ_PROPERTY_STAT) {
		index = code_index_in_array(obj_mods, code);
	} else if (prop->type == OBJ_PROPERTY_SKILL) {
		index = code_index_in_array(obj_mods, code);
	} else if (prop->type == OBJ_PROPERTY_MOD) {
		index = code_index_in_array(obj_mods, code);
	} else if (prop->type == OBJ_PROPERTY_FLAG) {
		index = code_index_in_array(obj_flags, code);
	} else if (prop->type == OBJ_PROPERTY_SLAY) {
		index = lookup_slay(code);
	} else if (prop->type == OBJ_PROPERTY_BRAND) {
		index = lookup_brand(code);
	} else if (prop->type == OBJ_PROPERTY_IGNORE) {
		index = code_index_in_array(element_names, code);
	} else if (prop->type == OBJ_PROPERTY_RESIST) {
		index = code_index_in_array(element_names, code);
	} else if (prop->type == OBJ_PROPERTY_VULN) {
		index = code_index_in_array(element_names, code);
	}
	if (index >= 0) {
		prop->index = index;
	} else {
		return PARSE_ERROR_INVALID_OBJ_PROP_CODE;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_smith_cat(struct parser *p) {
	struct obj_property *prop = parser_priv(p);
	const char *name = parser_getstr(p, "type");

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (streq(name, "stat")) {
		prop->smith_cat = SMITH_CAT_STAT;
	} else if (streq(name, "sustain")) {
		prop->smith_cat = SMITH_CAT_SUSTAIN;
	} else if (streq(name, "skill")) {
		prop->smith_cat = SMITH_CAT_SKILL;
	} else if (streq(name, "melee")) {
		prop->smith_cat = SMITH_CAT_MELEE;
	} else if (streq(name, "slay")) {
		prop->smith_cat = SMITH_CAT_SLAY;
	} else if (streq(name, "resist")) {
		prop->smith_cat = SMITH_CAT_RESIST;
	} else if (streq(name, "curse")) {
		prop->smith_cat = SMITH_CAT_CURSE;
	} else if (streq(name, "misc")) {
		prop->smith_cat = SMITH_CAT_MISC;
	} else {
		return PARSE_ERROR_INVALID_SMITHING_CATEGORY;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_smith_diff(struct parser *p) {
	struct obj_property *prop = parser_priv(p);

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	prop->smith_diff = parser_getint(p, "difficulty");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_smith_cost(struct parser *p) {
	struct obj_property *prop = parser_priv(p);
	const char *name = parser_getsym(p, "type");

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (streq(name, "STR")) {
		prop->smith_cost_type = SMITH_COST_STR;
	} else if (streq(name, "DEX")) {
		prop->smith_cost_type = SMITH_COST_DEX;
	} else if (streq(name, "CON")) {
		prop->smith_cost_type = SMITH_COST_CON;
	} else if (streq(name, "GRA")) {
		prop->smith_cost_type = SMITH_COST_GRA;
	} else if (streq(name, "EXP")) {
		prop->smith_cost_type = SMITH_COST_EXP;
	} else {
		return PARSE_ERROR_INVALID_SMITHING_COST_TYPE;
	}

	prop->smith_cost = parser_getint(p, "cost");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_adjective(struct parser *p) {
	struct obj_property *prop = parser_priv(p);
	const char *adj = parser_getstr(p, "adj");

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(prop->adjective);
	prop->adjective = string_make(adj);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_neg_adj(struct parser *p) {
	struct obj_property *prop = parser_priv(p);
	const char *adj = parser_getstr(p, "neg_adj");

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(prop->neg_adj);
	prop->neg_adj = string_make(adj);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_msg(struct parser *p) {
	struct obj_property *prop = parser_priv(p);
	const char *msg = parser_getstr(p, "msg");

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(prop->msg);
	prop->msg = string_make(msg);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_slay_msg(struct parser *p) {
	struct obj_property *prop = parser_priv(p);
	const char *msg = parser_getstr(p, "msg");

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(prop->slay_msg);
	prop->slay_msg = string_make(msg);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_object_property_desc(struct parser *p) {
	struct obj_property *prop = parser_priv(p);
	const char *desc = parser_getstr(p, "desc");

	if (!prop) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(prop->desc);
	prop->desc = string_make(desc);
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_object_property(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_object_property_name);
	parser_reg(p, "code str code", parse_object_property_code);
	parser_reg(p, "smith-cat str type", parse_object_property_smith_cat);
	parser_reg(p, "smith-difficulty int difficulty",
			   parse_object_property_smith_diff);
	parser_reg(p, "smith-cost sym type int cost",
			   parse_object_property_smith_cost);
	parser_reg(p, "type str type", parse_object_property_type);
	parser_reg(p, "subtype str subtype", parse_object_property_subtype);
	parser_reg(p, "id-type str id", parse_object_property_id_type);
	parser_reg(p, "adjective str adj", parse_object_property_adjective);
	parser_reg(p, "neg-adjective str neg_adj", parse_object_property_neg_adj);
	parser_reg(p, "msg str msg", parse_object_property_msg);
	parser_reg(p, "slay-msg str msg", parse_object_property_slay_msg);
	parser_reg(p, "desc str desc", parse_object_property_desc);
	return p;
}

static errr run_parse_object_property(struct parser *p) {
	return parse_file_quit_not_found(p, "object_property");
}

static errr finish_parse_object_property(struct parser *p) {
	struct obj_property *prop, *n;
	int idx;

	/* Scan the list for the max id */
	z_info->property_max = 0;
	prop = parser_priv(p);
	while (prop) {
		z_info->property_max++;
		prop = prop->next;
	}

	/* Allocate the direct access list and copy the data to it */
	obj_properties = mem_zalloc((z_info->property_max + 1) * sizeof(*prop));
	idx = z_info->property_max;
	for (prop = parser_priv(p); prop; prop = n, idx--) {
		assert(idx > 0);

		memcpy(&obj_properties[idx], prop, sizeof(*prop));
		n = prop->next;

		mem_free(prop);
	}
	z_info->property_max += 1;

	parser_destroy(p);
	return 0;
}

static void cleanup_object_property(void)
{
	int idx;
	for (idx = 0; idx < z_info->property_max; idx++) {
		struct obj_property *prop = &obj_properties[idx];

		string_free(prop->name);
		string_free(prop->adjective);
		string_free(prop->neg_adj);
		string_free(prop->slay_msg);
		string_free(prop->msg);
		string_free(prop->desc);
	}
	mem_free(obj_properties);
}

struct file_parser object_property_parser = {
	"object_property",
	init_parse_object_property,
	run_parse_object_property,
	finish_parse_object_property,
	cleanup_object_property
};

