/**
 * \file player-attack.h
 * \brief Attacks (both throwing and melee) by the player
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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

#ifndef PLAYER_ATTACK_H
#define PLAYER_ATTACK_H

#include "cmd-core.h"

struct player;
struct monster;

struct attack_result {
    int hit;
    int dmg;
	int crit_dice;
	bool pierce;
};

/**
 * A list of the different hit types and their associated special message
 */
struct hit_types {
	uint32_t msg_type;
	const char *text;
};

/**
 * Attack types
 */
enum attack_type {
	ATT_MAIN,
	ATT_FLANKING,
	ATT_CONTROLLED_RETREAT,
	ATT_ZONE_OF_CONTROL,
	ATT_OPPORTUNIST,
	ATT_POLEARM,
	ATT_FOLLOW_THROUGH,
	ATT_RIPOSTE,
	ATT_WHIRLWIND,
	ATT_RAGE,
	ATT_OPPORTUNITY
};

/**
 * ranged_attack is a function pointer, used to execute a kind of attack.
 *
 * This allows us to abstract details of throwing, shooting, etc. out while
 * keeping the core projectile tracking, monster cleanup, and display code
 * in common.
 */
typedef struct attack_result (*ranged_attack) (struct player *p,
											   struct object *obj,
											   struct monster *mon,
											   bool undo_rapid,
											   bool attack_penalty,
											   bool one_shot);

extern void do_cmd_fire(struct command *cmd);
extern void do_cmd_fire_quiver1(void);
extern void do_cmd_fire_quiver2(void);
extern void do_cmd_fire_at_nearest(void);
extern void do_cmd_throw(struct command *cmd);
extern void do_cmd_automatic_throw(void);


int prt_after_sharpness(struct player *p, const struct object *obj, int *flag);
void attack_punctuation(char *punctuation, size_t len, int net_dam,
						int crit_bonus_dice);
extern int breakage_chance(const struct object *obj, bool hit_target);
int archery_range(const struct object *bow);
int throwing_range(const struct object *obj);
int chance_of_melee_hit_base(const struct player *p,
	const struct object *weapon);
extern bool test_hit(int to_hit, int ac);
void hit_chance(random_chance *, int, int);
void apply_deadliness(int *die_average, int deadliness);
extern void py_attack(struct player *p, struct loc grid, int attack_type);
extern void py_attack_real(struct player *p, struct loc grid, int attack_type);
void attacks_of_opportunity(struct player *p, struct loc safe);

#endif /* !PLAYER_ATTACK_H */
