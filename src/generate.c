/**
 * \file generate.c
 * \brief Dungeon generation.
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2013 Erik Osheim, Nick McConnell
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
 * This is the top level dungeon generation file, which contains room profiles
 * (for determining what rooms are available and their parameters), cave
 * profiles (for determining the level generation function and parameters for
 * different styles of levels), initialisation functions for template rooms and
 * vaults, and the main level generation function (which calls the level
 * builders from gen-cave.c).
 *
 * See the "vault.txt" file for more on vault generation.
 * See the "room_template.txt" file for more room templates.
 */

#include "angband.h"
#include "cave.h"
#include "datafile.h"
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-spell.h"
#include "monster.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-util.h"
#include "trap.h"
#include "z-queue.h"
#include "z-type.h"

/*
 * Array of pit types
 */
struct vault *vaults;
static struct cave_profile *cave_profiles;
struct dun_data *dun;
struct room_template *room_templates;

static const struct {
	const char *name;
	cave_builder builder;
} cave_builders[] = {
	#define DUN(a, b) { a, b##_gen },
	#include "list-dun-profiles.h"
	#undef DUN
};

static const struct {
	const char *name;
	int max_height;
	int max_width;
	room_builder builder;
} room_builders[] = {
	#define ROOM(a, b, c, d) { a, b, c, build_##d },
	#include "list-rooms.h"
	#undef ROOM
};

static const char *room_flags[] = {
	"NONE",
	#define ROOMF(a, b) #a,
	#include "list-room-flags.h"
	#undef ROOMF
	NULL
};


/**
 * Parsing functions for dungeon_profile.txt
 */
static enum parser_error parse_profile_name(struct parser *p) {
	struct cave_profile *h = parser_priv(p);
	struct cave_profile *c = mem_zalloc(sizeof *c);
	size_t i;

	c->name = string_make(parser_getstr(p, "name"));
	for (i = 0; i < N_ELEMENTS(cave_builders); i++)
		if (streq(c->name, cave_builders[i].name))
			break;

	if (i == N_ELEMENTS(cave_builders))
		return PARSE_ERROR_NO_BUILDER_FOUND;
	c->builder = cave_builders[i].builder;
	c->next = h;
	parser_setpriv(p, c);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_profile_rooms(struct parser *p) {
	struct cave_profile *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->dun_rooms = parser_getint(p, "rooms");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_profile_streamer(struct parser *p) {
	struct cave_profile *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->str.den = parser_getint(p, "den");
	c->str.rng = parser_getint(p, "rng");
	c->str.qua = parser_getint(p, "qua");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_profile_alloc(struct parser *p) {
	struct cave_profile *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->alloc = parser_getint(p, "alloc");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_profile_room(struct parser *p) {
	struct cave_profile *c = parser_priv(p);
	struct room_profile *r = c->room_profiles;
	size_t i;

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* Go to the last valid room profile, then allocate a new one */
	if (!r) {
		c->room_profiles = mem_zalloc(sizeof(struct room_profile));
		r = c->room_profiles;
		/* First profile is empty */
		//r->next = mem_zalloc(sizeof(struct room_profile));
		//r = r->next;
	} else {
		while (r->next)
			r = r->next;
		r->next = mem_zalloc(sizeof(struct room_profile));
		r = r->next;
	}

	/* Now read the data */
	r->name = string_make(parser_getsym(p, "name"));
	for (i = 0; i < N_ELEMENTS(room_builders); i++)
		if (streq(r->name, room_builders[i].name))
			break;

	if (i == N_ELEMENTS(room_builders))
		return PARSE_ERROR_NO_ROOM_FOUND;
	r->builder = room_builders[i].builder;
	r->hardness = parser_getint(p, "hardness");
	r->level = parser_getint(p, "level");
	r->random = parser_getint(p, "random");
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_profile(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_profile_name);
	parser_reg(p, "rooms int rooms", parse_profile_rooms);
	parser_reg(p, "streamer int den int rng int qua", parse_profile_streamer);
	parser_reg(p, "alloc int alloc", parse_profile_alloc);
	parser_reg(p, "room sym name int hardness int level int random", parse_profile_room);
	return p;
}

static errr run_parse_profile(struct parser *p) {
	return parse_file_quit_not_found(p, "dungeon_profile");
}

static errr finish_parse_profile(struct parser *p) {
	struct cave_profile *n, *c = parser_priv(p);
	int num;

	/* Count the list */
	z_info->profile_max = 0;
	while (c) {
		z_info->profile_max++;
		c = c->next;
	}

	/* Allocate the array and copy the records to it */
	cave_profiles = mem_zalloc(z_info->profile_max * sizeof(*c));
	num = z_info->profile_max - 1;
	for (c = parser_priv(p); c; c = n) {
		/* Main record */
		memcpy(&cave_profiles[num], c, sizeof(*c));
		n = c->next;
		if (num < z_info->profile_max - 1) {
			cave_profiles[num].next = &cave_profiles[num + 1];
		} else {
			cave_profiles[num].next = NULL;
		}

		if (c->room_profiles) {
			struct room_profile *r_old = c->room_profiles;
			struct room_profile *r_new;
			int i;

			/* Count the room profiles */
			cave_profiles[num].n_room_profiles = 0;
			while (r_old) {
				++cave_profiles[num].n_room_profiles;
				r_old = r_old->next;
			}

			/* Now allocate the room profile array */
			r_new = mem_zalloc(cave_profiles[num].n_room_profiles
				* sizeof(*r_new));

			r_old = c->room_profiles;
			for (i = 0; i < cave_profiles[num].n_room_profiles; i++) {
				struct room_profile *r_temp = r_old;

				/* Copy from the linked list to the array */
				memcpy(&r_new[i], r_old, sizeof(*r_old));

				/* Set the next profile pointer correctly. */
				if (r_new[i].next) {
					r_new[i].next = &r_new[i + 1];
				}

				/* Tidy up and advance to the next profile. */
				r_old = r_old->next;
				mem_free(r_temp);
			}

			cave_profiles[num].room_profiles = r_new;
		} else {
			cave_profiles[num].n_room_profiles = 0;
			cave_profiles[num].room_profiles = NULL;
		}

		mem_free(c);
		num--;
	}

	parser_destroy(p);
	return 0;
}

static void cleanup_profile(void)
{
	int i, j;
	for (i = 0; i < z_info->profile_max; i++) {
		for (j = 0; j < cave_profiles[i].n_room_profiles; j++)
			string_free((char *) cave_profiles[i].room_profiles[j].name);
		mem_free(cave_profiles[i].room_profiles);
		string_free((char *) cave_profiles[i].name);
	}
	mem_free(cave_profiles);
}

static struct file_parser profile_parser = {
	"dungeon_profile",
	init_parse_profile,
	run_parse_profile,
	finish_parse_profile,
	cleanup_profile
};


/**
 * Parsing functions for vault.txt
 */
static enum parser_error parse_vault_name(struct parser *p) {
	struct vault *h = parser_priv(p);
	struct vault *v = mem_zalloc(sizeof *v);

	v->name = string_make(parser_getstr(p, "name"));
	v->next = h;
	v->index = z_info->v_max;
	z_info->v_max++;
	parser_setpriv(p, v);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_vault_type(struct parser *p) {
	struct vault *v = parser_priv(p);

	if (!v)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	v->typ = string_make(parser_getstr(p, "type"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_vault_depth(struct parser *p) {
	struct vault *v = parser_priv(p);

	if (!v)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	v->depth = parser_getuint(p, "depth");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_vault_rarity(struct parser *p) {
	struct vault *v = parser_priv(p);

	if (!v)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	v->rarity = parser_getuint(p, "rarity");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_vault_flags(struct parser *p) {
	struct vault *v = parser_priv(p);
	char *s, *st;

	if (!v)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	s = string_make(parser_getstr(p, "flags"));
	st = strtok(s, " |");
	while (st && !grab_flag(v->flags, ROOMF_SIZE, room_flags, st)) {
		st = strtok(NULL, " |");
	}
	mem_free(s);

	return st ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_vault_d(struct parser *p) {
	struct vault *v = parser_priv(p);
	const char *desc;
	size_t i;

	if (!v)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	desc = parser_getstr(p, "text");
	if (!v->wid) {
		v->wid = strlen(desc);
	}


	if (strlen(desc) != v->wid) {
		return PARSE_ERROR_VAULT_DESC_WRONG_LENGTH;
	} else {
		v->text = string_append(v->text, desc);
		v->hgt++;
		/* Note if there is a forge in the vault */
		if (strchr(desc, '0')) v->forge = true;
	}

	/* Make sure vaults are no higher or wider than the room profiles allow. */
	for (i = 0; i < N_ELEMENTS(room_builders); i++) {
		if (streq(v->typ, room_builders[i].name)) {
			break;
		}
	}
	if (i == N_ELEMENTS(room_builders)) {
		return PARSE_ERROR_NO_ROOM_FOUND;
	}
	if (v->wid > room_builders[i].max_width) {
		return PARSE_ERROR_VAULT_TOO_BIG;
	}
	if (v->hgt > room_builders[i].max_height) {
		return PARSE_ERROR_VAULT_TOO_BIG;
	}

	return PARSE_ERROR_NONE;
}

struct parser *init_parse_vault(void) {
	struct parser *p = parser_new();
	z_info->v_max = 0;
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_vault_name);
	parser_reg(p, "type str type", parse_vault_type);
	parser_reg(p, "depth uint depth", parse_vault_depth);
	parser_reg(p, "rarity uint rarity", parse_vault_rarity);
	parser_reg(p, "flags str flags", parse_vault_flags);
	parser_reg(p, "D str text", parse_vault_d);
	return p;
}

static errr run_parse_vault(struct parser *p) {
	return parse_file_quit_not_found(p, "vault");
}

static errr finish_parse_vault(struct parser *p) {
	uint32_t rarity_denom = 1;
	struct vault *v;

	vaults = parser_priv(p);
	parser_destroy(p);

	/*
	 * For use in random_vault(), convert rarities from the 1 per value
	 * specified in vault.txt to use a fixed denominator that is the
	 * smallest integer positive integer divisible by all the rarities.
	 */
	for (v = vaults; v; v = v->next) {
		if (v->rarity > 0) {
			/*
			 * Find the greatest common divisor of rarity_denom and
			 * v->rarity using the division-based version of
			 * Euclid's algorithm.
			 */
			uint32_t g = rarity_denom;
			uint32_t b = v->rarity;

			while (b) {
				uint32_t t = b;

				b = g % b;
				g = t;
			}

			/*
			 * Update rarity_denom with the factors from v->rarity
			 * not already present.
			 */
			if (rarity_denom > 4294967295 / (v->rarity / g)) {
				plog("Smallest integer divisible by all vault rarities is too large.");
				return PARSE_ERROR_OUT_OF_BOUNDS;
			}
			rarity_denom *= v->rarity / g;
		}
	}
	/*
	 * Avoid the potential of overflow in random_vault() as it accumulates
	 * the rarities of possible vaults.
	 */
	if (rarity_denom > 4294967295 / z_info->v_max) {
		plog("Product of number of vaults and smallest integer divisible by all vault rarities is too large.");
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	for (v = vaults; v; v = v->next) {
		if (v->rarity > 0) {
			v->rarity = rarity_denom / v->rarity;
		}
	}

	return 0;
}

static void cleanup_vault(void)
{
	struct vault *v, *next;
	for (v = vaults; v; v = next) {
		next = v->next;
		mem_free(v->name);
		mem_free(v->typ);
		mem_free(v->text);
		mem_free(v);
	}
}

static struct file_parser vault_parser = {
	"vault",
	init_parse_vault,
	run_parse_vault,
	finish_parse_vault,
	cleanup_vault
};

static void run_template_parser(void) {
	/* Initialize room info */
	event_signal_message(EVENT_INITSTATUS, 0,
						 "Initializing arrays... (dungeon profiles)");
	if (run_parser(&profile_parser))
		quit("Cannot initialize dungeon profiles");

	/* Initialize vault info */
	event_signal_message(EVENT_INITSTATUS, 0,
						 "Initializing arrays... (vaults)");
	if (run_parser(&vault_parser))
		quit("Cannot initialize vaults");
}


/**
 * Free the template arrays
 */
static void cleanup_template_parser(void)
{
	cleanup_parser(&profile_parser);
	cleanup_parser(&vault_parser);
}


/**
 * Find a cave_profile by name
 * \param name is the name of the cave_profile being looked for
 */
static const struct cave_profile *find_cave_profile(const char *name)
{
	int i;

	for (i = 0; i < z_info->profile_max; i++) {
		const struct cave_profile *profile;

		profile = &cave_profiles[i];
		if (streq(name, profile->name))
			return profile;
	}

	/* Not there */
	return NULL;
}

/**
 * Choose a cave profile (only one for most levels currently - NRM)
 * \param p is the player
 */
static const struct cave_profile *choose_profile(struct player *p)
{
	const struct cave_profile *profile = NULL;

	/* Make the profile choice */
	if (p->depth == 0) {
		profile = find_cave_profile("gates");
	} else if (p->depth == z_info->dun_depth) {
		profile = find_cave_profile("throne");
	} else {
		int total_alloc = 0;
		size_t i;

		/*
		 * Use PowerWyrm's selection algorithm from
		 * get_random_monster_object() so the selection can be done in
		 * one pass and without auxiliary storage (at the cost of more
		 * calls to randint0()).  The mth valid profile out of n valid
		 * profiles appears with probability, alloc(m) /
		 * sum(i = 0 to m, alloc(i)) * product(j = m + 1 to n - 1,
		 * 1 - alloc(j) / sum(k = 0 to j, alloc(k))) which is equal to
		 * alloc(m) / sum(i = 0 to m, alloc(i)) * product(j = m + 1 to
		 * n - 1, sum(k = 0 to j - 1, alloc(k)) / sum(l = 0 to j,
		 * alloc(l))) which, by the canceling of successive numerators
		 * and denominators is alloc(m) / sum(l = 0 to n - 1, alloc(l)).
		 */
		for (i = 0; i < z_info->profile_max; i++) {
			struct cave_profile *test_profile = &cave_profiles[i];
			if (test_profile->alloc <= 0) continue;
			total_alloc += test_profile->alloc;
			if (randint0(total_alloc) < test_profile->alloc) {
				profile = test_profile;
			}
		}
		if (!profile) {
			profile = find_cave_profile("cave");
		}
	}

	/* Return the profile or fail horribly */
	if (profile)
		return profile;
	else
		quit("Failed to find cave profile!");

	return NULL;
}

/**
 * Clear the dungeon, ready for generation to begin.
 */
static void cave_clear(struct chunk *c, struct player *p)
{
	/* Reset smithing leftover (as there is no access to the old forge) */
	p->smithing_leftover = 0;

    /* Reset the forced skipping of next turn (a bit rough to miss
	 * first turn if you fell down) */
    p->upkeep->knocked_back = false;

	/* Forget knowledge of old level */
	if (p->cave && (c == cave)) {
		int x, y;

		/* Deal with artifacts */
		for (y = 0; y < c->height; y++) {
			for (x = 0; x < c->width; x++) {
				struct object *obj = square_object(c, loc(x, y));
				while (obj) {
					if (obj->artifact && obj_is_known_artifact(obj)) {
						history_lose_artifact(p, obj->artifact);
						mark_artifact_created(obj->artifact, true);
					}

					obj = obj->next;
				}
			}
		}

		/* Free the known cave */
		cave_free(p->cave);
		p->cave = NULL;
	}

	/* Clear the monsters */
	wipe_mon_list(c, p);

	/* Forget the fire information */
	forget_fire(c);

	/* Free the chunk */
	cave_free(c);
}


/**
 * Release the dynamically allocated resources in a dun_data structure.
 */
static void cleanup_dun_data(struct dun_data *dd)
{
	int i;

	mem_free(dd->cent);
	mem_free(dd->corner);
	mem_free(dd->piece);
	for (i = 0; i < z_info->level_room_max; ++i) {
		mem_free(dd->connection[i]);
	}
	mem_free(dd->connection);
	for (i = 0; i < z_info->dungeon_hgt; ++i) {
		mem_free(dd->tunn1[i]);
		mem_free(dd->tunn2[i]);
	}
	mem_free(dd->tunn1);
	mem_free(dd->tunn2);
}

/**
 * Generate a random level.
 *
 * Confusingly, this function also generates the town level (level 0).
 * \param p is the current player struct, in practice the global player
 * \return a pointer to the new level
 */
static struct chunk *cave_generate(struct player *p)
{
	const char *error = "no generation";
	int i, tries = 0;
	struct chunk *chunk = NULL;

	/* Generate */
	for (tries = 0; tries < 100 && error; tries++) {
		int y, x;
		struct dun_data dun_body;
		bool forge_made = p->unique_forge_made;

		error = NULL;

		/* Mark the dungeon as being unready (to avoid artifact loss, etc) */
		character_dungeon = false;

		/* Allocate global data (will be freed when we leave the loop) */
		dun = &dun_body;
		dun->cent = mem_zalloc(z_info->level_room_max * sizeof(struct loc));
		dun->cent_n = 0;
		dun->corner = mem_zalloc(z_info->level_room_max
								 * sizeof(struct rectangle));
		dun->piece = mem_zalloc(z_info->level_room_max * sizeof(int));
		dun->tunn1 = mem_zalloc(z_info->dungeon_hgt * sizeof(int*));
		dun->tunn2 = mem_zalloc(z_info->dungeon_hgt * sizeof(int*));
		for (y = 0; y < z_info->dungeon_hgt; y++) {
			dun->tunn1[y] = mem_zalloc(z_info->dungeon_wid * sizeof(int));
			dun->tunn2[y] = mem_zalloc(z_info->dungeon_wid * sizeof(int));
		}
		dun->connection = mem_zalloc(z_info->level_room_max * sizeof(bool*));
		for (i = 0; i < z_info->level_room_max; ++i) {
			dun->connection[i] = mem_zalloc(z_info->level_room_max
											* sizeof(bool));
		}

		/* Choose a profile and build the level */
		dun->profile = choose_profile(p);
		event_signal_string(EVENT_GEN_LEVEL_START, dun->profile->name);
		chunk = dun->profile->builder(p);
		if (!chunk) {
			error = "Failed to find builder";
			cleanup_dun_data(dun);
			p->unique_forge_made = forge_made;
			event_signal_flag(EVENT_GEN_LEVEL_END, false);
			continue;
		}

		/* Clear generation flags */
		for (y = 0; y < chunk->height; y++) {
			for (x = 0; x < chunk->width; x++) {
				struct loc grid = loc(x, y);

				sqinfo_off(square(chunk, grid)->info, SQUARE_WALL_INNER);
				sqinfo_off(square(chunk, grid)->info, SQUARE_WALL_OUTER);
				sqinfo_off(square(chunk, grid)->info, SQUARE_WALL_SOLID);
			}
		}

		/* Regenerate levels that overflow their maxima */
		if (cave_monster_max(chunk) >= z_info->level_monster_max)
			error = "too many monsters";

		if (error) {
			if (OPT(p, cheat_room)) {
				msg("Generation restarted: %s.", error);
			}
			uncreate_artifacts(chunk);
			uncreate_greater_vaults(chunk, p);
			cave_clear(chunk, p);
			p->unique_forge_made = forge_made;
			event_signal_flag(EVENT_GEN_LEVEL_END, false);
		}

		cleanup_dun_data(dun);
	}

	if (error) quit_fmt("cave_generate() failed 100 times!");

	/* Validate the dungeon (we could use more checks here) */
	chunk_validate_objects(chunk);

	/* Allocate new known level */
	p->cave = cave_new(chunk->height, chunk->width);
	p->cave->depth = chunk->depth;
	p->cave->objects = mem_realloc(p->cave->objects, (chunk->obj_max + 1)
								   * sizeof(struct object*));
	p->cave->obj_max = chunk->obj_max;
	for (i = 0; i <= p->cave->obj_max; i++) {
		p->cave->objects[i] = NULL;
	}

	/* Clear stair creation */
	p->upkeep->create_stair = FEAT_NONE;

	return chunk;
}

/**
 * Prepare the level the player is about to enter
 *
 * \param p is the current player struct, in practice the global player
*/
void prepare_next_level(struct player *p)
{
	int x, y;
	bool noted = false;

	/* Deal with any existing current level */
	if (character_dungeon) {
		/* Deal with missing out on stuff */
		for (y = 0; y < cave->height; y++) {
			for (x = 0; x < cave->width; x++) {
				struct loc grid = loc(x, y);

				/* Artifacts */
				struct object *obj = square_object(cave, grid);
				while (obj) {
					if (obj->artifact) {
						history_lose_artifact(p, obj->artifact);
						mark_artifact_created(obj->artifact, true);
					}
					obj = obj->next;
				}

				/* Greater vaults */
				if (!noted && square_ismark(cave, grid) &&
					square_isgreatervault(cave, grid)) {
					history_add(p, format("Left without entering %s",
										  cave->vault_name), HIST_VAULT_LOST);
					noted = true;
				}
			}
		}

		/* Clear the old cave */
		if (cave) {
			cave_clear(cave, p);
			cave = NULL;
		}
	}

	/* Generate a new level */
	cave = cave_generate(p);
	event_signal_flag(EVENT_GEN_LEVEL_END, true);

	/* Note any forges generated, done here in case generation fails earlier */
	for (y = 0; y < cave->height; y++) {
		for (x = 0; x < cave->width; x++) {
			if (square_isforge(cave, loc(x, y))) {
				/* Reset the time since the last forge when an interesting room
				 * with a forge is generated */
				player->forge_drought = 0;
				player->forge_count++;
			}
		}
	}

	/* The dungeon is ready */
	character_dungeon = true;
}

/**
 * Return the number of room builders available.
 */
int get_room_builder_count(void)
{
	return (int) N_ELEMENTS(room_builders);
}

/**
 * Convert the name of a room builder into its index.  Return -1 if the
 * name does not match any of the room builders.
 */
int get_room_builder_index_from_name(const char *name)
{
	int i = 0;

	while (1) {
		if (i >= (int) N_ELEMENTS(room_builders)) {
			return -1;
		}
		if (streq(name, room_builders[i].name)) {
			return i;
		}
		++i;
	}
}

/**
 * Get the name of a room builder given its index.  Return NULL if the index
 * is out of bounds (less than one or greater than or equal to
 * get_room_builder_count()).
 */
const char *get_room_builder_name_from_index(int i)
{
	return (i >= 0 && i < (int) get_room_builder_count()) ?
		room_builders[i].name : NULL;
}

/**
 * Convert the name of a level profile into its index in the cave_profiles
 * list.  Return -1 if the name does not match any of the profiles.
 */
int get_level_profile_index_from_name(const char *name)
{
	const struct cave_profile *p = find_cave_profile(name);

	return (p) ? (int) (p - cave_profiles) : -1;
}

/**
 * Get the name of a level profile given its index.  Return NULL if the index
 * is out of bounds (less than one or greater than or equal to
 * z_info->profile_max).
 */
const char *get_level_profile_name_from_index(int i)
{
	return (i >= 0 && i < z_info->profile_max) ?
		cave_profiles[i].name : NULL;
}

/**
 * The generate module, which initialises template rooms and vaults
 * Should it clean up?
 */
struct init_module generate_module = {
	.name = "generate",
	.init = run_template_parser,
	.cleanup = cleanup_template_parser
};
