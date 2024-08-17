/**
 * \file mon-util.c
 * \brief Monster manipulation utilities.
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

#include "angband.h"
#include "cmd-core.h"
#include "effects.h"
#include "game-world.h"
#include "init.h"
#include "mon-attack.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-list.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-msg.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-summon.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "trap.h"
#include "songs.h"

/**
 * ------------------------------------------------------------------------
 * Lore utilities
 * ------------------------------------------------------------------------ */
static const struct monster_flag monster_flag_table[] =
{
	#define RF(a, b, c) { RF_##a, b, c },
	#include "list-mon-race-flags.h"
	#undef RF
	{RF_MAX, 0, NULL}
};

/**
 * Return a description for the given monster race flag.
 *
 * Returns an empty string for an out-of-range flag.
 *
 * \param flag is one of the RF_ flags.
 */
const char *describe_race_flag(int flag)
{
	const struct monster_flag *rf = &monster_flag_table[flag];

	if (flag <= RF_NONE || flag >= RF_MAX)
		return "";

	return rf->desc;
}

/**
 * Create a mask of monster flags of a specific type.
 *
 * \param f is the flag array we're filling
 * \param ... is the list of flags we're looking for
 *
 * N.B. RFT_MAX must be the last item in the ... list
 */
void create_mon_flag_mask(bitflag *f, ...)
{
	const struct monster_flag *rf;
	int i;
	va_list args;

	rf_wipe(f);

	va_start(args, f);

	/* Process each type in the va_args */
    for (i = va_arg(args, int); i != RFT_MAX; i = va_arg(args, int)) {
		for (rf = monster_flag_table; rf->index < RF_MAX; rf++)
			if (rf->type == i)
				rf_on(f, rf->index);
	}

	va_end(args);

	return;
}


/**
 * ------------------------------------------------------------------------
 * Lookup utilities
 * ------------------------------------------------------------------------ */
/**
 * Returns the monster with the given name. If no monster has the exact name
 * given, returns the first monster with the given name as a (case-insensitive)
 * substring.
 */
struct monster_race *lookup_monster(const char *name)
{
	int i;
	struct monster_race *closest = NULL;

	/* Look for it */
	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];
		if (!race->name)
			continue;

		/* Test for equality */
		if (my_stricmp(name, race->name) == 0)
			return race;

		/* Test for close matches */
		if (!closest && my_stristr(race->name, name))
			closest = race;
	}

	/* Return our best match */
	return closest;
}

/**
 * Return the monster base matching the given name.
 */
struct monster_base *lookup_monster_base(const char *name)
{
	struct monster_base *base;

	/* Look for it */
	for (base = rb_info; base; base = base->next) {
		if (streq(name, base->name))
			return base;
	}

	return NULL;
}

/**
 * Return whether the given base matches any of the names given.
 *
 * Accepts a variable-length list of name strings. The list must end with NULL.
 *
 * This function is currently unused, except in a test... -NRM-
 */
bool match_monster_bases(const struct monster_base *base, ...)
{
	bool ok = false;
	va_list vp;
	char *name;

	va_start(vp, base);
	while (!ok && ((name = va_arg(vp, char *)) != NULL))
		ok = base == lookup_monster_base(name);
	va_end(vp);

	return ok;
}

/**
 * ------------------------------------------------------------------------
 * Monster (and player) actual movement
 * ------------------------------------------------------------------------ */
/**
 * Check if the monster in the given location needs to fall down a chasm
 */
static void monster_fall_in_chasm(struct loc grid)
{
    struct monster *mon = square_monster(cave, grid);
    struct monster_race *race = mon ? mon->race : NULL;;
    char m_name[80];
    
    int dice;
    int dam;
    
    /* Paranoia */
    if (!mon) return;

    if (square_ischasm(cave, grid) && !rf_has(race->flags, RF_FLYING)) {
		/* Get the monster name */
        monster_desc(m_name, sizeof(m_name), mon, MDESC_DEFAULT);

        /* Message for visible monsters */
        if (monster_is_visible(mon)) {
            /* Dump a message */
            if (mon->morale < -200) {
				msg("%s leaps into the abyss!", m_name);
            } else {
				msg("%s topples into the abyss!", m_name);
			}
		}

		/* Pause so that the monster will be displayed in the chasm before
		 * it disappears */
		event_signal(EVENT_MESSAGE_FLUSH);

		/* Determine the falling damage */
		if (player->depth == z_info->dun_depth - 2) {
			dice = 3; /* only fall one floor in this case */
		} else {
			dice = 6;
		}

		/* Roll the damage dice */
        dam = damroll(dice, 4);

        /* Update combat rolls if visible */
        if (monster_is_visible(mon)) {
			event_signal_combat_attack(EVENT_COMBAT_ATTACK, source_grid(grid),
									   source_monster(mon->midx), true, -1, -1,
									   -1, -1, false);
			event_signal_combat_damage(EVENT_COMBAT_DAMAGE, dice, 4, dam, -1,
									   -1, 0, 0, PROJ_HURT, false);
        }
        
        /* Kill monsters which cannot survive the damage */
        if (mon->hp <= dam) {
            /* Kill the monster, gain experience etc */
            monster_death(mon, player, true, NULL, false);

            /* Delete the monster */
            delete_monster(cave, grid);
        } else {
			/* Otherwise the monster survives! (mainly relevant for uniques) */
			delete_monster(cave, grid);
        }
    }
}

/**
 * Swap the players/monsters (if any) at two locations.
 */
void monster_swap(struct loc grid1, struct loc grid2)
{
	struct monster *mon;
	char m_name[80];

	/* Monsters */
	int m1 = square(cave, grid1)->mon;
	int m2 = square(cave, grid2)->mon;

	/* Needed for polearms check */
	bool m1_is_monster = false;

	/* Nothing to do if locations are the same */
	if (loc_eq(grid1, grid2)) return;

	/* Monster 1 */
	if (m1 > 0) {
		bool opp = player_active_ability(player, "Opportunist");
		bool zone = player_active_ability(player, "Zone of Control");

		/* Monster */
		m1_is_monster = true;
		mon = cave_monster(cave, m1);

		/* Handle Opportunist and Zone of Control */
		if ((opp || zone) && monster_is_visible(mon) &&	!mon->skip_next_turn &&
			!player->truce && !player->timed[TMD_CONFUSED] &&
			!player->timed[TMD_AFRAID] && !player->timed[TMD_ENTRANCED] &&
			(player->timed[TMD_STUN] < 100) &&
			(distance(grid1, player->grid) == 1) &&
			(!OPT(player, forgo_attacking_unwary) ||
			 (mon->alertness >= ALERTNESS_ALERT))) {
			monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);

			/* Zone of control */
			if (zone && (distance(grid2, player->grid) == 1)) {
				msg("%s moves through your zone of control.", m_name);
				py_attack_real(player, grid1, ATT_ZONE_OF_CONTROL);
			}

			/* Opportunist */
			if (opp && (distance(grid2, player->grid) > 1)) {
				msg("%s moves away from you.", m_name);
				py_attack_real(player, grid1, ATT_OPPORTUNIST);
			}

			/* Monster may be dead */
			if (mon->hp <= 0) return;
		}

		/* Makes noise when moving */
		if (mon->noise == 0) {
			mon->noise = 5;
		}

		/* Update monster */
		mon->grid = grid2;
		update_mon(mon, cave, true);

		/* Affect light? */
		if (mon->race->light != 0)
			player->upkeep->update |= PU_UPDATE_VIEW | PU_MONSTERS;

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST);
	} else if (m1 < 0) {
		int y, x;

		/* Handle Opportunist and Zone of Control */
		for (y = player->grid.y - 1; y <= player->grid.y + 1; y++) {
			for (x = player->grid.x - 1; x <= player->grid.x + 1; x++) {
				struct loc grid = loc(x, y);
				mon = square_monster(cave, grid);

				if (mon && (mon->alertness >= ALERTNESS_ALERT) &&
					!mon->m_timed[MON_TMD_CONF] && !mon->skip_next_turn &&
					(mon->stance != STANCE_FLEEING) && !mon->skip_this_turn) {
					bool opp = rf_has(mon->race->flags, RF_OPPORTUNIST);
					bool zone = rf_has(mon->race->flags, RF_ZONE);
					struct monster_lore *lore = get_lore(mon->race);

					/* Opportunist */
					if (opp && (distance(grid2, mon->grid) > 1)) {
						monster_desc(m_name, sizeof(m_name), mon,
									 MDESC_STANDARD);
						msg("%s attacks you as you step away.", m_name);
						make_attack_normal(mon, player);

						/* Remember that the monster can do this */
						if (monster_is_visible(mon)) {
							rf_on(lore->flags, RF_OPPORTUNIST);
						}
					}

					/* Zone of control */
					if (zone && (distance(grid2, player->grid) == 1)) {
						monster_desc(m_name, sizeof(m_name), mon, MDESC_POSS);
						msg("You move through %s zone of control.", m_name);
						make_attack_normal(mon, player);

						/* Remember that the monster can do this */
						if (monster_is_visible(mon)) {
							rf_on(lore->flags, RF_ZONE);
						}
					}

					/* Player may be dead */
					if (player->chp < 0) return;
				}
			}
		}

		/* Move player */
		player->grid = grid2;

		/* Updates */
		player->upkeep->update |= (PU_PANEL | PU_UPDATE_VIEW | PU_DISTANCE);

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST);

		/* Don't allow command repeat if moved away from item used. */
		cmd_disable_repeat_floor_item();
	}

	/* Monster 2 */
	if (m2 > 0) {
		/* Monster */
		mon = cave_monster(cave, m2);

		/* Makes noise when moving */
		if (mon->noise == 0) {
			mon->noise = 5;
		}

		/* Update monster */
		mon->grid = grid1;
		update_mon(mon, cave, true);

		/* Affect light? */
		if (mon->race->light != 0)
			player->upkeep->update |= PU_UPDATE_VIEW | PU_MONSTERS;

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST);
	} else if (m2 < 0) {
		/* Player */
		player->grid = grid1;

		/* Updates */
		player->upkeep->update |= (PU_PANEL | PU_UPDATE_VIEW | PU_DISTANCE);

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST);

		/* Don't allow command repeat if moved away from item used. */
		cmd_disable_repeat_floor_item();
	}

	/* Update grids */
	square_set_mon(cave, grid1, m2);
	square_set_mon(cave, grid2, m1);

	/* Redraw */
	square_light_spot(cave, grid1);
	square_light_spot(cave, grid2);

	/* Deal with set polearm attacks */
	if (player_active_ability(player, "Polearm Mastery") && m1_is_monster) {
		mon = square_monster(cave, grid2);
		if (mon && monster_is_visible(mon)) {
			struct object *obj = equipped_item_by_slot_name(player, "weapon");

			if (!OPT(player, forgo_attacking_unwary) ||
				(mon->alertness >= ALERTNESS_ALERT)) {
				if ((distance(grid1, player->grid) > 1) &&
					(distance(grid2, player->grid) == 1) &&
					!player->truce && !player->timed[TMD_CONFUSED] &&
					!player->timed[TMD_AFRAID] && of_has(obj->flags, OF_POLEARM)
					&& player->focused) {
					char o_name[80];

					/* Get the basic name of the object */
					object_desc(o_name, sizeof(o_name), obj, ODESC_BASE,
								player);

					monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);

					msg("%s comes into reach of your %s.", m_name, o_name);
					py_attack_real(player, grid2, ATT_POLEARM);
				}
			}
		}
	}

	/* Deal with falling down chasms */
    if (m1 > 0) monster_fall_in_chasm(grid2);
    if (m2 > 0) monster_fall_in_chasm(grid1);

    /* Describe object you are standing on if any */
    if ((m1 < 0) || (m2 < 0)) {
        event_signal(EVENT_SEEFLOOR);
    }
}

/**
 * ------------------------------------------------------------------------
 * Awareness and learning
 * ------------------------------------------------------------------------ */
/**
 * Monster can see a grid
 */
bool monster_can_see(struct chunk *c, struct monster *mon, struct loc grid)
{
	return los(c, mon->grid, grid);
}

/**
 * Lets all monsters attempt to notice the player.
 * It can get called multiple times per player turn.
 *
 * Once each turn is the 'main roll' which is handled differently from the
 * others; the other rolls correspond to noisy events.  These events can be
 * caused by the player (in which case 'player_centered' is set to true),
 * or can be caused by a monster, in which case it will be false and
 * the monster_noise flow will be used instead of the usual player_noise flow.
 */

void monsters_hear(bool player_centered, bool main_roll, int difficulty)
{
	int i;
	int m_perception;
	int result;
	int noise_dist;
	int difficulty_roll;
	int difficulty_roll_alt;

	int combat_noise_bonus = 0;
	int combat_sight_bonus = 0;

	struct song *silence = lookup_song("Silence");

	/* Player is dead or leaving the current level */
	if (player->is_dead || !player->upkeep->playing ||
			player->upkeep->generate_level) return;

	/* No perception on the first turn of the game */
	if (turn == 0) return;

	/* If time is stopped, no monsters can perceive */
	if (OPT(player, cheat_timestop)) return;

	/* Bonuses for monsters if the player attacked a monster or was attacked */
	if (main_roll) {
		if (player->attacked) {
			combat_noise_bonus += 2;
			combat_sight_bonus += 2;
			player->attacked = false;

			/* Keep track of this for the ability 'Concentration' */
			player->consecutive_attacks++;
		}
		if (player->been_attacked) {
			combat_noise_bonus += 2;
			combat_sight_bonus += 2;
			player->been_attacked = false;
		}
	}

	/* Make the difficulty roll just once per sound source (i.e. once per call
	 * to this function).  This is a manual version of a 'skill_check()' and
	 * should be treated as such */
	difficulty_roll = difficulty + randint1(10);

	/* Deal with player curses for skill rolls.  This is not perfect as some
	 * 'player_centered' things are not actually caused by the player */
	difficulty_roll_alt = difficulty + randint1(10);
	if (player->cursed && player_centered) {
		difficulty_roll = MIN(difficulty_roll, difficulty_roll_alt);
	}

	/* The song of silence quietens this a bit */
	if (player_is_singing(player, silence)) {
		difficulty_roll += song_bonus(player,
									  player->state.skill_use[SKILL_SONG],
									  silence);
	}

	/* Process the monsters (backwards) */
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		/* Access the monster */
		struct monster *mon = cave_monster(cave, i);

		/* Ignore dead monsters */
		if (!mon->race) continue;

		/* Ignore if character is within detection range
		 * (unlimited for most monsters, 2 for shortsighted ones) */		
		if (rf_has(mon->race->flags, RF_SHORT_SIGHTED) && (mon->cdis > 2)) {
			continue;
		}

		if (player_centered) {
			noise_dist = flow_dist(cave->player_noise, mon->grid);
		} else {
			noise_dist = flow_dist(cave->monster_noise, mon->grid);
		}

		/* Start building up the monster's total perception */
		m_perception = monster_skill(mon, SKILL_PERCEPTION) - noise_dist
			+ combat_noise_bonus;

		/* Deal with bane ability (theoretically should modify player roll,
		 * but this is equivalent) */
		m_perception -= player_bane_bonus(player, mon);

		/* Increase morale for the Elf-Bane ability */
		m_perception += monster_elf_bane_bonus(mon, player);

		/* Monsters are looking more carefully during the escape */
		if (player->on_the_run) {
			m_perception += 5;
		}

		/* Monsters that are already alert get a penalty to the roll to
		 * stop them getting *too* alert */
		if (mon->alertness >= ALERTNESS_ALERT) {
			m_perception -= mon->alertness;
		}

		/* Aggravation makes non-sleeping monsters much more likely
		 * to notice the player */
		if (player->state.flags[OF_AGGRAVATE] &&
			(mon->alertness >= ALERTNESS_UNWARY) && 
			!rf_has(mon->race->flags, RF_MINDLESS)) {
			m_perception += player->state.flags[OF_AGGRAVATE] * 10;
		}

		/* Awake creatures who have line of sight on player get a bonus */
		if (los(cave, mon->grid, player->grid) &&
			(mon->alertness >= ALERTNESS_UNWARY)) {
			int d, dir, open_squares = 0;
			struct loc grid;

			/* Check adjacent squares for impassable squares */
			for (d = 0; d < 8; d++) {
				dir = cycle[d];
				grid = loc_sum(player->grid, ddgrid[dir]);
				if (square_ispassable(cave, grid)) {
					open_squares++;
				}
			}

			/* Bonus reduced if the player has 'disguise' */
			if (player_active_ability(player, "Disguise")) {
				m_perception += (open_squares + combat_sight_bonus) / 2;
			} else {
				m_perception += open_squares + combat_sight_bonus;
			}
		}

		/* Do the 'skill_check()' versus the quietness of the sound... */
		result = (m_perception + randint1(10)) - difficulty_roll;

		/* Debugging message */
		if (OPT(player, cheat_skill_rolls)) {
			msg("{%d+%d v %d+%d = %d}.",
				result - m_perception + difficulty_roll, m_perception, 
				difficulty_roll - difficulty, difficulty,
				result);
		}

		if (result > 0) {					
			struct monster_lore *lore = get_lore(mon->race);

			/* Partly alert monster */
			set_alertness(mon, mon->alertness + result);

			/* Still not alert */
			if (mon->alertness < ALERTNESS_ALERT) {
				if (monster_is_visible(mon) && (lore->ignore < UCHAR_MAX)) {
					lore->ignore++;
				}
			} else {
				/* Just became alert */
				if (monster_is_visible(mon) && (lore->notice < UCHAR_MAX)) {
					lore->notice++;
				}
			}
		}
	}
}

/**
 * ------------------------------------------------------------------------
 * Monster damage and death utilities
 * ------------------------------------------------------------------------ */
/**
 * This adjusts a monster's raw experience point value according to the number
 * killed so far. The formula is:
 *
 * (depth * 10) / (kills + 1)
 *
 * This is doubled for uniques.
 *
 * ((depth * 25) * 4) / (kills + 4)  <- previous version
 *
 * 100 90 83 76 71 66 62  (10,10)   <- earliest version?
 * 100 80 66 57 50 44 40  (4,4)     <- this is the previous version
 *                                     (without the 1.5 multiplier)
 * 100 66 50 40 33 28 25  (2,2)
 * 100 50 33 25 20 16 14  (1,1)     <- this is the current version
 *
 * 100 90 81 72 65 59 53  (10%)     <- exponential alternatives
 * 100 80 64 51 40 32 25  (20%)     
 *
 * This function is called when gaining experience and when displaying it in
 * monster recall.
 */
int32_t adjusted_mon_exp(const struct monster_race *race, bool kill)
{
	int32_t exp;
	int mexp = race->level * 10;
	struct monster_lore *lore = get_lore(race);

	if (kill) {
		if (rf_has(race->flags, RF_UNIQUE)) {
			exp = mexp;
		} else {
			exp = (mexp) / (lore->pkills + 1);
		}
	} else {
		if (rf_has(race->flags, RF_UNIQUE)) {
			exp = mexp;
		} else {
			exp = (mexp) / (lore->psights + 1);
		}
	}

	return exp;
}

/**
 * Return the number of things dropped by a monster.
 *
 * \param race is the monster race.
 * \param maximize should be set to false for a random number, true to find
 * out the maximum count.
 */
int mon_create_drop_count(const struct monster_race *race, bool maximize)
{
	int number = 0;

	if (maximize) {
		if (rf_has(race->flags, RF_DROP_33)) number++;
		if (rf_has(race->flags, RF_DROP_100)) number++;
		if (rf_has(race->flags, RF_DROP_1D2)) number += 2;
		if (rf_has(race->flags, RF_DROP_2D2)) number += 4;
		if (rf_has(race->flags, RF_DROP_3D2)) number += 6;
		if (rf_has(race->flags, RF_DROP_4D2)) number += 8;
	} else {
		if (rf_has(race->flags, RF_DROP_33) && percent_chance(33)) number++;
		if (rf_has(race->flags, RF_DROP_100)) number++;
		if (rf_has(race->flags, RF_DROP_1D2)) number += damroll(1, 2);
		if (rf_has(race->flags, RF_DROP_2D2)) number += damroll(2, 2);
		if (rf_has(race->flags, RF_DROP_3D2)) number += damroll(3, 2);
		if (rf_has(race->flags, RF_DROP_4D2)) number += damroll(4, 2);
	}
	return number;
}

/**
 * Creates a specific monster's drop, including any drops specified
 * in the monster.txt file.
 *
 * Returns true if anything is created, false if nothing is.
 */
static int mon_create_drop(struct chunk *c, struct monster *mon,
						   struct loc grid, bool stats)
{
	struct monster_drop *drop;
	bool great, good;
	bool visible;
	int number = 0, count = 0, level, j;
	struct object *obj;

	assert(mon);

	great = rf_has(mon->race->flags, RF_DROP_GREAT);
	good = rf_has(mon->race->flags, RF_DROP_GOOD);
	visible = monster_is_visible(mon) || monster_is_unique(mon);

	/* Determine how much we can drop */
	number = mon_create_drop_count(mon->race, false);

	/* Use the monster's level */
	level = mon->race->level;

	/* Specified drops */
	for (drop = mon->race->drops; drop; drop = drop->next) {
		if (percent_chance((int) drop->percent_chance)) {
			/* Specified by tval or by kind */
			if (drop->kind) {
				/* Allocate by hand, prep */
				obj = mem_zalloc(sizeof(*obj));
				object_prep(obj, drop->kind, level, RANDOMISE);
				obj->number = randcalc(drop->dice, 0, RANDOMISE);
				/* Deathblades only */
				if (streq(mon->race->base->name, "deathblade")) {
					apply_magic(obj, c->depth, false, false, false);
				}

				/* Identify non-special non-artefact weapons/armour */
				if (tval_has_variable_power(obj) && !tval_is_jewelry(obj)) {
					object_know(obj);
				}
			} else {
				/* Artifact */
				const struct artifact *art;
				struct object_kind *kind;
				assert(drop->art);
				art = drop->art;
				kind = lookup_kind(art->tval, art->sval);
				obj = mem_zalloc(sizeof(*obj));
				object_prep(obj, kind, 100, RANDOMISE);
				obj->artifact = art;
				copy_artifact_data(obj, obj->artifact);
				mark_artifact_created(art, true);
				pseudo_id(obj);
			}

			/* Skip if the object couldn't be created. */
			if (!obj) continue;

			/* Set origin details */
			obj->origin = visible || stats ? mon->origin : ORIGIN_DROP_UNKNOWN;
			obj->origin_depth = convert_depth_to_origin(c->depth);
			obj->origin_race = mon->race;
			number--;
			count++;

			drop_near(c, &obj, 0, grid, true, false);
		}
	}

	/* Make and drop some objects */
	for (j = 0; j < number; j++) {
		obj = make_object(c, level, good, great, lookup_drop("not useless"));

		if (!obj) continue;

		/* Set origin details */
		obj->origin = visible || stats ? mon->origin : ORIGIN_DROP_UNKNOWN;
		obj->origin_depth = convert_depth_to_origin(c->depth);
		obj->origin_race = mon->race;
		count++;

		drop_near(c, &obj, 0, grid, true, false);
	}

	return count;
}


/**
 * Drop monster carried items and generate treasure
 */
void drop_loot(struct chunk *c, struct monster *mon, struct loc grid,
			   bool stats)
{
	int dump_item;
	struct object *obj = mon->held_obj;

	bool visible = monster_is_visible(mon) || monster_is_unique(mon);
	bool stair = square_isstairs(c, grid) || square_isshaft(c, grid);

	/* Stone creatures turn into rubble */
	if (rf_has(mon->race->flags, RF_STONE) && !stair) {
		square_set_feat(c, grid, FEAT_RUBBLE);
	}

	/* Drop objects being carried */
	while (obj) {
		struct object *next = obj->next;

		/* Object no longer held */
		obj->held_m_idx = 0;
		pile_excise(&mon->held_obj, obj);

		/* Change origin if monster is invisible, unless we're in stats mode */
		if (!visible && !stats)
			obj->origin = ORIGIN_DROP_UNKNOWN;

		drop_near(c, &obj, 0, grid, (c == cave), false);
		obj = next;
	}

	/* Forget objects */
	mon->held_obj = NULL;

	/* Drop some objects */
	dump_item = mon_create_drop(c, mon, grid, stats);

	/* Take note of any dropped treasure */
	if (visible && dump_item && stats) {
		lore_treasure(mon, dump_item);
	}
}

/**
 * Handles the "death" of a monster.
 *
 * Disperses treasures carried by the monster centered at the monster location.
 * Note that objects dropped may disappear in crowded rooms.
 *
 * Checks for "Quest" completion when a quest monster is killed.
 *
 * If `stats` is true, then we skip updating the monster memory. This is
 * used by stats-generation code, for efficiency.
 */
void monster_death(struct monster *mon, struct player *p, bool by_player,
						  const char *note, bool stats)
{
	int32_t new_exp;
	struct monster_race *race = mon->race;
	struct monster_lore *lore = get_lore(mon->race);
	char m_name[80];
	char buf[80];
	int desc_mode = MDESC_DEFAULT | ((note) ? MDESC_COMMA : 0);
	int multiplier = 1;

	/* Assume normal death sound */
	int soundfx = MSG_KILL;

	/* Monster has fallen in a chasm */
	bool chasm = square_ischasm(cave, mon->grid) &&
		!rf_has(mon->race->flags, RF_FLYING);

	/* Extract monster name */
	monster_desc(m_name, sizeof(m_name), mon, desc_mode);

	/* Play a special sound if the monster was unique */
	if (rf_has(race->flags, RF_UNIQUE)) {
		/* Special message and flag setting for killing Morgoth */
		if (race->base == lookup_monster_base("Morgoth")) {
			soundfx = MSG_KILL_KING;
			p->morgoth_slain = true;
			msg("BUG: Morgoth has been defeated in combat.");
			msg("But this is not possible within the fates Illuvatar has decreed.");
			msg("Please post an 'ultimate bug-report' on http://angband.oook.cz/forum/ explaining how this happened.");
			msg("But for now, let's run with it, since it's undeniably impressive.");

			/* Display the ultimate bug text */
			event_signal_poem(EVENT_POEM, "ultimate_bug", 5, 15);
		} else {
			soundfx = MSG_KILL_UNIQUE;
		}
	}

	/* Death message */
	if (note) {
		if (strlen(note) <= 1) {
			/* Death by Spell attack - messages handled by project_m() */
		} else {
			/* Make sure to flush any monster messages first */
			notice_stuff(p);

			/* Death by Missile attack */
			my_strcap(m_name);
			msgt(soundfx, "%s%s", m_name, note);
		}
	} else {
		/* Make sure to flush any monster messages first */
		notice_stuff(p);

		if (!monster_is_visible(mon)) {
			/* Death by physical attack -- invisible monster
			 * You only get messages for unseen monsters if you kill them */
			if (by_player && (distance(mon->grid, p->grid) == 1)) {
				msgt(soundfx, "You have killed %s.", m_name);
			}
		} else if (monster_is_nonliving(mon)) {
			/* Death by Physical attack -- non-living monster */
			if (streq(race->base->name, "deathblade")) {
				/* Special message for deathblades */
				if (by_player) {
					msgt(soundfx, "You have subdued %s.", m_name);
				} else {
					my_strcap(m_name);
					msgt(soundfx, "%s has been subdued.", m_name);
				}
			} else {
				if (by_player) {
					msgt(soundfx, "You have destroyed %s.", m_name);
				} else {
					my_strcap(m_name);
					msgt(soundfx, "%s has been destroyed.", m_name);
				}
			}
		} else {
			/* Death by Physical attack -- living monster */
			if (by_player) {
				msgt(soundfx, "You have slain %s.", m_name);
			} else {
				my_strcap(m_name);
				msgt(soundfx, "%s has been slain.", m_name);
			}
		}
	}

	/* Give some experience for the kill */
    new_exp = adjusted_mon_exp(race, true);
    player_exp_gain(p, new_exp);
    p->kill_exp += new_exp;

	/* When the player kills a Unique, it stays dead */
	if (rf_has(race->flags, RF_UNIQUE)) {
		char unique_name[80];
		race->max_num = 0;

		/*
		 * This gets the correct name if we slay an invisible
		 * unique and don't have See Invisible.
		 */
		monster_desc(unique_name, sizeof(unique_name), mon, MDESC_DIED_FROM);

		/* Log the slaying of a unique */
		if (streq(race->base->name, "deathblade")) {
			strnfmt(buf, sizeof(buf), "Subdued %s", unique_name);
		} else if (monster_is_nonliving(mon)) {
			strnfmt(buf, sizeof(buf), "Destroyed %s", unique_name);
		} else {
			strnfmt(buf, sizeof(buf), "Killed %s", unique_name);
		}
		history_add(p, buf, HIST_SLAY_UNIQUE);
	}

	/* Count kills this life */
	if (lore->pkills < SHRT_MAX) lore->pkills++;

	/* Count kills in all lives */
	if (lore->tkills < SHRT_MAX) lore->tkills++;

    /* Since it was killed, it was definitely encountered */
    if (!mon->encountered) {
        new_exp = adjusted_mon_exp(mon->race, false);

        /* Gain experience for encounter */
        player_exp_gain(p, new_exp);
        p->encounter_exp += new_exp;

        /* Update stats */
        mon->encountered = true;
        lore->psights++;
        if (lore->tsights < SHRT_MAX) lore->tsights++;
    }

	/* Update lore and tracking */
	lore_update(mon->race, lore);
	monster_race_track(p->upkeep, mon->race);

	/* Lower the morale of similar monsters that can see the deed. */
	if (rf_has(race->flags, RF_ESCORT) || rf_has(race->flags, RF_ESCORTS)) {
		multiplier = 4;
	}
	scare_onlooking_friends(mon, -40 * multiplier);

	/* Generate treasure for eligible monsters */
	if (!chasm && !rf_has(mon->race->flags, RF_TERRITORIAL)) {
		drop_loot(cave, mon, mon->grid, false);
	}

	/* Update monster list window */
	p->upkeep->redraw |= PR_MONLIST;

	/* Delete the monster */
	delete_monster_idx(cave, mon->midx);
}

/**
 * Decreases a monster's hit points by `dam` and handle monster death.
 *
 * Hack -- we "delay" fear messages by passing around a "fear" flag.\\TODO??
 *
 * We announce monster death (using an optional "death message" (`note`)
 * if given, and a otherwise a generic killed/destroyed message).
 *
 * Returns true if the monster has been killed (and deleted).
 */
bool mon_take_hit(struct monster *mon, struct player *p, int dam,
				  const char *note)
{
	/* Redraw (later) if needed */
	if (p->upkeep->health_who == mon)
		p->upkeep->redraw |= (PR_HEALTH);

	/* No damage, we're done */
	if (dam == 0) return false;

	/* Hurt it */
	mon->hp -= dam;
	if (mon->hp <= 0) {
		/* It is dead now */
		monster_death(mon, p, true, note, false);

		/* Monster is dead */
		return true;
	}

	/* If there was real damage dealt... */
	if (dam > 0) {
		/* Wake it up */
		make_alert(mon, dam); 

		/* Recalculate desired minimum range */
		if (dam > 0) mon->min_range = 0;
	}

	/* Monster will always go active */
	mflag_on(mon->mflag, MFLAG_ACTIVE);

	/* Not dead yet */
	return false;
}

/**
 * Checks whether monsters on two separate coordinates are of the same type
 * (i.e. the same letter or share an RF3_ race flag)
 */
bool similar_monsters(struct monster *mon1, struct monster *mon2)
{
	bitflag mask[RF_SIZE];

	/* First check if there are two monsters */
	if (!mon1 || !mon2) return false;

	/* Monsters have the same base */
	if (mon1->race->base == mon2->race->base) return true;
	
	/* Monsters have the same race flag */
	create_mon_flag_mask(mask, RFT_RACE_N, RFT_MAX);
	rf_inter(mask, mon1->race->flags);
	if (rf_is_inter(mask, mon2->race->flags)) return true;
	
	/* Not the same */
	return false;
}

/**
 * Cause a temporary penalty to morale in monsters of the same type who can see
 * the  specified monster. (Used when it dies and for cruel blow).
 */
void scare_onlooking_friends(const struct monster *mon, int amount)
{
	int i;

	/* Scan monsters */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon1 = cave_monster(cave, i);;
		struct monster_race *race = mon1->race;

		/* Skip dead monsters */
		if (!race) continue;

		/* Only consider alert monsters of the same type in line of sight */
		if ((mon1->alertness >= ALERTNESS_ALERT) &&
			!rf_has(race->flags, RF_NO_FEAR) &&
			similar_monsters((struct monster *) mon, mon1) &&
			los(cave, mon1->grid, mon->grid)) {
			/* Cause a temporary morale penalty */
			mon1->tmp_morale += amount;
		}
	}

	return;
}
/**
 * ------------------------------------------------------------------------
 * Monster inventory utilities
 * ------------------------------------------------------------------------ */
/**
 * Add the given object to the given monster's inventory.
 *
 * Currently always returns true - it is left as a bool rather than
 * void in case a limit on monster inventory size is proposed in future.
 */
bool monster_carry(struct chunk *c, struct monster *mon, struct object *obj)
{
	struct object *held_obj;

	/* Scan objects already being held for combination */
	for (held_obj = mon->held_obj; held_obj; held_obj = held_obj->next) {
		/* Check for combination */
		if (object_mergeable(held_obj, obj, OSTACK_MONSTER)) {
			/* Combine the items */
			object_absorb(held_obj, obj);

			/* Result */
			return true;
		}
	}

	/* Forget location */
	obj->grid = loc(0, 0);

	/* Link the object to the monster */
	obj->held_m_idx = mon->midx;

	/* Add the object to the monster's inventory */
	list_object(c, obj);
	if (obj->known) {
		obj->known->oidx = obj->oidx;
		player->cave->objects[obj->oidx] = obj->known;
	}
	pile_insert(&mon->held_obj, obj);

	/* Result */
	return true;
}
