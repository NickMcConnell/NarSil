/**
 * \file obj-make.h
 * \brief Object generation functions.
 *
 * Copyright (c) 1987-2007 Angband contributors
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

#ifndef OBJECT_MAKE_H
#define OBJECT_MAKE_H

#include "cave.h"

/**
 * Define a value for minima which will be ignored (a replacement for 0,
 * because 0 and some small negatives are valid values).
 */
#define NO_MINIMUM 	255

/**
 * Define a value to be a placeholder for a random expression used in the 
 * generation of some jewellery.
 */
#define SPECIAL_VALUE 99

void ego_apply_magic(struct object *obj, bool smithing);
void copy_artifact_data(struct object *obj, const struct artifact *art);
bool make_fake_artifact(struct object *obj, const struct artifact *artifact);
void object_prep(struct object *obj, struct object_kind *kind, int lev,
				 aspect rand_aspect);
struct drop *lookup_drop(const char *name);
void apply_magic(struct object *obj, int lev, bool allow_artifacts, bool good,
				 bool great);
bool kind_is_good(const struct object_kind *kind);
struct object_kind *get_obj_num(int level);
struct object *make_object(struct chunk *c, int lev, bool good, bool great,
		struct drop *drop);
int extract_kind_pval(const struct object_kind *kind, aspect rand_aspect,
		bool *flip_sign_out);

#endif /* OBJECT_MAKE_H */
