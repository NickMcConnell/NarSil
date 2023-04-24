/**
 * \file effect-handler-attack.c
 * \brief Handler functions for attack effects
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

#include "combat.h"
#include "effect-handler.h"
#include "game-input.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "obj-desc.h"
#include "obj-knowledge.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "trap.h"


static void get_target(struct source origin, int dir, struct loc *grid,
					   int *flags)
{
	switch (origin.what) {
		case SRC_MONSTER: {
			struct monster *monster = cave_monster(cave, origin.which.monster);

			if (!monster) break;

			*flags |= (PROJECT_PLAY);

			if (monster->target.midx > 0) {
				struct monster *mon = cave_monster(cave, monster->target.midx);
				*grid = mon->grid;
			} else {
				*grid = player->grid;
			}

			break;
		}

		case SRC_PLAYER:
			if (dir == DIR_TARGET && target_okay(z_info->max_range)) {
				target_get(grid);
			} else {
				/* Use the adjacent grid in the given direction as target */
				*grid = loc_sum(player->grid, ddgrid[dir]);
			}

			break;

		default:
			*flags |= PROJECT_PLAY;
			*grid = player->grid;
			break;
	}
}

/**
 * Apply the project() function in a direction, or at a target
 */
static bool project_aimed(struct source origin, int typ, int dir, int dd,
						  int ds, int dif, int flg, const struct object *obj)
{
	struct loc grid = loc(-1, -1);

	/* Pass through the target if needed */
	flg |= (PROJECT_THRU);

	get_target(origin, dir, &grid, &flg);

	/* Aim at the target, do NOT explode */
	return (project(origin, 0, grid, dd, ds, dif, typ, flg, 0, false, obj));
}

/**
 * Heal the player by a given percentage of their wounds, or a minimum
 * amount, whichever is larger.
 *
 * context->value.base should be the minimum, and
 * context->value.m_bonus the percentage
 */
bool effect_handler_HEAL_HP(effect_handler_context_t *context)
{
	int num, minh;

	/* Always ID */
	context->ident = true;

	/* No healing needed */
	if (player->chp >= player->mhp) return (true);

	/* Figure percentage healing level */
	num = ((player->mhp - player->chp) * context->value.m_bonus) / 100;

	/* Enforce minimum */
	minh = context->value.base
		+ damroll(context->value.dice, context->value.sides);
	if (num < minh) num = minh;
	if (num <= 0) {
		/*
		 * There's no healing: either because not damaged enough for the
		 * the bonus amount to matter or the effect was misconfigured.
		 */
		return true;
	}

	/* Gain hitpoints */
	player->chp += num;

	/* Enforce maximum */
	if (player->chp >= player->mhp) {
		player->chp = player->mhp;
		player->chp_frac = 0;
	}

	/* Redraw */
	player->upkeep->redraw |= (PR_HP);

	/* Print a nice message */
	if (num < 5)
		msg("You feel a little better.");
	else if (num < 15)
		msg("You feel better.");
	else if (num < 35)
		msg("You feel much better.");
	else
		msg("You feel very good.");

	return (true);
}

/**
 * Deal damage from the current monster or trap to the player
 */
bool effect_handler_DAMAGE(effect_handler_context_t *context)
{
	int dam = effect_calculate_value(context);
	char killer[80];

	/* Always ID */
	context->ident = true;

	switch (context->origin.what) {
		case SRC_MONSTER: {
			struct monster *mon = cave_monster(cave,
											   context->origin.which.monster);

			monster_desc(killer, sizeof(killer), mon, MDESC_DIED_FROM);
			break;
		}

		case SRC_TRAP: {
			struct trap *trap = context->origin.which.trap;
			const char *article = is_a_vowel(trap->kind->desc[0]) ? "an " : "a ";
			strnfmt(killer, sizeof(killer), "%s%s", article, trap->kind->desc);
			break;
		}

		case SRC_CHEST_TRAP: {
			struct chest_trap *trap = context->origin.which.chest_trap;
			strnfmt(killer, sizeof(killer), "%s", trap->msg_death);
			break;
		}

		case SRC_PLAYER: {
			if (context->msg) {
				my_strcpy(killer, context->msg, sizeof(killer));
			} else {
				my_strcpy(killer, "yourself", sizeof(killer));
			}
			break;
		}

		case SRC_NONE: {
			my_strcpy(killer, "a bug", sizeof(killer));
			break;
		}

		default: break;
	}

	/* Hit the player */
	take_hit(player, dam, killer);

	return true;
}


/**
 * Dart trap (yes, it needs its own effect)
 */
bool effect_handler_DART(effect_handler_context_t *context)
{
	int dam = effect_calculate_value(context);
	int prt = protection_roll(player, PROJ_HURT, false, RANDOMISE);
	char *name;

	assert(context->origin.what == SRC_TRAP);
	name = context->origin.which.trap->kind->name;
	if (check_hit(context->radius, true, context->origin)) {
		if (dam > prt) {
			msg("A small dart hits you!");

			/* Do a tiny amount of damage */
			take_hit(player, context->other, name);
			event_signal_combat_damage(EVENT_COMBAT_DAMAGE, context->value.dice,
									   context->value.sides, prt + 1, -1, -1,
									   prt, 100, PROJ_HURT, false);

			/* Reduce the stat */
			player_stat_dec(player, context->subtype);
		} else {
			msg("A small dart hits you, but is deflected by your armour.");

			event_signal_combat_damage(EVENT_COMBAT_DAMAGE, context->value.dice,
									   context->value.sides, dam, -1, -1, prt,
									   100, PROJ_HURT, false);
		}
	} else {
		msg("A small dart barely misses you.");
	}

	/* Make a small amount of noise */
	monsters_hear(true, false, 5);

	return true;
}

/**
 * Fall in a pit - player only
 */
bool effect_handler_PIT(effect_handler_context_t *context)
{
	bool spiked = (context->subtype == 1);
	square_set_feat(cave, player->grid, spiked ? FEAT_SPIKED_PIT : FEAT_PIT);
	player_fall_in_pit(player, spiked);
	return true;
}

/**
 * Apply a "project()" directly to all viewable monsters.  If context->other is
 * set, the effect damage boost is applied.  This is a hack - NRM
 *
 * Note that affected monsters are NOT auto-tracked by this usage.
 */
bool effect_handler_PROJECT_LOS(effect_handler_context_t *context)
{
	int i;
	int typ = context->subtype;
	struct loc origin = origin_get_loc(context->origin);
	int flg = PROJECT_JUMP | PROJECT_KILL | PROJECT_HIDE;

	/* Affect all (nearby) monsters */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Paranoia -- Skip dead monsters */
		if (!mon->race) continue;

		/* Don't affect the caster */
		if (mon->midx == cave->mon_current) continue;

		/* Require line of sight */
		if (!los(cave, origin, mon->grid)) continue;

		/* Require line of fire - assumes player is the origin - NRM */
		if (!square_isfire(cave, mon->grid)) continue;

		/* Jump directly to the monster */
		if (project(source_player(), 0, mon->grid, 0, 0, context->value.base,
					typ, flg, 0, 0, context->obj)) {
			context->ident = true;
		}
	}

	/* Result */
	return true;
}

/**
 * Apply a "project()" directly to all grids.
 */
bool effect_handler_PROJECT_LOS_GRIDS(effect_handler_context_t *context)
{
	struct loc grid;
	int typ = context->subtype;
	int flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_JUMP | PROJECT_HIDE;

	/* Affect all viewable grids */
	for (grid.y = player->grid.y - z_info->max_sight;
		 grid.y <= player->grid.y + z_info->max_sight; grid.y++) {
		for (grid.x = player->grid.x - z_info->max_sight;
			 grid.x <= player->grid.x + z_info->max_sight; grid.x++) {
			/* Grid must be in bounds and in the player's LoS */
			if (!square_in_bounds_fully(cave, grid)) continue;
			if (!square_isview(cave, grid)) continue;

			if (project(source_player(), 0, grid, 0, 0, context->value.base,
						typ, flg, 0, 0, context->obj))
				context->ident = true;
		}
	}

	/* Result */
	return true;
}

/**
 * Drop the ceiling on the player.
 *
 * The player will take damage and jump into a safe grid if possible,
 * otherwise, they will take crush damage.  Players who dodge may still
 * be hit by rubble.
 */
bool effect_handler_DEADFALL(effect_handler_context_t *context)
{
	int i;
	struct loc pgrid = player->grid, safe_grid = loc(0, 0);
	int safe_grids = 0;
	int dam, prt, net_dam = 0;

	/* Check around the player */
	for (i = 0; i < 8; i++) {
		/* Get the location */
		struct loc grid = loc_sum(pgrid, ddgrid_ddd[i]);

		/* Skip non-empty grids - allow pushing into traps and webs */
		if (!square_isopen(cave, grid)) continue;

		/* Count "safe" grids, apply the randomizer */
		if ((++safe_grids > 1) && (randint0(safe_grids) != 0)) continue;

		/* Save the safe location */
		safe_grid = grid;
	}

	/* Check for safety */
	if (!safe_grids) {
		/* Hurt the player a lot */
		msg("You are severely crushed!");
		dam = damroll(6, 8);

		/* Protection */
		prt = protection_roll(player, PROJ_HURT, false, false);
		net_dam = (dam - prt > 0) ? (dam - prt) : 0;

		event_signal_combat_attack(EVENT_COMBAT_ATTACK, source_none(),
								   source_player(), true, -1, -1, -1, -1,
								   false);
		event_signal_combat_damage(EVENT_COMBAT_DAMAGE, 6, 8, dam, -1, -1, prt,
								   100, PROJ_HURT, false);

		(void)player_inc_timed(player, TMD_STUN, dam * 4, true, true,
			true);
	} else {
		/* Destroy the grid, and push the player to safety */
		if (check_hit(20, true, context->origin)) {
			msg("You are struck by rubble!");
			dam = damroll(4, 8);

			/* Protection */
			prt = protection_roll(player, PROJ_HURT, false, false);

			event_signal_combat_damage(EVENT_COMBAT_DAMAGE, 4, 8, dam, -1, -1,
									   prt, 100, PROJ_HURT, false);
			net_dam = (dam - prt > 0) ? (dam - prt) : 0;

			(void)player_inc_timed(player, TMD_STUN, dam * 4, true,
				true, true);
		} else {
			msg("You nimbly dodge the falling rock!");
		}

		/* Move player */
		monster_swap(pgrid, safe_grid);
	}

	/* Take the damage */
	take_hit(player, net_dam, "a deadfall");
			
	/* Drop rubble */
	square_set_feat(cave, pgrid, FEAT_RUBBLE);

	return true;
}

/**
 * Induce an earthquake of the radius context->radius centred on the
 * instigator.
 *
 * Does rd8 damage at the centre, and one less die each square out 
 * from there. If a square doesn't have a monster in it after the damage
 * it might be transformed to a different terrain (eg floor to rubble,
 * rubble to wall, wall to rubble), with a damage% chance. Note that
 * no damage is done to the square at the epicentre.
 * 
 * The player will take damage and jump into a safe grid if possible,
 * otherwise, he will tunnel through the rubble instantaneously.
 *
 * Monsters will take damage, and jump into a safe grid if possible,
 * otherwise they will be buried in the rubble, disappearing from
 * the level in the same way that they do when banished.
 *
 * Note that players and monsters (except eaters of walls and passers
 * through walls) will never occupy the same grid as a wall (or door).
 */
bool effect_handler_EARTHQUAKE(effect_handler_context_t *context)
{
	int r = effect_calculate_value(context);;
	bool melee = context->other;
	struct loc pgrid = player->grid;
	bool vis = (context->origin.what == SRC_PLAYER);
	int i;
	struct loc offset, pit = loc(0, 0);
	bool fall_in = false;
	struct loc centre = origin_get_loc(context->origin);
	int player_damage = 0, player_dd = 0, player_ds = 0;

	context->ident = true;

	/* No effect on the surface */
	if (!player->depth) {
		msg("The ground shakes for a moment.");
		return true;
	}

	/* Paranoia -- Enforce maximum range */
	if (r > 10) r = 10;

	/* If it's a monster creating the earthquake, get it */
	if (context->origin.what == SRC_MONSTER) {
		struct monster *mon = cave_monster(cave, context->origin.which.monster);

		/* Set visibility */
		vis = monster_is_visible(mon);

		/* Pit creation by Morgoth */
		if (mon->race == lookup_monster("Morgoth, Lord of Darkness")) {
			struct loc safe;
			bool in_pit = square_ispit(cave, player->grid);
			int num = 0;

			/* Locate the pit */
			if (melee) {
				int dir = rough_direction(mon->grid, pgrid);
				pit = loc_sum(mon->grid, ddgrid[dir]);
			} else {
				pit = pgrid;
			}

			/* See if the player is in the pit, and if they can dodge */
			if (loc_eq(pit, pgrid)) {
				if (!in_pit) {
					/* Check around the player for safe locations to dodge to */
					for (i = 0; i < 8; i++) {
						/* Get the location */
						struct loc test = loc_sum(pgrid, ddgrid_ddd[i]);

						/* Skip non-empty grids */
						if (!square_isempty(cave, test)) continue;

						/* Count "safe" grids, apply the randomizer */
						if ((++num > 1) && (randint0(num) != 0)) continue;

						/* Save the safe location */
						safe = test;
					}
				}

				if (num > 0) {
					monster_swap(pgrid, safe);
				} else {
					/* Remember to make the player fall into the pit later */
					fall_in = true;
				}
			}

			if (square_changeable(cave, pit)) {
				/* Delete objects */
				square_excise_pile(cave, pit);

				/* Change the feature */
				square_set_feat(cave, pit, FEAT_PIT);
			}
		}
	}

	/* Earthquake damage */
	for (offset.y = -r; offset.y <= r; offset.y++) {
		for (offset.x = -r; offset.x <= r; offset.x++) {
			int ds, dd, damage, net_dam, prt;

			/* Extract the location and distance */
			struct loc grid = loc_sum(centre, offset);
			int dist = distance(centre, grid);

			/* Get the monster, if any */
			struct monster *mon = square_monster(cave, grid);

			/* Skip illegal grids */
			if (!square_in_bounds_fully(cave, grid)) continue;

			/* Skip distant grids */
			if (dist > r) continue;

			/* Skip the epicenter */
			if (loc_is_zero(offset)) continue;

			/* Roll the damage for this square */
			dd = r + 1 - dist;
			ds = 8;
			damage = damroll(dd, ds);

			/* If the player is on the square... */
			if (square_isplayer(cave, grid)) {
				player_damage = damage;
				player_dd = dd;
				player_ds = ds;
			} else if (mon) {
				/* If a monster is on the square... */
				char m_name[80];

				/* Describe the monster */
				monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);

				/* Apply monster protection */
				prt = damroll(mon->race->pd, mon->race->ps);
				net_dam = damage - prt;

				/* Apply damage after protection */
				if (net_dam > 0) {
					bool killed = false;

					if (monster_is_visible(mon)) {
						/* Message for each visible monster */
						msg("%s is hit by falling debris.", m_name);

						/* Update combat rolls */
						event_signal_combat_attack(EVENT_COMBAT_ATTACK,
												   context->origin,
												   source_monster(mon->midx),
												   vis, -1, -1, -1, -1, false);
						event_signal_combat_damage(EVENT_COMBAT_DAMAGE, dd, ds,
												   damage, mon->race->pd,
												   mon->race->ps, prt, 100,
												   PROJ_HURT, false);
					}

					/* Do the damage and check for death */
					killed = mon_take_hit(mon, player, net_dam, NULL);

					/* Special effects for survivors */
					if (!killed) {
						/* Some creatures are resistant to stunning */
						if (rf_has(mon->race->flags, RF_NO_STUN)) {
							struct monster_lore *lore = get_lore(mon->race);

							/* Mark the lore */
							if (monster_is_visible(mon)) {
								rf_on(lore->flags, RF_NO_STUN);
							}
						} else {
							mon_inc_timed(mon, MON_TMD_STUN, net_dam * 4, 0);
						}

						/* Alert it */
						set_alertness(mon, MAX(mon->alertness + 10,
											   ALERTNESS_VERY_ALERT));

						/* Message for non-visible monsters */
						if (!monster_is_visible(mon)) {
							message_pain(mon, damage);
						}
					}
				}
			}

			/* Squares without monsters/player will sometimes get transformed;
			 * note that a monster may have been there but got killed by now */
			if (!square_isoccupied(cave, grid) && percent_chance(damage) &&
				!loc_eq(grid, pit)) {
				/* Destroy location (if valid) */
				if (square_changeable(cave, grid)) {
                    int t, feat = FEAT_FLOOR, adj_chasms = 0;

					/* Delete objects */
					square_excise_pile(cave, grid);

                    /* Count adjacent chasm squares */
                    for (i = 0; i < 8; i++) {
                        /* Get the location */
                        struct loc adj_grid = loc_sum(grid, ddgrid_ddd[i]);

                        /* count the chasms */
                        if (square_ischasm(cave, adj_grid)) {
							adj_chasms++;
						}
                    }

					/* Wall (or floor) type */
					t = randint0(100);

                    /* Change based on existing type */
                    if (square_ischasm(cave, grid)) {
                        /* If we started with a chasm - mostly unchanged */
                        if (one_in_(10)) {
                            if (t < 10) {
								feat = FEAT_RUBBLE;
                            } else if (t < 70) {
								feat = FEAT_GRANITE;
                            } else {
								feat = FEAT_QUARTZ;
							}
						}
                    } else if (!square_iswall(cave, grid)) {
						/* If we started with open floor */
                        if (randint1(8) <= adj_chasms + 1) {
							feat = FEAT_CHASM;
						} else if (t < 40) {
							feat = FEAT_RUBBLE;
						} else if (t < 80) {
							feat = FEAT_GRANITE;
						} else {
							feat = FEAT_QUARTZ;
						}
					} else if (square_isrubble(cave, grid)) {
						/* If we started with rubble */
                        if (randint1(32) <= adj_chasms) {
							feat = FEAT_CHASM;
                        } else if (t < 40) {
							feat = FEAT_FLOOR;
						} else if (t < 70) {
							feat = FEAT_GRANITE;
						} else {
                            feat = FEAT_QUARTZ;
						}
					} else {
						/* If we started with a wall of some sort */
                        if (randint1(32) <= adj_chasms) {
							feat = FEAT_CHASM;
						} else if (t < 80) {
							feat = FEAT_RUBBLE;
						} else {
                            feat = FEAT_FLOOR;
						}
					}

                    /* Change the feature (unless it would be making a chasm
					 * at 950 or 1000 ft) */
                    if ((feat != FEAT_CHASM) &&
						(player->depth < z_info->dun_depth - 1)) {
                        square_unmark(cave, grid);
						square_set_feat(cave, grid, feat);
                    }
				}
			}
		}
	}

	if (player_damage) {
		int prt, net_dam;

		/* Appropriate message */
		msg("You are pummeled with debris!");

		/* Apply protection */
		prt = protection_roll(player, PROJ_HURT, false, RANDOMISE);
		net_dam = player_damage - prt;

		/* Take the damage */
		if (net_dam > 0) {
			take_hit(player, net_dam, "an earthquake");
		}

		if (!player->is_dead) {
			player_inc_timed(player, TMD_STUN, net_dam * 4,
				true, true, true);
		}

		/* Update combat rolls */
		event_signal_combat_attack(EVENT_COMBAT_ATTACK, context->origin,
			source_player(), vis, -1, -1, -1, -1, false);
		event_signal_combat_damage(EVENT_COMBAT_DAMAGE, player_dd,
			player_ds, player_damage, -1, -1, prt, 100, PROJ_HURT,
			false);
	}

	/* Fall into the pit if there were no safe squares to jump to */
	if (fall_in && !player->is_dead && square_ispit(cave, pgrid)) {
		int damage;
		msg("You fall back into the newly made pit!");

		/* Falling damage */
		damage = damroll(2, 4);

		/* Update combat rolls */
		event_signal_combat_attack(EVENT_COMBAT_ATTACK, source_grid(pgrid),
								   source_player(), true, -1, -1, -1, -1,false);
		event_signal_combat_damage(EVENT_COMBAT_DAMAGE, 2, 4, damage, -1, -1,
								   0, 0, PROJ_HURT, false);

		/* Take the damage */
		take_hit(player, damage, "falling into a pit");
	}

	/* Make a lot of noise */
	monsters_hear(true, false, -30);

	/* Fully update the visuals */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw map and health bar */
	player->upkeep->redraw |= (PR_MAP | PR_HEALTH);

	/* Window stuff */
	player->upkeep->redraw |= (PR_MONLIST | PR_ITEMLIST);

	return true;
}

/**
 * Project from the source grid at the player, with full intensity out to
 * its radius
 * Affect the player
 */
bool effect_handler_SPOT(effect_handler_context_t *context)
{
	struct loc pgrid = player->grid;
	int rad = context->radius ? context->radius : 0;

	int flg = PROJECT_JUMP | PROJECT_PLAY;

	/* Aim at the target */
	if (project(context->origin, rad, pgrid, context->value.dice,
				context->value.sides, context->value.m_bonus, context->subtype,
				flg, 0, true, NULL))
		context->ident = true;

	return true;
}

/**
 * Project from the player's grid, act as a ball, with full intensity out as
 * far as the given diameter
 * Affect grids, objects, and monsters
 */
bool effect_handler_SPHERE(effect_handler_context_t *context)
{
	struct loc pgrid = player->grid;
	int rad = context->radius ? context->radius : 0;
	int diameter_of_source = context->other ? context->other : 0;

	int flg = PROJECT_STOP | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL;

	if (context->origin.what == SRC_MONSTER) {
		flg |= PROJECT_PLAY;
	}

	/* Explode */
	if (project(context->origin, rad, pgrid, context->value.dice,
				context->value.sides, context->value.m_bonus, context->subtype,
				flg, 0, diameter_of_source, NULL))
		context->ident = true;

	return true;
}

/**
 * Cast a ball spell that explodes immediately on the origin and
 * hurts everything.
 * Affect grids, objects, and monsters
 */
bool effect_handler_EXPLOSION(effect_handler_context_t *context)
{
	int dd = context->value.dice;
	int ds = context->value.sides;
	int dif = context->value.base;
	int rad = context->radius ? context->radius : 0;
	struct loc target = origin_get_loc(context->origin);

	int flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_JUMP |
		PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY;

	/* Explode at the target */
	if (project(context->origin, rad, target, dd, ds, dif, context->subtype,
				flg, 0, true, context->obj))
		context->ident = true;

	return true;
}

/**
 * Breathe an element, in a cone from the breather
 * Affect grids, objects, and monsters
 * context->subtype is element, context->other degrees of arc
 * If context->radius is set it is radius of breath, but it usually isn't
 */
bool effect_handler_BREATH(effect_handler_context_t *context)
{
	int type = context->subtype;
	struct loc target = player->grid;

	/* Breath width */
	int degrees_of_arc = context->other;

	/*
	 * Distance breathed generally has no fixed limit; if the radius set
	 * is zero, the displayed effect will only go out to the range where
	 * damage can still be inflicted (i.e. the PROJECT_RANGE_DAM flag).
	 */
	int rad = context->radius;

	int flg = PROJECT_ARC | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL
		| PROJECT_PLAY | PROJECT_RANGE_DAM;

	/* Breathe at the target */
	if (project(context->origin, rad, target, context->value.dice,
				context->value.sides, context->value.m_bonus, type, flg,
				degrees_of_arc, 0, context->obj))
		context->ident = true;

	return true;
}

/**
 * Cast a bolt spell
 * Stop if we hit a monster, as a bolt
 * Affect monsters (not grids or objects)
 */
bool effect_handler_BOLT(effect_handler_context_t *context)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	(void) project_aimed(context->origin, context->subtype, context->dir,
						 context->value.dice, context->value.sides,
						 context->value.m_bonus, flg, context->obj);
	if (!player->timed[TMD_BLIND])
		context->ident = true;
	return true;
}

/**
 * Cast a beam spell
 * Pass through monsters, as a beam
 * Affect monsters (not grids or objects)
 */
bool effect_handler_BEAM(effect_handler_context_t *context)
{
	int flg = PROJECT_BEAM | PROJECT_KILL;

	(void) project_aimed(context->origin, context->subtype, context->dir,
		context->value.dice, context->value.sides,
		context->value.m_bonus, flg, context->obj);
	if (!player->timed[TMD_BLIND]) {
		context->ident = true;
	}
	return true;
}

/**
 * Cast a beam spell which affects grids or objects, but not monsters.
 * Allows for targeting up or down (an effect that uses that should set the
 * other parameter for the effect to a non-zero value), but the handling of
 * the effect subtype there is not general:  currently assumes it is KILL_WALL.
 */
bool effect_handler_TERRAIN_BEAM(effect_handler_context_t *context)
{
	if (context->dir == DIR_UP || context->dir == DIR_DOWN) {
		/* Verify that the effect allows targeting up or down. */
		assert(context->other);
		assert(context->subtype == PROJ_KILL_WALL);
		if (context->dir == DIR_UP) {
			player_blast_ceiling(player);
		} else {
			player_blast_floor(player);
		}
		context->ident = true;
	} else {
		int flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM
			| PROJECT_WALL;

		(void) project_aimed(context->origin, context->subtype,
			context->dir, context->value.dice, context->value.sides,
			context->value.m_bonus, flg, context->obj);
		if (!player->timed[TMD_BLIND]) {
			context->ident = true;
		}
	}
	return true;
}
