/**
 * \file object.h
 * \brief basic object structs and enums
 */
#ifndef INCLUDED_OBJECT_H
#define INCLUDED_OBJECT_H

#include "z-type.h"
#include "z-quark.h"
#include "z-bitflag.h"
#include "z-dice.h"
#include "obj-properties.h"


/*** Game constants ***/

/**
 * Elements
 */
enum
{
	#define ELEM(a) ELEM_##a,
	#include "list-elements.h"
	#undef ELEM

	ELEM_MAX
};

#define ELEM_BASE_MIN  ELEM_ACID
#define ELEM_BASE_MAX  (ELEM_COLD + 1)
#define ELEM_HIGH_MIN  ELEM_POIS
#define ELEM_HIGH_MAX  (ELEM_DISEN + 1)

/**
 * Object origin kinds
 */

enum {
	#define ORIGIN(a, b, c) ORIGIN_##a,
	#include "list-origins.h"
	#undef ORIGIN

	ORIGIN_MAX
};


/*** Structures ***/

/**
 * Structure for possible object kinds for an ability or special item
 */
struct poss_item {
	uint32_t kidx;
	struct poss_item *next;
};

#define MAX_PREREQS 10

/**
 * Effect
 */
struct effect {
	struct effect *next;
	uint16_t index;	/**< The effect index */
	dice_t *dice;	/**< Dice expression used in the effect */
	int subtype;	/**< Projection type, timed effect type, etc. */
	int radius;		/**< Radius of the effect (if it has one) */
	int other;		/**< Extra parameter to be passed to the handler */
	char *msg;		/**< Message for death or whatever */
};

/**
 * Chests
 */
struct chest_trap {
	struct chest_trap *next;
	char *name;
	char *code;
	uint8_t flag;
	struct effect *effect;
	bool destroy;
	char *msg;
	char *msg_save;
	char *msg_bad;
	char *msg_death;
};

/**
 * Brand type
 */
struct brand {
	char *code;
	char *name;
	char *desc;
	int resist_flag;
	int vuln_flag;
	int dice;
	int vuln_dice;
	int smith_difficulty;
	struct brand *next;
};

/**
 * Slay type
 */
struct slay {
	char *code;
	char *name;
	int race_flag;
	int dice;
	int smith_difficulty;
	struct slay *next;
};

/**
 * Curse type
 */
struct curse {
	struct curse *next;
	char *name;
	bool *poss;
	struct object *obj;
	char *conflict;
	bitflag conflict_flags[OF_SIZE];
	char *desc;
};

enum {
	EL_INFO_HATES = 0x01,
	EL_INFO_IGNORE = 0x02,
	EL_INFO_RANDOM = 0x04,
};

/**
 * Element info type
 */
struct element_info {
	int16_t res_level;
	bitflag flags;
};

/**
 * Allocation structure
 */
struct allocation {
	struct allocation *next;
	uint8_t locale;
	uint8_t chance;
};

extern struct activation *activations;

/**
 * Information about object types, like rods, wands, etc.
 */
struct object_base {
	char *name;

	int tval;
	struct object_base *next;

	int attr;

	bitflag flags[OF_SIZE];
	bitflag kind_flags[KF_SIZE];			/**< Kind flags */
	struct element_info el_info[ELEM_MAX];

	bool smith_attack_valid;
	int smith_attack_artistry;
	int smith_attack_artefact;
	bitflag smith_flags[OF_SIZE];
	struct element_info smith_el_info[ELEM_MAX];
    int smith_modifiers[OBJ_MOD_MAX];
	bool *smith_slays;
	bool *smith_brands;

	int break_perc;
	int max_stack;
	int num_svals;
};

extern struct object_base *kb_info;

/**
 * Information about object kinds, including player knowledge.
 *
 * TODO: split out the user-changeable bits into a separate struct so this
 * one can be read-only.
 */
struct object_kind {
	char *name;
	char *text;

	struct object_base *base;

	struct object_kind *next;
	uint32_t kidx;

	int tval;					/**< General object type (see TV_ macros) */
	int sval;					/**< Object sub-type  */

	int pval;					/* Item extra-parameter */
	random_value special1;		/* Special parameter 1 */
	int special2;				/* Special parameter 1 */

	int att;					/**< Bonus to hit */
	int evn;					/**< Base armor */
	int dd;						/**< Damage dice */
	int ds;						/**< Damage sides */
	int pd;						/**< Damage dice */
	int ps;						/**< Damage sides */
	int weight;					/**< Weight, in 1/10lbs */

	int cost;					/**< Object base cost */

	bitflag flags[OF_SIZE];					/**< Flags */
	bitflag kind_flags[KF_SIZE];			/**< Kind flags */

	random_value modifiers[OBJ_MOD_MAX];
	struct element_info el_info[ELEM_MAX];

	bool *brands;
	bool *slays;

	uint8_t d_attr;			/**< Default object attribute */
	wchar_t d_char;			/**< Default object character */

	struct allocation *alloc;	/**< Allocation levels and chances */

	struct effect *effect;	/**< Effect this item produces (effects.c) */
	char *effect_msg;
	struct effect *thrown_effect;/**< Effect for thrown potions */
	struct ability *abilities;	    /* Abilities */

	int level;				/**< Level (difficulty of activation) */

	random_value charge;	/**< Number of charges (staves/wands) */

	int gen_mult_prob;		/**< Probability of generating more than one */
	random_value stack_size;/**< Number to generate */

	struct flavor *flavor;	/**< Special object flavor (or zero) */

	/** Also saved in savefile **/

	quark_t note_aware; 	/**< Autoinscription quark number */
	quark_t note_unaware; 	/**< Autoinscription quark number */

	bool aware;		/**< Set if player is aware of the kind's effects */
	bool tried;		/**< Set if kind has been tried */

	uint8_t ignore;  	/**< Ignore settings */
	bool everseen; 	/**< Kind has been seen (to despoilify ignore menus) */
};

extern struct object_kind *k_info;
extern struct object_kind *unknown_item_kind;
extern struct object_kind *unknown_gold_kind;
extern struct object_kind *pile_kind;
extern struct object_kind *curse_object_kind;

enum artifact_category {
	ARTIFACT_NORMAL,
	ARTIFACT_SELF_MADE,
	ARTIFACT_ULTIMATE,
	ARTIFACT_CATEGORY_MAX
};

/**
 * Unchanging information about artifacts.
 */
struct artifact {
	char *name;
	char *text;

	uint32_t aidx;
	enum artifact_category category;

	struct artifact *next;

	int tval;		/**< General artifact type (see TV_ macros) */
	int sval;		/**< Artifact sub-type  */
	int pval;		/**< Artifact power value  */

	int16_t att;			/* Bonus to attack */
	int16_t evn;			/* Bonus to evasion */
	uint8_t dd;		/**< Number of damage dice */
	uint8_t ds;		/**< Number of sides on each damage die */
	uint8_t pd;		/**< Number of protection dice */
	uint8_t ps;		/**< Number of sides on each protection die */

	int weight;	/**< Weight in 1/10lbs */

	int cost;		/**< Artifact (pseudo-)worth */

	bitflag flags[OF_SIZE];			/**< Flags */

	int modifiers[OBJ_MOD_MAX];
	struct element_info el_info[ELEM_MAX];

	bool *brands;
	bool *slays;

	struct ability *abilities;	    /* Abilities */

	uint8_t level;			/* Artefact level */
	uint8_t rarity;		/* Artefact rarity */
};

/**
 * Information about artifacts that changes during the course of play;
 * except for aidx, saved to the save file
 */
struct artifact_upkeep {
	uint32_t aidx;	/**< For cross-indexing with struct artifact */
	bool created;	/**< Whether this artifact has been created */
	bool seen;	/**< Whether this artifact has been seen this game */
	bool everseen;	/**< Whether this artifact has ever been seen  */
};

/**
 * The artifact arrays
 */
extern struct artifact *a_info;
extern struct artifact_upkeep *aup_info;


/**
 * Information about special items.
 */
struct ego_item {
	struct ego_item *next;

	char *name;
	char *text;

	uint32_t eidx;

	int cost;						/* Ego-item "cost" */

	bitflag flags[OF_SIZE];			/**< Flags */
	bitflag kind_flags[KF_SIZE];	/**< Kind flags */

	int modifiers[OBJ_MOD_MAX];
	int min_modifiers[OBJ_MOD_MAX];
	struct element_info el_info[ELEM_MAX];

	bool *brands;
	bool *slays;

	int rarity; 		/** Chance of being generated (i.e. rarity) */
	int level;			/** Minimum depth (can appear earlier) */
	int alloc_max;			/** Maximum depth (will NEVER appear deeper) */

	struct poss_item *poss_items;

	struct ability *abilities;	    /* Abilities */
	
	uint8_t att;		/* Maximum to-hit bonus */
	uint8_t dd;			/* bonus damge dice */
	uint8_t ds;			/* bonus damage sides */
	uint8_t evn;		/* Maximum to-e bonus */
	uint8_t pd;			/* bonus protection dice */
	uint8_t ps;			/* bonus protection sides */
	uint8_t pval;		/* Maximum pval */

	bool aware;				/* Has its type been detected this game? */
	bool everseen;			/* Do not spoil ignore menus */
};

/**
 * The ego-item array
 */
extern struct ego_item *e_info;

/**
 * Object drop types
 */
struct drop {
	struct drop *next;
	char *name;
	int idx;
	bool chest;
	struct poss_item *poss;
	struct poss_item *imposs;
};

/**
 * The drop array
 */
extern struct drop *drops;

/**
 * Flags for the obj->notice field
 */
enum {
	OBJ_NOTICE_SENSE = 0x01,
	OBJ_NOTICE_EMPTY = 0x02,
	OBJ_NOTICE_KNOWN = 0x04,
	OBJ_NOTICE_CURSED = 0x08,
	OBJ_NOTICE_BROKEN = 0x10,
	OBJ_NOTICE_SPOIL = 0x20,
	OBJ_NOTICE_IGNORE = 0x40,
	OBJ_NOTICE_PICKUP = 0x80,
};

/**
 * Values for the obj->pseudo field
 */
enum {
	OBJ_PSEUDO_NONE = 0,
	OBJ_PSEUDO_AVERAGE,
	OBJ_PSEUDO_CURSED_ART,
	OBJ_PSEUDO_CURSED_SPEC,
	OBJ_PSEUDO_CURSED,
	OBJ_PSEUDO_SPECIAL,
	OBJ_PSEUDO_ARTEFACT,
	OBJ_PSEUDO_UNCURSED,
	OBJ_PSEUDO_MAX,
};

/**
 * Object information, for a specific object.
 *
 * Note that inscriptions are now handled via the "quark_str()" function
 * applied to the "note" field, which will return NULL if "note" is zero.
 *
 * Each cave grid points to one (or zero) objects via the "obj" field in
 * its "squares" struct.  Each object then points to one (or zero) objects
 * via the "next" field, and (aside from the first) back via its "prev"
 * field, forming a doubly linked list, which in game terms represents a
 * stack of objects in the same grid.
 *
 * Each monster points to one (or zero) objects via the "held_obj"
 * field (see monster.h).  Each object then points to one (or zero) objects
 * and back to previous objects by its own "next" and "prev" fields,
 * forming a doubly linked list, which in game terms represents the
 * monster's inventory.
 *
 * The "held_m_idx" field is used to indicate which monster, if any,
 * is holding the object.  Objects being held have (0, 0) as a grid.
 *
 * Note that object records are not now copied, but allocated on object
 * creation and freed on object destruction.  These records are handed
 * around between player and monster inventories and the floor on a fairly
 * regular basis, and care must be taken when handling such objects.
 */
struct object {
	struct object_kind *kind;	/**< Kind of the object */
	struct object_kind *image_kind;	/**< Hallucination kind of the object */
	struct ego_item *ego;		/**< Ego item info of the object, if any */
	const struct artifact *artifact; /**< Artifact info of the object, if any */

	struct object *prev;	/**< Previous object in a pile */
	struct object *next;	/**< Next object in a pile */

	uint16_t oidx;		/**< Item list index, if any */

	struct loc grid;	/**< position on map, or (0, 0) */

	uint8_t tval;		/**< Item type (from kind) */
	uint8_t sval;		/**< Item sub-type (from kind) */

	int16_t pval;		/**< Item extra-parameter */

	int16_t weight;		/**< Item weight */

	int16_t att;			/* Bonus to attack */
	int16_t evn;			/* Bonus to evasion */
	uint8_t dd;		/**< Number of damage dice */
	uint8_t ds;		/**< Number of sides on each damage die */
	uint8_t pd;		/**< Number of protection dice */
	uint8_t ps;		/**< Number of sides on each protection die */

	bitflag flags[OF_SIZE];	/**< Object flags */
	int16_t modifiers[OBJ_MOD_MAX];	/**< Object modifiers*/
	struct element_info el_info[ELEM_MAX];	/**< Object element info */
	bool *brands;			/**< Flag absence/presence of each brand */
	bool *slays;			/**< Flag absence/presence of each slay */

	int16_t timeout;		/**< Timeout Counter */
	uint8_t used;			/**< Times used (for staffs) */

	uint8_t number;			/**< Number of items */
	bitflag notice;			/**< Sil - ID status */
	uint8_t pseudo;			/**< Sil - pseudo-id status */

	int16_t held_m_idx;		/**< Monster holding us (if any) */

	bool marked;			/**< Object is marked */

	uint8_t origin;			/**< How this item was found */
	uint8_t origin_depth;		/**< What depth the item was found at */
	struct monster_race *origin_race;	/**< Monster race that dropped it */

	quark_t note; 			/**< Inscription index */

	struct ability *abilities;	    /**< Object abilities */
};

/**
 * Null object constant, for safe initialization.
 */
static struct object const OBJECT_NULL = {
	.kind = NULL,
	.image_kind = NULL,
	.ego = NULL,
	.artifact = NULL,
	.prev = NULL,
	.next = NULL,
	.oidx = 0,
	.grid = { 0, 0 },
	.tval = 0,
	.sval = 0,
	.pval = 0,
	.weight = 0,
	.att = 0,
	.evn = 0,
	.dd = 0,
	.ds = 0,
	.pd = 0,
	.ps = 0,
	.flags = { 0 },
	.modifiers = { 0 },
	.el_info = { { 0, 0 } },
	.brands = NULL,
	.slays = NULL,
	.timeout = 0,
	.used = 0,
	.number = 0,
	.notice = 0,
	.held_m_idx = 0,
	.origin = 0,
	.origin_depth = 0,
	.origin_race = NULL,
	.note = 0,
	.abilities = NULL,
};

struct flavor
{
	char *text;
	struct flavor *next;
	unsigned int fidx;

	uint8_t tval;	/* Associated object type */
	uint8_t sval;	/* Associated object sub-type */

	uint8_t d_attr;	/* Default flavor attribute */
	wchar_t d_char;	/* Default flavor character */
};

extern struct flavor *flavors;


typedef bool (*item_tester)(const struct object *);


#endif /* !INCLUDED_OBJECT_H */
