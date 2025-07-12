/**
 *  \file project-mon.c
 *  \brief projection effects on monsters
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
#include "generate.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-msg.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "player-calcs.h"
#include "project.h"
#include "source.h"


/**
 * ------------------------------------------------------------------------
 * Monster handlers
 * ------------------------------------------------------------------------ */

typedef struct project_monster_handler_context_s {
	const struct source origin;
	const int r;
	const struct loc grid;
	int dam;
	int dif;
	const int type;
	bool seen; /* Ideally, this would be const, but we can't with C89 initialization. */
	const bool id;
	struct monster *mon;
	struct monster_lore *lore;
	bool obvious;
	bool skipped;
	bool alert;
	uint16_t flag;
	enum mon_messages hurt_msg;
	enum mon_messages die_msg;
	int mon_timed[MON_TMD_MAX];
} project_monster_handler_context_t;
typedef void (*project_monster_handler_f)(project_monster_handler_context_t *);

/**
 * Resist an attack if the monster has the given elemental flag.
 *
 * If the effect is seen, we learn that the monster has a given flag.
 * Resistance is divided by the factor.
 *
 * \param context is the project_m context.
 * \param flag is the RF_ flag that the monster must have.
 * \param factor is the divisor for the base damage.
 */
static void project_monster_resist_element(project_monster_handler_context_t *context, int flag)
{
	if (context->seen) rf_on(context->lore->flags, flag);
	if (rf_has(context->mon->race->flags, flag)) {
		context->hurt_msg = MON_MSG_RESIST_A_LOT;
		context->dam = 0;
	}
}

/**
 * Resist an attack if the monster has the given flag or hurt the monster
 * more if it has another flag.
 *
 * If the effect is seen, we learn the status of both flags. Hurt is multiplied by hurt_factor.
 *
 * \param context is the project_m context.
 * \param hurt_flag is the RF_ flag that the monster must have to use the hurt factor.
 * \param imm_flag is the RF_ flag that the monster must have to use the resistance factor.
 * \param hurt_factor is the hurt multiplier for the base damage.
 * \param hurt_msg is the message that should be displayed when the monster is hurt.
 * \param die_msg is the message that should be displayed when the monster dies.
 */
static void project_monster_hurt_immune(project_monster_handler_context_t *context, int hurt_flag, int imm_flag, int hurt_factor, enum mon_messages hurt_msg, enum mon_messages die_msg)
{
	if (context->seen) {
		rf_on(context->lore->flags, imm_flag);
		rf_on(context->lore->flags, hurt_flag);
	}

	if (rf_has(context->mon->race->flags, imm_flag)) {
		context->hurt_msg = MON_MSG_RESIST_A_LOT;
		context->dam = 0;
	}
	else if (rf_has(context->mon->race->flags, hurt_flag)) {
		context->hurt_msg = hurt_msg;
		context->die_msg = die_msg;
		context->dam *= hurt_factor;
	}
}

/**
 * Hurt the monster if it has a given flag or do no damage.
 *
 * If the effect is seen, we learn the status the flag. There is no damage
 * multiplier.
 *
 * \param context is the project_m context.
 * \param flag is the RF_ flag that the monster must have.
 * \param hurt_msg is the message that should be displayed when the monster is hurt.
 * \param die_msg is the message that should be displayed when the monster dies.
 */
static void project_monster_hurt_only(project_monster_handler_context_t *context, int flag, enum mon_messages hurt_msg, enum mon_messages die_msg)
{
	if (context->seen) rf_on(context->lore->flags, flag);

	if (rf_has(context->mon->race->flags, flag)) {
		int resist = monster_stat(context->mon, STAT_CON) * 2;
		if (skill_check(source_player(), context->dif, resist,
						source_monster(context->mon->midx)) > 0) {
			context->hurt_msg = hurt_msg;
			context->die_msg = die_msg;
		} else {
			context->hurt_msg = MON_MSG_RESIST_A_LOT;
			context->dam = 0;
		}
	} else {
		context->dam = 0;
	}
}

/**
 * Resist an attack if the monster has the given spell flag.
 *
 * If the effect is seen, we learn that the monster has that spell (useful
 * for breaths). Resistance is multiplied by the factor and reduced by
 * a small random amount.
 *
 * \param context is the project_m context.
 * \param flag is the RSF_ flag that the monster must have.
 * \param factor is the multiplier for the base damage.
 */
static void project_monster_breath(project_monster_handler_context_t *context, int flag)
{
	if (rsf_has(context->mon->race->spell_flags, flag)) {
		context->hurt_msg = MON_MSG_RESIST;
		context->dam = 0;
	}
}

/**
 * Run a skill check for a monster to apply a timed effect.
 *
 * If the monster has the given flag, it resists, and the player learns
 * monster lore if the effect is seen.
 *
 * \param context is the project_m context.
 * \param flag is the RF_ flag that the monster must have to resist.
 */
static int project_monster_skill_check(project_monster_handler_context_t *context, int flag)
{
	int resistance = monster_skill(context->mon, SKILL_WILL);
	int dif = context->dif - context->r; 
	if (rf_has(context->mon->race->flags, flag)) resistance += 100;
	return skill_check(context->origin, dif, resistance,
					   source_monster(context->mon->midx));
}

/* Fire damage */
static void project_monster_handler_FIRE(project_monster_handler_context_t *context)
{
	project_monster_hurt_immune(context, RF_HURT_FIRE, RF_RES_FIRE, 2, MON_MSG_CATCH_FIRE, MON_MSG_DISINTEGRATES);
}

/* Cold */
static void project_monster_handler_COLD(project_monster_handler_context_t *context)
{
	project_monster_hurt_immune(context, RF_HURT_COLD, RF_RES_COLD, 2, MON_MSG_BADLY_FROZEN, MON_MSG_FREEZE_SHATTER);
}

/* Poison */
static void project_monster_handler_POIS(project_monster_handler_context_t *context)
{
	project_monster_resist_element(context, RF_RES_POIS);
}

/* Dark -- opposite of Light */
static void project_monster_handler_DARK(project_monster_handler_context_t *context)
{
	project_monster_breath(context, RSF_BR_DARK);
	if (rf_has(context->mon->race->flags, RF_UNDEAD) ||
		context->mon->race->light < 0) {
		context->dam = 0;
		context->hurt_msg = MON_MSG_RESIST_A_LOT;
	}
}

static void project_monster_handler_NOTHING(project_monster_handler_context_t *context)
{
}

static void project_monster_handler_HURT(project_monster_handler_context_t *context)
{
}

static void project_monster_handler_ARROW(project_monster_handler_context_t *context)
{
}

static void project_monster_handler_BOULDER(project_monster_handler_context_t *context)
{
}

/* Acid */
static void project_monster_handler_ACID(project_monster_handler_context_t *context)
{
}

/* Sound -- Sound breathers resist */
static void project_monster_handler_SOUND(project_monster_handler_context_t *context)
{
	context->mon_timed[MON_TMD_STUN] = context->dam;
	context->dam = 0;
}

/* Force */
static void project_monster_handler_FORCE(project_monster_handler_context_t *context)
{
	int resist = monster_stat(context->mon, STAT_CON) * 2;
	if (skill_check(source_player(), context->dif, resist,
					source_monster(context->mon->midx)) > 0) {
		if (monster_is_visible(context->mon)) {
			context->hurt_msg = MON_MSG_PUSHED;
			context->obvious = true;
		}
		knock_back(player->grid, context->mon->grid);
	} else {
		if (monster_is_visible(context->mon)) {
			context->hurt_msg = MON_MSG_NOT_PUSHED;
			context->obvious = true;
		}
	}
}

/* Light -- opposite of Dark */
static void project_monster_handler_LIGHT(project_monster_handler_context_t *context)
{
	if (context->seen) rf_on(context->lore->flags, RF_HURT_LIGHT);

	if (rf_has(context->mon->race->flags, RF_HURT_LIGHT)) {
		context->mon_timed[MON_TMD_STUN] = context->dam;
		context->hurt_msg = MON_MSG_CRINGE_LIGHT;
	}
	context->alert = false;
	context->dam = 0;	
}

/* Stone to Mud */
static void project_monster_handler_KILL_WALL(project_monster_handler_context_t *context)
{
	project_monster_hurt_only(context, RF_STONE, MON_MSG_LOSE_SKIN,
							  MON_MSG_DISSOLVE);
}

/* Sleep */
static void project_monster_handler_SLEEP(project_monster_handler_context_t *context)
{
	int result = project_monster_skill_check(context, RF_NO_SLEEP);
	if (result > 0) {
		if (context->seen) context->obvious = true;
		set_alertness(context->mon, context->mon->alertness - (result + 5));
	} else {
		context->hurt_msg = MON_MSG_UNAFFECTED;
		context->obvious = false;
		if (context->seen && rf_has(context->mon->race->flags, RF_NO_SLEEP)) {
			rf_on(context->lore->flags, RF_NO_SLEEP);
		}
	}
	context->alert = false;
	context->dam = 0;
}

/* Speed Monster (Ignore "dam") */
static void project_monster_handler_SPEED(project_monster_handler_context_t *context)
{
	if (context->seen) context->obvious = true;
	context->mon_timed[MON_TMD_FAST] = context->dam;
	if (context->mon->alertness < ALERTNESS_UNWARY) context->alert = false;
	context->dam = 0;
}

/* Slow Monster */
static void project_monster_handler_SLOW(project_monster_handler_context_t *context)
{
	int result = project_monster_skill_check(context, RF_NO_SLOW);
	if (result > 0) {
		if (context->seen) context->obvious = true;
		context->mon_timed[MON_TMD_SLOW] = result + 10;
	} else {
		context->alert = false;
		context->hurt_msg = MON_MSG_UNAFFECTED;
		context->obvious = false;
		if (context->seen && rf_has(context->mon->race->flags, RF_NO_SLOW)) {
			rf_on(context->lore->flags, RF_NO_SLOW);
		}
	}
	if (context->mon->alertness < ALERTNESS_UNWARY) context->alert = false;
	context->dam = 0;
}

/* Confusion */
static void project_monster_handler_CONFUSION(project_monster_handler_context_t *context)
{
	int result = project_monster_skill_check(context, RF_NO_CONF);
	if (result > 0) {
		if (context->seen) context->obvious = true;
		context->mon_timed[MON_TMD_CONF] = result + 10;
	} else {
		context->hurt_msg = MON_MSG_UNAFFECTED;
		context->obvious = false;
		if (context->seen && rf_has(context->mon->race->flags, RF_NO_CONF)) {
			rf_on(context->lore->flags, RF_NO_CONF);
		}
	}
	context->alert = false;
	context->dam = 0;
}

/* Fear */
static void project_monster_handler_FEAR(project_monster_handler_context_t *context)
{
	int result;
	context->dif += 5;
	result = project_monster_skill_check(context, RF_NO_FEAR);
	if (result > 0) {
		if (context->seen) context->obvious = true;
		context->mon->tmp_morale -= result * 20;
	} else {
		context->alert = false;
		context->hurt_msg = MON_MSG_UNAFFECTED;
		context->obvious = false;
		if (context->seen && rf_has(context->mon->race->flags, RF_NO_FEAR)) {
			rf_on(context->lore->flags, RF_NO_FEAR);
		}
	}
	context->dam = 0;
}

static void project_monster_handler_EARTHQUAKE(project_monster_handler_context_t *context)
{
	context->skipped = true;
	context->dam = 0;
}

static void project_monster_handler_DARK_WEAK(project_monster_handler_context_t *context)
{
	context->skipped = true;
	context->dam = 0;
}

static void project_monster_handler_KILL_DOOR(project_monster_handler_context_t *context)
{
	context->skipped = true;
	context->dam = 0;
}

static void project_monster_handler_LOCK_DOOR(project_monster_handler_context_t *context)
{
	context->skipped = true;
	context->dam = 0;
}

static void project_monster_handler_KILL_TRAP(project_monster_handler_context_t *context)
{
	context->skipped = true;
	context->dam = 0;
}

/* Dispel monster */
static void project_monster_handler_DISP_ALL(project_monster_handler_context_t *context)
{
	context->hurt_msg = MON_MSG_SHUDDER;
	context->die_msg = MON_MSG_DISSOLVE;
	context->dam = context->dif;
}

static const project_monster_handler_f monster_handlers[] = {
	#define ELEM(a) project_monster_handler_##a,
	#include "list-elements.h"
	#undef ELEM
	#define PROJ(a) project_monster_handler_##a,
	#include "list-projections.h"
	#undef PROJ
	NULL
};


/**
 * Deal damage to a monster from another monster.
 *
 * This is a helper for project_m(). It is very similar to mon_take_hit(),
 * but eliminates the player-oriented stuff of that function. It isn't a type
 * handler, but we take a handler context since that has a lot of what we need.
 *
 * \param context is the project_m context.
 * \param m_idx is the cave monster index.
 * \return true if the monster died, false if it is still alive.
 */
static bool project_m_monster_attack(project_monster_handler_context_t *context, int m_idx)
{
	bool mon_died = false;
	bool seen = context->seen;
	int dam = context->dam;
	enum mon_messages die_msg = context->die_msg;
	enum mon_messages hurt_msg = context->hurt_msg;
	struct monster *mon = context->mon;

	/* Redraw (later) if needed */
	if (player->upkeep->health_who == mon)
		player->upkeep->redraw |= (PR_HEALTH);

	/* Become active */
	mflag_on(mon->mflag, MFLAG_ACTIVE);

	/* Hurt the monster */
	mon->hp -= dam;

	/* Dead or damaged monster */
	if (mon->hp <= 0) {
		/* Give detailed messages if destroyed */
		if (!seen) die_msg = MON_MSG_MORIA_DEATH;

		/* Death message */
		add_monster_message(mon, die_msg, false);

		/* Generate treasure, etc */
		monster_death(mon, player, false, NULL, false);

		mon_died = true;
	} else {
		/* Alert it */
		make_alert(mon, 0);

		/* Give detailed messages if visible or destroyed */
		if (seen) {
			if (hurt_msg != MON_MSG_NONE) {
				add_monster_message(mon, hurt_msg, false);
			}
		} else if (dam > 0) {
			/* Pain message */
			message_pain(mon, dam);
		}
	}

	return mon_died;
}

/**
 * Deal damage to a monster from a non-monster source (usually a player,
 * but could also be a trap)
 *
 * This is a helper for project_m(). It isn't a type handler, but we take a
 * handler context since that has a lot of what we need.
 *
 * \param context is the project_m context.
 * \return true if the monster died, false if it is still alive.
 */
static bool project_m_player_attack(project_monster_handler_context_t *context)
{
	bool mon_died = false;
	bool seen = context->seen;
	int dam = context->dam;
	enum mon_messages die_msg = context->die_msg;
	enum mon_messages hurt_msg = context->hurt_msg;
	struct monster *mon = context->mon;

	/* The monster is going to be killed, so display a specific death message.
	 * If the monster is not visible to the player, use a generic message.
	 *
	 * Note that mon_take_hit() below is passed a zero-length string, which
	 * ensures it doesn't print any death message and allows correct ordering
	 * of messages. */
	if (dam > mon->hp) {
		if (!seen) die_msg = MON_MSG_MORIA_DEATH;
		add_monster_message(mon, die_msg, false);
	}

	/* No damage is now going to mean the monster is not hit - and hence
	 * is not woken or released from holding */
	if (dam) {
		mon_died = mon_take_hit(mon, player, dam, "");
	}

	/* If the monster didn't die, provide additional messages about how it was
	 * hurt/damaged. If a specific message isn't provided, display a message
	 * based on the amount of damage dealt. Also display a message
	 * if the hit caused the monster to flee. */
	if (!mon_died) {
		if (seen) {
			if (hurt_msg != MON_MSG_NONE) {
				add_monster_message(mon, hurt_msg, false);
			}
		} else if (dam > 0) {
			message_pain(mon, dam);
		}
	}

	return mon_died;
}

/**
 * Apply side effects from an attack onto a monster.
 *
 * This is a helper for project_m(). It isn't a type handler, but we take a
 * handler context since that has a lot of what we need.
 *
 * \param context is the project_m context.
 * \param m_idx is the cave monster index.
 */
static void project_m_apply_side_effects(project_monster_handler_context_t *context, int m_idx)
{
	struct monster *mon = context->mon;

	for (int i = 0; i < MON_TMD_MAX; i++) {
		if (context->mon_timed[i] > 0) {
			mon_inc_timed(mon,
						  i,
						  context->mon_timed[i],
						  context->flag | MON_TMD_FLG_NOTIFY);
			context->obvious = context->seen;
		}
	}
}

/**
 * Called from project() to affect monsters
 *
 * Called for projections with the PROJECT_KILL flag set, which includes
 * bolt, beam, ball and breath effects.
 *
 * \param origin is the monster list index of the caster
 * \param r is the distance from the centre of the effect
 * \param y the coordinates of the grid being handled
 * \param x the coordinates of the grid being handled
 * \param dam is the "damage" from the effect at distance r from the centre
 * \param typ is the projection (PROJ_) type
 * \param flg consists of any relevant PROJECT_ flags
 * \return whether the effects were obvious
 *
 * Note that this routine can handle "no damage" attacks (like teleport) by
 * taking a zero damage, and can even take parameters to attacks (like
 * confuse) by accepting a "damage", using it to calculate the effect, and
 * then setting the damage to zero.  Note that actual damage should be already 
 * adjusted for distance from the "epicenter" when passed in, but other effects 
 * may be influenced by r.
 *
 * Note that "polymorph" is dangerous, since a failure in "place_monster()"'
 * may result in a dereference of an invalid pointer.  XXX XXX XXX
 *
 * Various messages are produced, and damage is applied.
 *
 * Just casting an element (e.g. plasma) does not make you immune, you must
 * actually be made of that substance, or breathe big balls of it.
 *
 * We assume that "Plasma" monsters, and "Plasma" breathers, are immune
 * to plasma.
 *
 * We assume "Nether" is an evil, necromantic force, so it doesn't hurt undead,
 * and hurts evil less.  If can breath nether, then it resists it as well.
 * This should actually be coded into monster records rather than aasumed - NRM
 *
 * Damage reductions use the following formulas:
 *   Note that "dam = dam * 6 / (randint1(6) + 6);"
 *     gives avg damage of .655, ranging from .858 to .500
 *   Note that "dam = dam * 5 / (randint1(6) + 6);"
 *     gives avg damage of .544, ranging from .714 to .417
 *   Note that "dam = dam * 4 / (randint1(6) + 6);"
 *     gives avg damage of .444, ranging from .556 to .333
 *   Note that "dam = dam * 3 / (randint1(6) + 6);"
 *     gives avg damage of .327, ranging from .427 to .250
 *   Note that "dam = dam * 2 / (randint1(6) + 6);"
 *     gives something simple.
 *
 * In this function, "result" messages are postponed until the end, where
 * the "note" string is appended to the monster name, if not NULL.  So,
 * to make a spell have no effect just set "note" to NULL.  You should
 * also set "notice" to false, or the player will learn what the spell does.
 *
 * Note that this function determines if the player can see anything that
 * happens by taking into account: blindness, line-of-sight, and illumination.
 *
 * Hack -- effects on grids which are memorized but not in view are also seen.
 */
void project_m(struct source origin, int r, struct loc grid, int dam, int ds,
			   int dif, int typ, int flg, bool *did_hit, bool *was_obvious)
{
	struct monster *mon;
	struct monster_lore *lore;

	/* Is the monster "seen"? */
	bool seen = false;
	bool mon_died = false;

	/* Is the effect obvious? */
	bool obvious = false;

	/* Are we trying to id the source of this effect? */
	bool id = (origin.what == SRC_PLAYER) ? !obvious : false;

	int m_idx = square(cave, grid)->mon;

	project_monster_handler_f monster_handler = monster_handlers[typ];
	project_monster_handler_context_t context = {
		origin,
		r,
		grid,
		dam,
		dif,
		typ,
		seen,
		id,
		NULL, /* mon */
		NULL, /* lore */
		obvious,
		false, /* skipped */
		true, /* alert */
		0, /* flag */
		MON_MSG_NONE, /* hurt_msg */
		MON_MSG_DIE, /* die_msg */
		{ 0, 0, 0, 0 },
	};

	*did_hit = false;
	*was_obvious = false;

	/* Walls protect monsters */
	if (!square_ispassable(cave, grid)) return;

	/* No monster here */
	if (!(m_idx > 0)) return;

	/* Never affect projector */
	if (origin.what == SRC_MONSTER && origin.which.monster == m_idx) return;

	/* Obtain monster info */
	mon = cave_monster(cave, m_idx);
	lore = get_lore(mon->race);
	context.mon = mon;
	context.lore = lore;

	/* See visible monsters */
	if (monster_is_visible(mon)) {
		seen = true;
		context.seen = seen;
	}

	/* Some monsters get "destroyed" */
	if (monster_is_nonliving(mon))
		context.die_msg = MON_MSG_DESTROYED;

	/* Force obviousness for certain types if seen. */
	if (projections[typ].obvious && context.seen)
		context.obvious = true;

	/* Monster goes active */
	mflag_on(mon->mflag, MFLAG_ACTIVE);

	/* Mark the monster as attacked by the player */
	if (origin.what == SRC_PLAYER) mflag_on(mon->mflag, MFLAG_HIT_BY_RANGED);

	if (monster_handler != NULL)
		monster_handler(&context);

	/* Wake monster if required */
	if (projections[typ].wake)
		make_alert(mon, 0);

	/* Absolutely no effect */
	if (context.skipped) return;

	/* Apply damage to the monster, based on who did the damage. */
	if (origin.what == SRC_MONSTER) {
		mon_died = project_m_monster_attack(&context, m_idx);
	} else {
		mon_died = project_m_player_attack(&context);
	}

	if (!mon_died)
		project_m_apply_side_effects(&context, m_idx);

	/* Update locals, since the project_m_* functions can change some values. */
	mon = context.mon;
	obvious = context.obvious;

	/* Check for NULL, since polymorph can occasionally return NULL. */
	if (mon != NULL) {
		/* Update the monster */
		if (!mon_died)
			update_mon(mon, cave, false);

		/* Redraw the (possibly new) monster grid */
		square_light_spot(cave, mon->grid);

		/* Update monster recall window */
		if (player->upkeep->monster_race == mon->race) {
			/* Window stuff */
			player->upkeep->redraw |= (PR_MONSTER);
		}
	}

	/* Track it */
	*did_hit = true;

	/* Return "Anything seen?" */
	*was_obvious = !!obvious;
}

