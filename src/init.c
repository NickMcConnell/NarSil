/**
 * \file init.c
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
 * This file is used to initialize various variables and arrays for the
 * Angband game.
 *
 * Several of the arrays for Angband are built from data files in the
 * "lib/gamedata" directory.
 */


#include "angband.h"
#include "buildid.h"
#include "cave.h"
#include "cmds.h"
#include "cmd-core.h"
#include "datafile.h"
#include "effects.h"
#include "game-event.h"
#include "game-world.h"
#include "generate.h"
#include "hint.h"
#include "init.h"
#include "mon-init.h"
#include "mon-list.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-msg.h"
#include "mon-summon.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-chest.h"
#include "obj-ignore.h"
#include "obj-init.h"
#include "obj-list.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "option.h"
#include "player.h"
#include "player-abilities.h"
#include "player-history.h"
#include "player-timed.h"
#include "project.h"
#include "randname.h"
#include "songs.h"
#include "trap.h"
#include "ui-visuals.h"

bool play_again = false;

/**
 * Structure (not array) of game constants
 */
struct angband_constants *z_info;

/*
 * Hack -- The special Angband "System Suffix"
 * This variable is used to choose an appropriate "pref-xxx" file
 */
const char *ANGBAND_SYS = "xxx";

/**
 * Various directories. These are no longer necessarily all subdirs of "lib"
 */
char *ANGBAND_DIR_GAMEDATA;
char *ANGBAND_DIR_CUSTOMIZE;
char *ANGBAND_DIR_HELP;
char *ANGBAND_DIR_SCREENS;
char *ANGBAND_DIR_FONTS;
char *ANGBAND_DIR_TILES;
char *ANGBAND_DIR_SOUNDS;
char *ANGBAND_DIR_ICONS;
char *ANGBAND_DIR_USER;
char *ANGBAND_DIR_SAVE;
char *ANGBAND_DIR_PANIC;
char *ANGBAND_DIR_SCORES;
char *ANGBAND_DIR_ARCHIVE;

static const char *slots[] = {
	#define EQUIP(a, b, c, d, e) #a,
	#include "list-equip-slots.h"
	#undef EQUIP
	NULL
};

const char *list_obj_flag_names[] = {
	"NONE",
	#define OF(a, b) #a,
	#include "list-object-flags.h"
	#undef OF
	NULL
};

const char *list_element_names[] = {
	#define ELEM(a) #a,
	#include "list-elements.h"
	#undef ELEM
	NULL
};

static const char *effect_list[] = {
	"NONE",
	#define EFFECT(x, a, b, c, d, e, f) #x,
	#include "list-effects.h"
	#undef EFFECT
	"MAX"
};

static const char *terrain_flags[] =
{
	#define TF(a, b) #a,
	#include "list-terrain-flags.h"
	#undef TF
    NULL
};

static const char *player_info_flags[] =
{
#define PF(a, b) #a,
	#include "list-player-flags.h"
	#undef PF
	NULL
};

errr grab_effect_data(struct parser *p, struct effect *effect)
{
	const char *type;
	int val;

	if (grab_name("effect", parser_getsym(p, "eff"), effect_list,
				  N_ELEMENTS(effect_list), &val))
		return PARSE_ERROR_INVALID_EFFECT;
	effect->index = val;

	if (parser_hasval(p, "type")) {
		type = parser_getsym(p, "type");

		if (type == NULL)
			return PARSE_ERROR_UNRECOGNISED_PARAMETER;

		/* Check for a value */
		val = effect_subtype(effect->index, type);
		if (val < 0)
			return PARSE_ERROR_INVALID_VALUE;
		else
			effect->subtype = val;
	}

	if (parser_hasval(p, "radius"))
		effect->radius = parser_getint(p, "radius");

	if (parser_hasval(p, "other"))
		effect->other = parser_getint(p, "other");

	return PARSE_ERROR_NONE;
}

/**
 * Find the default paths to all of our important sub-directories.
 *
 * All of the sub-directories should, for a single-user install, be
 * located inside the main directory, whose location is very system-dependent.
 * For shared installations, typically on Unix or Linux systems, the
 * directories may be scattered - see config.h for more info.
 *
 * This function takes buffers, holding the paths to the "config", "lib",
 * and "data" directories (for example, those could be "/etc/angband/",
 * "/usr/share/angband", and "/var/games/angband").  Some system-dependent
 * expansion/substitution may be done when copying those base paths to the
 * paths Angband uses:  see path_process() in z-file.c for details (Unix
 * implementations, for instance, try to replace a leading ~ or ~username with
 * the path to a home directory).
 *
 * Various command line options may allow some of the important
 * directories to be changed to user-specified directories, most
 * importantly, the "scores" and "user" and "save" directories,
 * but this is done after this function, see "main.c".
 *
 * In general, the initial path should end in the appropriate "PATH_SEP"
 * string.  All of the "sub-directory" paths (created below or supplied
 * by the user) will NOT end in the "PATH_SEP" string, see the special
 * "path_build()" function in "util.c" for more information.
 *
 * Hack -- first we free all the strings, since this is known
 * to succeed even if the strings have not been allocated yet,
 * as long as the variables start out as "NULL".  This allows
 * this function to be called multiple times, for example, to
 * try several base "path" values until a good one is found.
 */
void init_file_paths(const char *configpath, const char *libpath, const char *datapath)
{
	char buf[1024];
	char *userpath = NULL;

	/*** Free everything ***/

	/* Free the sub-paths */
	string_free(ANGBAND_DIR_GAMEDATA);
	string_free(ANGBAND_DIR_CUSTOMIZE);
	string_free(ANGBAND_DIR_HELP);
	string_free(ANGBAND_DIR_SCREENS);
	string_free(ANGBAND_DIR_FONTS);
	string_free(ANGBAND_DIR_TILES);
	string_free(ANGBAND_DIR_SOUNDS);
	string_free(ANGBAND_DIR_ICONS);
	string_free(ANGBAND_DIR_USER);
	string_free(ANGBAND_DIR_SAVE);
	string_free(ANGBAND_DIR_PANIC);
	string_free(ANGBAND_DIR_SCORES);
	string_free(ANGBAND_DIR_ARCHIVE);

	/*** Prepare the paths ***/

#define BUILD_DIRECTORY_PATH(dest, basepath, dirname) { \
	path_build(buf, sizeof(buf), (basepath), (dirname)); \
	dest = string_make(buf); \
}

	/* Paths generally containing configuration data for Angband. */
#ifdef GAMEDATA_IN_LIB
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_GAMEDATA, libpath, "gamedata");
#else
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_GAMEDATA, configpath, "gamedata");
#endif
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_CUSTOMIZE, configpath, "customize");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_HELP, libpath, "help");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SCREENS, libpath, "screens");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_FONTS, libpath, "fonts");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_TILES, libpath, "tiles");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SOUNDS, libpath, "sounds");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_ICONS, libpath, "icons");

#ifdef PRIVATE_USER_PATH

	/* Build the path to the user specific directory */
	if (strncmp(ANGBAND_SYS, "test", 4) == 0)
		path_build(buf, sizeof(buf), PRIVATE_USER_PATH, "Test");
	else
		path_build(buf, sizeof(buf), PRIVATE_USER_PATH, VERSION_NAME);
	ANGBAND_DIR_USER = string_make(buf);

#else /* !PRIVATE_USER_PATH */

#ifdef MACH_O_CARBON
	/* Remove any trailing separators, since some deeper path creation functions
	 * don't like directories with trailing slashes. */
	if (suffix(datapath, PATH_SEP)) {
		/* Hacky way to trim the separator. Since this is just for OS X, we can
		 * assume a one char separator. */
		int last_char_index = strlen(datapath) - 1;
		my_strcpy(buf, datapath, sizeof(buf));
		buf[last_char_index] = '\0';
		ANGBAND_DIR_USER = string_make(buf);
	}
	else {
		ANGBAND_DIR_USER = string_make(datapath);
	}
#else /* !MACH_O_CARBON */
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_USER, datapath, "user");
#endif /* MACH_O_CARBON */

#endif /* PRIVATE_USER_PATH */

	/* Build the path to the archive directory. */
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_ARCHIVE, ANGBAND_DIR_USER, "archive");

#ifdef USE_PRIVATE_PATHS
	userpath = ANGBAND_DIR_USER;
#else /* !USE_PRIVATE_PATHS */
	userpath = (char *)datapath;
#endif /* USE_PRIVATE_PATHS */

	/* Build the path to the score and save directories */
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SCORES, userpath, "scores");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SAVE, userpath, "save");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_PANIC, userpath, "panic");

#undef BUILD_DIRECTORY_PATH
}


/**
 * Create any missing directories. We create only those dirs which may be
 * empty (user/, save/, scores/, info/, help/). The others are assumed
 * to contain required files and therefore must exist at startup
 * (edit/, pref/, file/, xtra/).
 *
 * ToDo: Only create the directories when actually writing files.
 */
void create_needed_dirs(void)
{
	char dirpath[512];

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_USER, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_SAVE, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_PANIC, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_SCORES, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_ARCHIVE, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);
}

/**
 * ------------------------------------------------------------------------
 * Initialize game constants
 * ------------------------------------------------------------------------ */

static enum parser_error parse_constants_level_max(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "monsters"))
		z->level_monster_max = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_mon_gen(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "chance"))
		z->alloc_monster_chance = value;
	else if (streq(label, "group-max"))
		z->monster_group_max = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_mon_play(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "mult-rate"))
		z->repro_monster_rate = value;
	else if (streq(label, "mana-cost"))
		z->mana_cost = value;
	else if (streq(label, "mana-max"))
		z->mana_max = value;
	else if (streq(label, "flee-range"))
		z->flee_range = value;
	else if (streq(label, "turn-range"))
		z->turn_range = value;
	else if (streq(label, "hide-range"))
		z->hide_range = value;
	else if (streq(label, "wander-range"))
		z->wander_range = value;
	else if (streq(label, "regen-hp-period"))
		z->mon_regen_hp_period = value;
	else if (streq(label, "regen-sp-period"))
		z->mon_regen_sp_period = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_dun_gen(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "room-max"))
		z->level_room_max = value;
	else if (streq(label, "room-min"))
		z->level_room_min = value;
	else if (streq(label, "block-hgt"))
		z->block_hgt = value;
	else if (streq(label, "block-wid"))
		z->block_wid = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_world(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "dun-depth"))
		z->dun_depth = value;
	else if (streq(label, "max-depth"))
		z->max_depth = value;
	else if (streq(label, "day-length"))
		z->day_length = value;
	else if (streq(label, "dungeon-hgt"))
		z->dungeon_hgt = value;
	else if (streq(label, "dungeon-wid"))
		z->dungeon_wid = value;
	else if (streq(label, "move-energy"))
		z->move_energy = value;
	else if (streq(label, "flow-max"))
		z->flow_max = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_carry_cap(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "pack-size"))
		z->pack_size = value;
	else if (streq(label, "floor-size"))
		z->floor_size = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_obj_make(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "max-depth"))
		z->max_obj_depth = value;
	else if (streq(label, "great-obj"))
		z->great_obj = value;
	else if (streq(label, "great-spec"))
		z->great_ego = value;
	else if (streq(label, "default-torch"))
		z->default_torch = value;
	else if (streq(label, "fuel-torch"))
		z->fuel_torch = value;
	else if (streq(label, "default-lamp"))
		z->default_lamp = value;
	else if (streq(label, "fuel-lamp"))
		z->fuel_lamp = value;
	else if (streq(label, "self-arts"))
		z->self_arts_max = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_player(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "max-sight"))
		z->max_sight = value;
	else if (streq(label, "max-range"))
		z->max_range = value;
	else if (streq(label, "start-exp"))
		z->start_exp = value;
	else if (streq(label, "ability-cost"))
		z->ability_cost = value;
	else if (streq(label, "stealth-bonus"))
		z->stealth_bonus = value;
	else if (streq(label, "regen-period"))
		z->player_regen_period = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_constants(void) {
	struct angband_constants *z = mem_zalloc(sizeof *z);
	struct parser *p = parser_new();

	parser_setpriv(p, z);
	parser_reg(p, "level-max sym label int value", parse_constants_level_max);
	parser_reg(p, "mon-gen sym label int value", parse_constants_mon_gen);
	parser_reg(p, "mon-play sym label int value", parse_constants_mon_play);
	parser_reg(p, "dun-gen sym label int value", parse_constants_dun_gen);
	parser_reg(p, "world sym label int value", parse_constants_world);
	parser_reg(p, "carry-cap sym label int value", parse_constants_carry_cap);
	parser_reg(p, "obj-make sym label int value", parse_constants_obj_make);
	parser_reg(p, "player sym label int value", parse_constants_player);
	return p;
}

static errr run_parse_constants(struct parser *p) {
	return parse_file_quit_not_found(p, "constants");
}

static errr finish_parse_constants(struct parser *p) {
	z_info = parser_priv(p);
	parser_destroy(p);
	return 0;
}

static void cleanup_constants(void)
{
	mem_free(z_info);
}

struct file_parser constants_parser = {
	"constants",
	init_parse_constants,
	run_parse_constants,
	finish_parse_constants,
	cleanup_constants
};

/**
 * Initialize game constants.
 *
 * Assumption: Paths are set up correctly before calling this function.
 */
void init_game_constants(void)
{
	event_signal_message(EVENT_INITSTATUS, 0, "Initializing constants");
	if (run_parser(&constants_parser))
		quit_fmt("Cannot initialize constants.");
}

/**
 * Free the game constants
 */
static void cleanup_game_constants(void)
{
	cleanup_parser(&constants_parser);
}

/**
 * ------------------------------------------------------------------------
 * Intialize world map
 * ------------------------------------------------------------------------ */
static enum parser_error parse_world_level(struct parser *p) {
	const int depth = parser_getint(p, "depth");
	const char *name = parser_getsym(p, "name");
	const char *up = parser_getsym(p, "up");
	const char *down = parser_getsym(p, "down");
	struct level *last = parser_priv(p);
	struct level *lev = mem_zalloc(sizeof *lev);

	if (last) {
		last->next = lev;
	} else {
		world = lev;
	}
	lev->depth = depth;
	lev->name = string_make(name);
	lev->up = streq(up, "None") ? NULL : string_make(up);
	lev->down = streq(down, "None") ? NULL : string_make(down);
	parser_setpriv(p, lev);
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_world(void) {
	struct parser *p = parser_new();

	parser_reg(p, "level int depth sym name sym up sym down",
			   parse_world_level);
	return p;
}

static errr run_parse_world(struct parser *p) {
	return parse_file_quit_not_found(p, "world");
}

static errr finish_parse_world(struct parser *p) {
	struct level *level_check;

	/* Check that all levels referred to exist */
	for (level_check = world; level_check; level_check = level_check->next) {
		struct level *level_find = world;

		/* Check upwards */
		if (level_check->up) {
			while (level_find && !streq(level_check->up, level_find->name)) {
				level_find = level_find->next;
			}
			if (!level_find) {
				quit_fmt("Invalid level reference %s", level_check->up);
			}
		}

		/* Check downwards */
		level_find = world;
		if (level_check->down) {
			while (level_find && !streq(level_check->down, level_find->name)) {
				level_find = level_find->next;
			}
			if (!level_find) {
				quit_fmt("Invalid level reference %s", level_check->down);
			}
		}
	}

	parser_destroy(p);
	return 0;
}

static void cleanup_world(void)
{
	struct level *level = world;
	while (level) {
		struct level *old = level;
		string_free(level->name);
		string_free(level->up);
		string_free(level->down);
		level = level->next;
		mem_free(old);
	}
}

struct file_parser world_parser = {
	"world",
	init_parse_world,
	run_parse_world,
	finish_parse_world,
	cleanup_world
};


/**
 * ------------------------------------------------------------------------
 * Initialize terrain
 * ------------------------------------------------------------------------ */

static enum parser_error parse_feat_code(struct parser *p) {
	const char *code = parser_getstr(p, "code");
	int idx = lookup_feat_code(code);
	struct feature *f;

	if (idx < 0) {
		/*
		 * Of the existing parser errors, PARSE_ERROR_INVALID_VALUE
		 * could alsoe e used; this matches what ui-prefs.c returns
		 * for an unknown feature code or name.
		 */
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	assert(idx < FEAT_MAX);
	f = &f_info[idx];
	f->fidx = idx;
	parser_setpriv(p, f);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (f->name) {
		return PARSE_ERROR_REPEATED_DIRECTIVE;
	}
	f->name = string_make(name);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_graphics(struct parser *p) {
	wchar_t glyph = parser_getchar(p, "glyph");
	const char *color = parser_getsym(p, "color");
	int attr = 0;
	struct feature *f = parser_priv(p);

	if (!f)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	f->d_char = glyph;
	if (strlen(color) > 1)
		attr = color_text_to_attr(color);
	else
		attr = color_char_to_attr(color[0]);
	if (attr < 0)
		return PARSE_ERROR_INVALID_COLOR;
	f->d_attr = attr;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_mimic(struct parser *p) {
	const char *mimic_name = parser_getstr(p, "feat");
	struct feature *f = parser_priv(p);
	int mimic_idx;

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Verify that it refers to a valid feature. */
	mimic_idx = lookup_feat_code(mimic_name);
	if (mimic_idx < 0) {
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	f->mimic = &f_info[mimic_idx];
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_priority(struct parser *p) {
	unsigned int priority = parser_getuint(p, "priority");
	struct feature *f = parser_priv(p);

	if (!f)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	f->priority = priority;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_flags(struct parser *p) {
	struct feature *f = parser_priv(p);
	char *flags, *s;

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (!parser_hasval(p, "flags")) {
		return PARSE_ERROR_NONE;
	}
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(f->flags, TF_SIZE, terrain_flags, s)) {
			break;
		}
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_info(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	f->forge_bonus = parser_getint(p, "bonus");
	f->dig = parser_getint(p, "dig");
	f->pit_difficulty = parser_getint(p, "pit");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_desc(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->desc = string_append(f->desc, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_walk_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->walk_msg = string_append(f->walk_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_run_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->run_msg = string_append(f->run_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_hurt_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->hurt_msg = string_append(f->hurt_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_dig_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->dig_msg = string_append(f->dig_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_fail_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->fail_msg = string_append(f->fail_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_str_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->str_msg = string_append(f->str_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_die_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->die_msg = string_append(f->die_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_confused_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->confused_msg =
		string_append(f->confused_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_look_prefix(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->look_prefix =
		string_append(f->look_prefix, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_look_in_preposition(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->look_in_preposition =
		string_append(f->look_in_preposition, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_feat(void) {
	struct parser *p = parser_new();

	parser_setpriv(p, NULL);
	parser_reg(p, "code str code", parse_feat_code);
	parser_reg(p, "name str name", parse_feat_name);
	parser_reg(p, "graphics char glyph sym color", parse_feat_graphics);
	parser_reg(p, "mimic str feat", parse_feat_mimic);
	parser_reg(p, "priority uint priority", parse_feat_priority);
	parser_reg(p, "flags ?str flags", parse_feat_flags);
	parser_reg(p, "info int bonus int dig int pit", parse_feat_info);
	parser_reg(p, "desc str text", parse_feat_desc);
	parser_reg(p, "walk-msg str text", parse_feat_walk_msg);
	parser_reg(p, "run-msg str text", parse_feat_run_msg);
	parser_reg(p, "hurt-msg str text", parse_feat_hurt_msg);
	parser_reg(p, "dig-msg str text", parse_feat_dig_msg);
	parser_reg(p, "fail-msg str text", parse_feat_fail_msg);
	parser_reg(p, "str-msg str text", parse_feat_str_msg);
	parser_reg(p, "die-msg str text", parse_feat_die_msg);
	parser_reg(p, "confused-msg str text", parse_feat_confused_msg);
	parser_reg(p, "look-prefix str text", parse_feat_look_prefix);
	parser_reg(p, "look-in-preposition str text", parse_feat_look_in_preposition);

	/*
	 * Since the layout of the terrain array is fixed by list-terrain.h,
	 * allocate it now and fill in the customizable parts when parsing.
	 */
	f_info = mem_zalloc(FEAT_MAX * sizeof(*f_info));

	return p;
}

static errr run_parse_feat(struct parser *p) {
	return parse_file_quit_not_found(p, "terrain");
}

static errr finish_parse_feat(struct parser *p) {
	int fidx;

	for (fidx = 0; fidx < FEAT_MAX; ++fidx) {
		/*
		 * Ensure the prefixes and prepositions end with a space for
		 * ease of use with the targeting code.
		 */
		if (f_info[fidx].look_prefix && !suffix(
				f_info[fidx].look_prefix, " ")) {
			f_info[fidx].look_prefix = string_append(
				f_info[fidx].look_prefix, " ");
		}
		if (f_info[fidx].look_in_preposition && !suffix(
				f_info[fidx].look_in_preposition, " ")) {
			f_info[fidx].look_in_preposition =
				string_append(f_info[fidx].look_in_preposition,
				" ");
		}
	}

	parser_destroy(p);
	return 0;
}

static void cleanup_feat(void) {
	int idx;
	for (idx = 0; idx < FEAT_MAX; idx++) {
		string_free(f_info[idx].look_in_preposition);
		string_free(f_info[idx].look_prefix);
		string_free(f_info[idx].confused_msg);
		string_free(f_info[idx].str_msg);
		string_free(f_info[idx].fail_msg);
		string_free(f_info[idx].dig_msg);
		string_free(f_info[idx].die_msg);
		string_free(f_info[idx].hurt_msg);
		string_free(f_info[idx].run_msg);
		string_free(f_info[idx].walk_msg);
		string_free(f_info[idx].desc);
		string_free(f_info[idx].name);
	}
	mem_free(f_info);
}

struct file_parser feat_parser = {
	"terrain",
	init_parse_feat,
	run_parse_feat,
	finish_parse_feat,
	cleanup_feat
};

/**
 * ------------------------------------------------------------------------
 * Intialize player bodies
 * ------------------------------------------------------------------------ */

static enum parser_error parse_body_body(struct parser *p) {
	struct player_body *h = parser_priv(p);
	struct player_body *b = mem_zalloc(sizeof *b);

	b->next = h;
	b->name = string_make(parser_getstr(p, "name"));
	parser_setpriv(p, b);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_body_slot(struct parser *p) {
	struct player_body *b = parser_priv(p);
	struct equip_slot *slot;
	int n;

	if (!b) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the last valid slot, then allocate a new one */
	slot = b->slots;
	if (!slot) {
		b->slots = mem_zalloc(sizeof(struct equip_slot));
		slot = b->slots;
	} else {
		while (slot->next) slot = slot->next;
		slot->next = mem_zalloc(sizeof(struct equip_slot));
		slot = slot->next;
	}

	n = lookup_flag(slots, parser_getsym(p, "slot"));
	if (!n) {
		return PARSE_ERROR_INVALID_FLAG;
	}
	slot->type = n;
	slot->name = string_make(parser_getsym(p, "name"));
	b->count++;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_body(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "body str name", parse_body_body);
	parser_reg(p, "slot sym slot sym name", parse_body_slot);
	return p;
}

static errr run_parse_body(struct parser *p) {
	return parse_file_quit_not_found(p, "body");
}

static errr finish_parse_body(struct parser *p) {
	struct player_body *b;
	int i;
	bodies = parser_priv(p);

	/* Scan the list for the max slots */
	z_info->equip_slots_max = 0;
	for (b = bodies; b; b = b->next) {
		if (b->count > z_info->equip_slots_max)
			z_info->equip_slots_max = b->count;
	}

	/* Allocate the slot list and copy */
	for (b = bodies; b; b = b->next) {
		struct equip_slot *s_new;

		s_new = mem_zalloc(z_info->equip_slots_max * sizeof(*s_new));
		if (b->slots) {
			struct equip_slot *s_temp, *s_old = b->slots;

			/* Allocate space and copy */
			for (i = 0; i < z_info->equip_slots_max; i++) {
				memcpy(&s_new[i], s_old, sizeof(*s_old));
				s_old = s_old->next;
				if (!s_old) break;
			}

			/* Make next point correctly */
			for (i = 0; i < z_info->equip_slots_max; i++)
				if (s_new[i].next)
					s_new[i].next = &s_new[i + 1];

			/* Tidy up */
			s_old = b->slots;
			s_temp = s_old;
			while (s_temp) {
				s_temp = s_old->next;
				mem_free(s_old);
				s_old = s_temp;
			}
		}
		b->slots = s_new;
	}
	parser_destroy(p);
	return 0;
}

static void cleanup_body(void)
{
	struct player_body *b = bodies;
	struct player_body *next;
	int i;

	while (b) {
		next = b->next;
		string_free((char *)b->name);
		for (i = 0; i < b->count; i++)
			string_free((char *)b->slots[i].name);
		mem_free(b->slots);
		mem_free(b);
		b = next;
	}
}

struct file_parser body_parser = {
	"body",
	init_parse_body,
	run_parse_body,
	finish_parse_body,
	cleanup_body
};

/**
 * ------------------------------------------------------------------------
 * Initialize player histories
 * ------------------------------------------------------------------------ */

static struct history_chart *histories;

static struct history_chart *findchart(struct history_chart *hs,
									   unsigned int idx) {
	for (; hs; hs = hs->next)
		if (hs->idx == idx)
			break;
	return hs;
}

static enum parser_error parse_history_chart(struct parser *p) {
	struct history_chart *oc = parser_priv(p);
	struct history_chart *c;
	struct history_entry *e = mem_zalloc(sizeof *e);
	unsigned int idx = parser_getuint(p, "chart");

	if (!(c = findchart(oc, idx))) {
		c = mem_zalloc(sizeof *c);
		c->next = oc;
		c->idx = idx;
		parser_setpriv(p, c);
	}

	e->isucc = parser_getint(p, "next");
	e->roll = parser_getint(p, "roll");

	e->next = c->entries;
	c->entries = e;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_history_phrase(struct parser *p) {
	struct history_chart *h = parser_priv(p);

	if (!h)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	assert(h->entries);
	h->entries->text = string_append(h->entries->text, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_history(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "chart uint chart int next int roll", parse_history_chart);
	parser_reg(p, "phrase str text", parse_history_phrase);
	return p;
}

static errr run_parse_history(struct parser *p) {
	return parse_file_quit_not_found(p, "history");
}

static errr finish_parse_history(struct parser *p) {
	struct history_chart *c;
	struct history_entry *e, *prev, *next;
	histories = parser_priv(p);

	/* Go fix up the entry successor pointers. We can't compute them at
	 * load-time since we may not have seen the successor history yet. Also,
	 * we need to put the entries in the right order; the parser actually
	 * stores them backwards, which is not desirable.
	 */
	for (c = histories; c; c = c->next) {
		e = c->entries;
		prev = NULL;
		while (e) {
			next = e->next;
			e->next = prev;
			prev = e;
			e = next;
		}
		c->entries = prev;
		for (e = c->entries; e; e = e->next) {
			if (!e->isucc)
				continue;
			e->succ = findchart(histories, e->isucc);
			if (!e->succ) {
				return -1;
			}
		}
	}

	parser_destroy(p);
	return 0;
}

static void cleanup_history(void)
{
	struct history_chart *c, *next_c;
	struct history_entry *e, *next_e;

	c = histories;
	while (c) {
		next_c = c->next;
		e = c->entries;
		while (e) {
			next_e = e->next;
			mem_free(e->text);
			mem_free(e);
			e = next_e;
		}
		mem_free(c);
		c = next_c;
	}
}

struct file_parser history_parser = {
	"history",
	init_parse_history,
	run_parse_history,
	finish_parse_history,
	cleanup_history
};

/**
 * ------------------------------------------------------------------------
 * Intialize player sexes
 * ------------------------------------------------------------------------ */

static enum parser_error parse_sex_name(struct parser *p) {
	struct player_sex *h = parser_priv(p);
	struct player_sex *s = mem_zalloc(sizeof *s);

	s->next = h;
	s->name = string_make(parser_getstr(p, "name"));
	parser_setpriv(p, s);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_sex_possess(struct parser *p) {
	struct player_sex *s = parser_priv(p);
	if (!s)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	string_free((char*)s->possessive);
	s->possessive = string_make(parser_getstr(p, "pronoun"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_sex_poetry(struct parser *p) {
	struct player_sex *s = parser_priv(p);
	if (!s)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	string_free((char*)s->poetry_name);
	s->poetry_name = string_make(parser_getstr(p, "name"));
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_sex(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_sex_name);
	parser_reg(p, "possess str pronoun", parse_sex_possess);
	parser_reg(p, "poetry str name", parse_sex_poetry);
	return p;
}

static errr run_parse_sex(struct parser *p) {
	return parse_file_quit_not_found(p, "sex");
}

static errr finish_parse_sex(struct parser *p) {
	struct player_sex *s;
	int num = 0;
	sexes = parser_priv(p);
	for (s = sexes; s; s = s->next) num++;
	for (s = sexes; s; s = s->next, num--) {
		assert(num);
		s->sidx = num - 1;
	}
	parser_destroy(p);
	return 0;
}

static void cleanup_sex(void)
{
	struct player_sex *s = sexes;
	struct player_sex *next;

	while (s) {
		next = s->next;
		string_free((char *)s->poetry_name);
		string_free((char *)s->possessive);
		string_free((char *)s->name);
		mem_free(s);
		s = next;
	}
}

struct file_parser sex_parser = {
	"sex",
	init_parse_sex,
	run_parse_sex,
	finish_parse_sex,
	cleanup_sex
};

/**
 * ------------------------------------------------------------------------
 * Intialize player races
 * ------------------------------------------------------------------------ */

static enum parser_error parse_race_name(struct parser *p) {
	struct player_race *h = parser_priv(p);
	struct player_race *r = mem_zalloc(sizeof *r);

	r->next = h;
	r->name = string_make(parser_getstr(p, "name"));
	/* Default body is humanoid */
	r->body = 0;
	parser_setpriv(p, r);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_race_stats(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->stat_adj[STAT_STR] = parser_getint(p, "str");
	r->stat_adj[STAT_DEX] = parser_getint(p, "dex");
	r->stat_adj[STAT_CON] = parser_getint(p, "con");
	r->stat_adj[STAT_GRA] = parser_getint(p, "gra");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_race_skills(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->skill_adj[SKILL_MELEE] = parser_getint(p, "mel");
	r->skill_adj[SKILL_ARCHERY] = parser_getint(p, "arc");
	r->skill_adj[SKILL_EVASION] = parser_getint(p, "evn");
	r->skill_adj[SKILL_STEALTH] = parser_getint(p, "stl");
	r->skill_adj[SKILL_PERCEPTION] = parser_getint(p, "per");
	r->skill_adj[SKILL_WILL] = parser_getint(p, "wil");
	r->skill_adj[SKILL_SMITHING] = parser_getint(p, "smt");
	r->skill_adj[SKILL_SONG] = parser_getint(p, "sng");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_race_history(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->history = findchart(histories, parser_getuint(p, "hist"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_race_age(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->b_age = parser_getint(p, "base_age");
	r->m_age = parser_getint(p, "mod_age");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_race_height(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->base_hgt = parser_getint(p, "base_hgt");
	r->mod_hgt = parser_getint(p, "mod_hgt");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_race_weight(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->base_wgt = parser_getint(p, "base_wgt");
	r->mod_wgt = parser_getint(p, "mod_wgt");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_race_play_flags(struct parser *p) {
	struct player_race *r = parser_priv(p);
	char *flags;
	char *s;

	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(r->pflags, PF_SIZE, player_info_flags, s))
			break;
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_race_equip(struct parser *p) {
	struct player_race *r = parser_priv(p);
	struct start_item *si;
	int tval, sval;

	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	tval = tval_find_idx(parser_getsym(p, "tval"));
	if (tval < 0)
		return PARSE_ERROR_UNRECOGNISED_TVAL;

	sval = lookup_sval(tval, parser_getsym(p, "sval"));
	if (sval < 0)
		return PARSE_ERROR_UNRECOGNISED_SVAL;

	si = mem_zalloc(sizeof *si);
	si->tval = tval;
	si->sval = sval;
	si->min = parser_getuint(p, "min");
	si->max = parser_getuint(p, "max");

	if (si->min > 99 || si->max > 99) {
		mem_free(si);
		return PARSE_ERROR_INVALID_ITEM_NUMBER;
	}

	si->next = r->start_items;
	r->start_items = si;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_race_desc(struct parser *p) {
	struct player_race *r = parser_priv(p);

	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	r->desc = string_append((char *)r->desc, parser_getstr(p, "desc"));
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_race(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_race_name);
	parser_reg(p, "stats int str int dex int con int gra", parse_race_stats);
	parser_reg(p, "skills int mel int arc int evn int stl int per int wil int smt int sng", parse_race_skills);
	parser_reg(p, "history uint hist", parse_race_history);
	parser_reg(p, "age int base_age int mod_age", parse_race_age);
	parser_reg(p, "height int base_hgt int mod_hgt", parse_race_height);
	parser_reg(p, "weight int base_wgt int mod_wgt", parse_race_weight);
	parser_reg(p, "player-flags ?str flags", parse_race_play_flags);
	parser_reg(p, "equip sym tval sym sval uint min uint max",
			   parse_race_equip);
	parser_reg(p, "desc str desc", parse_race_desc);
	return p;
}

static errr run_parse_race(struct parser *p) {
	return parse_file_quit_not_found(p, "race");
}

static errr finish_parse_race(struct parser *p) {
	struct player_race *r;
	int num = 0;
	races = parser_priv(p);
	for (r = races; r; r = r->next) num++;
	for (r = races; r; r = r->next, num--) {
		assert(num);
		r->ridx = num - 1;
	}
	parser_destroy(p);
	return 0;
}

static void cleanup_race(void)
{
	struct player_race *r = races;
	struct player_race *next;
	struct start_item *item, *item_next;

	while (r) {
		next = r->next;
		item = r->start_items;
		while(item) {
			item_next = item->next;
			mem_free(item);
			item = item_next;
		}
		string_free((char *)r->name);
		string_free((char *)r->desc);
		mem_free(r);
		r = next;
	}
}

struct file_parser race_parser = {
	"race",
	init_parse_race,
	run_parse_race,
	finish_parse_race,
	cleanup_race
};


/**
 * ------------------------------------------------------------------------
 * Initialize player houses
 * ------------------------------------------------------------------------ */

static enum parser_error parse_house_name(struct parser *p) {
	struct player_house *n = parser_priv(p);
	struct player_house *h = mem_zalloc(sizeof *h);
	h->name = string_make(parser_getstr(p, "name"));
	h->next = n;
	parser_setpriv(p, h);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_house_alt_name(struct parser *p) {
	struct player_house *h = parser_priv(p);
	if (!h) return PARSE_ERROR_MISSING_RECORD_HEADER;
	string_free((char*)h->alt_name);
	h->alt_name = string_make(parser_getstr(p, "name"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_house_short_name(struct parser *p) {
	struct player_house *h = parser_priv(p);
	if (!h) return PARSE_ERROR_MISSING_RECORD_HEADER;
	string_free((char*)h->short_name);
	h->short_name = string_make(parser_getstr(p, "name"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_house_race(struct parser *p) {
	struct player_house *h = parser_priv(p);
	struct player_race *r;
	const char *race_name = parser_getstr(p, "name");
	if (!h) return PARSE_ERROR_MISSING_RECORD_HEADER;
	for (r = races; r; r = r->next) {
		if (streq(r->name, race_name)) {
			h->race = r;
			break;
		}
	}
	if (!r) return PARSE_ERROR_INVALID_PLAYER_RACE;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_house_stats(struct parser *p) {
	struct player_house *h = parser_priv(p);

	if (!h)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	h->stat_adj[STAT_STR] = parser_getint(p, "str");
	h->stat_adj[STAT_DEX] = parser_getint(p, "dex");
	h->stat_adj[STAT_CON] = parser_getint(p, "con");
	h->stat_adj[STAT_GRA] = parser_getint(p, "gra");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_house_skills(struct parser *p) {
	struct player_house *h = parser_priv(p);

	if (!h)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	h->skill_adj[SKILL_MELEE] = parser_getint(p, "mel");
	h->skill_adj[SKILL_ARCHERY] = parser_getint(p, "arc");
	h->skill_adj[SKILL_EVASION] = parser_getint(p, "evn");
	h->skill_adj[SKILL_STEALTH] = parser_getint(p, "stl");
	h->skill_adj[SKILL_PERCEPTION] = parser_getint(p, "per");
	h->skill_adj[SKILL_WILL] = parser_getint(p, "wil");
	h->skill_adj[SKILL_SMITHING] = parser_getint(p, "smt");
	h->skill_adj[SKILL_SONG] = parser_getint(p, "sng");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_house_play_flags(struct parser *p) {
	struct player_house *h = parser_priv(p);
	char *flags;
	char *s;

	if (!h)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(h->pflags, PF_SIZE, player_info_flags, s))
			break;
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_house_desc(struct parser *p) {
	struct player_house *h = parser_priv(p);

	if (!h)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	h->desc = string_append((char *)h->desc, parser_getstr(p, "desc"));
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_house(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_house_name);
	parser_reg(p, "alt-name str name", parse_house_alt_name);
	parser_reg(p, "short-name str name", parse_house_short_name);
	parser_reg(p, "race str name", parse_house_race);
	parser_reg(p, "stats int str int dex int con int gra", parse_house_stats);
	parser_reg(p, "skills int mel int arc int evn int stl int per int wil int smt int sng", parse_house_skills);
	parser_reg(p, "player-flags ?str flags", parse_house_play_flags);
	parser_reg(p, "desc str desc", parse_house_desc);
	return p;
}

static errr run_parse_house(struct parser *p) {
	return parse_file_quit_not_found(p, "house");
}

static errr finish_parse_house(struct parser *p) {
	struct player_house *h;
	int num = 0;
	houses = parser_priv(p);
	for (h = houses; h; h = h->next) num++;
	for (h = houses; h; h = h->next, num--) {
		assert(num);
		h->hidx = num - 1;
	}
	parser_destroy(p);
	return 0;
}

static void cleanup_house(void)
{
	struct player_house *h = houses;
	struct player_house *next;

	while (h) {
		next = h->next;
		string_free((char *)h->name);
		string_free((char *)h->alt_name);
		string_free((char *)h->short_name);
		string_free((char *)h->desc);
		mem_free(h);
		h = next;
	}
}

struct file_parser house_parser = {
	"house",
	init_parse_house,
	run_parse_house,
	finish_parse_house,
	cleanup_house
};

/**
 * ------------------------------------------------------------------------
 * Intialize random names
 * ------------------------------------------------------------------------ */

struct name {
	struct name *next;
	char *str;
};

struct names_parse {
	unsigned int section;
	unsigned int nnames[RANDNAME_NUM_TYPES];
	struct name *names[RANDNAME_NUM_TYPES];
};

static enum parser_error parse_names_section(struct parser *p) {
	unsigned int section = parser_getuint(p, "section");
	struct names_parse *s = parser_priv(p);

	if (section >= RANDNAME_NUM_TYPES) {
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	s->section = section;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_names_word(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct names_parse *s = parser_priv(p);
	struct name *ns = mem_zalloc(sizeof *ns);

	s->nnames[s->section]++;
	ns->next = s->names[s->section];
	ns->str = string_make(name);
	s->names[s->section] = ns;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_names(void) {
	struct parser *p = parser_new();
	struct names_parse *n = mem_zalloc(sizeof *n);
	n->section = 0;
	parser_setpriv(p, n);
	parser_reg(p, "section uint section", parse_names_section);
	parser_reg(p, "word str name", parse_names_word);
	return p;
}

static errr run_parse_names(struct parser *p) {
	return parse_file_quit_not_found(p, "names");
}

static errr finish_parse_names(struct parser *p) {
	int i;
	unsigned int j;
	struct names_parse *n = parser_priv(p);
	struct name *nm;
	name_sections = mem_zalloc(sizeof(char**) * RANDNAME_NUM_TYPES);
	for (i = 0; i < RANDNAME_NUM_TYPES; i++) {
		name_sections[i] = mem_alloc(sizeof(char*) * (n->nnames[i] + 1));
		for (nm = n->names[i], j = 0; nm && j < n->nnames[i]; nm = nm->next, j++) {
			name_sections[i][j] = nm->str;
		}
		name_sections[i][n->nnames[i]] = NULL;
		while (n->names[i]) {
			nm = n->names[i]->next;
			mem_free(n->names[i]);
			n->names[i] = nm;
		}
	}
	mem_free(n);
	parser_destroy(p);
	return 0;
}

static void cleanup_names(void)
{
	int i, j;
	for (i = 0; i < RANDNAME_NUM_TYPES; i++) {
		for (j = 0; name_sections[i][j]; j++) {
			string_free((char *)name_sections[i][j]);
		}
		mem_free(name_sections[i]);
	}
	mem_free(name_sections);
}

struct file_parser names_parser = {
	"names",
	init_parse_names,
	run_parse_names,
	finish_parse_names,
	cleanup_names
};

/**
 * ------------------------------------------------------------------------
 * Intialize flavors
 * ------------------------------------------------------------------------ */

static wchar_t flavor_glyph;
static unsigned int flavor_tval;

static enum parser_error parse_flavor_flavor(struct parser *p) {
	struct flavor *h = parser_priv(p);
	struct flavor *f = mem_zalloc(sizeof *f);

	const char *attr;
	int d_attr;

	f->next = h;

	f->fidx = parser_getuint(p, "index");
	f->tval = flavor_tval;
	f->d_char = flavor_glyph;

	if (parser_hasval(p, "sval"))
		f->sval = lookup_sval(f->tval, parser_getsym(p, "sval"));
	else
		f->sval = SV_UNKNOWN;

	attr = parser_getsym(p, "attr");
	if (strlen(attr) == 1)
		d_attr = color_char_to_attr(attr[0]);
	else
		d_attr = color_text_to_attr(attr);

	if (d_attr < 0)
		return PARSE_ERROR_INVALID_COLOR;
	f->d_attr = d_attr;

	if (parser_hasval(p, "desc"))
		f->text = string_append(f->text, parser_getstr(p, "desc"));

	parser_setpriv(p, f);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_flavor_kind(struct parser *p) {
	int tval = tval_find_idx(parser_getsym(p, "tval"));

	if (tval <= 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	flavor_glyph = parser_getchar(p, "glyph");
	flavor_tval = tval;
	return PARSE_ERROR_NONE;
}


static struct parser *init_parse_flavor(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);

	parser_reg(p, "kind sym tval char glyph", parse_flavor_kind);
	parser_reg(p, "flavor uint index sym attr ?str desc", parse_flavor_flavor);
	parser_reg(p, "fixed uint index sym sval sym attr ?str desc", parse_flavor_flavor);

	return p;
}

static errr run_parse_flavor(struct parser *p) {
	return parse_file_quit_not_found(p, "flavor");
}

static errr finish_parse_flavor(struct parser *p) {
	flavors = parser_priv(p);
	parser_destroy(p);
	return 0;
}

static void cleanup_flavor(void)
{
	struct flavor *f, *next;

	f = flavors;
	while(f) {
		next = f->next;
		string_free(f->text);
		mem_free(f);
		f = next;
	}
}

struct file_parser flavor_parser = {
	"flavor",
	init_parse_flavor,
	run_parse_flavor,
	finish_parse_flavor,
	cleanup_flavor
};


/**
 * ------------------------------------------------------------------------
 * Game data initialization
 * ------------------------------------------------------------------------ */

/**
 * A list of all the above parsers, plus those found in mon-init.c and
 * obj-init.c
 */
static struct {
	const char *name;
	struct file_parser *parser;
} pl[] = {
	{ "world", &world_parser },
	{ "projections", &projection_parser },
	{ "features", &feat_parser },
	{ "slays", &slay_parser },
	{ "brands", &brand_parser },
	{ "object bases", &object_base_parser },
	{ "monster pain messages", &pain_parser },
	{ "monster pursuit messages", &pursuit_parser },
	{ "monster warning messages", &warning_parser },
	{ "monster bases", &mon_base_parser },
	{ "summons", &summon_parser },
	{ "objects", &object_parser },
	{ "abilities", &ability_parser },
	{ "ego-items", &ego_parser },
	{ "history charts", &history_parser },
	{ "bodies", &body_parser },
	{ "player races", &race_parser },
	{ "player houses", &house_parser },
	{ "player sexes", &sex_parser },
	{ "artifacts", &artifact_parser },
	{ "drops", &drop_parser },
	{ "object properties", &object_property_parser },
	{ "timed effects", &player_timed_parser },
	{ "blow methods", &meth_parser },
	{ "blow effects", &eff_parser },
	{ "monster spells", &mon_spell_parser },
	{ "monsters", &monster_parser },
	{ "monster lore" , &lore_parser },
	{ "traps", &trap_parser },
	{ "songs", &song_parser },
	{ "chest_traps", &chest_trap_parser },
	{ "flavours", &flavor_parser },
	{ "random names", &names_parser }
};

/**
 * Initialize just the internal arrays.
 * This should be callable by the test suite, without relying on input, or
 * anything to do with a user or savefiles.
 *
 * Assumption: Paths are set up correctly before calling this function.
 */
void init_arrays(void)
{
	unsigned int i;

	for (i = 0; i < N_ELEMENTS(pl); i++) {
		char *msg = string_make(format("Initializing %s...", pl[i].name));
		event_signal_message(EVENT_INITSTATUS, 0, msg);
		string_free(msg);
		if (run_parser(pl[i].parser))
			quit_fmt("Cannot initialize %s.", pl[i].name);
	}
}

/**
 * Free all the internal arrays
 */
static void cleanup_arrays(void)
{
	unsigned int i;

	for (i = 1; i < N_ELEMENTS(pl); i++)
		cleanup_parser(pl[i].parser);

	cleanup_parser(pl[0].parser);
}

static struct init_module arrays_module = {
	.name = "arrays",
	.init = init_arrays,
	.cleanup = cleanup_arrays
};


extern struct init_module z_quark_module;
extern struct init_module generate_module;
extern struct init_module rune_module;
extern struct init_module obj_make_module;
extern struct init_module ignore_module;
extern struct init_module mon_make_module;
extern struct init_module player_module;
extern struct init_module store_module;
extern struct init_module messages_module;
extern struct init_module options_module;
extern struct init_module ui_equip_cmp_module;
extern struct init_module tutorial_module;

static struct init_module *modules[] = {
	&z_quark_module,
	&messages_module,
	&arrays_module,
	&generate_module,
	&player_module,
	&obj_make_module,
	&ignore_module,
	&mon_make_module,
	&options_module,
	&tutorial_module,
	NULL
};

/**
 * Initialise Angband's data stores and allocate memory for structures,
 * etc, so that the game can get started.
 *
 * The only input/output in this file should be via event_signal_string().
 * We cannot rely on any particular UI as this part should be UI-agnostic.
 * We also cannot rely on anything else having being initialised into any
 * particlar state.  Which is why you'd be calling this function in the
 * first place.
 *
 * Old comment, not sure if still accurate:
 * Note that the "graf-xxx.prf" file must be loaded separately,
 * if needed, in the first (?) pass through "TERM_XTRA_REACT".
 */
bool init_angband(void)
{
	int i;

	event_signal(EVENT_ENTER_INIT);

	init_game_constants();

	/* Initialise modules */
	for (i = 0; modules[i]; i++)
		if (modules[i]->init)
			modules[i]->init();

	/* Initialise field-of-fire (should be rewritten, or a module - NRM) */
	(void) vinfo_init();

	/* Initialize some other things */
	event_signal_message(EVENT_INITSTATUS, 0, "Initializing other stuff...");

	/* List display codes */
	monster_list_init();
	object_list_init();

	/* Initialise RNG */
	event_signal_message(EVENT_INITSTATUS, 0, "Getting the dice rolling...");
	Rand_init();

	return true;
}

/**
 * Free all the stuff initialised in init_angband()
 */
void cleanup_angband(void)
{
	int i;

	for (i = 0; modules[i]; i++)
		if (modules[i]->cleanup)
			modules[i]->cleanup();

	event_remove_all_handlers();

	/* Free the main cave */
	if (cave) {
		forget_fire(cave);
		cave_free(cave);
		cave = NULL;
	}

	monster_list_finalize();
	object_list_finalize();

	cleanup_game_constants();

	cmdq_flush();

	if (play_again) return;

	/* Free the format() buffer */
	vformat_kill();

	/* Free the directories */
	string_free(ANGBAND_DIR_GAMEDATA);
	string_free(ANGBAND_DIR_CUSTOMIZE);
	string_free(ANGBAND_DIR_HELP);
	string_free(ANGBAND_DIR_SCREENS);
	string_free(ANGBAND_DIR_FONTS);
	string_free(ANGBAND_DIR_TILES);
	string_free(ANGBAND_DIR_SOUNDS);
	string_free(ANGBAND_DIR_ICONS);
	string_free(ANGBAND_DIR_USER);
	string_free(ANGBAND_DIR_SAVE);
	string_free(ANGBAND_DIR_PANIC);
	string_free(ANGBAND_DIR_SCORES);
	string_free(ANGBAND_DIR_ARCHIVE);
}
