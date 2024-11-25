/**
 * \file player-calcs.h
 * \brief Player temporary status structures.
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

#ifndef PLAYER_CALCS_H
#define PLAYER_CALCS_H

#include "player.h"

/**
 * Bit flags for the "player->upkeep->notice" variable
 */
#define PN_COMBINE      0x00000001L    /* Combine the pack */
#define PN_IGNORE       0x00000002L    /* Ignore stuff */
#define PN_MON_MESSAGE	0x00000004L	   /* flush monster pain messages */


/**
 * Bit flags for the "player->upkeep->update" variable
 */
#define PU_BONUS		0x00000001L	/* Calculate bonuses */
#define PU_TORCH		0x00000002L	/* Calculate torch radius */
#define PU_HP			0x00000004L	/* Calculate chp and mhp */
#define PU_MANA			0x00000008L	/* Calculate csp and msp */
#define PU_SPELLS		0x00000010L	/* Calculate spells */
#define PU_UPDATE_VIEW	0x00000020L	/* Update field of view */
#define PU_MONSTERS		0x00000040L	/* Update monsters */
#define PU_DISTANCE		0x00000080L	/* Update distances */
#define PU_PANEL		0x00000100L	/* Update panel */
#define PU_INVEN		0x00000200L	/* Update inventory */


/**
 * Bit flags for the "player->upkeep->redraw" variable
 */
#define PR_MISC			0x00000001L	/* Display Race/Class */
#define PR_TITLE		0x00000002L	/* Display Title */
#define PR_TERRAIN		0x00000004L	/* Display Level */
#define PR_EXP			0x00000008L	/* Display Experience */
#define PR_STATS		0x00000010L	/* Display Stats */
#define PR_ARMOR		0x00000020L	/* Display Armor */
#define PR_HP			0x00000040L	/* Display Hitpoints */
#define PR_MANA			0x00000080L	/* Display Mana */
#define PR_SONG			0x00000100L	/* Display Songs */
#define PR_HEALTH		0x00000200L	/* Display Health Bar */
#define PR_SPEED		0x00000400L	/* Display Extra (Speed) */
#define PR_MELEE		0x00000800L	/* Display Melee */
#define PR_DEPTH		0x00001000L	/* Display Depth */
#define PR_STATUS		0x00002000L
#define PR_ARC			0x00004000L /* Display Archery */
#define PR_STATE		0x00008000L	/* Display Extra (State) */
#define PR_MAP			0x00010000L	/* Redraw whole map */
#define PR_INVEN		0x00020000L /* Display inven/equip */
#define PR_EQUIP		0x00040000L /* Display equip/inven */
#define PR_MESSAGE		0x00080000L /* Display messages */
#define PR_MONSTER		0x00100000L /* Display monster recall */
#define PR_OBJECT		0x00200000L /* Display object recall */
#define PR_MONLIST		0x00400000L /* Display monster list */
#define PR_ITEMLIST		0x00800000L /* Display item list */
#define PR_FEELING		0x01000000L /* Display level feeling */
#define PR_LIGHT		0x02000000L /* Display light level */
#define PR_COMBAT		0x04000000L /* Display combat rolls */

/**
 * Display Basic Info
 */
#define PR_BASIC \
	(PR_MISC | PR_STATS | PR_TERRAIN | PR_EXP | PR_SONG | PR_ARMOR \
	 | PR_HP | PR_MELEE | PR_ARC | PR_MANA | PR_DEPTH | PR_HEALTH | PR_SPEED)

/**
 * Display Extra Info
 */
#define PR_EXTRA \
	(PR_STATUS | PR_STATE | PR_TERRAIN)

/**
 * Display Subwindow Info
 */
#define PR_SUBWINDOW \
	(PR_MONSTER | PR_OBJECT | PR_MONLIST | PR_ITEMLIST)


uint8_t total_mds(struct player *p, struct player_state *state,
				  const struct object *obj, int str_adjustment);
int hand_and_a_half_bonus(struct player *p, const struct object *obj);
bool two_handed_melee(struct player *p);
int blade_bonus(struct player *p, const struct object *obj);
int axe_bonus(struct player *p, const struct object *obj);
int polearm_bonus(struct player *p, const struct object *obj);
uint8_t total_ads(struct player *p, struct player_state *state,
				  const struct object *obj, bool single_shot);
int equipped_item_slot(struct player_body body, struct object *obj);
void calc_inventory(struct player *p);
void calc_voice(struct player *p, bool update);
bool weapon_glows(struct object *obj, int near);
void calc_light(struct player *p);
int weight_limit(struct player_state state);
int weight_remaining(struct player *p);
void calc_bonuses(struct player *p, struct player_state *state, bool known_only,
				  bool update);

void health_track(struct player_upkeep *upkeep, struct monster *mon);
void monster_race_track(struct player_upkeep *upkeep, 
						struct monster_race *race);
void track_object(struct player_upkeep *upkeep, struct object *obj);
void track_object_kind(struct player_upkeep *upkeep, struct object_kind *kind);
void track_object_cancel(struct player_upkeep *upkeep);
bool tracked_object_is(struct player_upkeep *upkeep, struct object *obj);

void notice_stuff(struct player *p);
void update_stuff(struct player *p);
void redraw_stuff(struct player *p);
void handle_stuff(struct player *p);

#endif /* !PLAYER_CALCS_H */
