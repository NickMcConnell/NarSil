/**
 * \file obj-knowledge.h
 * \brief Object knowledge
 *
 * Copyright (c) 2016 Nick McConnell
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
#include "object.h"
#include "player.h"

enum rune_variety {
	RUNE_VAR_COMBAT,
	RUNE_VAR_MOD,
	RUNE_VAR_RESIST,
	RUNE_VAR_BRAND,
	RUNE_VAR_SLAY,
	RUNE_VAR_CURSE,
	RUNE_VAR_FLAG
};

struct rune {
	enum rune_variety variety;
	int index;
	quark_t note;
	const char *name;
};

int max_runes(void);
enum rune_variety rune_variety(size_t i);
bool player_knows_rune(struct player *p, size_t i);
const char *rune_name(size_t i);
const char *rune_desc(size_t i);
quark_t rune_note(size_t i);
void rune_set_note(size_t i, const char *inscription);

bool player_knows_flag(struct player *p, int f);
bool player_knows_brand(struct player *p, int i);
bool player_knows_slay(struct player *p, int i);
bool player_knows_ego(struct player *p, struct ego_item *ego);
bool object_is_known_artifact(const struct object *obj);
bool object_has_rune(const struct object *obj, int rune_no);
bool object_runes_known(const struct object *obj);
bool object_flag_is_known(const struct player *p, const struct object *obj,
	int flag);
bool object_element_is_known(const struct player *p, const struct object *obj,
	int element);

void object_set_base_known(struct player *p, struct object *obj);
void object_see(struct player *p, struct object *obj);
void object_touch(struct player *p, struct object *obj);
void player_know_object(struct player *p, struct object *obj);
void update_player_object_knowledge(struct player *p);

void player_learn_flag(struct player *p, int flag);
void player_learn_brand(struct player *p, int index);
void player_learn_slay(struct player *p, int index);
void player_learn_all_runes(struct player *p);

void object_learn_unknown_rune(struct player *p, struct object *obj);
void object_learn_on_wield(struct player *p, struct object *obj);

void equip_learn_flag(struct player *p, int flag);
void equip_learn_element(struct player *p, int element);
void equip_learn_after_time(struct player *p);

bool easy_know(const struct object *obj);
bool object_flavor_is_aware(const struct object *obj);
bool object_flavor_was_tried(const struct object *obj);
void object_flavor_aware(struct player *p, struct object *obj);
void object_flavor_tried(struct object *obj);

int object_value(const struct object *obj);
