/**
 * \file tutorial-init.h
 * \brief Declare interface for parsing tutorial data.
 */

#ifndef INCLUDED_TUTORIAL_INIT_H
#define INCLUDED_TUTORIAL_INIT_H

#include "init.h"
#include "player.h"
#include "h-basic.h"
#include "z-dict.h"
#include "z-rand.h"
struct parser;
struct ability;

enum tutorial_component {
	TUTORIAL_ARCHETYPE,
	TUTORIAL_NOTE,
	TUTORIAL_TRIGGER,
	TUTORIAL_SECTION
};

enum tutorial_item_tweak_kind {
	TWEAK_FLAG,
	TWEAK_SLAY,
	TWEAK_BRAND,
	TWEAK_ELEM_IGNORE,
	TWEAK_ELEM_HATE,
	TWEAK_MODIFIER,
	TWEAK_ELEM_RESIST,
	TWEAK_PVAL
};
struct tutorial_item_tweak {
	char *dice; /**< only used for PVAL */
	random_value value; /**< not used for FLAG, SLAY, BRAND, ELEM_IGNORE,
				ELEM_HATE, PVAL */
	enum tutorial_item_tweak_kind kind;
	int idx; /**< not used for PVAL */
};
struct tutorial_item {
	union {
		const struct artifact *art;
		struct {
			struct ego_item *ego;
			struct tutorial_item_tweak *tweaks;
			random_value number;
			int tval, sval;
			int tweak_count;
		} details;
	} v;
	bool is_artifact;
};
struct tutorial_kit_item {
	struct tutorial_item item;
	bool equipped;
};

struct tutorial_area_flag {
	bitflag flags[SQUARE_SIZE];
	struct loc ul; /**< upper left corner of the area to mark */
	struct loc lr; /**< lower right corner of the area to mark */
	bool clear; /**< if true, clear the indicated flags rather than set them */
};

enum tutorial_section_sym_kind {
	#define TSYM(a, b, c, d) SECTION_SYM_##a,
	#include "list-tutorial-sym.h"
	#undef TSYM
};

/**
 * Apply a layer of type checking for the generic dictionary type to get
 * the symbol table for tutorial section layouts.
 */
struct tutorial_section_sym_table { dict_type d; };
struct tutorial_section_sym_key {
	char symbol[5]; /**< UTF-8 for single code point plus terminating null */
	int x; /**< use -1 when the symbol's location is not set */
	int y; /**< use -1 when the symbol's location is not set */
};
struct tutorial_section_sym_val {
	union {
		struct tutorial_item item;
		struct {
			struct monster_race *race;
			char *note;
			int sleepiness;
			bool sleepiness_fixed;
		} monster;
		struct { struct trap_kind *kind; bool vis; } trap;
		struct { int feat, power; } door;
		struct { int feat, uses; } forge;
		struct { char *dest; char *note; int feat; } gate;
		char *name; /**< for a note, trigger, or starting position;
				for starting position, set when wrapping up
				parsing */
		int feat; /**< most predefined symbols */
	} v;
	bool is_predefined;
	enum tutorial_section_sym_kind kind;
};

enum trigger_op_kind {
	TRIGGER_OP_NONE,
	/* Unary boolean operators */
	TRIGGER_OP_NOT,
	/* Binary boolean operators */
	TRIGGER_OP_AND,
	TRIGGER_OP_OR,
	TRIGGER_OP_XOR,
	/* Boolean primaries */
	TRIGGER_OP_ABILITY,
	TRIGGER_OP_CARRIED,
	TRIGGER_OP_DRAINED,
	TRIGGER_OP_EQUIPPED,
	TRIGGER_OP_FALSE,
	TRIGGER_OP_TIMED,
	TRIGGER_OP_TIMED_ABOVE,
	TRIGGER_OP_TIMED_BELOW,
	TRIGGER_OP_TRUE,
};

struct trigger_compiled_op {
	enum trigger_op_kind kind;
	int tval, sval, idx;
	char *name; /**< ability name for TRIGGER_OP_ABILITY or grade name for
			TRIGGER_OP_TIMED_ABOVE or TRIGGER_OP_TIMED_BELOW */
};

struct trigger_compiled_expr {
	struct trigger_compiled_op *ops;
	int n_op, n_stack;
};

/**
 * Apply a layer of type checking for the generic dictionary type to get
 * something to hold all the tutorial components.
 */
struct tutorial_dict_type { dict_type d; };
struct tutorial_dict_key_type { char *name; enum tutorial_component comp; };
struct tutorial_dict_val_type {
	/** Hold, but do not manage, a pointer to the key. */
	struct tutorial_dict_key_type *key;
	union {
		struct {
			char *race_name;
			char *house_name;
			char *sex_name;
			char *character_name;
			char *history;
			struct ability **added_abilities;
			struct tutorial_kit_item *kit;
			int stat_adj[STAT_MAX];
			int skill_adj[SKILL_MAX];
			int ability_count;
			int ability_alloc;
			int kit_count;
			int kit_alloc;
			int32_t unspent_experience;
			bool purge_kit;
		} archetype;
		struct { char *text; int pval; } note;
		struct {
			char *text;
			char *death_note_name;
			struct trigger_compiled_expr expr;
			bool changes_death_note;
		} trigger;
		struct {
			char *start_note_name;
			char *death_note_name;
			char **lines;
			struct tutorial_area_flag *area_flags;
			struct tutorial_section_sym_table symt;
			int rows, columns;
			int area_flag_count, area_flag_alloc;
		} section;
	} v;
};

struct tutorial_parsed_result {
	struct tutorial_dict_type d;
	struct tutorial_dict_val_type *default_archetype;
	struct tutorial_dict_val_type *default_section;
	struct tutorial_dict_val_type **pval_to_note_table;
	struct tutorial_dict_val_type ***trigger_gate_map;
	struct object_kind *note_kind;
	char *curr_death_note;
	int note_table_n, note_table_a;
};

struct tutorial_parser_priv {
	struct tutorial_parsed_result *r;
	struct tutorial_dict_val_type *curr_value;
	int section_lines_parsed;
};


extern struct tutorial_parsed_result tutorial_parsed_data;
extern struct init_module tutorial_module;


void tutorial_parse_data(void);
void tutorial_cleanup_parsed_data(void);

/* These are public so they can be shared by tutorial.c and tutorial-init.c. */
struct tutorial_section_sym_val *tutorial_section_sym_table_has(
		struct tutorial_section_sym_table t,
		const struct tutorial_section_sym_key *key);
struct tutorial_dict_val_type *tutorial_dict_has(struct tutorial_dict_type d,
		const struct tutorial_dict_key_type *key);
bool tutorial_text_escaped(const char *cursor, const char *limit);
size_t tutorial_copy_strip_escapes(char *dest, size_t sz, const char *src,
		size_t rd);
void tutorial_cleanup_trigger_gate_map(struct tutorial_dict_val_type ***m);

/* These are public only to facilitate writing test cases. */
struct parser *tutorial_init_parser(void);
errr tutorial_finish_parser(struct parser *p);

#endif /* INCLUDED_TUTORIAL_INIT_H */
