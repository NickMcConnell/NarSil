/**
 * \file tutorial.h
 * \brief Declare interface for building and managing tutorial levels.
 */

#ifndef INCLUDED_TUTORIAL_H
#define INCLUDED_TUTORIAL_H

#include "game-event.h"
#include "z-textblock.h"
struct player;
struct monster_race;
struct object_kind;
struct tutorial_item;

bool in_tutorial(void);

void tutorial_prepare_section(const char *name, struct player *p);
void tutorial_leave_section(struct player *p);
const char *tutorial_get_next_section(const struct player *p);
textblock *tutorial_expand_message(int pval);
void tutorial_textblock_show(textblock *tb, const char *header);
void tutorial_display_death_note(const struct player *p);

void tutorial_textblock_append_command_phrase(textblock *tb,
	const char *command_name, bool capital, bool gerund);
void tutorial_textblock_append_direction_phrase(textblock *tb, int dirnum,
	bool capital, bool gerund);
void tutorial_textblock_append_direction_rose(textblock *tb);
void tutorial_textblock_append_feature_symbol(textblock *tb, int feat);
void tutorial_textblock_append_monster_symbol(textblock *tb,
	const struct monster_race *race);
void tutorial_textblock_append_object_symbol(textblock *tb,
	const struct object_kind *kind);

struct object *tutorial_create_artifact(const struct artifact* art);
struct object *tutorial_create_object(const struct tutorial_item *item);
void tutorial_handle_enter_world(game_event_type t, game_event_data *d,
	void *u);
void tutorial_handle_leave_world(game_event_type t, game_event_data *d,
	void *u);

extern void (*tutorial_textblock_show_hook)(textblock *tb, const char *header);
extern void (*tutorial_textblock_append_command_phrase_hook)(textblock *tb,
	const char *command_name, bool capital, bool gerund);
extern void (*tutorial_textblock_append_direction_phrase_hook)(textblock *tb,
	int dirnum, bool capital, bool gerund);
extern void (*tutorial_textblock_append_direction_rose_hook)(textblock *tb);
extern void (*tutorial_textblock_append_feature_symbol_hook)(textblock *tb,
	int feat);
extern void (*tutorial_textblock_append_monster_symbol_hook)(textblock *tb,
	const struct monster_race *race);
extern void (*tutorial_textblock_append_object_symbol_hook)(textblock *tb,
	const struct object_kind *kind);

#endif /* INCLUDED_TUTORIAL_H */
