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

bool player_knows_ego(struct player *p, struct ego_item *ego);
bool easy_know(const struct object *obj);
bool object_flavor_is_aware(const struct object *obj);
bool object_flavor_was_tried(const struct object *obj);
void object_flavor_aware(struct player *p, struct object *obj);
void object_flavor_tried(struct object *obj);
void object_set_base_known(struct player *p, struct object *obj);
void object_see(struct player *p, struct object *obj);
void object_touch(struct player *p, struct object *obj);
void update_player_object_knowledge(struct player *p);
bool object_is_cursed(const struct object *obj);
bool object_is_broken(const struct object *obj);
int pseudo_id_check_weak(const struct object *obj);
int pseudo_id_check_strong(const struct object *obj);
bool can_be_pseudo_ided(const struct object *obj);
void pseudo_id(struct object *obj);
void pseudo_id_everything(void);
bool object_is_known(const struct object *obj);
void object_know(struct object *obj);
void ident(struct object *obj);
void ident_on_wield(struct player *p, struct object *obj);
void ident_flag(struct player *p, int flag);
void ident_element(struct player *p, int element);
void ident_passive(struct player *p);
void ident_see_invisible(const struct monster *mon, struct player *p);
void ident_haunted(struct player *p);
void ident_cowardice(struct player *p);
void ident_hunger(struct player *p);
void ident_weapon_by_use(struct object *obj, char *m_name, int flag, int brand,
						 int slay, struct player *p);
void ident_bow_arrow_by_use(struct object *bow, struct object *arrows,
							char *m_name, int bow_brand, int bow_slay,
							int arrow_flag, int arrow_brand, int arrow_slay,
							struct player *p);
void id_known_specials(void);
int object_value(const struct object *obj);
