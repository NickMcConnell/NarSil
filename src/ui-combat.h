/**
 * \file ui-combat.h
 * \brief Printing of combat roll information
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

#ifndef UI_COMBAT_H
#define UI_COMBAT_H

/**
 * Types of attack for combat rolls
 */
enum combat_roll_type {
	COMBAT_ROLL_NONE = 1,
	COMBAT_ROLL_ROLL = 2,
	COMBAT_ROLL_AUTO = 3
};

#define MAX_COMBAT_ROLLS 50

/**
 * Information on a combat roll for printing
 */
struct combat_roll {
	int att_type;			/* The type of attack - enum combat_roll_type */
	int dam_type;			/* The type of damage (GF_HURT, GF_FIRE etc) */

	wchar_t attacker_char;		/* The symbol of the attacker */
	uint8_t attacker_attr;		/* Default attribute of the attacker */
	wchar_t defender_char;		/* The symbol of the defender */
	uint8_t defender_attr;		/* Default attribute of the defender */
	int att;				/* The attack bonus */
	int att_roll;			/* The attack roll (d20 value) */
	int evn;				/* The evasion bonus */
	int evn_roll;			/* The evasion roll (d20 value */
	
	int dd;					/* The number of damage dice */
	int ds;					/* The number of damage sides */
	int dam;				/* The total damage rolled */
	int pd;					/* The number of protection dice */
	int ps;					/* The number of protection sides */
	int prot;				/* The total protection rolled */

	int prt_percent;		/* The percentage of protection that is effective
							 * (eg 100 normally) */
	bool melee;				/* Was it a melee attack? (used for working out
							 * if blocking is effective) */
};

void new_combat_round(game_event_type type, game_event_data *data, void *user);
void update_combat_rolls_attack(game_event_type type, game_event_data *data,
								void *user);
void update_combat_rolls_damage(game_event_type type, game_event_data *data,
								void *user);
void display_combat_rolls(game_event_type type, game_event_data *data,
								void *user);

#endif /* !UI_COMBAT_H */
