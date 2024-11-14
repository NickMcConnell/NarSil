/**
 * \file player-util.h
 * \brief Player utility functions
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2014 Nick McConnell
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

#ifndef PLAYER_UTIL_H
#define PLAYER_UTIL_H

#include "cave.h"
#include "cmd-core.h"
#include "player.h"

/* Player regeneration constants */
#define PY_REGEN_NORMAL		197		/* Regen factor*2^16 when full */
#define PY_REGEN_WEAK		98		/* Regen factor*2^16 when weak */
#define PY_REGEN_FAINT		33		/* Regen factor*2^16 when fainting */
#define PY_REGEN_HPBASE		1442	/* Min amount hp regen*2^16 */
#define PY_REGEN_MNBASE		524		/* Min amount mana regen*2^16 */

/* Player over-exertion */
enum {
	PY_EXERT_NONE = 0x00,
	PY_EXERT_CON = 0x01,
	PY_EXERT_FAINT = 0x02,
	PY_EXERT_SCRAMBLE = 0x04,
	PY_EXERT_CUT = 0x08,
	PY_EXERT_CONF = 0x10,
	PY_EXERT_HALLU = 0x20,
	PY_EXERT_SLOW = 0x40,
	PY_EXERT_HP = 0x80
};

/**
 * Special values for the number of turns to rest, these need to be
 * negative numbers, as postive numbers are taken to be a turncount,
 * and zero means "not resting". 
 */
enum {
	REST_COMPLETE = -2,
	REST_ALL_POINTS = -1,
	REST_SOME_POINTS = -3
};

/**
 * Stealth mode off, on, or stopping. 
 */
enum {
	STEALTH_MODE_OFF = 0,
	STEALTH_MODE_STOPPING = 1,
	STEALTH_MODE_ON = 2
};

/**
 * Minimum number of turns required for regeneration to kick in during resting.
 */
#define REST_REQUIRED_FOR_REGEN 5

int player_min_depth(struct player *p);
int dungeon_get_next_level(struct player *p, int dlev, int added);
void dungeon_change_level(struct player *p, int dlev);
int int_exp(int base, int power);
void take_hit(struct player *p, int dam, const char *kb_str);
void death_knowledge(struct player *p);
int energy_per_move(struct player *p);
void player_regen_hp(struct player *p);
void player_regen_mana(struct player *p);
void convert_mana_to_hp(struct player *p, int32_t sp);
void player_digest(struct player *p);
void player_update_light(struct player *p);
struct object *player_best_digger(struct player *p, bool forbid_stack);
bool player_radiates(struct player *p);
void player_fall_in_pit(struct player *p, bool spiked);
void player_falling_damage(struct player *p, bool stun);
void player_fall_in_chasm(struct player *p);
void player_flanking_or_retreat(struct player *p, struct loc grid);
void player_opportunist_or_zone(struct player *p, struct loc grid1,
								struct loc grid2, bool opp_only);
void player_polearm_passive_attack(struct player *p, struct loc grid_from,
								   struct loc grid_to);
bool player_can_leap(struct player *p, struct loc grid, int dir);
bool player_break_web(struct player *p);
bool player_escape_pit(struct player *p);
void player_blast_ceiling(struct player *p);
void player_blast_floor(struct player *p);
int lookup_skill(const char *name);
bool player_action_is_movement(struct player *p, int n);
int player_dodging_bonus(struct player *p);
bool player_can_riposte(struct player *p, int hit_result);
bool player_is_sprinting(struct player *p);
int player_bane_type_killed(int bane_type);
int calc_bane_bonus(struct player *p);
int player_bane_bonus(struct player *p, struct monster *mon);
int player_spider_bane_bonus(struct player *p);
bool player_can_fire(struct player *p, bool show_msg);
bool player_can_fire_quiver1(struct player *p, bool show_msg);
bool player_can_fire_quiver2(struct player *p, bool show_msg);
bool player_can_refuel(struct player *p, bool show_msg);
bool player_can_fire_prereq(void);
bool player_can_fire_quiver1_prereq(void);
bool player_can_fire_quiver2_prereq(void);
bool player_can_refuel_prereq(void);
bool player_can_debug_prereq(void);
bool player_can_save_prereq(void);
bool player_confuse_dir(struct player *p, int *dir, bool too);
bool player_resting_is_special(int16_t count);
bool player_is_resting(const struct player *p);
int16_t player_resting_count(const struct player *p);
void player_resting_set_count(struct player *p, int16_t count);
void player_resting_cancel(struct player *p, bool disturb);
bool player_resting_can_regenerate(const struct player *p);
void player_resting_step_turn(struct player *p);
void player_resting_complete_special(struct player *p);
int player_get_resting_repeat_count(struct player *p);
void player_set_resting_repeat_count(struct player *p, int16_t count);
bool player_resists(const struct player *p, int element);
void player_place(struct chunk *c, struct player *p, struct loc grid);
void disturb(struct player *p, bool stop_stealth);
void search(struct player *p);
void perceive(struct player *p);

#endif /* !PLAYER_UTIL_H */
