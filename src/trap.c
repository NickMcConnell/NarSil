/**
 * \file trap.c
 * \brief The trap layer - player traps, runes and door locks
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

#include "angband.h"
#include "cave.h"
#include "combat.h"
#include "effects.h"
#include "init.h"
#include "mon-attack.h"
#include "mon-util.h"
#include "obj-knowledge.h"
#include "player-attack.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "songs.h"
#include "trap.h"
#include "tutorial.h"

/**
 * ------------------------------------------------------------------------
 * Intialize traps
 * ------------------------------------------------------------------------ */

static const char *trap_flags[] =
{
	#define TRF(a, b) #a,
	#include "list-trap-flags.h"
	#undef TRF
    NULL
};

static enum parser_error parse_trap_name(struct parser *p) {
    const char *name = parser_getsym(p, "name");
    const char *desc = parser_getstr(p, "desc");
    struct trap_kind *h = parser_priv(p);

    struct trap_kind *t = mem_zalloc(sizeof *t);
    t->next = h;
    t->name = string_make(name);
	t->desc = string_make(desc);
    parser_setpriv(p, t);
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_graphics(struct parser *p) {
    wchar_t glyph = parser_getchar(p, "glyph");
    const char *color = parser_getsym(p, "color");
    int attr = 0;
    struct trap_kind *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->d_char = glyph;
    if (strlen(color) > 1)
		attr = color_text_to_attr(color);
    else
		attr = color_char_to_attr(color[0]);
    if (attr < 0)
		return PARSE_ERROR_INVALID_COLOR;
    t->d_attr = attr;
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_rarity(struct parser *p) {
    struct trap_kind *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->rarity =  parser_getuint(p, "rarity");
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_min_depth(struct parser *p) {
    struct trap_kind *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->min_depth =  parser_getuint(p, "mindepth");
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_max_depth(struct parser *p) {
    struct trap_kind *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->max_depth =  parser_getuint(p, "maxdepth");
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_power(struct parser *p) {
    struct trap_kind *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->power =  parser_getint(p, "power");
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_stealth(struct parser *p) {
    struct trap_kind *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->stealth =  parser_getint(p, "stealth");
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_flags(struct parser *p) {
    char *flags;
    struct trap_kind *t = parser_priv(p);
    char *s;

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

    if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
    flags = string_make(parser_getstr(p, "flags"));

    s = strtok(flags, " |");
    while (s) {
		if (grab_flag(t->flags, TRF_SIZE, trap_flags, s)) {
			mem_free(flags);
			return PARSE_ERROR_INVALID_FLAG;
		}
		s = strtok(NULL, " |");
    }

    mem_free(flags);
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_effect(struct parser *p) {
    struct trap_kind *t = parser_priv(p);
	struct effect *effect;
	struct effect *new_effect = mem_zalloc(sizeof(*new_effect));

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* Go to the next vacant effect and set it to the new one  */
	if (t->effect) {
		effect = t->effect;
		while (effect->next)
			effect = effect->next;
		effect->next = new_effect;
	} else
		t->effect = new_effect;

	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_trap_dice(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	dice_t *dice = NULL;
	struct effect *effect = t->effect;
	const char *string = NULL;

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* If there is no effect, assume that this is human and not parser error. */
	if (effect == NULL)
		return PARSE_ERROR_NONE;

	while (effect->next) effect = effect->next;

	dice = dice_new();

	if (dice == NULL)
		return PARSE_ERROR_INVALID_DICE;

	string = parser_getstr(p, "dice");

	if (dice_parse_string(dice, string)) {
		effect->dice = dice;
	}
	else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_expr(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect = t->effect;
	expression_t *expression = NULL;
	expression_base_value_f function = NULL;
	const char *name;
	const char *base;
	const char *expr;

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* If there is no effect, assume that this is human and not parser error. */
	if (effect == NULL)
		return PARSE_ERROR_NONE;

	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL)
		return PARSE_ERROR_NONE;

	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (expression == NULL)
		return PARSE_ERROR_INVALID_EXPRESSION;

	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0)
		return PARSE_ERROR_BAD_EXPRESSION_STRING;

	if (dice_bind_expression(effect->dice, name, expression) < 0)
		return PARSE_ERROR_UNBOUND_EXPRESSION;

	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_effect_xtra(struct parser *p) {
    struct trap_kind *t = parser_priv(p);
	struct effect *effect;
	struct effect *new_effect = mem_zalloc(sizeof(*new_effect));

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* Go to the next vacant effect and set it to the new one  */
	if (t->effect_xtra) {
		effect = t->effect_xtra;
		while (effect->next)
			effect = effect->next;
		effect->next = new_effect;
	} else
		t->effect_xtra = new_effect;

	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_trap_dice_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	dice_t *dice = NULL;
	struct effect *effect = t->effect_xtra;
	const char *string = NULL;

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* If there is no effect, assume that this is human and not parser error. */
	if (effect == NULL)
		return PARSE_ERROR_NONE;

	while (effect->next) effect = effect->next;

	dice = dice_new();

	if (dice == NULL)
		return PARSE_ERROR_INVALID_DICE;

	string = parser_getstr(p, "dice");

	if (dice_parse_string(dice, string)) {
		effect->dice = dice;
	}
	else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_expr_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect = t->effect_xtra;
	expression_t *expression = NULL;
	expression_base_value_f function = NULL;
	const char *name;
	const char *base;
	const char *expr;

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* If there is no effect, assume that this is human and not parser error. */
	if (effect == NULL)
		return PARSE_ERROR_NONE;

	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL)
		return PARSE_ERROR_NONE;

	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (expression == NULL)
		return PARSE_ERROR_INVALID_EXPRESSION;

	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0)
		return PARSE_ERROR_BAD_EXPRESSION_STRING;

	if (dice_bind_expression(effect->dice, name, expression) < 0)
		return PARSE_ERROR_UNBOUND_EXPRESSION;

	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_desc(struct parser *p) {
    struct trap_kind *t = parser_priv(p);
    assert(t);

    t->text = string_append(t->text, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg(struct parser *p) {
    struct trap_kind *t = parser_priv(p);
    assert(t);

    t->msg = string_append(t->msg, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg2(struct parser *p) {
    struct trap_kind *t = parser_priv(p);
    assert(t);

    t->msg2 = string_append(t->msg2, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg3(struct parser *p) {
    struct trap_kind *t = parser_priv(p);
    assert(t);

    t->msg3 = string_append(t->msg3, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg_vis(struct parser *p) {
    struct trap_kind *t = parser_priv(p);
    assert(t);

    t->msg_vis = string_append(t->msg_vis, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg_silence(struct parser *p) {
    struct trap_kind *t = parser_priv(p);
    assert(t);

    t->msg_silence = string_append(t->msg_silence, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg_good(struct parser *p) {
    struct trap_kind *t = parser_priv(p);
    assert(t);

    t->msg_good = string_append(t->msg_good, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg_bad(struct parser *p) {
    struct trap_kind *t = parser_priv(p);
    assert(t);

    t->msg_bad = string_append(t->msg_bad, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg_xtra(struct parser *p) {
    struct trap_kind *t = parser_priv(p);
    assert(t);

    t->msg_xtra = string_append(t->msg_xtra, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

struct parser *init_parse_trap(void) {
    struct parser *p = parser_new();
    parser_setpriv(p, NULL);
    parser_reg(p, "name sym name str desc", parse_trap_name);
    parser_reg(p, "graphics char glyph sym color", parse_trap_graphics);
    parser_reg(p, "rarity uint rarity", parse_trap_rarity);
    parser_reg(p, "min-depth uint mindepth", parse_trap_min_depth);
    parser_reg(p, "max-depth uint maxdepth", parse_trap_max_depth);
    parser_reg(p, "power int power", parse_trap_power);
    parser_reg(p, "stealth int stealth", parse_trap_stealth);
    parser_reg(p, "flags ?str flags", parse_trap_flags);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_trap_effect);
	parser_reg(p, "dice str dice", parse_trap_dice);
	parser_reg(p, "expr sym name sym base str expr", parse_trap_expr);
	parser_reg(p, "effect-xtra sym eff ?sym type ?int radius ?int other", parse_trap_effect_xtra);
	parser_reg(p, "dice-xtra str dice", parse_trap_dice_xtra);
	parser_reg(p, "expr-xtra sym name sym base str expr", parse_trap_expr_xtra);
	parser_reg(p, "desc str text", parse_trap_desc);
	parser_reg(p, "msg str text", parse_trap_msg);
	parser_reg(p, "msg2 str text", parse_trap_msg2);
	parser_reg(p, "msg3 str text", parse_trap_msg3);
	parser_reg(p, "msg-vis str text", parse_trap_msg_vis);
	parser_reg(p, "msg-silence str text", parse_trap_msg_silence);
	parser_reg(p, "msg-good str text", parse_trap_msg_good);
	parser_reg(p, "msg-bad str text", parse_trap_msg_bad);
	parser_reg(p, "msg-xtra str text", parse_trap_msg_xtra);
    return p;
}

static errr run_parse_trap(struct parser *p) {
    return parse_file_quit_not_found(p, "trap");
}

static errr finish_parse_trap(struct parser *p) {
	struct trap_kind *t, *n;
	int tidx;
	
	/* Scan the list for the max id */
	z_info->trap_max = 0;
	t = parser_priv(p);
	while (t) {
		z_info->trap_max++;
		t = t->next;
	}

	trap_info = mem_zalloc((z_info->trap_max + 1) * sizeof(*t));
	tidx = z_info->trap_max - 1;
    for (t = parser_priv(p); t; t = t->next, tidx--) {
		assert(tidx >= 0);

		memcpy(&trap_info[tidx], t, sizeof(*t));
		trap_info[tidx].tidx = tidx;
		if (tidx < z_info->trap_max - 1)
			trap_info[tidx].next = &trap_info[tidx + 1];
		else
			trap_info[tidx].next = NULL;
    }

    t = parser_priv(p);
    while (t) {
		n = t->next;
		mem_free(t);
		t = n;
    }

    parser_destroy(p);
    return 0;
}

static void cleanup_trap(void)
{
	int i;
	for (i = 0; i < z_info->trap_max; i++) {
		string_free(trap_info[i].name);
		mem_free(trap_info[i].text);
		string_free(trap_info[i].desc);
		string_free(trap_info[i].msg);
		string_free(trap_info[i].msg2);
		string_free(trap_info[i].msg3);
		string_free(trap_info[i].msg_vis);
		string_free(trap_info[i].msg_silence);
		string_free(trap_info[i].msg_good);
		string_free(trap_info[i].msg_bad);
		string_free(trap_info[i].msg_xtra);
		free_effect(trap_info[i].effect);
		free_effect(trap_info[i].effect_xtra);
	}
	mem_free(trap_info);
}

struct file_parser trap_parser = {
    "trap",
    init_parse_trap,
    run_parse_trap,
    finish_parse_trap,
    cleanup_trap
};

/**
 * ------------------------------------------------------------------------
 * General trap routines
 * ------------------------------------------------------------------------ */
struct trap_kind *trap_info;

/**
 * Find a trap kind based on its short description
 */
struct trap_kind *lookup_trap(const char *desc)
{
	int i;
	struct trap_kind *closest = NULL;

	/* Look for it */
	for (i = 1; i < z_info->trap_max; i++) {
		struct trap_kind *kind = &trap_info[i];
		if (!kind->name)
			continue;

		/* Test for equality */
		if (streq(desc, kind->desc))
			return kind;

		/* Test for close matches */
		if (!closest && my_stristr(kind->desc, desc))
			closest = kind;
	}

	/* Return our best match */
	return closest;
}

/**
 * Is there a specific kind of trap in this square?
 */
bool square_trap_specific(struct chunk *c, struct loc grid, int t_idx)
{
    struct trap *trap = square_trap(c, grid);
	
    /* First, check the trap marker */
    if (!square_istrap(c, grid))
		return false;
	
    /* Scan the square trap list */
    while (trap) {
		/* We found a trap of the right kind */
		if (trap->t_idx == t_idx)
			return true;
		trap = trap->next;
	}

    /* Report failure */
    return false;
}

/**
 * Is there a trap with a given flag in this square?
 */
bool square_trap_flag(struct chunk *c, struct loc grid, int flag)
{
    struct trap *trap = square_trap(c, grid);

    /* First, check the trap marker */
    if (!square_istrap(c, grid))
		return false;
	
    /* Scan the square trap list */
    while (trap) {
		/* We found a trap with the right flag */
		if (trf_has(trap->flags, flag))
			return true;
		trap = trap->next;
    }

    /* Report failure */
    return false;
}

/**
 * Determine if a trap actually exists in this square.
 *
 * Called with vis = 0 to accept any trap, = 1 to accept only visible
 * traps, and = -1 to accept only invisible traps.
 *
 * Clear the SQUARE_TRAP flag if none exist.
 */
static bool square_verify_trap(struct chunk *c, struct loc grid, int vis)
{
    struct trap *trap = square_trap(c, grid);
    bool trap_exists = false;

    /* Scan the square trap list */
    while (trap) {
		/* Accept any trap */
		if (!vis)
			return true;

		/* Accept traps that match visibility requirements */
		if ((vis == 1) && trf_has(trap->flags, TRF_VISIBLE)) 
			return true;

		if ((vis == -1)  && !trf_has(trap->flags, TRF_VISIBLE)) 
			return true;

		/* Note that a trap does exist */
		trap_exists = true;
    }

    /* No traps in this location. */
    if (!trap_exists) {
		/* No traps */
		sqinfo_off(square(c, grid)->info, SQUARE_TRAP);

		/* Take note */
		square_note_spot(c, grid);
    }

    /* Report failure */
    return false;
}

/**
 * Free memory for all traps on a grid
 */
void square_free_trap(struct chunk *c, struct loc grid)
{
	struct trap *next, *trap = square_trap(c, grid);

	while (trap) {
		next = trap->next;
		mem_free(trap);
		trap = next;
	}
}

/**
 * Remove all traps from a grid.
 *
 * Return true if traps were removed.
 */
bool square_remove_all_traps(struct chunk *c, struct loc grid)
{
	struct trap *trap = square(c, grid)->trap;
	bool were_there_traps = trap == NULL ? false : true;

	assert(square_in_bounds(c, grid));
	while (trap) {
		struct trap *next_trap = trap->next;
		mem_free(trap);
		trap = next_trap;
	}

	square_set_trap(c, grid, NULL);

	/* Refresh grids that the character can see */
	if (square_isseen(c, grid)) {
		square_light_spot(c, grid);
	}

	(void)square_verify_trap(c, grid, 0);

	return were_there_traps;
}

/**
 * Remove all traps with the given index.
 *
 * Return true if traps were removed.
 */
bool square_remove_trap(struct chunk *c, struct loc grid, int t_idx_remove)
{
	bool removed = false;

	/* Look at the traps in this grid */
	struct trap *prev_trap = NULL;
	struct trap *trap = square(c, grid)->trap;

	assert(square_in_bounds(c, grid));
	while (trap) {
		struct trap *next_trap = trap->next;

		if (t_idx_remove == trap->t_idx) {
			mem_free(trap);
			removed = true;

			if (prev_trap) {
				prev_trap->next = next_trap;
			} else {
				square_set_trap(c, grid, next_trap);
			}

			break;
		}

		prev_trap = trap;
		trap = next_trap;
	}

	/* Refresh grids that the character can see */
	if (square_isseen(c, grid))
		square_light_spot(c, grid);

	(void)square_verify_trap(c, grid, 0);

	return removed;
}

/**
 * ------------------------------------------------------------------------
 * Player traps
 * ------------------------------------------------------------------------ */
/**
 * Determine if a trap affects the player, based on player's evasion.
 */
bool check_hit(int power, bool display_roll, struct source against)
{
	int skill = player->state.skill_use[SKILL_EVASION] +
		player_dodging_bonus(player);
	return hit_roll(power, skill, against, source_player(),
					display_roll) > 0;
}

/**
 * Determine if a cave grid is allowed to have player traps in it.
 */
bool square_player_trap_allowed(struct chunk *c, struct loc grid)
{

    /* We currently forbid multiple traps in a grid under normal conditions.
     * If this changes, various bits of code elsewhere will have to change too.
     */
    if (square_istrap(c, grid))
		return false;

    /* We currently forbid traps in a grid with objects. */
    if (square_object(c, grid))
		return false;

    /* Check it's a trappable square */
    return (square_istrappable(c, grid));
}

/**
 * Instantiate a player trap
 */
static int pick_trap(struct chunk *c, int feat, int trap_level)
{
    int i, pick;
	int *trap_probs = NULL;
	int trap_prob_max = 0;

    /* Paranoia */
    if (!feat_is_trap_holding(feat))
		return -1;

    /* No traps in town */
    if (c->depth == 0)
		return -1;

    /* Get trap probabilities */
	trap_probs = mem_zalloc(z_info->trap_max * sizeof(int));
	for (i = 0; i < z_info->trap_max; i++) {
		/* Get this trap */
		struct trap_kind *kind = &trap_info[i];
		trap_probs[i] = trap_prob_max;

		/* Ensure that this is a valid player trap */
		if (!kind->name) continue;
		if (!kind->rarity) continue;
		if (!trf_has(kind->flags, TRF_TRAP)) continue;

		/* Check depth conditions */
		if (kind->min_depth > trap_level) continue;
		if (kind->max_depth < trap_level) continue;
		if (!trap_level && !trf_has(kind->flags, TRF_SURFACE)) continue;

		/* Floor? */
		if (feat_is_floor(feat) && !trf_has(kind->flags, TRF_FLOOR))
			continue;

		/* Check legality of trapdoors. */
		if (trf_has(kind->flags, TRF_DOWN)) {
			/* No trap doors on the deepest level */
			if (player->depth >= z_info->dun_depth)
				continue;
	    }

		/* Trap is okay, store the cumulative probability */
		trap_probs[i] += (100 / kind->rarity);
		trap_prob_max = trap_probs[i];
	}

	/* No valid trap */
	if (trap_prob_max == 0) {
		mem_free(trap_probs);
		return -1;
	}

	/* Pick at random. */
	pick = randint0(trap_prob_max);
	for (i = 0; i < z_info->trap_max; i++) {
		if (pick < trap_probs[i]) {
			break;
		}
	}

	mem_free(trap_probs);

    /* Return our chosen trap */
    return i < z_info->trap_max ? i : -1;
}

/**
 * Make a new trap of the given type.  Return true if successful.
 *
 * We choose a player trap at random if the index is not legal. This means that
 * things which are not player traps must be picked by passing a valid index.
 *
 * This should be the only function that places traps in the dungeon
 * except the savefile loading code.
 */
void place_trap(struct chunk *c, struct loc grid, int t_idx, int trap_level)
{
	struct trap *new_trap;

	/* We've been called with an illegal index; choose a random trap */
	if ((t_idx <= 0) || (t_idx >= z_info->trap_max)) {
		/* Require the correct terrain */
		if (!square_player_trap_allowed(c, grid)) return;

		t_idx = pick_trap(c, square(c, grid)->feat, trap_level);
	}

	/* Failure */
	if (t_idx < 0) return;
	/* Don't allow trap doors in the tutorial. */
	if (in_tutorial() && trf_has(trap_info[t_idx].flags, TRF_DOWN)) {
		return;
	}

	/* Allocate a new trap for this grid (at the front of the list) */
	new_trap = mem_zalloc(sizeof(*new_trap));
	new_trap->next = square_trap(c, grid);
	square_set_trap(c, grid, new_trap);

	/* Set the details */
	new_trap->t_idx = t_idx;
	new_trap->kind = &trap_info[t_idx];
	new_trap->grid = grid;
	new_trap->power = new_trap->kind->power;
	trf_copy(new_trap->flags, trap_info[t_idx].flags);

	/* Toggle on the trap marker */
	sqinfo_on(square(c, grid)->info, SQUARE_TRAP);

	/* Redraw the grid */
	square_note_spot(c, grid);
	square_light_spot(c, grid);
}

/**
 * Reveal some of the player traps in a square
 */
bool square_reveal_trap(struct chunk *c, struct loc grid, bool domsg)
{
    int found_trap = 0;
	struct trap *trap = square_trap(c, grid);
    
    /* Check there is a player trap */
    if (!square_isplayertrap(c, grid))
		return false;

	/* Scan the grid */
	while (trap) {
		/* Skip non-player traps */
		if (!trf_has(trap->flags, TRF_TRAP)) {
			trap = trap->next;
			continue;
		}
		
		/* Trap is invisible */
		if (!trf_has(trap->flags, TRF_VISIBLE)) {
			/* See the trap (actually, see all the traps) */
			trf_on(trap->flags, TRF_VISIBLE);

			/* We found a trap */
			found_trap++;
		}
		trap = trap->next;
	}

    /* We found at least one trap */
    if (found_trap) {
		/* We want to talk about it */
		if (domsg) {
			if (found_trap == 1)
				msg("You have found a trap.");
			else
				msg("You have found %d traps.", found_trap);
		}

		/* Memorize */
		square_mark(c, grid);

		/* Redraw */
		square_light_spot(c, grid);
    }

    /* Return true if we found any traps */
    return (found_trap != 0);
}

/**
 * Hit a trap. 
 */
void hit_trap(struct loc grid)
{
	bool ident = false;
	struct trap *trap;
	struct effect *effect;

	/* Look at the traps in this grid */
	for (trap = square_trap(cave, grid); trap; trap = trap->next) {
		struct song *silence = lookup_song("Silence");
		bool saved = false;

		/* Require that trap be capable of affecting the character */
		if (!trf_has(trap->kind->flags, TRF_TRAP)) continue;

		/* Disturb the player */
		disturb(player, false);

		/* Give a message */
		if (player_is_singing(player, silence) && trap->kind->msg_silence) {
			msg("%s", trap->kind->msg_silence);
		} else if (trap->kind->msg) {
			msg("%s", trap->kind->msg);
		}
		if (trap->kind->msg2) {
			event_signal(EVENT_MESSAGE_FLUSH);
			msg("%s", trap->kind->msg2);
		}
		if (trap->kind->msg3) {
			event_signal(EVENT_MESSAGE_FLUSH);
			msg("%s", trap->kind->msg3);
		}

		/* Test for save due to saving throw */
		if (trf_has(trap->kind->flags, TRF_SAVE_SKILL)) {
			int result = skill_check(source_player(),
									 player->state.skill_use[SKILL_PERCEPTION],
									 10, source_trap(trap));
			if (result > 0) saved = true;
		}

		/* Save, or fire off the trap */
		if (saved) {
			if (trap->kind->msg_good)
				msg("%s", trap->kind->msg_good);
		} else {
			if (trap->kind->msg_bad)
				msg("%s", trap->kind->msg_bad);
			if (trap->kind->msg_vis && !player->timed[TMD_BLIND])
				msg("%s", trap->kind->msg_vis);

			/* Affect stealth */
			player->stealth_score += trap->kind->stealth;

			effect = trap->kind->effect;
			effect_do(effect, source_trap(trap), NULL, &ident, false, 0, NULL);

			/* Trap may have gone or the player may be dead */
			if (!square_trap(cave, grid) || player->is_dead) break;

			/* Do any extra effects (hack - use ident as the trigger - NRM) */
			if (trap->kind->msg_xtra && ident) {
				msg("%s", trap->kind->msg_xtra);
				if (trap->kind->effect_xtra) {
					effect = trap->kind->effect_xtra;
					effect_do(effect, source_trap(trap), NULL, &ident, false,
							  0, NULL);
				}

				/* Trap may have gone or the player may be dead */
				if (!square_trap(cave, grid) || player->is_dead) break;
			}
		}

		/* Some traps drop you a dungeon level */
		if (trf_has(trap->kind->flags, TRF_DOWN)) {
			int next = dungeon_get_next_level(player, player->depth, 1);
			dungeon_change_level(player, next);
			history_add(player, format("Fell through a %s", trap->kind->name),
						HIST_FELL_DOWN_LEVEL);
		}

		/* Some traps drop you onto them */
		if (trf_has(trap->kind->flags, TRF_PIT))
			monster_swap(player->grid, trap->grid);

		/* Some traps disappear after activating */
		if (trf_has(trap->kind->flags, TRF_ONETIME)) {
			square_destroy_trap(cave, grid);
			square_unmark(cave, grid);
		}

		/* Trap may have gone */
		if (!square_trap(cave, grid)) break;

		/* Trap becomes visible */
		trf_on(trap->flags, TRF_VISIBLE);
	}

    /* Verify traps (remove marker if appropriate) */
    if (square_verify_trap(cave, grid, 0)) {
		/* At least one trap left.  Memorize the grid. */
		square_mark(cave, grid);
    }
    if (square_isseen(cave, grid)) {
		square_light_spot(cave, grid);
    }
}

/**
 * ------------------------------------------------------------------------
 * Door locks and jams
 * ------------------------------------------------------------------------ */
/**
 * Lock a closed door to a given power
 */
void square_set_door_lock(struct chunk *c, struct loc grid, int power)
{
	struct trap_kind *lock = lookup_trap("door lock");
	struct trap *trap;

	/* Verify it's a closed door */
	if (!square_iscloseddoor(c, grid))
		return;

	/* If there's no lock there, add one */
	if (!square_trap_specific(c, grid, lock->tidx))
		place_trap(c, grid, lock->tidx, 0);

	/* Set the power (of all locks - there should be only one) */
	trap = square_trap(c, grid);
	while (trap) {
		if (trap->kind == lock)
			trap->power = power;
		trap = trap->next;
	}
}

/**
 * Return the power of the lock on a door
 */
int square_door_lock_power(struct chunk *c, struct loc grid)
{
	struct trap_kind *lock = lookup_trap("door lock");
	struct trap *trap;

	/* Verify it's a closed door */
	if (!square_iscloseddoor(c, grid))
		return 0;

	/* Is there a lock there? */
	if (!square_trap_specific(c, grid, lock->tidx))
		return 0;

	/* Get the power and return it */
	trap = square_trap(c, grid);
	while (trap) {
		if (trap->kind == lock)
			return trap->power;
		trap = trap->next;
	}

	return 0;
}

/**
 * Jam a closed door to a given power
 */
void square_set_door_jam(struct chunk *c, struct loc grid, int power)
{
	struct trap_kind *jam = lookup_trap("door jam");
	struct trap *trap;

	/* Verify it's a closed door */
	if (!square_iscloseddoor(c, grid))
		return;

	/* If there's no jam there, add one */
	if (!square_trap_specific(c, grid, jam->tidx))
		place_trap(c, grid, jam->tidx, 0);

	/* Set the power (of all jams - there should be only one) */
	trap = square_trap(c, grid);
	while (trap) {
		if (trap->kind == jam)
			trap->power = power;
		trap = trap->next;
	}
}

/**
 * Return the power of the jam on a door
 */
int square_door_jam_power(struct chunk *c, struct loc grid)
{
	struct trap_kind *jam = lookup_trap("door jam");
	struct trap *trap;

	/* Verify it's a closed door */
	if (!square_iscloseddoor(c, grid))
		return 0;

	/* Is there a jam there? */
	if (!square_trap_specific(c, grid, jam->tidx))
		return 0;

	/* Get the power and return it */
	trap = square_trap(c, grid);
	while (trap) {
		if (trap->kind == jam)
			return trap->power;
		trap = trap->next;
	}

	return 0;
}

/**
 * ------------------------------------------------------------------------
 * Forges
 * ------------------------------------------------------------------------ */
/**
 * Set a forge to a given number of uses
 */
void square_set_forge(struct chunk *c, struct loc grid, int uses)
{
	struct trap_kind *forge = lookup_trap("forge use");
	struct trap *trap;

	/* Verify it's a forge */
	if (!square_isforge(c, grid))
		return;

	/* If there's no "forge trap" there, add one */
	if (!square_trap_specific(c, grid, forge->tidx))
		place_trap(c, grid, forge->tidx, 0);

	/* Set the power (of all forges - there should be only one) */
	trap = square_trap(c, grid);
	while (trap) {
		if (trap->kind == forge)
			trap->power = uses;
		trap = trap->next;
	}
}

/**
 * Return the number of uses of a forge
 */
int square_forge_uses(struct chunk *c, struct loc grid)
{
	struct trap_kind *forge = lookup_trap("forge use");
	struct trap *trap;

	/* Verify it's a forge */
	if (!square_isforge(c, grid))
		return 0;

	/* Does it have any uses left? */
	if (!square_trap_specific(c, grid, forge->tidx))
		return 0;

	/* Get the power and return it */
	trap = square_trap(c, grid);
	while (trap) {
		if (trap->kind == forge)
			return trap->power;
		trap = trap->next;
	}

	return 0;
}
