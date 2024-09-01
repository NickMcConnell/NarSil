/**
 * \file init.h
 * \brief initialization
 *
 * Copyright (c) 2000 Robert Ruehlmann
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */

#ifndef INCLUDED_INIT_H
#define INCLUDED_INIT_H

#include "h-basic.h"
#include "z-bitflag.h"
#include "z-file.h"
#include "z-rand.h"
#include "datafile.h"
#include "object.h"

/**
 * Information about maximal indices of certain arrays.
 *
 * This will become a list of "all" the game constants - NRM
 */
struct angband_constants
{
	/* Array bounds etc, set on parsing edit files */
	uint16_t trap_max;	/**< Maximum number of trap kinds */
	uint16_t k_max;		/**< Maximum number of object base kinds */
	uint16_t drop_max;	/**< Maximum number of object drop types */
	uint16_t a_max;		/**< Maximum number of artifact kinds */
	uint16_t e_max;		/**< Maximum number of ego-item kinds */
	uint16_t r_max;		/**< Maximum number of monster races */
	uint16_t pain_max;	/**< Maximum number of monster pain message sets */
	uint16_t pursuit_max;/**< Maximum number of monster pursuit message sets */
	uint16_t warning_max;/**< Maximum number of monster warning message sets */
	uint16_t s_max;		/**< Maximum number of magic spells */
	uint16_t v_max;		/**< Maximum number of vault templates */
	uint16_t pit_max;	/**< Maximum number of monster pit types */
	uint16_t act_max;	/**< Maximum number of activations for randarts */
	uint8_t curse_max;	/**< Maximum number of curses */
	uint8_t slay_max;	/**< Maximum number of slays */
	uint8_t brand_max;	/**< Maximum number of brands */
	uint16_t mon_blows_max;	/**< Maximum number of monster blows */
	uint16_t blow_methods_max;	/**< Maximum number of monster blow methods */
	uint16_t blow_effects_max;	/**< Maximum number of monster blow effects */
	uint16_t equip_slots_max;	/**< Maximum number of player equipment slots */
	uint16_t profile_max;	/**< Maximum number of cave_profiles */
	uint16_t quest_max;	/**< Maximum number of quests */
	uint16_t projection_max;	/**< Maximum number of projection types */
	uint16_t calculation_max;	/**< Maximum number of object power calculations */
	uint16_t property_max;	/**< Maximum number of object properties */
	uint16_t ordinary_kind_max;	/**< Maximum number of ordinary object kinds */
	uint16_t obj_alloc_max;	/**< Maximum number of object allocations */

	/* Maxima of things on a given level, read from constants.txt */
	uint16_t level_monster_max;	/**< Maximum number of monsters on a given level */

	/* Monster generation constants, read from constants.txt */
	uint16_t alloc_monster_chance;	/**< 1/per-turn-chance of generation */
	uint16_t level_monster_min;	/**< Minimum number generated */
	uint16_t town_monsters_day;	/**< Townsfolk generated - day */
	uint16_t town_monsters_night;	/**< Townsfolk generated  - night */
	uint16_t repro_monster_max;	/**< Maximum breeders on a level */
	uint16_t ood_monster_chance;	/**< Chance of OoD monster is 1 in this */
	uint16_t ood_monster_amount;	/**< Max number of levels OoD */
	uint16_t monster_group_max;	/**< Maximum size of a group */
	uint16_t monster_group_dist;	/**< Max dist of a group from a related group */

	/* Monster gameplay constants, read from constants.txt */
	uint16_t glyph_hardness;	/**< How hard for a monster to break a glyph */
	uint16_t repro_monster_rate;	/**< Monster reproduction rate-slower */
	uint16_t mana_cost;			/**< Mana it costs a monster to cast a  spell */
	uint16_t mana_max;			/**< Maximum amount of mana a monster can have*/
	uint16_t flee_range;		/**< Monsters run this many grids out of view */
	uint16_t turn_range;		/**< Monsters turn to fight closer than this */
	uint16_t hide_range;		/**< Monsters look for safety this far away */
	uint16_t wander_range;		/**< Monsters wander this far */
	uint16_t mon_regen_hp_period;/**< Monster turns for complete regeneration */
	uint16_t mon_regen_sp_period;/**< Monster turns for complete regeneration */

	/* Dungeon generation constants, read from constants.txt */
	uint16_t level_room_max;	/**< Maximum number of rooms on a level */
	uint16_t level_room_min;	/**< Minimum number of rooms on a level */
	uint16_t block_hgt;			/**< Height of each dungeon block */
	uint16_t block_wid;			/**< Width of each dungeon block */

	/* World shape constants, read from constants.txt */
	uint16_t dun_depth;	/* Maximum dungeon level */
	uint16_t max_depth;	/* Maximum generation level */
	uint16_t day_length;	/* Number of turns from dawn to dawn */
	uint16_t dungeon_hgt;	/**< Maximum number of vertical grids on a level */
	uint16_t dungeon_wid;	/**< Maximum number of horizontical grids on a level */
	uint16_t move_energy;	/* Energy the player or monster needs to move */
	uint16_t flow_max;		/* Maximum distance measured in a flow */

	/* Carrying capacity constants, read from constants.txt */
	uint16_t pack_size;		/**< Maximum number of pack slots */
	uint16_t quiver_size;		/**< Maximum number of quiver slots */
	uint16_t quiver_slot_size;	/**< Maximum number of missiles per quiver slot */
	uint16_t thrown_quiver_mult;	/**< Size multiplier for non-ammo in quiver */
	uint16_t floor_size;		/**< Maximum number of items per floor grid */

	/* Object creation constants, read from constants.txt */
	uint16_t max_obj_depth;	/* Maximum depth used in object allocation */
	uint16_t great_obj;	/* 1/chance of inflating the requested object level */
	uint16_t great_ego;	/* 1/chance of inflating the requested ego item level */
	uint16_t default_torch;	/* Default amount of fuel in a torch  */
	uint16_t fuel_torch;	/* Maximum amount of fuel in a torch */
	uint16_t default_lamp;	/* Default amount of fuel in a lantern  */
	uint16_t fuel_lamp;		/* Maximum amount of fuel in a lantern */
	uint16_t self_arts_max;	/* Maximum number of self-made artefacts */

	/* Player constants, read from constants.txt */
	uint16_t max_sight;	/* Maximum visual range */
	uint16_t max_range;	/* Maximum missile and spell range */
	uint16_t start_exp;	/* Amount of experience the player starts with */
	uint16_t ability_cost;	/* Base experience cost of an ability */
	uint16_t stealth_bonus;	/* Bonus to stealth in stealth mode */
	uint16_t player_regen_period;	/* Player turns for complete regeneration */
};

struct init_module {
	const char *name;
	void (*init)(void);
	void (*cleanup)(void);
};

extern bool play_again;

extern const char *list_element_names[];
extern const char *list_obj_flag_names[];

extern struct angband_constants *z_info;

extern const char *ANGBAND_SYS;

extern char *ANGBAND_DIR_GAMEDATA;
extern char *ANGBAND_DIR_CUSTOMIZE;
extern char *ANGBAND_DIR_HELP;
extern char *ANGBAND_DIR_SCREENS;
extern char *ANGBAND_DIR_FONTS;
extern char *ANGBAND_DIR_TILES;
extern char *ANGBAND_DIR_SOUNDS;
extern char *ANGBAND_DIR_ICONS;
extern char *ANGBAND_DIR_USER;
extern char *ANGBAND_DIR_SAVE;
extern char *ANGBAND_DIR_PANIC;
extern char *ANGBAND_DIR_SCORES;
extern char *ANGBAND_DIR_INFO;
extern char *ANGBAND_DIR_ARCHIVE;

extern struct parser *init_parse_artifact(void);
extern struct parser *init_parse_race(void);
extern struct parser *init_parse_house(void);
extern struct parser *init_parse_sex(void);
extern struct parser *init_parse_ego(void);
extern struct parser *init_parse_feat(void);
extern struct parser *init_parse_history(void);
extern struct parser *init_parse_object(void);
extern struct parser *init_parse_object_base(void);
extern struct parser *init_parse_pain(void);
extern struct parser *init_parse_p_race(void);
extern struct parser *init_parse_pit(void);
extern struct parser *init_parse_monster(void);
extern struct parser *init_parse_vault(void);
extern struct parser *init_parse_constants(void);
extern struct parser *init_parse_flavor(void);
extern struct parser *init_parse_names(void);
extern struct parser *init_parse_hints(void);
extern struct parser *init_parse_trap(void);
extern struct parser *init_parse_chest_trap(void);
extern struct parser *init_parse_quest(void);

extern struct file_parser flavor_parser;

errr grab_effect_data(struct parser *p, struct effect *effect);
extern void init_file_paths(const char *config, const char *lib, const char *data);
extern void init_game_constants(void);
extern void init_arrays(void);
extern void create_needed_dirs(void);
extern bool init_angband(void);
extern void cleanup_angband(void);

#endif /* INCLUDED_INIT_H */
