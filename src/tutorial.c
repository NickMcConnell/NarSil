/**
 * \file tutorial.c
 * \brief Implement generation and management of tutorial levels.
 *
 * Copyright (c) 2022 Eric Branlund
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
/*
 * Issues:
 * 1) While the tutorial notes can display a graphical tile for a feature,
 * monster, or object, that only works when the tile takes up one grid.  When
 * tiles take more than that either need to fall back to not displaying tiles,
 * have the text in a textblock flow around (by skipping lines) the tile, or
 * have the terminals and front-ends be able to display tiles scaled to
 * different sizes at the same time.
 * 2) There's no way to specify monster groups from tutorial.txt.
 * 3) Would be nice to be able to switch archetypes (i.e. the character
 * the player controls in the tutorial) when switching tutorial sections.
 *
 * NarSil-specific issues:
 * 1) The pit and spiked pit terrain types can't be included in a tutorial
 * section.
 */

#include "cmd-core.h"
#include "game-world.h"
#include "message.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-util.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player.h"
#include "player-abilities.h"
#include "player-history.h"
#include "player-timed.h"
#include "player-util.h"
#include "trap.h"
#include "tutorial.h"
#include "tutorial-init.h"
#include "z-dice.h"
#include "z-expression.h"
#include "z-form.h"
#include "z-util.h"


typedef void (*place_thing_func)(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val);


static void tutorial_section_place_feature(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val);
static void tutorial_section_place_trap(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val);
static void tutorial_section_place_note(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val);
static void tutorial_section_place_trigger(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val);
static void tutorial_section_place_gate(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val);
static void tutorial_section_place_forge(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val);
static void tutorial_section_place_object(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val);
static void tutorial_section_place_monster(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val);
static void tutorial_section_place_custom_trap(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val);
static void tutorial_section_place_custom_door(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val);


void (*tutorial_textblock_show_hook)(textblock *tb, const char *header) = NULL;
void (*tutorial_textblock_append_command_phrase_hook)(textblock *tb,
	const char *command_name, bool capital, bool gerund) = NULL;
void (*tutorial_textblock_append_direction_phrase_hook)(textblock *tb,
	int dirnum, bool capital, bool gerund) = NULL;
void (*tutorial_textblock_append_direction_rose_hook)(textblock *tb);
void (*tutorial_textblock_append_feature_symbol_hook)(textblock *tb, int feat)
	= NULL;
void (*tutorial_textblock_append_monster_symbol_hook)(textblock *tb,
	const struct monster_race *race) = NULL;
void (*tutorial_textblock_append_object_symbol_hook)(textblock *tb,
	const struct object_kind *kind) = NULL;


static place_thing_func place_ftable[] = {
	#define TSYM(a, b, c, d) d,
	#include "list-tutorial-sym.h"
	#undef TSYM
};


static struct object *create_tutorial_note(char *name)
{
	struct tutorial_dict_key_type dkey;
	struct tutorial_dict_val_type *dval;
	struct object *obj;

	dkey.name = name;
	dkey.comp = TUTORIAL_NOTE;
	dval = tutorial_dict_has(tutorial_parsed_data.d, &dkey);
	if (!dval) {
		msg("Tutorial has an unknown note, %s", name);
		return NULL;
	}

	obj = object_new();
	object_prep(obj, tutorial_parsed_data.note_kind, 0, RANDOMISE);
	obj->pval = dval->v.note.pval;
	obj->number = 1;
	return obj;
}


static void tutorial_section_place_feature(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val)
{
	assert(square_in_bounds_fully(c, grid));
	square_set_feat(c, grid, val->v.feat);
}


static void tutorial_section_place_trap(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val)
{
	assert(val->kind == SECTION_SYM_TRAP_RANDOM);
	assert(square_in_bounds_fully(c, grid));
	square_set_feat(c, grid, FEAT_FLOOR);
	square_add_trap(c, grid);
}


static void tutorial_section_place_note(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val)
{
	assert(val->kind == SECTION_SYM_NOTE || val->kind == SECTION_SYM_START);
	assert(square_in_bounds_fully(c, grid));
	square_set_feat(c, grid, FEAT_FLOOR);
	if (val->v.name) {
		struct object *obj = create_tutorial_note(val->v.name);

		if (obj) {
			bool dummy;

			obj->origin = ORIGIN_FLOOR;
			obj->origin_depth = convert_depth_to_origin(c->depth);
			if (floor_carry(c, grid, obj, &dummy)) {
				list_object(c, obj);
			} else {
				object_delete(c, &obj);
			}
		}
	}
}


static void tutorial_section_place_trigger(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val)
{
	struct tutorial_dict_key_type dkey;
	struct tutorial_dict_val_type *dval;

	assert(val->kind == SECTION_SYM_TRIGGER);
	assert(square_in_bounds_fully(c, grid));
	square_set_feat(c, grid, FEAT_FLOOR);

	dkey.name = val->v.name;
	dkey.comp = TUTORIAL_TRIGGER;
	dval = tutorial_dict_has(tutorial_parsed_data.d, &dkey);
	if (dval) {
		/* Add to the trigger and gate map. */
		assert(!tutorial_parsed_data.trigger_gate_map[grid.y][2 * grid.x]);
		tutorial_parsed_data.trigger_gate_map[grid.y][2 * grid.x] = dval;
	} else {
		msg("Tutorial has an unknown trigger, %s", val->v.name);
	}
}


static void tutorial_section_place_gate(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val)
{
	struct tutorial_dict_key_type dkey;
	struct tutorial_dict_val_type *dval;

	assert(val->kind == SECTION_SYM_GATE);
	assert(square_in_bounds_fully(c, grid));

	dkey.name = val->v.gate.dest;
	dkey.comp = TUTORIAL_SECTION;
	dval = tutorial_dict_has(tutorial_parsed_data.d, &dkey);
	if (dval) {
		/* Add to the trigger and gate map. */
		assert(!tutorial_parsed_data.trigger_gate_map[grid.y][2 * grid.x]);
		tutorial_parsed_data.trigger_gate_map[grid.y][2 * grid.x] = dval;
		square_set_feat(c, grid, val->v.gate.feat);
		if (val->v.gate.note) {
			dkey.name = val->v.gate.note;
			dkey.comp = TUTORIAL_NOTE;
			dval = tutorial_dict_has(tutorial_parsed_data.d,
				&dkey);
			if (dval) {
				assert(!tutorial_parsed_data.trigger_gate_map[grid.y][2 * grid.x + 1]);
				tutorial_parsed_data.trigger_gate_map[grid.y][2 * grid.x + 1] = dval;
			} else {
				msg("Tutorial has an unknown note, %s",
					val->v.gate.note);
			}
		}
	} else {
		square_set_feat(c, grid, FEAT_FLOOR);
		msg("Tutorial has a gate to an unknown section, %s",
			val->v.gate.dest);
	}
}


static void tutorial_section_place_forge(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val)
{
	assert(val->kind == SECTION_SYM_FORGE);
	assert(square_in_bounds_fully(c, grid));
	square_set_feat(c, grid, val->v.forge.feat);
	square_set_forge(c, grid, val->v.forge.uses);
}


static void tutorial_section_place_object(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val)
{
	struct object *obj;
	bool dummy;

	assert(val->kind == SECTION_SYM_ITEM);
	assert(square_in_bounds_fully(c, grid));
	square_set_feat(c, grid, FEAT_FLOOR);
	obj = (val->v.item.is_artifact) ?
		tutorial_create_artifact(val->v.item.v.art) :
		tutorial_create_object(&val->v.item);
	if (!obj) {
		return;
	}
	obj->origin = ORIGIN_FLOOR;
	obj->origin_depth = convert_depth_to_origin(c->depth);
	dummy = true;
	if (!floor_carry(c, grid, obj, &dummy)) {
		if (obj->artifact) {
			mark_artifact_created(obj->artifact, false);
		}
		object_delete(c, &obj);
		return;
	}
	list_object(c, obj);
}


static void tutorial_section_place_monster(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val)
{
	struct monster_group_info gi = { 0, 0 };
	struct monster *mon;

	assert(val->kind == SECTION_SYM_MONSTER);
	assert(square_in_bounds_fully(c, grid));
	square_set_feat(c, grid, FEAT_FLOOR);
	place_new_monster(c, grid, val->v.monster.race, false, false, gi,
		ORIGIN_DROP);
	mon = square_monster(c, grid);
	if (mon && val->v.monster.note) {
		struct object *obj = create_tutorial_note(val->v.monster.note);

		if (obj) {
			obj->origin = ORIGIN_DROP;
			obj->origin_depth = convert_depth_to_origin(c->depth);
			if (!monster_carry(c, mon, obj)) {
				object_free(obj);
			}
		}
	}
	if (mon) {
		int amount;

		if (val->v.monster.sleepiness_fixed) {
			amount = val->v.monster.sleepiness;
		} else {
			assert(val->v.monster.sleepiness > 0);
			amount = randint1(val->v.monster.sleepiness);
		}
		mon->alertness = MAX(ALERTNESS_MIN, MIN(ALERTNESS_MAX,
			ALERTNESS_ALERT - amount));
	}
}


static void tutorial_section_place_custom_trap(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val)
{
	assert(val->kind == SECTION_SYM_TRAP);
	assert(square_in_bounds_fully(c, grid));
	square_set_feat(c, grid, FEAT_FLOOR);
	place_trap(c, grid, val->v.trap.kind->tidx, c->depth);
	if (val->v.trap.vis) {
		square_reveal_trap(c, grid, false);
	}
}


static void tutorial_section_place_custom_door(struct chunk *c, struct loc grid,
	struct tutorial_section_sym_val *val)
{
	assert(val->kind == SECTION_SYM_DOOR);
	assert(square_in_bounds_fully(c, grid));
	square_set_feat(c, grid, val->v.door.feat);
	if (val->v.door.power > 0) {
		square_set_door_lock(c, grid, val->v.door.power);
	} else if (val->v.door.power < 0) {
		square_set_door_jam(c, grid, -val->v.door.power);
	}
}


static void append_with_case_sensitive_first(textblock *tb, const char *src,
		bool capital)
{
	if (isupper(src[0])) {
		if (capital) {
			textblock_append(tb, "%s", src);
		} else {
			textblock_append(tb, "%c%s", tolower(src[0]), src + 1);
		}
	} else if (capital) {
		textblock_append(tb, "%c%s", toupper(src[0]), src + 1);
	} else {
		textblock_append(tb, "%s", src);
	}
}


static textblock *tutorial_expand_message_from_string(const char *text,
		bool note)
{
	textblock *tb = textblock_new();

	if (!text || !text[0]) {
		if (note) {
			textblock_append(tb, "This note is blank.");
		}
		return tb;
	}
	if (note) {
		textblock_append(tb, "This note reads:\n");
	}

	while (1) {
		const char *lb;
		const char *rb;
		char *tmp;

		if (!text[0]) {
			/* Reached the end. */
			break;
		}

		/* Find an unescaped left brace; it introduces an expression. */
		lb = text;
		while (1) {
			lb = strchr(lb, '{');
			if (!lb || !tutorial_text_escaped(lb, text)) {
				break;
			}
			++lb;
		}

		if (lb != text) {
			/*
			 * Add what was before the expression to the textblock.
			 * Do it in blocks of 512 characters to avoid
			 * internal buffer limits in z-form.c.
			 */
			size_t sz = (lb) ? (size_t)(lb - text) : strlen(text);
			size_t buf_sz = 512;

			tmp = mem_alloc(buf_sz);
			while (sz) {
				size_t read = tutorial_copy_strip_escapes(
					tmp, (sz < buf_sz) ? sz + 1: buf_sz,
					text, sz);

				assert(read <= sz);
				sz -= read;
				text += read;
				textblock_append(tb, "%s", tmp);
			}
			mem_free(tmp);
			/* There was no more directives; everything's done. */
			if (!lb) {
				break;
			}
		}

		text = lb + 1;
		/*
		 * Find an unescaped right brace; it terminates the expression.
		 */
		rb = text;
		while (1) {
			rb = strchr(rb, '}');
			if (!rb || !tutorial_text_escaped(rb, text)) {
				break;
			}
			++rb;
		}

		/*
		 * Expand expression.  Drop unterminated or unrecognized ones.
		 */
		if (!rb) {
			break;
		}
		text = lb + 1;
		if (prefix(text, "command:")
				|| prefix(text, "Command:")
				|| prefix(text, "commanding:")
				|| prefix(text, "Commanding:")) {
			bool capital = (text[0] == 'C');
			bool gerund;

			text = strchr(text, ':') + 1;
			gerund = (text[-2] == 'g');
			tmp = mem_alloc((rb - text) + 1);
			(void) tutorial_copy_strip_escapes(tmp,
				(rb - text) + 1, text, rb - text);
			tutorial_textblock_append_command_phrase(tb,
				tmp, capital, gerund);
			mem_free(tmp);
		} else if (prefix(text, "direction:")
				|| prefix(text, "Direction:")
				|| prefix(text, "directioning:")
				|| prefix(text, "Directioning:")) {
			bool capital = (text[0] == 'D');
			bool gerund;
			int dir;

			text = strchr(text, ':') + 1;
			gerund = (text[-2] == 'g');
			if (prefix(text, "north}")) {
				assert(text + 5 == rb);
				dir = 8;
			} else if (prefix(text, "northeast}")) {
				assert(text + 9 == rb);
				dir = 9;
			} else if (prefix(text, "east}")) {
				assert(text + 4 == rb);
				dir = 6;
			} else if (prefix(text, "southeast}")) {
				assert(text + 9 == rb);
				dir = 3;
			} else if (prefix(text, "south}")) {
				assert(text + 5 == rb);
				dir = 2;
			} else if (prefix(text, "southwest}")) {
				assert(text + 9 == rb);
				dir = 1;
			} else if (prefix(text, "west}")) {
				assert(text + 4 == rb);
				dir = 4;
			} else if (prefix(text, "northwest}")) {
				assert(text + 9 == rb);
				dir = 7;
			} else if (prefix(text, "stay}")) {
				assert(text + 4 == rb);
				dir = 5;
			} else {
				dir = -1;
			}
			if (dir > 0) {
				tutorial_textblock_append_direction_phrase(
					tb, dir, capital, gerund);
			}
		} else if (prefix(text, "direction-rose}")) {
			assert(text + 14 == rb);
			tutorial_textblock_append_direction_rose(tb);
		} else if (prefix(text, "feature:")) {
			int feat;

			text += 8;
			tmp = mem_alloc((rb - text) + 1);
			(void) tutorial_copy_strip_escapes(tmp,
				(rb - text) + 1, text, rb - text);
			feat = lookup_feat(tmp);
			mem_free(tmp);
			if (feat >= 0) {
				tutorial_textblock_append_feature_symbol(tb,
					feat);
			}
		} else if (prefix(text, "house}") || prefix(text, "House}")) {
			assert(text + 5 == rb);
			append_with_case_sensitive_first(tb,
				player->house->name, (text[0] == 'H'));
		} else if (prefix(text, "monster:")) {
			struct monster_race *race;

			text += 8;
			tmp = mem_alloc((rb - text) + 1);
			(void) tutorial_copy_strip_escapes(tmp,
				(rb - text) + 1, text, rb - text);
			race = lookup_monster(tmp);
			mem_free(tmp);
			if (race) {
				tutorial_textblock_append_monster_symbol(tb,
					race);
			}
		} else if (prefix(text, "name}")) {
			assert(text + 4 == rb);
			textblock_append(tb, "%s", player->full_name);
		} else if (prefix(text, "object:")) {
			int tval = -1;
			int sval = -1;
			const char *colon;

			text += 7;

			/* Find unescaped colon. */
			colon = text;
			while (1) {
				colon = strchr(colon, ':');
				if (!colon || !tutorial_text_escaped(colon, text)) {
					break;
				}
				++colon;
			}
			if (colon) {
				tmp = mem_alloc((colon - text) + 1);
				(void) tutorial_copy_strip_escapes(tmp,
					(colon - text) + 1, text, colon - text);
				tval = tval_find_idx(tmp);
				mem_free(tmp);
				text = colon + 1;
			}
			if (colon && tval >= 0) {
				tmp = mem_alloc((rb - text) + 1);
				(void) tutorial_copy_strip_escapes(tmp,
					(rb - text) + 1, text, rb - text);
				if (streq(tmp, "*")) {
					sval = 1;
				} else {
					sval = lookup_sval(tval, tmp);
				}
				mem_free(tmp);
			}
			if (tval >= 0 && sval >= 0) {
				struct object_kind *kind =
					lookup_kind(tval, sval);

				if (kind) {
					tutorial_textblock_append_object_symbol(
						tb, kind);
				}
			}
		} else if (prefix(text, "paragraphbreak}")) {
			assert(text + 14 == rb);
			textblock_append(tb, "\n\n");
		} else if (prefix(text, "race}") || prefix(text, "Race}")) {
			assert(text + 4 == rb);
			append_with_case_sensitive_first(tb,
				player->race->name, (text[0] == 'R'));
		}

		text = rb + 1;
	}

	return tb;
}


static void tutorial_handle_player_move(game_event_type t, game_event_data *d,
		void *u)
{
	struct tutorial_dict_val_type *entry;
	const struct object *obj;

	assert(t == EVENT_PLAYERMOVED || t == EVENT_NEW_LEVEL_DISPLAY);

	/* Check for a trigger.  The map should always be available. */
	assert(tutorial_parsed_data.trigger_gate_map);
	assert(cave && player && player->grid.x >= 0 && player->grid.y >= 0
		&& player->grid.x < cave->width
		&& player->grid.y < cave->height);
	entry = tutorial_parsed_data.trigger_gate_map[player->grid.y][
		2 * player->grid.x];
	if (entry && entry->key->comp == TUTORIAL_TRIGGER
			&& entry->v.trigger.expr.n_stack > 0
			&& (entry->v.trigger.text
			|| entry->v.trigger.changes_death_note)) {
		bool *estack = mem_alloc(entry->v.trigger.expr.n_stack
			* sizeof(*estack));
		int next = 0, iop;
		bool triggered;

		for (iop = 0; iop < entry->v.trigger.expr.n_op; ++iop) {
			const struct trigger_compiled_op *op =
					entry->v.trigger.expr.ops + iop;

			switch (op->kind) {
			case TRIGGER_OP_NOT:
				assert(next > 0);
				estack[next - 1] = !estack[next - 1];
				break;

			case TRIGGER_OP_AND:
				assert(next > 1);
				estack[next - 2] = estack[next - 2]
					&& estack[next - 1];
				--next;
				break;

			case TRIGGER_OP_OR:
				assert(next > 1);
				estack[next - 2] = estack[next - 2]
					|| estack[next - 1];
				--next;
				break;

			case TRIGGER_OP_XOR:
				assert(next > 1);
				if (estack[next - 2]) {
					estack[next - 2] = !estack[next - 1];
				} else if (estack[next - 1]) {
					estack[next - 2] = true;
				} else {
					estack[next - 2] = false;
				}
				--next;
				break;

			case TRIGGER_OP_ABILITY:
				assert(next < entry->v.trigger.expr.n_stack);
				estack[next] =
					player_has_ability(player, op->name);
				++next;
				break;

			case TRIGGER_OP_CARRIED:
				assert(next < entry->v.trigger.expr.n_stack);
				obj = player->gear;
				while (1) {
					if (!obj) {
						estack[next] = false;
						break;
					}
					if (obj->kind && obj->kind->tval
							== op->tval
							&& (op->sval == -1
							|| obj->kind->sval
							== op->sval)) {
						estack[next] = true;
						break;
					}
					obj = obj->next;
				}
				++next;
				break;

			case TRIGGER_OP_DRAINED:
				assert(next < entry->v.trigger.expr.n_stack);
				if (op->idx >= 0 && op->idx < STAT_MAX) {
					estack[next] =
						player->stat_drain[op->idx] < 0;
				} else if (op->idx == STAT_MAX) {
					estack[next] = player->chp
						< player->mhp;
				} else if (op->idx == STAT_MAX + 1) {
					estack[next] = player->csp
						< player->msp;
				} else {
					quit("Unexpected drained criteria for tutorial trigger");
				}
				++next;
				break;

			case TRIGGER_OP_EQUIPPED:
				assert(next < entry->v.trigger.expr.n_stack);
				obj = player->gear;
				while (1) {
					if (!obj) {
						estack[next] = false;
						break;
					}
					if (obj->kind && obj->kind->tval
							== op->tval
							&& (op->sval == -1
							|| obj->kind->sval
							== op->sval)
							&& object_is_equipped(player->body, obj)) {
						estack[next] = true;
						break;
					}
					obj = obj->next;
				}
				++next;
				break;

			case TRIGGER_OP_FALSE:
				assert(next < entry->v.trigger.expr.n_stack);
				estack[next] = false;
				++next;
				break;

			case TRIGGER_OP_TIMED:
				assert(next < entry->v.trigger.expr.n_stack);
				assert(op->idx >= 0 && op->idx < TMD_MAX);
				estack[next] = player->timed[op->idx];
				++next;
				break;

			case TRIGGER_OP_TIMED_ABOVE:
				assert(next < entry->v.trigger.expr.n_stack);
				assert(op->idx >= 0 && op->idx < TMD_MAX);
				estack[next] = player_timed_grade_gt(player,
					op->idx, op->name);
				++next;
				break;

			case TRIGGER_OP_TIMED_BELOW:
				assert(next < entry->v.trigger.expr.n_stack);
				assert(op->idx >= 0 && op->idx < TMD_MAX);
				estack[next] = player_timed_grade_lt(player,
					op->idx, op->name);
				++next;
				break;

			case TRIGGER_OP_TRUE:
				assert(next < entry->v.trigger.expr.n_stack);
				estack[next] = true;
				++next;
				break;

			default:
				quit("Unexpected trigger operation in tutorial");
				break;
			}
		}
		assert(next == 1);
		triggered = estack[0];
		mem_free(estack);

		if (triggered) {
			if (entry->v.trigger.text) {
				textblock *text =
					tutorial_expand_message_from_string(
					entry->v.trigger.text, false);

				tutorial_textblock_show(text, NULL);
				textblock_free(text);
			}
			if (entry->v.trigger.changes_death_note) {
				tutorial_parsed_data.curr_death_note =
					entry->v.trigger.death_note_name;
			}
		}
	}

	/* Check for a tutorial note in the current grid. */
	obj = square_object(cave, player->grid);
	while (obj) {
		if (obj->kind == tutorial_parsed_data.note_kind) {
			textblock *text = tutorial_expand_message(obj->pval);

			tutorial_textblock_show(text, NULL);
			textblock_free(text);
		}
		obj = obj->next;
	}
}


static void tutorial_leave_section_helper(struct tutorial_dict_val_type **dest,
		struct tutorial_dict_val_type **note, const struct player *p)
{
	if (p->grid.x < 0 || p->grid.y < 0 || !cave || p->grid.x >= cave->width
			|| p->grid.y >= cave->height) {
		quit("Logic error:  player coordinates are invalid when "
			"leaving a tutorial section.");
	}
	if (!tutorial_parsed_data.trigger_gate_map) {
		quit("Logic error:  there's no trigger/gate map when leaving "
			"a tutorial section.");
	}
	*dest = tutorial_parsed_data.trigger_gate_map[p->grid.y][
		2 * p->grid.x];
	if (!*dest || (*dest)->key->comp != TUTORIAL_SECTION) {
		quit("Logic error:  the trigger/gate map entry is invalid "
			"when leaving a tutorial section.");
	}
	*note = tutorial_parsed_data.trigger_gate_map[p->grid.y][
		2 * p->grid.x + 1];
}


/**
 * Test for whether a tutorial is in progress.
 */
bool in_tutorial(void)
{
	return player && player->game_type < 0;
}


/**
 * Generate the given tutorial section and place the player in it.
 *
 * \param name is the name of the tutorial section to generate or NULL for
 * the default tutorial section.
 * \param p is the current player struct, in practice the global player
 *
 * Acts much like prepare_next_level() does in normal gameplay.
 */
void tutorial_prepare_section(const char *name, struct player *p)
{
	struct tutorial_dict_val_type *section;
	struct loc grid;
	int i;

	/* Deal with the previous tutorial section. */
	if (character_dungeon) {
		assert(cave);
		/* Deal with artifacts. */
		for (grid.y = 0; grid.y < cave->height; ++grid.y) {
			for (grid.x = 0; grid.x < cave->width; ++grid.x) {
				struct object *obj;

				for (obj = square_object(cave, grid);
						obj; obj = obj->next) {
					if (!obj->artifact) continue;
					history_lose_artifact(p, obj->artifact);
					mark_artifact_created(
						obj->artifact, true);
				}
			}
		}

		/* Mimic cave_clear() in generate.c. */
		p->smithing_leftover = 0;
		p->upkeep->knocked_back = false;
		wipe_mon_list(cave, p);
		forget_fire(cave);
		cave_free(cave);
	}

	/* Generate the tutorial section. */
	character_dungeon = false;
	if (name) {
		struct tutorial_dict_key_type key = {
			string_make(name), TUTORIAL_SECTION
		};

		section = tutorial_dict_has(tutorial_parsed_data.d, &key);
		string_free(key.name);
		if (!section) {
			quit_fmt("There's no tutorial section named, %s.",
				name);
		}
	} else {
		section = tutorial_parsed_data.default_section;
		if (!section) {
			quit("No default tutorial section was defined");
		}
	}

	/*
	 * Give the player a non-zero depth so the rest of the game doesn't
	 * think the player's in town.
	 */
	p->depth = 1;

	assert(section->v.section.rows > 0
		&& section->v.section.rows < z_info->dungeon_hgt - 1
		&& section->v.section.columns > 0
		&& section->v.section.columns < z_info->dungeon_wid - 1);

	/* Set up an auxiliary map to lookup triggers and gates. */
	tutorial_cleanup_trigger_gate_map(
		tutorial_parsed_data.trigger_gate_map);
	/* There's one extra row as a NULL sentinel. */
	tutorial_parsed_data.trigger_gate_map = mem_alloc(
		(section->v.section.rows + 3)
		* sizeof(*tutorial_parsed_data.trigger_gate_map));
	for (i = 0; i < section->v.section.rows + 2; ++i) {
		/*
		 * Store two pointers per grid:  one for the gate or trigger
		 * and one for a gate's note if it has one.
		 */
		tutorial_parsed_data.trigger_gate_map[i] =
			mem_zalloc(2 * (size_t) (section->v.section.columns + 2)
				* sizeof(**tutorial_parsed_data.trigger_gate_map));
	}
	tutorial_parsed_data.trigger_gate_map[section->v.section.rows + 2] =
		NULL;

	/* Remember the death note for this section. */
	tutorial_parsed_data.curr_death_note =
		section->v.section.death_note_name;

	/* Set up the authoritative version of the cave. */
	cave = cave_new(section->v.section.rows + 2,
		section->v.section.columns + 2);
	cave->depth = p->depth;
	cave->turn = turn;
	/* Encase in permanent rock. */
	grid.y = 0;
	for (grid.x = 0; grid.x < cave->width; ++grid.x) {
		square_set_feat(cave, grid, FEAT_PERM);
	}
	for (grid.y = 1; grid.y < cave->height - 1; ++grid.y) {
		char *sym = section->v.section.lines[grid.y - 1];

		grid.x = 0;
		square_set_feat(cave, grid, FEAT_PERM);
		for (grid.x = 1; grid.x < cave->width - 1; ++grid.x) {
			/* Fill in the customized contents. */
			char *next_sym = utf8_fskip(sym, 1, NULL);
			struct tutorial_section_sym_key key;
			struct tutorial_section_sym_val *val;

			if (next_sym) {
				assert((size_t) (next_sym - sym)
					< sizeof(key.symbol));
				strnfmt(key.symbol, sizeof(key.symbol),
					"%.*s", (int) (next_sym - sym), sym);
			} else {
				assert(grid.x == cave->width - 2);
				my_strcpy(key.symbol, sym, sizeof(key.symbol));
			}

			key.x = grid.x - 1;
			key.y = grid.y - 1;
			val = tutorial_section_sym_table_has(
				section->v.section.symt, &key);
			if (!val) {
				key.x = -1;
				key.y = -1;
				val = tutorial_section_sym_table_has(
					section->v.section.symt, &key);
				assert(val);
			}
			(*place_ftable[val->kind])(cave, grid, val);
			if (val->kind == SECTION_SYM_START) {
				player_place(cave, p, grid);
			}
			sym = next_sym;
		}
		grid.x = cave->width - 1;
		square_set_feat(cave, grid, FEAT_PERM);
	}
	grid.y = cave->height - 1;
	for (grid.x = 0; grid.x < cave->width; ++grid.x) {
		square_set_feat(cave, grid, FEAT_PERM);
	}

	/* Apply the square flags. */
	for (i = 0; i < section->v.section.area_flag_count; ++i) {
		const struct tutorial_area_flag *flag =
			section->v.section.area_flags + i;
		int yst = MAX(0, flag->ul.y + 1);
		int ylim = MIN(cave->height - 1, flag->lr.y + 1);
		int xst = MAX(0, flag->ul.x + 1);
		int xlim = MIN(cave->width - 1, flag->lr.x + 1);

		if (flag->clear) {
			for (grid.y = yst; grid.y <= ylim; ++grid.y) {
				for (grid.x = xst; grid.x <= xlim; ++grid.x) {
					sqinfo_diff(square(cave, grid)->info,
						flag->flags);
				}
			}
		} else {
			for (grid.y = yst; grid.y <= ylim; ++grid.y) {
				for (grid.x = xst; grid.x <= xlim; ++grid.x) {
					sqinfo_union(square(cave, grid)->info,
						flag->flags);
				}
			}
		}
	}

	/* It's ready to go. */
	character_dungeon = true;
}


void tutorial_leave_section(struct player *p)
{
	struct tutorial_dict_val_type *dest = NULL;
	struct tutorial_dict_val_type *note = NULL;

	tutorial_leave_section_helper(&dest, &note, p);
	if (note) {
		textblock *text;

		assert(note->key->comp == TUTORIAL_NOTE);
		if (note->v.note.text) {
			text = tutorial_expand_message_from_string(
				note->v.note.text, false);
			event_signal_poem_textblock(EVENT_POEM, text, 5, 10);
			textblock_free(text);
		}
	}
	if (dest->key->name && streq(dest->key->name, "EXIT")) {
		p->upkeep->playing = false;
	}
}


const char *tutorial_get_next_section(const struct player *p)
{
	struct tutorial_dict_val_type *dest = NULL;
	struct tutorial_dict_val_type *note = NULL;

	tutorial_leave_section_helper(&dest, &note, p);
	return dest->key->name;
}


/**
 * Expand the message for a tutorial note with the given pval.
 *
 * \param pval is the pval for the note.
 * \return a textblock with the expanded message.  The textblock should be
 * released with textblock_free() when no longer needed.
 */
textblock *tutorial_expand_message(int pval)
{
	struct tutorial_dict_val_type *note;

	if (pval < 0 || pval >= tutorial_parsed_data.note_table_n) {
		quit_fmt("A tutorial note had an invalid pval, %d.", pval);
	}
	if (!tutorial_parsed_data.pval_to_note_table) {
		quit("Logic error:  missing tutorial note lookup table");
	}
	note = tutorial_parsed_data.pval_to_note_table[pval];
	if (!note) {
		quit_fmt("Logic error:  have a gap in the note lookup table");
	}
	assert(note->key->comp == TUTORIAL_NOTE && note->v.note.pval == pval);
	return tutorial_expand_message_from_string(note->v.note.text, true);
}


/**
 * Display a textblock, let the player interact with it, and then return when
 * done.
 *
 * \param tb is the block of text to display.
 * \param header is, if not NULL, the text to always display above the text
 * block.
 */
void tutorial_textblock_show(textblock *tb, const char *header)
{
	/* Defer the implementation to the UI. */
	if (tutorial_textblock_show_hook) {
		(*tutorial_textblock_show_hook)(tb, header);
	}
}


/**
 * Display the configured message when a player dies in the tutorial.
 *
 * \param p is the player that died.
 */
void tutorial_display_death_note(const struct player *p)
{
	if (tutorial_parsed_data.curr_death_note) {
		struct tutorial_dict_key_type dkey;
		struct tutorial_dict_val_type *dval;

		dkey.name = tutorial_parsed_data.curr_death_note;
		dkey.comp = TUTORIAL_NOTE;
		dval = tutorial_dict_has(tutorial_parsed_data.d, &dkey);
		if (dval) {
			textblock *text;

			assert(dval->key->comp == TUTORIAL_NOTE);
			text = tutorial_expand_message_from_string(
				dval->v.note.text, false);
			event_signal_poem_textblock(EVENT_POEM, text, 5, 10);
			textblock_free(text);
		} else {
			msg("Tutorial has an unknown note, %s",
				dkey.name);
		}
	}
}


/**
 * Append a phrase describing how to invoke a command to a textblock.
 *
 * \param tb is the textblock to modify.
 * \param command_name is the name of the command to describe.
 * \param capital sets whether to begin the phrase with an uppercase or
 * lowercase letter.  If true, the phrase will begin with an uppercase letter.
 * \param gerund sets whether or not to use a gerund phrase.
 */
void tutorial_textblock_append_command_phrase(textblock *tb,
		const char *command_name, bool capital, bool gerund)
{
	/* Defer the implementation to the UI. */
	if (tutorial_textblock_append_command_phrase_hook) {
		(*tutorial_textblock_append_command_phrase_hook)(tb,
			command_name, capital, gerund);
	}
}


/**
 * Append a phrase describing how to move in given direction to a textblock.
 *
 * \param tb is the textblock to modify.
 * \param dirnum is the number (as in the aiming direction number) of the
 * direction.
 * \param capital sets whether to begin the phrase with an uppercase or
 * lowercase letter.  If true, the phrase will begin with an uppercase letter.
 * \param gerund sets whether or not to use a gerund phrase.
 */
void tutorial_textblock_append_direction_phrase(textblock *tb, int dirnum,
		bool capital, bool gerund)
{
	/* Defer the implementation to the UI. */
	if (tutorial_textblock_append_direction_phrase_hook) {
		(*tutorial_textblock_append_direction_phrase_hook)(tb,
			dirnum, capital, gerund);
	}
}


/**
 * Append a description of how to move in any direction to a textblock.
 *
 * \param tb is the textblock to modify.
 */
void tutorial_textblock_append_direction_rose(textblock *tb)
{
	/* Defer the implementation to the UI. */
	if (tutorial_textblock_append_direction_rose_hook) {
		(*tutorial_textblock_append_direction_rose_hook)(tb);
	}
}


/**
 * Append the symbol for a dungeon feature to a textblock.
 *
 * \param tb is the textblock to modify.
 * \param feat is the feature index for the terrain.
 */
void tutorial_textblock_append_feature_symbol(textblock *tb, int feat)
{
	/* Defer the implementation to the UI. */
	if (tutorial_textblock_append_feature_symbol_hook) {
		(*tutorial_textblock_append_feature_symbol_hook)(tb, feat);
	}
}

/**
 * Append the symbol for a monster to a textblock.
 *
 * \param tb is the textblock to modify.
 * \param race is the race of the monster.
 */
void tutorial_textblock_append_monster_symbol(textblock *tb,
		const struct monster_race *race)
{
	/* Defer the implementation to the UI. */
	if (tutorial_textblock_append_monster_symbol_hook) {
		(*tutorial_textblock_append_monster_symbol_hook)(tb, race);
	}
}


/**
 * Append the symbol for an object to a textblock.
 *
 * \param tb is the textblock to modify.
 * \param kind is the kind of object to describe.
 */
void tutorial_textblock_append_object_symbol(textblock *tb,
		const struct object_kind *kind)
{
	/* Defer the implementation to the UI. */
	if (tutorial_textblock_append_object_symbol_hook) {
		(*tutorial_textblock_append_object_symbol_hook)(tb, kind);
	}
}


struct object *tutorial_create_artifact(const struct artifact *art)
{
	struct object_kind *kind;
	struct object *obj;

	if (!art->name || is_artifact_created(art)) return NULL;
	kind = lookup_kind(art->tval, art->sval);
	if (!kind) return NULL;

	obj = object_new();
	object_prep(obj, kind, art->level, RANDOMISE);
	obj->artifact = art;
	copy_artifact_data(obj, art);
	mark_artifact_created(art, true);
	pseudo_id(obj);
	return obj;
}


struct object *tutorial_create_object(const struct tutorial_item *item)
{
	struct object_kind *kind = lookup_kind(item->v.details.tval,
		item->v.details.sval);
	int n = randcalc(item->v.details.number, 0, RANDOMISE);
	struct object *obj;
	int j;

	assert(kind);
	n = MIN(n, kind->base->max_stack);
	if (n <= 0) {
		return NULL;
	}
	obj = object_new();
	object_prep(obj, kind, 0, RANDOMISE);
	if (item->v.details.ego) {
		obj->ego = item->v.details.ego;
		ego_apply_magic(obj, 0);
	}
	obj->number = n;
	for (j = 0; j < item->v.details.tweak_count; ++j) {
		const struct tutorial_item_tweak *tweak =
			item->v.details.tweaks + j;
		dice_t *dice;

		switch (tweak->kind) {
		case TWEAK_FLAG:
			of_on(obj->flags, tweak->idx);
			break;

		case TWEAK_SLAY:
			assert(tweak->idx >= 0
				&& tweak->idx < z_info->slay_max);
			if (!obj->slays) {
				obj->slays = mem_zalloc(z_info->slay_max
					* sizeof(bool));
			}
			obj->slays[tweak->idx] = true;
			break;

		case TWEAK_BRAND:
			assert(tweak->idx >= 0
				&& tweak->idx < z_info->brand_max);
			if (!obj->brands) {
				obj->brands = mem_zalloc(z_info->brand_max
					* sizeof(bool));
			}
			obj->brands[tweak->idx] = true;
			break;

		case TWEAK_ELEM_IGNORE:
			assert(tweak->idx >= 0 && tweak->idx < ELEM_MAX);
			obj->el_info[tweak->idx].flags |= EL_INFO_IGNORE;
			break;

		case TWEAK_ELEM_HATE:
			assert(tweak->idx >= 0 && tweak->idx < ELEM_MAX);
			obj->el_info[tweak->idx].flags |= EL_INFO_HATES;
			break;

		case TWEAK_MODIFIER:
			assert(tweak->idx >= 0 && tweak->idx < OBJ_MOD_MAX);
			obj->modifiers[tweak->idx] =
				randcalc(tweak->value, 0, RANDOMISE);
			break;

		case TWEAK_ELEM_RESIST:
			assert(tweak->idx >= 0 && tweak->idx < ELEM_MAX);
			obj->el_info[tweak->idx].res_level =
				randcalc(tweak->value, 0, RANDOMISE);
			break;

		case TWEAK_PVAL:
			dice = dice_new();
			if (dice_parse_string(dice, tweak->dice)) {
				expression_t *expr = expression_new();

				expression_set_fixed_base(expr, obj->number);
				dice_bind_expression(dice, "N", expr);
				obj->pval = dice_roll(dice, NULL);
			}
			dice_free(dice);
			break;
		}
	}
	return obj;
}


void tutorial_handle_enter_world(game_event_type t, game_event_data *d,
		void *u)
{
	assert(t == EVENT_ENTER_WORLD);
	event_add_handler(EVENT_PLAYERMOVED, tutorial_handle_player_move, NULL);
	event_add_handler(EVENT_NEW_LEVEL_DISPLAY,
		tutorial_handle_player_move, NULL);
}


void tutorial_handle_leave_world(game_event_type t, game_event_data *d,
		void *u)
{
	assert(t == EVENT_LEAVE_WORLD);
	event_remove_handler(EVENT_PLAYERMOVED, tutorial_handle_player_move,
		NULL);
	event_remove_handler(EVENT_NEW_LEVEL_DISPLAY,
		tutorial_handle_player_move, NULL);
}
