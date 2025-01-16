/**
 * \file obj-smith.h
 * \brief Smithing of objects
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
 */
#ifndef INCLUDED_OBJSMITH_H
#define INCLUDED_OBJSMITH_H

#include "player.h"

/**
 * Broad types of items requiring different smithing specialties
 */
enum smithing_type {
	SMITH_TYPE_WEAPON,
	SMITH_TYPE_ARMOUR,
	SMITH_TYPE_JEWELRY
};

/**
 * Categories of smithing properties
 */
enum smithing_category {
	SMITH_CAT_STAT,
	SMITH_CAT_SUSTAIN,
	SMITH_CAT_SKILL,
	SMITH_CAT_MELEE,
	SMITH_CAT_SLAY,
	SMITH_CAT_RESIST,
	SMITH_CAT_CURSE,
	SMITH_CAT_MISC,
	SMITH_CAT_MAX
};

/**
 * Types of smithing cost that can apply to object properties.
 * Note that the first four correspond numerically with the stats.
 */
enum smithing_cost_xtra {
	SMITH_COST_STR,
	SMITH_COST_DEX,
	SMITH_COST_CON,
	SMITH_COST_GRA,
	SMITH_COST_EXP,
};

enum smithing_numbers_mod_index {
	SMITH_NUM_INC_ATT,
	SMITH_NUM_DEC_ATT,
	SMITH_NUM_INC_DS,
	SMITH_NUM_DEC_DS,
	SMITH_NUM_INC_EVN,
	SMITH_NUM_DEC_EVN,
	SMITH_NUM_INC_PS,
	SMITH_NUM_DEC_PS,
	SMITH_NUM_INC_PVAL,
	SMITH_NUM_DEC_PVAL,
	SMITH_NUM_INC_WGT,
	SMITH_NUM_DEC_WGT,
	SMITH_NUM_MAX
};

/**
 * A structure to hold the costs of smithing something
 */
struct smithing_cost {
	int stat[STAT_MAX];
	int exp;
	int mithril;
	int uses;
	int drain;
	int difficulty;
    bool weaponsmith;
    bool armoursmith;
    bool jeweller;
    bool enchantment;
    bool artistry;
    bool artifice;
};

#define MAX_SMITHING_TVALS 18

/**
 * A structure to hold a tval and its smithing category and description
 */
struct smithing_tval_desc {
	int category;
	int tval;
	const char *desc;
};

/**
 * A list of tvals and their textual names and smithing categories
 */
extern const struct smithing_tval_desc smithing_tvals[MAX_SMITHING_TVALS];

int att_valid(struct object *obj);
int att_max(struct object *obj, bool assume_artistry);
int att_min(struct object *obj);
int ds_valid(struct object *obj);
int ds_max(struct object *obj, bool assume_artistry);
int ds_min(struct object *obj);
int evn_valid(struct object *obj);
int evn_max(struct object *obj, bool assume_artistry);
int evn_min(struct object *obj);
int ps_valid(struct object *obj);
int ps_max(struct object *obj, bool assume_artistry);
int ps_min(struct object *obj);
int pval_valid(struct object *obj);
int pval_default(struct object *obj);
int pval_max(struct object *obj);
int pval_min(struct object *obj);
int wgt_valid(struct object *obj);
int wgt_max(struct object *obj);
int wgt_min(struct object *obj);
void modify_numbers(struct object *obj, int choice, int *pval);
bool object_is_mithril(const struct object *obj);
bool melt_mithril_item(struct player *p, struct object *obj);
int mithril_items_carried(struct player *p);
int mithril_carried(struct player *p);
int object_difficulty(struct object *obj, struct smithing_cost *smithing_cost);
bool smith_affordable(struct object *obj, struct smithing_cost *smithing_cost);
void create_base_object(struct object_kind *kind, struct object *obj);
void create_special(struct object *obj, struct ego_item *ego);
void artefact_copy(struct artifact *a_dst, struct artifact *a_src);
void add_artefact_details(struct artifact *art, struct object *obj);
bool applicable_property(struct obj_property *prop, struct object *obj);
bool object_has_property(struct obj_property *prop, struct object *obj,
						 bool negative);
void add_object_property(struct obj_property *prop, struct object *obj,
						   bool negative);
void remove_object_property(struct obj_property *prop, struct object *obj);
void do_cmd_smith_aux(bool flush);
void do_cmd_smith(struct command *cmd);

#endif /* !INCLUDED_OBJSMITH_H */
