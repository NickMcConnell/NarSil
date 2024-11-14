/**
 * \file mon-util.h
 * \brief Functions for monster utilities.
 *
 * Copyright (c) 1997-2007 Ben Harrison, James E. Wilson, Robert A. Koeneke
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

#ifndef MONSTER_UTILITIES_H
#define MONSTER_UTILITIES_H

#include "monster.h"
#include "mon-msg.h"

const char *describe_race_flag(int flag);
void create_mon_flag_mask(bitflag *f, ...);
struct monster_race *lookup_monster(const char *name);
struct monster_base *lookup_monster_base(const char *name);
bool match_monster_bases(const struct monster_base *base, ...);
void monster_opportunist_or_zone(struct player *p, struct loc grid_to);
void monster_swap(struct loc grid1, struct loc grid2);
void monster_wake(struct monster *mon, bool notify, int aware_chance);
bool monster_can_see(struct chunk *c, struct monster *mon, struct loc grid);
void update_smart_learn(struct monster *mon, struct player *p, int flag,
						int pflag, int element);
void monsters_hear(bool player_centered, bool main_roll, int difficulty);
int32_t adjusted_mon_exp(const struct monster_race *race, bool kill);
int mon_create_drop_count(const struct monster_race *race, bool maximize);
void drop_loot(struct chunk *c, struct monster *mon, struct loc grid,
			   bool stats);
void monster_death(struct monster *mon, struct player *p, bool by_player,
				   const char *note, bool stats);
bool mon_take_nonplayer_hit(int dam, struct monster *t_mon,
							enum mon_messages hurt_msg,
							enum mon_messages die_msg);
bool mon_take_hit(struct monster *mon, struct player *p, int dam,
	const char *note);
bool similar_monsters(struct monster *mon1, struct monster *mon2);
void scare_onlooking_friends(const struct monster *mon, int amount);
bool monster_carry(struct chunk *c, struct monster *mon, struct object *obj);

#endif /* MONSTER_UTILITIES_H */
