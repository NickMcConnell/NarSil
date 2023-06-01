/**
 * \file game-event.h
 * \brief Allows the registering of handlers to be told about game events.
 *
 * Copyright (c) 2007 Antony Sidwell
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

#ifndef INCLUDED_GAME_EVENT_H
#define INCLUDED_GAME_EVENT_H

#include "source.h"
#include "z-type.h"
struct textblock;

/**
 * The various events we can send signals about.
 */
typedef enum game_event_type
{
	EVENT_MAP = 0,		/* Some part of the map has changed. */

	EVENT_NAME,			/* Name. */
	EVENT_STATS,  		/* One or more of the stats. */
	EVENT_SKILLS,  		/* One or more of the skills. */
	EVENT_HP,	   		/* HP or MaxHP. */
	EVENT_MANA,			/* Mana or MaxMana. */
	EVENT_MELEE,
	EVENT_ARCHERY,
	EVENT_ARMOR,
	EVENT_EXPERIENCE,	/* Experience or MaxExperience. */
	EVENT_EXP_CHANGE,	/* Experience or MaxExperience. */
	EVENT_SONG,			/* Player's singing. */
	EVENT_MONSTERHEALTH,	/* Observed monster's health level. */
	EVENT_DUNGEONLEVEL,	/* Dungeon depth */
	EVENT_PLAYERSPEED,	/* Player's speed */
	EVENT_RACE_CLASS,	/* Race or Class */
	EVENT_STATUS,		/* Status */
	EVENT_LIGHT,		/* Light level */
	EVENT_STATE,		/* The two 'R's: Resting and Repeating */

	EVENT_PLAYERMOVED,
	EVENT_SEEFLOOR,         /* When the player would "see" floor objects */
	EVENT_EXPLOSION,
	EVENT_BOLT,
	EVENT_MISSILE,
	EVENT_HIT,

	EVENT_INVENTORY,
	EVENT_EQUIPMENT,
	EVENT_ITEMLIST,
	EVENT_MONSTERLIST,
	EVENT_MONSTERTARGET,
	EVENT_OBJECTTARGET,
	EVENT_MESSAGE,
	EVENT_COMBAT_RESET,
	EVENT_COMBAT_ATTACK,
	EVENT_COMBAT_DAMAGE,
	EVENT_COMBAT_DISPLAY,
	EVENT_SOUND,
	EVENT_BELL,
	EVENT_USE_STORE,
	EVENT_STORECHANGED,	/* Triggered on a successful buy/retrieve or sell/drop */	

	EVENT_INPUT_FLUSH,
	EVENT_MESSAGE_FLUSH,
	EVENT_CHECK_INTERRUPT,
	EVENT_REFRESH,
	EVENT_NEW_LEVEL_DISPLAY,
	EVENT_COMMAND_REPEAT,
	EVENT_ANIMATE,
	EVENT_CHEAT_DEATH,
	EVENT_POEM,
	EVENT_DEATH,

	EVENT_INITSTATUS,	/* New status message for initialisation */
	EVENT_STATPOINTS,	/* Change in the (birth) stat points */
	EVENT_SKILLPOINTS,	/* Change in the skill points */

	/* Changing of the game state/context. */
	EVENT_ENTER_INIT,
	EVENT_LEAVE_INIT,
	EVENT_ENTER_BIRTH,
	EVENT_LEAVE_BIRTH,
	EVENT_ENTER_GAME,
	EVENT_LEAVE_GAME,
	EVENT_ENTER_WORLD,
	EVENT_LEAVE_WORLD,
	EVENT_ENTER_STORE,
	EVENT_LEAVE_STORE,
	EVENT_ENTER_DEATH,
	EVENT_LEAVE_DEATH,

	/* Events for introspection into dungeon generation */
	EVENT_GEN_LEVEL_START, /* has string in event data for profile name */
	EVENT_GEN_LEVEL_END, /* has flag in event data indicating success */
	EVENT_GEN_ROOM_START, /* has string in event data for room type */
	EVENT_GEN_ROOM_CHOOSE_SIZE, /* has size in event data */
	EVENT_GEN_ROOM_CHOOSE_SUBTYPE, /* has string in event data with name */
	EVENT_GEN_ROOM_END, /* has flag in event data indicating success */
	EVENT_GEN_TUNNEL_FINISHED, /* has tunnel in event data with results */

	EVENT_END  /* Can be sent at the end of a series of events */
} game_event_type;

#define  N_GAME_EVENTS EVENT_END + 1

typedef enum tunnel_direction_type {
	TUNNEL_HOR, TUNNEL_VER, TUNNEL_BENT
} tunnel_direction_type;
typedef enum tunnel_type {
	TUNNEL_ROOM_TO_ROOM, TUNNEL_ROOM_TO_CORRIDOR, TUNNEL_DESPERATE
} tunnel_type;

typedef union
{
	struct loc point;

	const char *string;

	bool flag;

	struct {
		const char *msg;
		int type;
	} message;

	struct
	{
		bool reset;
		const char *hint;
		int n_choices;
		int initial_choice;
		const char **choices;
		const char **helptexts;
		void *xtra;
	} birthstage;

  	struct
	{
		const int *points;
		const int *inc_points;
		int remaining;
	} points;

  	struct
	{
		const int *exp;
		const int *inc_exp;
		int remaining;
	} exp;

	struct
	{
		int proj_type;
		int num_grids;
		int *distance_to_grid;
		bool drawing;
		bool *player_sees_grid;
		struct loc *blast_grid;
		struct loc centre;
	} explosion;

	struct
	{
		int proj_type;
		bool drawing;
		bool seen;
		bool beam;
		int oy;
		int ox;
		int y;
		int x;
	} bolt;

	struct
	{
		struct object *obj;
		bool seen;
		int y;
		int x;
	} missile;

	struct
	{
		int dam;
		int dam_type;
		bool fatal;
		struct loc grid;
	} hit;

	struct
	{
		int h, w;
	} size;

	struct
	{
		tunnel_type t;
		tunnel_direction_type dir;
		/*
		 * tunnel's length vertically and horizontally; the two legs
		 * of a bent tunnel share a grid which is counted with
		 * whichever leg was dug first
		 */
		int vlength, hlength;
	} tunnel;

	struct
	{
		struct source attacker;
		struct source defender;
		bool vis;
		int att;
		int att_roll;
		int evn;
		int evn_roll;
		bool melee;
	} combat_attack;

	struct
	{
		int dd;
		int ds;
		int dam;
		int pd;
		int ps;
		int prot;
		int prt_percent;
		int dam_type;
		bool melee;
	} combat_damage;

	struct
	{
		/*
		 * Either load the text from a file if filename is not NULL
		 * (filename is assumed to be relative to ANGBAND_DIR_GAMEDATA
		 * and has been stripped of a ".txt" extension) or get it from
		 * the given textblock.
		 */
		const char *filename;
		struct textblock *text;
		int row, col;
	} verse;
} game_event_data;


/**
 * A function called when a game event occurs - these are registered to be
 * called by event_add_handler or event_add_handler_set, and deregistered
 * when they should no longer be called through event_remove_handler or
 * event_remove_handler_set.
 */
typedef void game_event_handler(game_event_type type, game_event_data *data, void *user);

void event_add_handler(game_event_type type, game_event_handler *fn, void *user);
void event_remove_handler(game_event_type type, game_event_handler *fn, void *user);
void event_remove_handler_type(game_event_type type);
void event_remove_all_handlers(void);
void event_add_handler_set(game_event_type *type, size_t n_types, game_event_handler *fn, void *user);
void event_remove_handler_set(game_event_type *type, size_t n_types, game_event_handler *fn, void *user);

void event_signal_birthpoints(const int *points, const int *inc_points,
	int remaining);
void event_signal_skillpoints(const int *points, const int *inc_points,
	int remaining);

void event_signal_point(game_event_type, int x, int y);
void event_signal_string(game_event_type, const char *s);
void event_signal_message(game_event_type type, int t, const char *s);
void event_signal_flag(game_event_type type, bool flag);
void event_signal(game_event_type);
void event_signal_blast(game_event_type type,
						int proj_type,
						int num_grids,
						int *distance_to_grid,
						bool seen,
						bool *player_sees_grid,
						struct loc *blast_grid,
						struct loc centre);
void event_signal_bolt(game_event_type type,
					   int proj_type,
					   bool drawing,
					   bool seen,
					   bool beam,
					   int oy,
					   int ox,
					   int y,
					   int x);
void event_signal_missile(game_event_type type,
						  struct object *obj,
						  bool seen,
						  int y,
						  int x);
void event_signal_hit(game_event_type type,
					  int dam,
					  int dam_type,
					  bool fatal,
					  struct loc grid);					  
void event_signal_size(game_event_type type, int h, int w);
void event_signal_tunnel(game_event_type type, tunnel_type t,
	tunnel_direction_type dir, int vlength, int hlength);
void event_signal_combat_attack(game_event_type type, struct source attacker,
								struct source defender, bool vis, int att,
								int att_roll, int evn, int evn_roll,
								bool melee);
void event_signal_combat_damage(game_event_type type, int dd, int ds, int dam,
								int pd, int ps, int prot, int prt_percent,
								int dam_type, bool melee);
void event_signal_poem(game_event_type type, const char *name, int row,
					   int col);
void event_signal_poem_textblock(game_event_type type, struct textblock *tb,
					   int row, int col);
#endif /* INCLUDED_GAME_EVENT_H */
