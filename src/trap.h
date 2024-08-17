/**
 * \file trap.h 
 * \brief trap predicates, structs and functions
 */

#ifndef TRAP_H
#define TRAP_H

/*** Trap flags ***/

enum
{
	#define TRF(a,b) TRF_##a,
	#include "list-trap-flags.h"
	#undef TRF
	TRF_MAX
};

#define TRF_SIZE                FLAG_SIZE(TRF_MAX)

#define trf_has(f, flag)        flag_has_dbg(f, TRF_SIZE, flag, #f, #flag)
#define trf_next(f, flag)       flag_next(f, TRF_SIZE, flag)
#define trf_is_empty(f)         flag_is_empty(f, TRF_SIZE)
#define trf_is_full(f)          flag_is_full(f, TRF_SIZE)
#define trf_is_inter(f1, f2)    flag_is_inter(f1, f2, TRF_SIZE)
#define trf_is_subset(f1, f2)   flag_is_subset(f1, f2, TRF_SIZE)
#define trf_is_equal(f1, f2)    flag_is_equal(f1, f2, TRF_SIZE)
#define trf_on(f, flag)         flag_on_dbg(f, TRF_SIZE, flag, #f, #flag)
#define trf_off(f, flag)        flag_off(f, TRF_SIZE, flag)
#define trf_wipe(f)             flag_wipe(f, TRF_SIZE)
#define trf_setall(f)           flag_setall(f, TRF_SIZE)
#define trf_negate(f)           flag_negate(f, TRF_SIZE)
#define trf_copy(f1, f2)        flag_copy(f1, f2, TRF_SIZE)
#define trf_union(f1, f2)       flag_union(f1, f2, TRF_SIZE)
#define trf_inter(f1, f2)       flag_inter(f1, f2, TRF_SIZE)
#define trf_diff(f1, f2)        flag_diff(f1, f2, TRF_SIZE)


/* Types of glyph */
enum {
	GLYPH_NONE,
	GLYPH_WARDING
};

/**
 * A trap template.
 */
struct trap_kind
{
	char *name;					/**< Name  */
	char *text;					/**< Text  */
	char *desc;					/**< Short description  */
	char *msg;					/**< Message on hitting */
	char *msg2;					/**< 2nd message on hitting */
	char *msg3;					/**< 3rd message on hitting */
	char *msg_vis;				/**< Message on hitting, only when visible */
	char *msg_silence;			/**< Message on hitting with song of silence */
	char *msg_good;				/**< Message on saving */
	char *msg_bad;				/**< Message on failing to save */
	char *msg_xtra;				/**< Message on getting an extra effect */

	struct trap_kind *next;
	int tidx;					/**< Trap kind index */

	uint8_t d_attr;				/**< Default trap attribute */
	wchar_t d_char;				/**< Default trap character */

	int rarity;					/**< Rarity */
	int min_depth;				/**< Minimum depth */
	int max_depth;				/**< Maximum depth */
	int power;					/**< Power of trap (multiple uses) */
	int stealth;				/**< change to player's stealh score when hit */

	bitflag flags[TRF_SIZE];	/**< Trap flags (all traps of this kind) */

	struct effect *effect;		/**< Effect on entry to grid */
	struct effect *effect_xtra;	/**< Possible extra effect */
};

extern struct trap_kind *trap_info;

/**
 * An actual trap.
 */
struct trap
{
	uint8_t t_idx;			/**< Trap kind index */
	struct trap_kind *kind;		/**< Trap kind */
	struct trap *next;		/**< Next trap in this location */

	struct loc grid;		/**< Location of trap */

	uint8_t power;			/**< Power for locks/jams, disarm diff for traps */

	bitflag flags[TRF_SIZE];	/**< Trap flags (only this particular trap) */
};

extern struct file_parser trap_parser;

struct trap_kind *lookup_trap(const char *desc);
bool square_trap_specific(struct chunk *c, struct loc grid, int t_idx);
bool square_trap_flag(struct chunk *c, struct loc grid, int flag);
bool square_reveal_trap(struct chunk *c, struct loc grid, bool domsg);
void square_memorize_traps(struct chunk *c, struct loc grid);
void hit_trap(struct loc grid);
bool check_hit(int power, bool display_roll, struct source against);
bool square_player_trap_allowed(struct chunk *c, struct loc grid);
void place_trap(struct chunk *c, struct loc grid, int t_idx, int trap_level);
void square_free_trap(struct chunk *c, struct loc grid);
void wipe_trap_list(struct chunk *c);
bool square_remove_all_traps(struct chunk *c, struct loc grid);
bool square_remove_trap(struct chunk *c, struct loc grid, int t_idx);
bool square_set_trap_timeout(struct chunk *c, struct loc grid, bool domsg,
							 int t_idx, int time);
int square_trap_timeout(struct chunk *c, struct loc grid, int t_idx);
void square_set_door_lock(struct chunk *c, struct loc grid, int power);
int square_door_lock_power(struct chunk *c, struct loc grid);
void square_set_door_jam(struct chunk *c, struct loc grid, int power);
int square_door_jam_power(struct chunk *c, struct loc grid);
void square_set_forge(struct chunk *c, struct loc grid, int uses);
int square_forge_uses(struct chunk *c, struct loc grid);

#endif /* !TRAP_H */
