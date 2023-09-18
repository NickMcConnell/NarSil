/**
 * \file player.h
 * \brief Player implementation
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2011 elly+angband@leptoquark.net. See COPYING.
 * Copyright (c) 2015 Nick McConnell
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

#ifndef PLAYER_H
#define PLAYER_H

#include "guid.h"
#include "obj-properties.h"
#include "object.h"
#include "option.h"

/**
 * Indexes of the player stats (hard-coded by savefiles).
 */
enum {
	#define STAT(a) STAT_##a,
	#include "list-stats.h"
	#undef STAT

	STAT_MAX
};

/**
 * Player race and class flags
 */
enum
{
	#define PF(a, b) PF_##a,
	#include "list-player-flags.h"
	#undef PF
	PF_MAX
};

#define PF_SIZE                FLAG_SIZE(PF_MAX)

#define pf_has(f, flag)        flag_has_dbg(f, PF_SIZE, flag, #f, #flag)
#define pf_next(f, flag)       flag_next(f, PF_SIZE, flag)
#define pf_is_empty(f)         flag_is_empty(f, PF_SIZE)
#define pf_is_full(f)          flag_is_full(f, PF_SIZE)
#define pf_is_inter(f1, f2)    flag_is_inter(f1, f2, PF_SIZE)
#define pf_is_subset(f1, f2)   flag_is_subset(f1, f2, PF_SIZE)
#define pf_is_equal(f1, f2)    flag_is_equal(f1, f2, PF_SIZE)
#define pf_on(f, flag)         flag_on_dbg(f, PF_SIZE, flag, #f, #flag)
#define pf_off(f, flag)        flag_off(f, PF_SIZE, flag)
#define pf_wipe(f)             flag_wipe(f, PF_SIZE)
#define pf_setall(f)           flag_setall(f, PF_SIZE)
#define pf_negate(f)           flag_negate(f, PF_SIZE)
#define pf_copy(f1, f2)        flag_copy(f1, f2, PF_SIZE)
#define pf_union(f1, f2)       flag_union(f1, f2, PF_SIZE)
#define pf_inter(f1, f2)       flag_inter(f1, f2, PF_SIZE)
#define pf_diff(f1, f2)        flag_diff(f1, f2, PF_SIZE)

/**
 * The range of possible indexes into tables based upon stats.
 * Currently things range from 3 to 18/220 = 40.
 */
#define STAT_RANGE 38

/**
 * The internal minimum and  maximum for a given stat.
 */
#define BASE_STAT_MIN -9
#define BASE_STAT_MAX 20

/**
 * Player constants
 */
#define PY_MAX_EXP		99999999L	/* Maximum exp */
#define PY_KNOW_LEVEL	30			/* Level to know all runes */
#define PY_MAX_LEVEL	50			/* Maximum level */

/**
 * Flags for player.spell_flags[]
 */
#define PY_SPELL_LEARNED    0x01 	/* Spell has been learned */
#define PY_SPELL_WORKED     0x02 	/* Spell has been successfully tried */
#define PY_SPELL_FORGOTTEN  0x04 	/* Spell has been forgotten */

#define BTH_PLUS_ADJ    	3 		/* Adjust BTH per plus-to-hit */

/**
 * Ways in which players can be marked as cheaters
 */
#define NOSCORE_WIZARD		0x0002
#define NOSCORE_DEBUG		0x0008
#define NOSCORE_JUMPING     0x0010

/**
 * Action types (for remembering what the player did)
 */
enum {
	ACTION_NOTHING = 0,
	ACTION_NW = 7,
	ACTION_N = 8,
	ACTION_NE = 9,
	ACTION_W = 4,
	ACTION_STAND = 5,
	ACTION_E = 6,
	ACTION_SW = 1,
	ACTION_S = 2,
	ACTION_SE = 3,
	ACTION_MISC = 10
};

/**
 * Number of actions stored
 */
#define MAX_ACTION 6

/**
 * Terrain that the player has a chance of digging through
 */
enum {
	DIGGING_RUBBLE = 0,
	DIGGING_MAGMA,
	DIGGING_QUARTZ,
	DIGGING_GRANITE,
	DIGGING_DOORS,

	DIGGING_MAX
};

/**
 * Skill indices
 */
enum {
	#define SKILL(a, b) SKILL_##a,
	#include "list-skills.h"
	#undef SKILL

	SKILL_MAX
};

/**
 * Structure for the "quests"
 */
struct quest
{
	struct quest *next;
	uint8_t index;
	char *name;
	uint8_t level;			/* Dungeon level */
	struct monster_race *race;	/* Monster race */
	int cur_num;			/* Number killed (unused) */
	int max_num;			/* Number required (unused) */
};

struct song {
	struct song *next;
	char *name;
	char *verb;
	char *desc;
	char *msg;
	struct alt_song_desc *alt_desc;
	int index;
	int bonus_mult;
	int bonus_div;
	int bonus_min;
	int noise;
	bool extend;
	struct effect *effect;
};

enum song_index {
	SONG_MAIN,
	SONG_MINOR,
	SONG_MAX
};

/**
 * A single equipment slot
 */
struct equip_slot {
	struct equip_slot *next;

	uint16_t type;
	char *name;
	struct object *obj;
};

/**
 * A player 'body'
 */
struct player_body {
	struct player_body *next;

	char *name;
	uint16_t count;
	struct equip_slot *slots;
};

/**
 * Items the player starts with.  Used in player_race and specified in race.txt.
 */
struct start_item {
	int tval;	/**< General object type (see TV_ macros) */
	int sval;	/**< Object sub-type  */
	int min;	/**< Minimum starting amount */
	int max;	/**< Maximum starting amount */
	struct start_item *next;
};

/**
 * Player sex info
 */
struct player_sex {
	struct player_sex *next;
	const char *name;			/* Type of sex */
	const char *possessive;		/* Possessive pronoun */
	const char *poetry_name;	/* Name of entry poetry file */
	unsigned int sidx;
};

/**
 * Player race info
 */
struct player_race {
	struct player_race *next;
	const char *name;
	const char *desc;

	unsigned int ridx;

	int b_age;		/**< Base age */
	int m_age;		/**< Mod age */
	int base_hgt;	/**< Base height */
	int mod_hgt;	/**< Mod height */
	int base_wgt;	/**< Base weight */
	int mod_wgt;	/**< Mod weight */

	struct start_item *start_items; /**< Starting inventory */

	int body;		/**< Race body */

	int stat_adj[STAT_MAX];		/**< Stat modifiers */
	int skill_adj[SKILL_MAX];	/**< Skill modifiers */

	bitflag pflags[PF_SIZE];	/**< Racial (player) flags */

	struct history_chart *history;
};

/**
 * Player house info
 */
struct player_house {
	struct player_house *next;
	struct player_race *race;
	const char *name;
	const char *alt_name;
	const char *short_name;
	const char *desc;
	unsigned int hidx;

	int stat_adj[STAT_MAX];		/**< Stat modifiers */
	int skill_adj[SKILL_MAX];	/**< Skill modifiers */

	bitflag pflags[PF_SIZE];	/**< House (player) flags */
};

/**
 * Info for player abilities
 */
struct player_ability {
	struct player_ability *next;
	uint16_t index;			/* PF_*, OF_* or element index */
	char *type;			/* Ability type */
	char *name;			/* Ability name */
	char *desc;			/* Ability description */
	int group;			/* Ability group (set locally when viewing) */
	int value;			/* Resistance value for elements */
};

/**
 * Info for status of a player's abilities
 */
struct ability_info {
	bool innate;
	bool active;
};

/**
 * Histories are a graph of charts; each chart contains a set of individual
 * entries for that chart, and each entry contains a text description and a
 * successor chart to move history generation to.
 * For example:
 * 	chart 1 {
 * 		entry {
 * 			desc "You are the illegitimate and unacknowledged child";
 * 			next 2;
 * 		};
 * 		entry {
 * 			desc "You are the illegitimate but acknowledged child";
 * 			next 2;
 * 		};
 * 		entry {
 * 			desc "You are one of several children";
 * 			next 3;
 * 		};
 * 	};
 *
 * History generation works by walking the graph from the starting chart for
 * each race, picking a random entry (with weighted probability) each time.
 */
struct history_entry {
	struct history_entry *next;
	struct history_chart *succ;
	int isucc;
	int roll;
	char *text;
};

struct history_chart {
	struct history_chart *next;
	struct history_entry *entries;
	unsigned int idx;
};

/**
 * Player history information
 *
 * See player-history.c/.h
 */
struct player_history {
	struct history_info *entries;	/**< List of entries */
	size_t next;					/**< First unused entry */
	size_t length;					/**< Current length */
};

/**
 * All the variable state that changes when you put on/take off equipment.
 * Player flags are not currently variable, but useful here so monsters can
 * learn them.
 */
struct player_state {
	int stat_equip_mod[STAT_MAX];	/**< Equipment stat bonuses */
	int stat_misc_mod[STAT_MAX];	/**< Misc stat bonuses */
	int stat_use[STAT_MAX];			/**< Current modified stats */

	int skill_stat_mod[SKILL_MAX];	/**< Stat skill bonuses */
	int skill_equip_mod[SKILL_MAX];	/**< Equipment skill bonuses */
	int skill_misc_mod[SKILL_MAX];	/**< Misc skill bonuses */
	int skill_use[SKILL_MAX];		/**< Current modified skills */

	int speed;			/**< Current speed */
	int hunger;			/**< Current hunger rate */

	int ammo_tval;		/**< Ammo variety */

	int to_mdd;		/* Bonus to melee damage dice */
	int mdd;		/* Total melee damage dice */
	int to_mds;		/* Bonus to melee damage sides */
	int mds;		/* Total melee damage sides */
	
	int offhand_mel_mod;	/* Modifier to off-hand melee score
							 * (relative to main hand) */
	int mdd2;			/* Total melee damage dice for off-hand weapon */
	int to_ads;			/* Bonus to archery damage sides */
	int mds2;			/* Total melee damage sides for off-hand weapon */

	int add;			/* Total archery damage dice */
	int ads;			/* Total archery damage sides */

	int p_min;	/* minimum protection roll, to test for changes to it */
	int p_max;	/* maximum protection roll, to test for changes to it */

	int dig;			/* Digging ability */

	int16_t flags[OF_MAX];				/**< Status flags from race and items */
	bitflag pflags[PF_SIZE];				/**< Player intrinsic flags */
	struct element_info el_info[ELEM_MAX];	/**< Resists from race and items */
};

#define player_has(p, flag)       (pf_has(p->state.pflags, (flag)))

/**
 * Temporary, derived, player-related variables used during play but not saved
 *
 * XXX Some of these probably should go to the UI
 */
struct player_upkeep {
	bool leaping;           /* Player is currently in the air */
	bool riposte;			/* Player has used a riposte */
	bool was_entranced;		/* Player has just woken up from entrancement */
	bool knocked_back;		/* Player was knocked back */

	bool playing;			/* True if player is playing */
	bool autosave;			/* True if autosave is pending */
	bool generate_level;	/* True if level needs regenerating */
	bool dropping;			/* True if auto-drop is in progress */

	int energy_use;			/* Energy use this turn */

	int cur_light;		/**< Radius of light (if any) */

	struct monster *health_who;			/* Health bar trackee */
	struct monster_race *monster_race;	/* Monster race trackee */
	struct object *object;				/* Object trackee */
	struct object_kind *object_kind;	/* Object kind trackee */

	uint32_t notice;		/* Bit flags for pending actions such as
							 * reordering inventory, ignoring, etc. */
	uint32_t update;		/* Bit flags for recalculations needed
							 * such as HP, or visible area */
	uint32_t redraw;		/* Bit flags for things that /have/ changed,
							 * and just need to be redrawn by the UI,
							 * such as HP, Speed, etc.*/

	int command_wrk;		/* Used by the UI to decide whether
							 * to start off showing equipment or
							 * inventory listings when offering
							 * a choice.  See obj-ui.c */

	int create_stair;	/* Stair to create on next level */
	bool create_rubble;	/* Create rubble on next level */
	bool force_forge;		/* Force the generation of a forge on this level */

	int smithing;			/* Smithing counter */
	int resting;			/* Resting counter */
	int running;				/* Running counter */
	bool running_withpathfind;	/* Are we using the pathfinder ? */
	bool running_firststep;		/* Is this our first step running? */

	struct object **inven;	/* Inventory objects */
	int total_weight;		/* Total weight being carried */
	int inven_cnt;			/* Number of items in inventory */
	int equip_cnt;			/* Number of items in equipment */
	int recharge_pow;		/* Power of recharge effect */
};

/**
 * Most of the "player" information goes here.
 *
 * This stucture gives us a large collection of player variables.
 *
 * This entire structure is wiped when a new character is born.
 *
 * This structure is more or less laid out so that the information
 * which must be saved in the savefile precedes all the information
 * which can be recomputed as needed.
 */
struct player {
	const struct player_sex *sex;
	const struct player_race *race;
	const struct player_house *house;

	struct loc grid;	/* Player location */

	int16_t game_type;		/* Whether this is a normal game (=0), tutorial (<0), puzzle (>0) */

	int16_t age;		/* Characters age */
	int16_t ht;		/* Height */
	int16_t wt;		/* Weight */
	int16_t sc;		/* Social class */

	int16_t max_depth;	/* Max depth */
	int16_t depth;		/* Cur depth */

	int32_t new_exp;	/* New experience */
	int32_t exp;		/* Current experience */
	int32_t turn;		/* Player turn */

	int32_t encounter_exp;	/* Total experience from ecountering monsters */
	int32_t kill_exp;		/* Total experience from killing monsters */
	int32_t descent_exp;	/* Total experience from descending to new levels */
	int32_t ident_exp;		/* Total experience from identifying objects */

	int16_t mhp;		/* Max hit pts */
	int16_t chp;		/* Cur hit pts */
	uint16_t chp_frac;	/* Cur hit frac (times 2^16) */

	int16_t msp;		/* Max mana pts */
	int16_t csp;		/* Cur mana pts */
	uint16_t csp_frac;	/* Cur mana frac (times 2^16) */

	int16_t stat_base[STAT_MAX];	/* The base ('internal') stat values */
	int16_t stat_drain[STAT_MAX];	/* The negative modifier from stat drain */
	int16_t skill_base[SKILL_MAX];	/* The base skill values */

	struct ability *abilities;		/* Player innate abilities */
	struct ability *item_abilities;	/* Player item abilities */

	int16_t last_attack_m_idx;	/* Index of the monster attacked last round */
	int16_t consecutive_attacks;/* Rounds spent attacking this monster */
	int16_t bane_type;			/* Monster type you have specialized against */
	uint8_t previous_action[MAX_ACTION];/* Previous actions you have taken */
	bool attacked;				/* Has the player attacked anyone this round? */
	bool been_attacked;			/* Has anyone attacked the player? */
	bool focused;				/* Currently focusing for an attack */

	int16_t *timed;				/* Timed effects */

	int16_t energy;				/* Current energy */
	uint32_t total_energy;			/* Total energy used (including resting) */
	uint32_t resting_turn;			/* Number of player turns spent resting */

	int16_t food;				/* Current nutrition */

	uint16_t stairs_taken;	/* The number of times stairs have been used */
	uint16_t staircasiness;	/* Increases on taking stairs and slowly decays */

	uint16_t forge_drought;	/* Number of turns since a forge was generated */
	uint16_t forge_count;	/* The number of forges that have been generated */

	uint8_t stealth_mode;	/* Stealth mode */

	uint8_t self_made_arts;	/* Number of self-made artefacts so far */

	struct song *song[SONG_MAX];	/* Current songs */
	int16_t wrath;			/* The counter for the song of slaying */
	int16_t song_duration;	/* The duration of the current song */
	int16_t stealth_score;	/* Modified stealth_score for this round */

	int16_t smithing_leftover; /* Turns needed to finish the current item */
	bool unique_forge_made; /* Has the unique forge been generated */
	bool unique_forge_seen; /* Has the unique forge been encountered */

	bool *vaults;				/* Which greater vaults have been generated? */
	uint8_t num_artefacts;		/* Number of artefacts generated so far */

	uint8_t unignoring;			/* Unignoring */

	char full_name[PLAYER_NAME_LEN];	/* Full name */
	char died_from[80];					/* Cause of death */
	char *history;						/* Player history */
	bool truce;				/* Player will not be attacked initially at 1000ft */
	bool crown_hint;		/* Player has been told about the Iron Crown */
	bool crown_shatter;		/* Player has had a weapon shattered by the Crown */
	bool cursed;			/* Player has taken a third Silmaril */
	bool on_the_run;		/* Player is on the run from Angband */
	bool morgoth_slain;		/* Player has slain Morgoth */
	uint8_t morgoth_hits;		/* Number of big hits against Morgoth */
	bool escaped;			/* Player has escaped Angband */

	uint16_t noscore;			/* Cheating flags */

	bool is_dead;				/* Player is dead */

	bool wizard;				/* Player is in wizard mode */
	bool automaton;         /* Player is AI controlled? */

	int16_t player_hp[PY_MAX_LEVEL];	/* HP gained per level */

	/* Saved values for quickstart */
	int32_t au_birth;			/* Birth gold when option birth_money is false */
	int16_t stat_birth[STAT_MAX];		/* Birth "natural" stat values */
	int16_t ht_birth;			/* Birth Height */
	int16_t wt_birth;			/* Birth Weight */

	struct player_options opts;			/* Player options */
	struct player_history hist;			/* Player history (see player-history.c) */

	struct player_body body;			/* Equipment slots available */

	struct object *gear;				/* Real gear */


	struct player_state state;			/* Calculatable state */
	struct player_state known_state;	/* What the player can know of the above */
	struct player_upkeep *upkeep;		/* Temporary player-related values */
};


/**
 * ------------------------------------------------------------------------
 * Externs
 * ------------------------------------------------------------------------ */

extern struct player_body *bodies;
extern struct player_race *races;
extern struct player_sex *sexes;
extern struct player_house *houses;
extern struct player_ability *player_abilities;

extern const int32_t player_exp[PY_MAX_LEVEL];
extern struct player *player;

/* player.c */
struct player_race *player_id2race(guid id);
struct player_house *player_id2house(guid id);
struct player_house *player_house_from_count(int idx);
struct player_sex *player_id2sex(guid id);

int stat_name_to_idx(const char *name);
const char *stat_idx_to_name(int type);
bool player_stat_inc(struct player *p, int stat);
bool player_stat_res(struct player *p, int stat, int points);
void player_stat_dec(struct player *p, int stat);
void player_exp_gain(struct player *p, int32_t amount);
void player_exp_lose(struct player *p, int32_t amount);
void player_flags(struct player *p, bitflag f[OF_SIZE]);
void player_flags_timed(struct player *p, bitflag f[OF_SIZE]);
uint8_t player_hp_attr(struct player *p);
uint8_t player_sp_attr(struct player *p);
bool player_restore_mana(struct player *p, int amt);
size_t player_random_name(char *buf, size_t buflen);
void player_safe_name(char *safe, size_t safelen, const char *name, bool strip_suffix);
void player_cleanup_members(struct player *p);

#endif /* !PLAYER_H */
