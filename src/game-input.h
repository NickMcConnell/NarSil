/**
 * \file game-input.h
 * \brief Ask for non-command input from the UI.
 *
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

#ifndef INCLUDED_GAME_INPUT_H
#define INCLUDED_GAME_INPUT_H

#include "cmd-core.h"
#include "obj-smith.h"
#include "player.h"

/**
 * Bit flags for get_item() function
 */
#define USE_EQUIP     0x0001	/* Allow equip items */
#define USE_INVEN     0x0002	/* Allow inven items */
#define USE_FLOOR     0x0004	/* Allow floor items */
#define USE_QUIVER    0x0008	/* Allow quiver items */
#define IS_HARMLESS   0x0010	/* Ignore generic warning inscriptions */
#define SHOW_FAIL     0x0020 	/* Show device failure in item lists */
#define SHOW_EMPTY    0x0040	/* Show empty slots in equipment display */
#define SHOW_THROWING 0x0080	/* Show inventory/quiver/floor throwables*/


extern bool (*get_string_hook)(const char *prompt, char *buf, size_t len);
extern int (*get_quantity_hook)(const char *prompt, int max);
extern bool (*get_check_hook)(const char *prompt);
extern bool (*get_com_hook)(const char *prompt, char *command);
extern bool (*get_rep_dir_hook)(int *dir, bool allow_none);
extern bool (*get_aim_dir_hook)(int *dir, int range);
extern bool (*get_item_hook)(struct object **choice, const char *pmt,
							 const char *str, cmd_code cmd, item_tester tester,
							 int mode);
extern int (*get_effect_from_list_hook)(const char *prompt,
	struct effect *effect, int count, bool allow_random);
extern bool (*confirm_debug_hook)(void);
extern void (*get_panel_hook)(int *min_y, int *min_x, int *max_y, int *max_x);
extern bool (*panel_contains_hook)(unsigned int y, unsigned int x);
extern bool (*map_is_visible_hook)(void);
extern struct object *(*smith_object_hook)(struct smithing_cost *cost);
extern void (*view_abilities_hook)(struct player_ability *ability_list,
								   int num_abilities);
extern void (*change_song_hook)(void);

bool get_string(const char *prompt, char *buf, size_t len);
int get_quantity(const char *prompt, int max);
bool get_check(const char *prompt);
bool get_com(const char *prompt, char *command);
bool get_rep_dir(int *dir, bool allow_none);
bool get_aim_dir(int *dir, int range);
bool get_item(struct object **choice, const char *pmt, const char *str,
			  cmd_code cmd, item_tester tester, int mode);
int get_effect_from_list(const char *prompt, struct effect *effect, int count,
	bool allow_random);
void get_panel(int *min_y, int *min_x, int *max_y, int *max_x);
bool confirm_debug(void);
bool panel_contains(unsigned int y, unsigned int x);
bool map_is_visible(void);
struct object *smith_object(struct smithing_cost *cost);
void view_ability_menu(struct player_ability *ability_list,
						 int num_abilities);
void change_song(void);

#endif /* INCLUDED_GAME_INPUT_H */
