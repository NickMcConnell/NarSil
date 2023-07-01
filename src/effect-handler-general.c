/**
 * \file effect-handler-general.c
 * \brief Handler functions for general effects
 *
 * Copyright (c) 2007 Andi Sidwell
 * Copyright (c) 2016 Ben Semmler, Nick McConnell
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

#include "cave.h"
#include "combat.h"
#include "effect-handler.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-summon.h"
#include "mon-util.h"
#include "obj-chest.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "songs.h"
#include "source.h"
#include "target.h"
#include "trap.h"


/**
 * Set value for a chain of effects
 */
static int set_value = 0;

int effect_calculate_value(effect_handler_context_t *context)
{
	int final = 0;

	if (set_value) {
		return set_value;
	}

	if (context->value.base > 0 ||
		(context->value.dice > 0 && context->value.sides > 0)) {
		final = context->value.base +
			damroll(context->value.dice, context->value.sides);
	}

	return final;
}

/**
 * Stat adjectives
 */
static const char *desc_stat(int stat, bool positive)
{
	struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_STAT, stat);
	if (positive) {
		return prop->adjective;
	}
	return prop->neg_adj;
}

/**
 * Attempt to close a single square of chasm.
 *
 * Marks grids to be closed with the SQUARE_TEMP flag..
 */
static bool close_chasm(struct loc grid, int power)
{
    int adj_chasms = 0;
    int y, x;
    bool effect = false;

    for (y = grid.y - 1; y <= grid.y + 1; y++) {
        for (x = grid.x - 1; x <= grid.x + 1; x++) {
			struct loc adj = loc(x, y);
            if (!loc_eq(adj, grid) && square_in_bounds(cave, adj) &&
				square_ischasm(cave, adj)) {
				adj_chasms++;
			}
		}
    }

    /* Cannot close chasms that are completely surrounded */
    if (adj_chasms < 8) {
        if (skill_check(source_player(), power, 20 + adj_chasms,
						source_none()) > 0) {
            square_mark(cave, grid);
            effect = true;
        }
    }

    return effect;
}

/**
 * Close all marked chasms
 */
static void close_marked_chasms(void)
{
	struct loc grid;

	/* Search the whole map */
	for (grid.y = 0; grid.y < cave->height; grid.y++) {
		for (grid.x = 0; grid.x < cave->width; grid.x++) {
			/* Find all the marked chasms */
			if (square_ischasm(cave, grid) && square_ismark(cave, grid)) {
				/* Unmark and add floor */
				square_unmark(cave, grid);
				square_set_feat(cave, grid, FEAT_FLOOR);

				/* Memorize */
				square_mark(cave, grid);
				square_light_spot(cave, grid);
			}
		}
	}
}

/**
 * Selects items that have at least one unknown rune.
 */
static bool item_tester_unknown(const struct object *obj)
{
    return object_is_known(obj) ? false : true;
}

/**
 * ------------------------------------------------------------------------
 * Effect handlers
 * ------------------------------------------------------------------------ */
/**
 * Feed the player, or set their satiety level.
 */
bool effect_handler_NOURISH(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context);
	if (context->subtype == 0) {
		/* Increase food level by amount */
		player_inc_timed(player, TMD_FOOD, MAX(amount, 0), false, false);
	} else if (context->subtype == 1) {
		/* Decrease food level by amount */
		player_dec_timed(player, TMD_FOOD, MAX(amount, 0), false);
	} else {
		return false;
	}
	context->ident = true;
	return true;
}

/**
 * Cure a player status condition.
 */
bool effect_handler_CURE(effect_handler_context_t *context)
{
	int type = context->subtype;
	(void) player_clear_timed(player, type, true);
	context->ident = true;
	return true;
}

/**
 * Set a (positive or negative) player status condition.
 */
bool effect_handler_TIMED_SET(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context);
	player_set_timed(player, context->subtype, MAX(amount, 0), true);
	context->ident = true;
	return true;
}

/**
 * Extend a (positive or negative) player status condition.
 */
bool effect_handler_TIMED_INC(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context);
	player_inc_timed(player, context->subtype, MAX(amount, 0), true, true);
	context->ident = true;
	return true;
}

/**
 * Check if we can impose a player status condition.
 * This effect uses context->ident to report whether or not the check is
 * successful, so should never be used for objects.
 */
bool effect_handler_TIMED_INC_CHECK(effect_handler_context_t *context)
{
	context->ident = player_inc_check(player, context->subtype, false);
	return true;
}

/**
 * Extend a (positive or negative) player status condition unresistably.
 */
bool effect_handler_TIMED_INC_NO_RES(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context);
	player_inc_timed(player, context->subtype, MAX(amount, 0), true, false);
	context->ident = true;
	return true;
}

/**
 * Create a glyph.
 */
bool effect_handler_GLYPH(effect_handler_context_t *context)
{
	/* Always notice */
	context->ident = true;

	/* See if the effect works */
	if (!square_istrappable(cave, player->grid)) {
		msg("You cannot draw a glyph without a clean expanse of floor.");
		return false;
	}

	/* Push objects off the grid */
	if (square_object(cave, player->grid))
		push_object(player->grid);

	/* Create a glyph */
	msg("You trace out a glyph of warding upon the floor.");
	square_add_glyph(cave, player->grid, context->subtype);

	return true;
}

/**
 * Restore a stat; the stat index is context->subtype
 */
bool effect_handler_RESTORE_STAT(effect_handler_context_t *context)
{
	int stat = context->subtype;
	int gain = effect_calculate_value(context);

	/* Check bounds */
	if (stat < 0 || stat >= STAT_MAX) return false;

	/* Attempt to increase */
	if (player_stat_res(player, stat, gain)) {
		/* Message */
		msg("You feel less %s.", desc_stat(stat, false));

		/* ID */
		context->ident = true;
	}

	return true;
}

/**
 * Drain a stat temporarily.  The stat index is context->subtype.
 */
bool effect_handler_DRAIN_STAT(effect_handler_context_t *context)
{
	int stat = context->subtype;
	int flag = sustain_flag(stat);
	struct monster *mon = cave_monster(cave, cave->mon_current);

	/* Bounds check */
	if (flag < 0) return false;

	/* Sustain */
	if (player_saving_throw(player, mon, player->state.flags[flag])) {
		/* Message */
		msg("You feel %s for a moment, but it passes.", desc_stat(stat, false));

		/* Notice effect */
		ident_flag(player, flag);
		context->ident = true;

		return true;
	}

	/* Reduce the stat */
	player_stat_dec(player, stat);
	msgt(MSG_DRAIN_STAT, "You feel %s.", desc_stat(stat, false));

	/* ID */
	context->ident = true;

	return true;
}

bool effect_handler_RESTORE_MANA(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context);
	if (!amount) amount = player->msp;
	if (player->csp < player->msp) {
		player->csp += amount;
		if (player->csp > player->msp) {
			player->csp = player->msp;
			player->csp_frac = 0;
			msg("You feel your power renew.");
		} else
			msg("You feel your power renew somewhat.");
		player->upkeep->redraw |= (PR_MANA);
	}
	context->ident = true;

	return true;
}

/**
 * Uncurse all equipped objects
 */
bool effect_handler_REMOVE_CURSE(effect_handler_context_t *context)
{
	int i;
	bool removed = false;

	for (i = 0; i < player->body.count; i++) {
		struct object *obj = slot_object(player, i);

		/* Skip non-objects, non-cursed objects */
		if (!obj || !obj->kind) continue;
		if (!object_is_cursed(obj)) continue;

		/* Uncurse the object */
		uncurse_object(obj);
		removed = true;
	}

	if (removed) {
		context->ident = true;
		msg("You feel sanctified.");
	}

	return true;
}

/**
 * Map the dungeon level.
 */
bool effect_handler_MAP_AREA(effect_handler_context_t *context)
{
	int i, x, y;

	/* Scan the dungeon */
	for (y = 1; y < cave->height - 1; y++) {
		for (x = 1; x < cave->width - 1; x++) {
			struct loc grid = loc(x, y);

			/* All non-walls are "checked" */
			if (!square_seemslikewall(cave, grid)) {
				if (!square_in_bounds_fully(cave, grid)) continue;

				/* Memorize normal features */
				if (!square_isfloor(cave, grid))
					square_mark(cave, grid);

				/* Memorize known walls */
				for (i = 0; i < 8; i++) {
					int yy = y + ddy_ddd[i];
					int xx = x + ddx_ddd[i];

					/* Memorize walls (etc) */
					if (square_seemslikewall(cave, loc(xx, yy)))
						square_mark(cave, loc(xx, yy));
				}
			}

			/* Forget unprocessed, unknown grids in the mapping area */
			if (square_isnotknown(cave, grid))
				square_unmark(cave, grid);
		}
	}

	/* Fully update the visuals */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw whole map, monster list */
	player->upkeep->redraw |= (PR_MAP | PR_MONLIST | PR_ITEMLIST);

	/* Notice */
	context->ident = true;

	return true;
}

/**
 * Detect traps in the player's line of sight.
 */
bool effect_handler_DETECT_TRAPS(effect_handler_context_t *context)
{
	struct loc grid;
	bool detect = false;

	/* Affect all viewable grids */
	for (grid.y = player->grid.y - z_info->max_sight;
		 grid.y <= player->grid.y + z_info->max_sight; grid.y++) {
		for (grid.x = player->grid.x - z_info->max_sight;
			 grid.x <= player->grid.x + z_info->max_sight; grid.x++) {
			/* Grid must be in bounds and in the player's LoS */
			if (!square_in_bounds_fully(cave, grid)) continue;
			if (!square_isview(cave, grid)) continue;

			/* Detect traps */
			if (square_isplayertrap(cave, grid)) {
				/* Reveal trap */
				if (square_reveal_trap(cave, grid, false)) {
					detect = true;
				}
			}
		}
	}

	/* Describe */
	if (detect) {
		msg("You sense the presence of traps!");
	}

	/* Notice */
	context->ident = true;

	return true;
}

/**
 * Detect doors in the player's line of sight.
 */
bool effect_handler_DETECT_DOORS(effect_handler_context_t *context)
{
	struct loc grid;
	bool doors = false;

	/* Affect all viewable grids */
	for (grid.y = player->grid.y - z_info->max_sight;
		 grid.y <= player->grid.y + z_info->max_sight; grid.y++) {
		for (grid.x = player->grid.x - z_info->max_sight;
			 grid.x <= player->grid.x + z_info->max_sight; grid.x++) {
			/* Grid must be in bounds and in the player's LoS */
			if (!square_in_bounds_fully(cave, grid)) continue;
			if (!square_isview(cave, grid)) continue;

			/* Detect secret doors */
			if (square_issecretdoor(cave, grid)) {
				/* Put an actual door */
				place_closed_door(cave, grid);

				/* Memorize */
				square_mark(cave, grid);
				square_light_spot(cave, grid);

				/* Obvious */
				doors = true;
			}
		}
	}

	/* Describe */
	if (doors)
		msg("You sense the presence of doors!");
	else if (context->aware)
		msg("You sense no doors.");

	context->ident = true;

	return true;
}

/**
 * Detect monsters which satisfy the given predicate around the player.
 */
static bool detect_monsters(monster_predicate pred)
{
	int i;
	bool monsters = false;

	/* Scan monsters */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Skip dead monsters */
		if (!mon->race) continue;

		/* Detect all appropriate, obvious monsters */
		if (!pred || pred(mon)) {
			/* Detect the monster */
			mflag_on(mon->mflag, MFLAG_MARK);
			mflag_on(mon->mflag, MFLAG_SHOW);

			/* Note invisible monsters */
			if (monster_is_invisible(mon)) {
				struct monster_lore *lore = get_lore(mon->race);
				rf_on(lore->flags, RF_INVISIBLE);
			}

			/* Update monster recall window */
			if (player->upkeep->monster_race == mon->race)
				/* Redraw stuff */
				player->upkeep->redraw |= (PR_MONSTER);

			/* Update the monster */
			update_mon(mon, cave, false);

			/* Detect */
			monsters = true;
		}
	}

	return monsters;
}

/**
 * Detect objects on the level.
 */
bool effect_handler_DETECT_OBJECTS(effect_handler_context_t *context)
{
	int x, y;
	bool objects = false;

	/* Scan the area for objects */
	for (y = 1; y <= cave->height - 1; y++) {
		for (x = 1; x <= cave->width - 1; x++) {
			struct loc grid = loc(x, y);
			struct object *obj = square_object(cave, grid);
			if (!obj) continue;

			/* Notice an object is detected */
			if (!ignore_item_ok(player, obj)) {
				objects = true;
				context->ident = true;
			}

			/* Mark the pile as seen */
			square_know_pile(cave, grid);
		}
	}

	if (objects)
		msg("You detect the presence of objects!");
	else if (context->aware)
		msg("You detect no objects.");

	/* Redraw whole map, monster list */
	player->upkeep->redraw |= PR_ITEMLIST;

	return true;
}

/**
 * Detect monsters on the level.
 */
bool effect_handler_DETECT_MONSTERS(effect_handler_context_t *context)
{
	bool monsters = detect_monsters(NULL);

	if (monsters) {
		msg("You sense the presence of your enemies!");
		context->ident = true;
	}
	return monsters;
}

/**
 * Reveal an invisible monster.
 */
bool effect_handler_REVEAL_MONSTER(effect_handler_context_t *context)
{
	assert(context->origin.what == SRC_MONSTER);

	struct monster *mon = cave_monster(cave, context->origin.which.monster);
	char m_name[80];
	struct monster_lore *lore = get_lore(mon->race);

	/* Reject if no effect */
	if (monster_is_visible(mon) || !rf_has(mon->race->flags, RF_INVISIBLE)) {
		return false;
	}

	/* Mark as visible */
	mflag_on(mon->mflag, MFLAG_VISIBLE);

	/* Re-draw the spot*/
	square_light_spot(cave, mon->grid);

	/* Get the monster name */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_DEFAULT);

	/* Monster forgets player history (?) */
	msg("%s appears for an instant!", m_name);

	/* Update the lore*/
	rf_on(lore->flags, RF_INVISIBLE);

	context->ident = true;

	return true;
}


/**
 * Close chasms in the player's line of sight.
 */
bool effect_handler_CLOSE_CHASMS(effect_handler_context_t *context)
{
	struct loc grid;
	bool closed = false;
	int power = effect_calculate_value(context);

	/* Affect all viewable grids */
	for (grid.y = player->grid.y - z_info->max_sight;
		 grid.y <= player->grid.y + z_info->max_sight; grid.y++) {
		for (grid.x = player->grid.x - z_info->max_sight;
			 grid.x <= player->grid.x + z_info->max_sight; grid.x++) {
			/* Grid must be in bounds and in the player's LoS */
			if (!square_in_bounds_fully(cave, grid)) continue;
			if (!square_isview(cave, grid)) continue;

			/* Attempt to mark chasms for closing */
			if (square_ischasm(cave, grid)) {
				closed |= close_chasm(grid, power);
			}
		}
	}

	if (closed) {
		close_marked_chasms();
	}

	context->ident = true;

	return true;
}

/**
 * Identify an unknown item.
 */
bool effect_handler_IDENTIFY(effect_handler_context_t *context)
{
	struct object *obj;
	const char *q, *s;
	int itemmode = (USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR);
	bool used = false;

	context->ident = true;

	/* Get an item */
	q = "Identify which item? ";
	s = "You have nothing to identify.";
	if (context->cmd) {
		if (cmd_get_item(context->cmd, "tgtitem", &obj, q, s,
				item_tester_unknown, itemmode)) {
			return used;
		}
	} else if (!get_item(&obj, q, s, 0, item_tester_unknown, itemmode))
		return used;

	/* Identify the object */
	ident(obj);

	return true;
}


/**
 * Recharge a staff from the pack or on the floor.  Number of charges
 * is context->value.base.
 */
bool effect_handler_RECHARGE(effect_handler_context_t *context)
{
	int num = context->value.base;
	int itemmode = (USE_INVEN | USE_FLOOR);
	struct object *obj;
	bool used = false;
	const char *q, *s;

	/* Immediately obvious */
	context->ident = true;

	/* Get an item */
	q = "Recharge which item? ";
	s = "You have nothing to recharge.";
	if (context->cmd) {
		if (cmd_get_item(context->cmd, "tgtitem", &obj, q, s,
				tval_can_have_charges, itemmode)) {
			return used;
		}
	} else if (!get_item(&obj, q, s, 0, tval_can_have_charges, itemmode)) {
		return (used);
	}

	obj->pval += num;

	/* Combine the pack (later) */
	player->upkeep->notice |= (PN_COMBINE);

	/* Redraw stuff */
	player->upkeep->redraw |= (PR_INVEN);

	/* Something was done */
	return true;
}

/**
 * Summon context->value monsters of context->subtype type.
 * If context->other is set, summon random monsters on stairs
 */
bool effect_handler_SUMMON(effect_handler_context_t *context)
{
	int summon_max = effect_calculate_value(context);
	int summon_type = context->subtype;
	bool stairs = context->other ? true : false;
	int level_boost = damroll(2, 2) - damroll(2, 2);
	int message_type = summon_message_type(summon_type);
	int count = 0;

	sound(message_type);

	if (stairs) {
		int i;
		for (i = 0; i < summon_max; i++) {
			if (pick_and_place_monster_on_stairs(cave, player, false,
												 player->depth, false))
				context->ident = true;
		}
	} else {
		/* Summon some monsters */
		while (summon_max) {
			count += summon_specific(player->grid, player->depth + level_boost,
									 summon_type);
			summon_max--;
		}

		/* Identify */
		context->ident = count ? true : false;
	}

	return true;
}

/**
 * Teleport player or target monster to a grid near the given location
 * This function is slightly obsessive about correctness.
 * This function allows teleporting into vaults (!)
 */
bool effect_handler_TELEPORT_TO(effect_handler_context_t *context)
{
	struct loc start = player->grid, aim, land;
	int dis = 0, ctr = 0, dir = DIR_TARGET;

	context->ident = true;

	/* Where are we going? */
	do {
		if (!get_aim_dir(&dir, cave->width)) return false;
	} while (dir == DIR_TARGET && !target_okay(cave->width));

	if (dir == DIR_TARGET)
		target_get(&aim);
	else
		aim = loc_offset(start, ddx[dir], ddy[dir]);


	/* Find a usable location */
	while (1) {
		/* Pick a nearby legal location */
		while (1) {
			land = rand_loc(aim, dis, dis);
			if (square_in_bounds_fully(cave, land)) break;
		}

		/* Accept "naked" floor grids */
		if (square_isempty(cave, land)) break;

		/* Occasionally advance the distance */
		if (++ctr > (4 * dis * dis + 4 * dis + 1)) {
			ctr = 0;
			dis++;
		}
	}

	/* Sound */
	sound(MSG_TELEPORT);

	/* Move player or monster */
	monster_swap(start, land);

	/* Cancel target if necessary */
	target_set_location(loc(0, 0));

	/* Lots of updates after monster_swap */
	handle_stuff(player);

	return true;
}

bool effect_handler_DARKEN_LEVEL(effect_handler_context_t *context)
{
	wiz_dark(cave, player);
	context->ident = true;
	return true;
}

/**
 * Call light around the player
 */
bool effect_handler_LIGHT_AREA(effect_handler_context_t *context)
{
	struct loc pgrid = player->grid;
	int rad = context->radius ? context->radius : 0;

	int flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_KILL;

	/* Message */
	if (!player->timed[TMD_BLIND])
		msg("You are surrounded by a white light.");

	/* Lots of light */
	(void) project(source_player(), rad, pgrid, context->value.dice,
				   context->value.sides, -1, context->subtype, flg, 0, false,
				   NULL);

	/* Assume seen */
	context->ident = true;
	return (true);
}


/**
 * Call darkness around the player or target monster
 */
bool effect_handler_DARKEN_AREA(effect_handler_context_t *context)
{
	bool message = player->timed[TMD_BLIND] ? false : true;

	if (message) {
		msg("Darkness surrounds you.");
	}

	/* Darken the room */
	light_room(player->grid, false);

	/* Assume seen */
	context->ident = true;
	return (true);
}

/**
 * Attempt to decrease morale of all intelligent monsters.
 */
bool effect_handler_SONG_OF_ELBERETH(effect_handler_context_t *context)
{
	int i;
	int score = song_bonus(player, player->state.skill_use[SKILL_SONG],
						   lookup_song("Elbereth"));
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		struct monster *mon = cave_monster(cave, i);
		int resistance;
		int result;

		/* Ignore dead monsters */
		if (!mon->race) continue;

		/* Only intelligent monsters are affected */
		if (!rf_has(mon->race->flags, RF_SMART)) continue;

		/* Morgoth is not affected */
		if (!rf_has(mon->race->flags, RF_QUESTOR)) continue;

		/* Resistance is monster will, modified by distance from the player */
		resistance = monster_skill(mon, SKILL_WILL);
		resistance += flow_dist(cave->player_noise, mon->grid);
		result = skill_check(source_player(), score, resistance,
							 source_monster(mon->midx));

		/* If successful, cause fear in the monster */
		if (result > 0) {
			/* Decrease temporary morale */
			mon->tmp_morale -= result * 10;
		}
	}
	return true;
}

/**
 * Attempt to decrease alertness of all monsters.
 */
bool effect_handler_SONG_OF_LORIEN(effect_handler_context_t *context)
{
	int i;
	int score = song_bonus(player, player->state.skill_use[SKILL_SONG],
						   lookup_song("Lorien"));
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		struct monster *mon = cave_monster(cave, i);
		int resistance;
		int result;

		/* Ignore dead monsters */
		if (!mon->race) continue;

		/* Deal with sleep resistance */
		if (rf_has(mon->race->flags, RF_NO_SLEEP)) {
			struct monster_lore *lore = get_lore(mon->race);;
			if (monster_is_visible(mon)) {
				rf_on(lore->flags, RF_NO_SLEEP);
			}
			continue;
		}

		/* Resistance is monster will, modified by distance from the player */
		resistance = monster_skill(mon, SKILL_WILL);
		resistance += 5 + flow_dist(cave->player_noise, mon->grid);
		result = skill_check(source_player(), score, resistance,
							 source_monster(mon->midx));

		/* If successful, (partially) put the monster to sleep */
		if (result > 0) {
			set_alertness(mon, mon->alertness - result);
		}
	}
	return true;
}

/**
 * Affect a variety of terrain to help the player escape
 */
bool effect_handler_SONG_OF_FREEDOM(effect_handler_context_t *context)
{
	int base_diff = player->depth ? player->depth / 2 : 10;
	int score = song_bonus(player, player->state.skill_use[SKILL_SONG],
						   lookup_song("Freedom"));
	struct loc grid;
	bool closed_chasm = false;

    /* Scan the map */
    for (grid.y = 0; grid.y < cave->height; grid.y++) {
        for (grid.x = 0; grid.x < cave->width; grid.x++) {
			struct object *obj;
			if (!square_in_bounds_fully(cave, grid)) continue;
			obj = square_object(cave, grid);
			if (obj && tval_is_chest(obj) && (obj->pval > 0)) {
				/* Chest */
				int diff = base_diff + 5 + flow_dist(cave->player_noise, grid);
				if (skill_check(source_player(), score, diff, source_none())) {
                        /* Disarm or Unlock */
                        obj->pval = (0 - obj->pval);

                        /* Identify */
                        object_know(obj);
				}
			} else if (square_ischasm(cave, grid)) {
				/* Chasm */
				int power = score - flow_dist(cave->player_noise, grid) - 5;
                closed_chasm |= close_chasm(grid, power);
			} else if (square_issecrettrap(cave, grid)) {
				/* Invisible trap */
				int diff = base_diff + 5 + flow_dist(cave->player_noise, grid);
				if (skill_check(source_player(), score, diff, source_none())
					> 0) {
					square_destroy_trap(cave, grid);
				}
			} else if (square_isvisibletrap(cave, grid)) {
				/* Visible trap */
				int diff = base_diff + 5 + flow_dist(cave->player_noise, grid);
				if (skill_check(source_player(), score, diff, source_none())
					> 0) {
					square_destroy_trap(cave, grid);
					square_light_spot(cave, grid);
				}
			} else if (square_issecretdoor(cave, grid)) {
				/* Secret door */
				int diff = base_diff + flow_dist(cave->player_noise, grid);
				if (skill_check(source_player(), score, diff, source_none())
					> 0) {
					place_closed_door(cave, grid);
					if (square_isseen(cave, grid)) {
						msg("You have found a secret door.");
						disturb(player, false);
					}
				}
			} else if (square_isjammeddoor(cave, grid)) {
				/* Stuck door */
				int diff = base_diff + flow_dist(cave->player_noise, grid);
				int result = skill_check(source_player(), score, diff,
										 source_none());
				if (result > 0) {
					int jam = square_door_jam_power(cave, grid) - result;
					square_set_door_jam(cave, grid, MAX(jam, 0));
				}
			} else if (square_islockeddoor(cave, grid)) {
				/* Locked door */
				int diff = base_diff + flow_dist(cave->player_noise, grid);
				int result = skill_check(source_player(), score, diff,
										 source_none());
				if (result > 0) {
					int lock = square_door_lock_power(cave, grid) - result;
					square_set_door_lock(cave, grid, MAX(lock, 0));
				}
			} else if (square_isrubble(cave, grid)) {
				/* Rubble */
                int d, noise_dist = 100;
				int diff, result;

                /* Check adjacent squares for valid noise distances, since
				 * rubble is impervious to sound */
                for (d = 0; d < 8; d++) {
                    int dir = cycle[d];
					int noise_dist_new = flow_dist(cave->player_noise,
												   loc_sum(grid, ddgrid[dir]));
					noise_dist = MIN(noise_dist, noise_dist_new);
                }
                noise_dist++;

                diff = base_diff + 5 + noise_dist;
                result = skill_check(source_player(), score, diff,
									 source_none());
                if (result > 0) {
					square_set_feat(cave, grid, FEAT_FLOOR);
                    player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
				}
			}
		}
	}

    /* Then, if any chasms were marked to be closed, do the closing */
    if (closed_chasm) {
		close_marked_chasms();
   }

	return true;
}


/**
 * Affect a variety of terrain to hinder the player's escape
 */
bool effect_handler_SONG_OF_BINDING(effect_handler_context_t *context)
{
	struct monster *mon = cave_monster(cave, context->origin.which.monster);
	int song_skill = monster_sing(mon, lookup_song("Binding"));
	struct loc grid;
	int dist, result, resistance;

	/* Use the monster noise flow to represent the song levels at each square */
	cave->monster_noise.centre = mon->grid;
    update_flow(cave, &cave->monster_noise, NULL);

    /* Scan the map, closing doors */
    for (grid.y = 0; grid.y < cave->height; grid.y++) {
        for (grid.x = 0; grid.x < cave->width; grid.x++) {
            if (!square_in_bounds_fully(cave, grid)) continue;

            /* If there is no player/monster in the square, and it's a door,
			* and the door isn't between the monster and the player */
            if (!square_monster(cave, grid) && square_isdoor(cave, grid) &&
				!((mon->grid.y <= grid.y) && (grid.y <= player->grid.y) &&
				  (mon->grid.y <= grid.y) && (grid.y <= player->grid.y))) {
				dist = 15 + flow_dist(cave->monster_noise, grid);
				result = skill_check(source_monster(mon->midx), song_skill,
									 dist, source_none());
				square_set_door_lock(cave, grid, result);
			}
		}
    }

    /* Determine the player's resistance */
	dist = flow_dist(cave->monster_noise, player->grid);
    resistance = player->state.skill_use[SKILL_WILL] +
		(player->state.flags[OF_FREE_ACT] * 10) + dist;

	/* Perform the skill check */
    result = skill_check(source_monster(mon->midx), song_skill, resistance,
						 source_player());

    /* If the check succeeds, the player is slowed for at least 2 rounds.
	 * Note that only the first of these affects you as you aren't slow on the
	 * round it wears off */
    if (result > 0) {
		int slow = MAX(player->timed[TMD_SLOW], 2);
        player_set_timed(player, TMD_SLOW, slow, false);
    }

	return true;
}


/**
 * Increase the singing monster's alertness
 */
bool effect_handler_SONG_OF_PIERCING(effect_handler_context_t *context)
{
	struct monster *mon = cave_monster(cave, context->origin.which.monster);
	int song_skill = monster_sing(mon, lookup_song("Piercing"));
	int dist = flow_dist(cave->player_noise, mon->grid);
	int result, resistance;
    char name[80];

    /* Get the monster name */
    monster_desc(name, sizeof(name), mon, MDESC_POSS);

    /* Determine the player's resistance */
    resistance = player->state.skill_use[SKILL_WILL] + 5 + dist;

	/* Perform the skill check */
    result = skill_check(source_monster(mon->midx), song_skill, resistance,
						 source_player());

    /* If the check succeeds, Morgoth knows the player's location */
    if (result > 0) {
        msg("You feel your mind laid bare before %s will.", name);
        set_alertness(mon, MIN(result, ALERTNESS_VERY_ALERT));
    } else if (result > -5) {
        msg("You feel the force of %s will searching for the intruder.", name);
    }

	return true;
}

/**
 * Increase the singing monster's alertness
 */
bool effect_handler_SONG_OF_OATHS(effect_handler_context_t *context)
{
	struct monster *mon = cave_monster(cave, context->origin.which.monster);
	int song_skill = monster_sing(mon, lookup_song("Oaths"));
	int result, resistance = 15;

	/* Use the monster noise flow to represent the song levels at each square */
	cave->monster_noise.centre = mon->grid;
    update_flow(cave, &cave->monster_noise, NULL);

	/* Perform the skill check */
    result = skill_check(source_monster(mon->midx), song_skill, resistance,
						 source_player());

    /* If the check was successful, summon an oathwraith to a nearby square */
    if (result > 0) {
        /* The greatest distance away the wraith can be summoned --
		 * smaller is typically better */
        int range = MAX(15 - result, 3);
		struct loc grid;

		/* Summon an oathwraith within 'range' of the player */
		while (true) {
			struct monster *new;
			struct monster_group_info info = { 0, 0 };
			cave_find(cave, &grid, square_isarrivable);
			if (flow_dist(cave->monster_noise, grid) > range) continue;

			/* Place it */
			place_new_monster_one(cave, grid, lookup_monster("Oathwraith"),
								  true, false, info, ORIGIN_DROP_SUMMON);
			new = square_monster(cave, grid);

			/* Message if visible */
			if (monster_is_visible(new)) {
				msg("An Oathwraith appears.");
			}

			/* Mark the wraith as having been summoned */
			mflag_on(new->mflag, MFLAG_SUMMONED);

			/* Let it know where the player is */
			set_alertness(new, ALERTNESS_QUITE_ALERT);

			break;
		}
	}

	return true;
}

/**
 * Make nearby monsters aggressive.
 */
bool effect_handler_AGGRAVATE(effect_handler_context_t *context)
{
	int i;
	for (i = 1; i < cave_monster_max(cave); i++) {
		/* Check the i'th monster */
		struct monster *mon = cave_monster(cave, i);
		struct monster_race *race = mon ? mon->race : NULL;

		if (!race) continue;
		
		if ((mon->alertness >= ALERTNESS_ALERT) &&
			(flow_dist(cave->player_noise, mon->grid) <= 10)) {
			mflag_on(mon->mflag, MFLAG_AGGRESSIVE);

            /* Notice if the monster is visible */
            if (monster_is_visible(mon)) context->ident = true;
            
			if (rf_has(race->flags, RF_SMART) &&
			    (rf_has(race->flags, RF_FRIENDS) ||
				 rf_has(race->flags, RF_FRIEND) ||
				 rf_has(race->flags, RF_UNIQUE_FRIEND) ||
				 rf_has(race->flags, RF_ESCORT) ||
				 rf_has(race->flags, RF_ESCORTS) ||
				 rsf_has(race->spell_flags, RSF_SHRIEK))) {
				tell_allies(mon, MFLAG_AGGRESSIVE);

                /* Notice if you hear them shout */
                context->ident = true;
			}
		}
	}
	return true;
}

/**
 * Make noise that monsters may hear and react to.
 */
bool effect_handler_NOISE(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context);
	bool player_centred = context->subtype ? true : false;
	if (context->origin.what == SRC_MONSTER) {
		struct monster *mon = cave_monster(cave, context->origin.which.monster);
		cave->monster_noise.centre = mon->grid;
		update_flow(cave, &cave->monster_noise, NULL);

		/* Radius is used for monster making its own noise */
		if (context->radius) mon->noise += context->radius;
	}
	monsters_hear(player_centred, false, amount);
	return true;
}

/**
 * Create traps.
 */
bool effect_handler_CREATE_TRAPS(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context);
	while (amount--) {
		struct loc grid;
		cave_find(cave, &grid, square_isunseen);
		square_add_trap(cave, grid);
	}
	return true;
}

