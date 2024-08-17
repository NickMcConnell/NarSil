/**
 * \file obj-chest.c
 * \brief Encapsulation of chest-related functions
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2012 Peter Denison
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
#include "game-input.h"
#include "init.h"
#include "mon-lore.h"
#include "obj-chest.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"

/**
 * Chest traps are specified in the file chest_trap.txt.
 *
 * Chests are described by their pval as follows:
 * - pval of 0 is an empty chest
 * - pval of 1 is a locked chest with no traps
 * - pval > 1  is a trapped chest, with the pval serving as in index into
 *             the chest_trap_list[] array to determine which traps occur
 * - pval < 1  is a disarmed/unlocked chest; the disarming process is simply
 *             to negate the pval
 *
 * The chest pval also determines the difficulty of disarming the chest.
 * Currently the maximum difficulty is 60 (32 + 16 + 8 + 4); if more traps are
 * added to chest_trap.txt, the disarming calculation will need adjusting.
 */

struct chest_trap *chest_traps;

/**
 * Each chest has a certain set of traps, determined by pval
 * Each chest has a "pval" from 1 to the chest level (max 25)
 * If the "pval" is negative then the trap has been disarmed
 * The "pval" of a chest determines the quality of its treasure
 * Note that disarming a trap on a chest also removes the lock.
 */
const uint8_t chest_trap_list[] =
{
	0,					/* 0 == empty */
	(CHEST_GAS_CONF),
	(CHEST_GAS_CONF),
	(CHEST_GAS_STUN),
	0,
	(CHEST_GAS_STUN),
	(CHEST_GAS_POISON),
	(CHEST_GAS_POISON),
	0,
	(CHEST_NEEDLE_ENTRANCE),
	(CHEST_NEEDLE_ENTRANCE),
	(CHEST_NEEDLE_HALLU),
	0,
	(CHEST_NEEDLE_HALLU),
	(CHEST_NEEDLE_LOSE_STR),
	(CHEST_NEEDLE_LOSE_STR),
	0,
	(CHEST_GAS_CONF | CHEST_NEEDLE_HALLU),
	(CHEST_GAS_CONF | CHEST_NEEDLE_HALLU),
	(CHEST_GAS_STUN | CHEST_NEEDLE_LOSE_STR),
	0,
	(CHEST_GAS_STUN | CHEST_NEEDLE_LOSE_STR),
	(CHEST_GAS_POISON | CHEST_NEEDLE_ENTRANCE),
	(CHEST_GAS_POISON | CHEST_NEEDLE_ENTRANCE),
	0,
	(CHEST_GAS_POISON | CHEST_NEEDLE_ENTRANCE),			/* 25 == best */
};

/**
 * ------------------------------------------------------------------------
 * Parsing functions for chest_trap.txt and chest.txt
 * ------------------------------------------------------------------------ */
static enum parser_error parse_chest_trap_name(struct parser *p)
{
    const char *name = parser_getstr(p, "name");
    struct chest_trap *h = parser_priv(p);
    struct chest_trap *t = mem_zalloc(sizeof *t);

	/* Order the traps correctly and set the flag */
	if (h) {
		h->next = t;
		t->flag = h->flag ? h->flag * 2 : 1;
	} else {
		chest_traps = t;
	}
    t->name = string_make(name);
    parser_setpriv(p, t);
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_effect(struct parser *p) {
    struct chest_trap *t = parser_priv(p);
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

static enum parser_error parse_chest_trap_dice(struct parser *p) {
	struct chest_trap *t = parser_priv(p);
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

static enum parser_error parse_chest_trap_msg(struct parser *p) {
    struct chest_trap *t = parser_priv(p);

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->msg = string_append(t->msg, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_msg_save(struct parser *p) {
    struct chest_trap *t = parser_priv(p);

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->msg_save = string_append(t->msg_save, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_msg_bad(struct parser *p) {
    struct chest_trap *t = parser_priv(p);

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->msg_bad = string_append(t->msg_bad, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_msg_death(struct parser *p) {
    struct chest_trap *t = parser_priv(p);

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->msg_death = string_append(t->msg_death, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

struct parser *init_parse_chest_trap(void) {
    struct parser *p = parser_new();
    parser_setpriv(p, NULL);
    parser_reg(p, "name str name", parse_chest_trap_name);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_chest_trap_effect);
	parser_reg(p, "dice str dice", parse_chest_trap_dice);
	parser_reg(p, "msg str text", parse_chest_trap_msg);
	parser_reg(p, "msg-save str text", parse_chest_trap_msg_save);
	parser_reg(p, "msg-bad str text", parse_chest_trap_msg_bad);
	parser_reg(p, "msg-death str text", parse_chest_trap_msg_death);
    return p;
}

static errr run_parse_chest_trap(struct parser *p) {
    return parse_file_quit_not_found(p, "chest_trap");
}

static errr finish_parse_chest_trap(struct parser *p) {
	parser_destroy(p);
	return 0;
}

static void cleanup_chest_trap(void)
{
	struct chest_trap *trap = chest_traps;
	while (trap) {
		struct chest_trap *old = trap;
		string_free(trap->name);
		string_free(trap->msg);
		string_free(trap->msg_save);
		string_free(trap->msg_bad);
		string_free(trap->msg_death);
		free_effect(trap->effect);
		trap = trap->next;
		mem_free(old);
	}
}

struct file_parser chest_trap_parser = {
    "chest_trap",
    init_parse_chest_trap,
    run_parse_chest_trap,
    finish_parse_chest_trap,
    cleanup_chest_trap
};

/**
 * ------------------------------------------------------------------------
 * Chest trap information
 * ------------------------------------------------------------------------ */
/**
 * The name of a chest trap
 */
const char *chest_trap_name(const struct object *obj)
{
	int16_t trap_value = obj->pval;

	/* Non-zero value means there either were or are still traps */
	if (trap_value < 0) {
		return (trap_value == -1) ? "unlocked" : "disarmed";
	} else if (trap_value > 0) {
		struct chest_trap *trap = chest_traps, *found = NULL;
		while (trap) {
			if (trap_value & trap->flag) {
				if (found) {
					return "multiple traps";
				}
				found = trap;
			}
			trap = trap->next;
		}
		if (found) {
			return found->name;
		}
	}

	return "empty";
}

/**
 * Determine if a chest is trapped
 */
bool is_trapped_chest(const struct object *obj)
{
	if (!tval_is_chest(obj))
		return false;

	/* Disarmed or opened chests are not trapped */
	if (obj->pval <= 0)
		return false;

	/* Some chests simply don't have traps */
	return (obj->pval == 1) ? false : true;
}


/**
 * Determine if a chest is locked or trapped
 */
bool is_locked_chest(const struct object *obj)
{
	if (!tval_is_chest(obj))
		return false;

	/* Disarmed or opened chests are not locked */
	return (obj->pval > 0);
}

/**
 * ------------------------------------------------------------------------
 * Chest trap actions
 * ------------------------------------------------------------------------ */
/**
 * Unlock a chest
 */
void unlock_chest(struct object *obj)
{
	obj->pval = (0 - obj->pval);
}

/**
 * Determine if a grid contains a chest matching the query type, and
 * return a pointer to the first such chest
 */
struct object *chest_check(const struct player *p, struct loc grid,
		enum chest_query check_type)
{
	struct object *obj;

	/* Scan all objects in the grid */
	for (obj = square_object(cave, grid); obj; obj = obj->next) {
		/* Ignore if requested */
		if (ignore_item_ok(p, obj)) continue;

		/* Check for chests */
		switch (check_type) {
		case CHEST_ANY:
			if (tval_is_chest(obj))
				return obj;
			break;
		case CHEST_OPENABLE:
			if (tval_is_chest(obj) && (obj->pval != 0))
				return obj;
			break;
		case CHEST_TRAPPED:
			if (is_trapped_chest(obj) && obj->known && obj->known->pval)
				return obj;
			break;
		}
	}

	/* No chest */
	return NULL;
}


/**
 * Return the number of grids holding a chests around (or under) the character.
 * If requested, count only trapped chests.
 */
int count_chests(struct loc *grid, enum chest_query check_type)
{
	int d, count;

	/* Count how many matches */
	count = 0;

	/* Check around (and under) the character */
	for (d = 0; d < 9; d++) {
		/* Extract adjacent (legal) location */
		struct loc grid1 = loc_sum(player->grid, ddgrid_ddd[d]);

		/* No (visible) chest is there */
		if (!chest_check(player, grid1, check_type)) continue;

		/* Count it */
		++count;

		/* Remember the location of the last chest found */
		*grid = grid1;
	}

	/* All done */
	return count;
}


/**
 * Choose the theme for a chest
 */
static struct drop *choose_chest_contents(void)
{
	struct drop *theme;
	int pick, count = 0;

	/* Count the possible themes */
	for (theme = drops; theme; theme = theme->next) {
		if (theme->chest) count++;
	}

	/* Pick one at random, find it */
	pick = randint0(count);
	for (theme = drops; theme; theme = theme->next) {
		if (theme->chest) count--;
		if (count == pick) break;
	}
	assert(theme);
	return theme;
}

/**
 * Allocate objects upon opening a chest
 *
 * Disperse treasures from the given chest, centered at (x,y).
 *
 * Small chests get 2-3 objects, large chests get 4.
 *
 * Judgment of size and construction of chests is currently made from the name.
 */
static void chest_death(struct loc grid, struct object *chest)
{
	int number = 1, level;
	struct drop *theme;

	/* Zero pval means empty chest */
	if (!chest->pval)
		return;

	/* Determine how much to drop (see above) */
	if (strstr(chest->kind->name, "Small")) {
		number = rand_range(2, 3);
	} else if (strstr(chest->kind->name, "Large")) {
		number = 4;
	} else if (strstr(chest->kind->name, "present")) {
		number = 1;
	}

	/* Drop some objects (non-chests) */
	level = chest->pval;
	theme = choose_chest_contents();
	while (number > 0) {
		int quality = randint1(level);
		struct object *treasure;
		bool good = false, great = false;

		/* Determine quality */
		if (strstr(chest->kind->name, "steel")) {
			quality += 5;
		} else if (strstr(chest->kind->name, "jewelled")) {
			quality += 10;
		} else if (strstr(chest->kind->name, "present")) {
			quality += 20;
		}

		/* Decide if object is good, great or both */
		if (quality > 10) {
			if (quality <= 15) {
				good = true;
			} else if (quality <= 20) {
				great = true;
			} else {
				good = true;
				great = true;
			}
		}

		/* Sil sets a limit on number of tries; hoping that's not needed NRM */
		treasure = make_object(cave, level, good, great, theme);
		if (!treasure)
			continue;
		if (tval_is_chest(treasure)) {
			object_delete(cave, player->cave, &treasure);
			continue;
		}

		treasure->origin = ORIGIN_CHEST;
		treasure->origin_depth = chest->origin_depth;
		drop_near(cave, &treasure, 0, grid, true, false);
		number--;
	}

	/* Chest is now empty */
	chest->pval = 0;
	chest->known->pval = 0;
}


/**
 * Chests have traps too.
 */
static void chest_trap(struct object *obj)
{
	uint8_t flags; 
	struct chest_trap *trap;
	bool ident = false;
	int old[TMD_MAX];

	/* Ignore disarmed chests */
	if (obj->pval <= 0) return;

	/* Record current timed effect status */
	memcpy(old, player->timed, TMD_MAX);

	/* Get the flags */
	assert(obj->pval < (int) N_ELEMENTS(chest_trap_list));
	flags = chest_trap_list[obj->pval];

	/* Apply trap effects */
	for (trap = chest_traps; trap; trap = trap->next) {
		if (flags & trap->flag) {
			bool save = false;
			if (trap->msg_save) {
				int difficulty = player->state.stat_use[STAT_DEX] * 2;
				if (skill_check(source_none(), 2, difficulty, source_player())
					<= 0) {
					save = true;
				}
			}
			if (save) {
				msg(trap->msg_save);
			} else {
				if (trap->msg) {
					msg(trap->msg);
				}
				if (trap->effect) {
					effect_do(trap->effect, source_chest_trap(trap), obj,
							  &ident, false, DIR_NONE, NULL);
					/* Bit of a hack */
					if (trap->msg_bad) {
						if (player_timed_inc_happened(player, old, TMD_MAX)) {
							msg(trap->msg_bad);
						} else {
							msg("You resist the effects.");
						}
					}
				}
			}
			if (trap->destroy) {
				obj->pval = 0;
				break;
			}
		}
	}
}


/**
 * Attempt to open the given chest at the given location
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
bool do_cmd_open_chest(struct loc grid, struct object *obj)
{
	bool flag = true;
	bool more = false;

	/* Cause problems opening presents before Christmas day */
	if (strstr(obj->kind->name, "present")) {
		time_t c = time((time_t *)0);
		struct tm *tp = localtime(&c);
		
		if ((tp->tm_mon == 11) && (tp->tm_mday >= 20) && (tp->tm_mday < 25)) {
			if (get_check("Are you sure you wish to open your present before Christmas? ")) {
				msg("You have a very bad feeling about this.");
				player->cursed = true;
			} else {
				return false;
			}
		}
	}

	/* Attempt to unlock it */
	if (obj->pval > 0) {
		/* Get the score in favour (=perception) */
		int score = player->state.skill_use[SKILL_PERCEPTION];

		/* Determine trap power based on the chest pval (power is 1--7) */
		int power = 1 + (obj->pval / 4);

		/* Base difficulty is the lock power + 5 */
		int difficulty = power + 5;

		/* Assume locked, and thus not open */
		flag = false;

		/* Penalize some conditions */
		if (player->timed[TMD_BLIND] || no_light(player) ||
			player->timed[TMD_CONFUSED] || player->timed[TMD_IMAGE]) {
			difficulty += 5;
		}

		/* Success -- May still have traps */
		if (skill_check(source_player(), score, difficulty, source_none()) > 0){
			msg("You have picked the lock.");
			flag = true;
		} else {
			/* We may continue repeating */
			more = true;
			event_signal(EVENT_INPUT_FLUSH);
			msgt(MSG_LOCKPICK_FAIL, "You failed to pick the lock.");
		}
	}

	/* Allowed to open */
	if (flag) {
		/* Apply chest traps, if any and player is not trapsafe */
		chest_trap(obj);

		/* Let the Chest drop items */
		chest_death(grid, obj);

		/* Ignore chest if autoignore calls for it */
		player->upkeep->notice |= PN_IGNORE;
	}

	/* Empty chests were always ignored in ignore_item_okay so we
	 * might as well ignore it here
	 */
	if (obj->pval == 0)
		obj->known->notice |= OBJ_NOTICE_IGNORE;

	/* Redraw chest, to be on the safe side (it may have been ignored) */
	square_light_spot(cave, grid);

	/* Result */
	return (more);
}


/**
 * Attempt to disarm the chest at the given location
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
bool do_cmd_disarm_chest(struct object *obj)
{
	int result;
	bool more = false;

	/* Get the score in favour (=perception) */
	int score = player->state.skill_use[SKILL_PERCEPTION];

	/* Determine trap power (= difficulty; power is 1--7) */
	int difficulty = 1 + (obj->pval / 4);

	/* Penalize some conditions */
	if (player->timed[TMD_BLIND] || no_light(player) ||
		player->timed[TMD_CONFUSED] || player->timed[TMD_IMAGE]) {
		difficulty += 5;
	}

	/* Perform the check */
	result = skill_check(source_player(), score, difficulty, source_none());

	/* Must find the trap first. */
	if (!obj->known->pval || ignore_item_ok(player, obj)) {
		msg("I don't see any traps.");
	} else if (!is_trapped_chest(obj)) {
		/* Already disarmed/unlocked or no traps */
		msg("The chest is not trapped.");
	} else if (result > 0) {
		/* Success */
		msgt(MSG_DISARM, "You have disarmed the chest.");
		obj->pval = (0 - obj->pval);
	} else if (result > -3) {
		/* Failure -- Keep trying */
		more = true;
		event_signal(EVENT_INPUT_FLUSH);
		msg("You failed to disarm the chest.");
	} else {
		/* Failure -- Set off the trap */
		msg("You set off a trap!");
		chest_trap(obj);
	}

	/* Result */
	return more;
}
