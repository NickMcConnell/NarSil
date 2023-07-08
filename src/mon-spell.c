/**
 * \file mon-spell.c
 * \brief Monster spell casting and selection
 *
 * Copyright (c) 2010-14 Chris Carr and Nick McConnell
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
#include "effects.h"
#include "init.h"
#include "mon-attack.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "obj-knowledge.h"
#include "obj-util.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "songs.h"

/**
 * ------------------------------------------------------------------------
 * Spell casting
 * ------------------------------------------------------------------------ */
typedef enum {
	SPELL_TAG_NONE,
	SPELL_TAG_NAME,
	SPELL_TAG_PRONOUN,
} spell_tag_t;

static spell_tag_t spell_tag_lookup(const char *tag)
{
	if (strncmp(tag, "name", 4) == 0)
		return SPELL_TAG_NAME;
	else if (strncmp(tag, "pronoun", 7) == 0)
		return SPELL_TAG_PRONOUN;
	else
		return SPELL_TAG_NONE;
}

/**
 * Lookup a race-specific message for a spell.
 *
 * \param r is the race.
 * \param s_idx is the spell index.
 * \param msg_type is the type of message.
 * \return the text of the message if there's a race-specific one or NULL if
 * there is not.
 */
static const char *find_alternate_spell_message(const struct monster_race *r,
		int s_idx, enum monster_altmsg_type msg_type)
{
	const struct monster_altmsg *am = r->spell_msgs;

	while (1) {
		if (!am) {
			return NULL;
		}
		if (am->index == s_idx && am->msg_type == msg_type) {
			 return am->message;
		}
		am = am->next;
	}
}

/**
 * Print a monster spell message.
 *
 * We fill in the monster name and/or pronoun where necessary in
 * the message to replace instances of {name} or {pronoun}.
 */
static void spell_message(struct monster *mon,
						  const struct monster_spell *spell,
						  bool seen)
{
	const char punct[] = ".!?;:,'";
	char buf[1024] = "\0";
	const char *next;
	const char *s;
	const char *tag;
	const char *in_cursor;
	size_t end = 0;
	struct monster_spell_level *level = spell->level;
	bool smart = rf_has(mon->race->flags, RF_SMART);
	bool silence = player_is_singing(player, lookup_song("Silence"));
	bool is_leading;

	/* Get the right level of message */
	while (level->next && mon->race->spell_power >= level->next->power) {
		level = level->next;
	}

	/* Get the message */
	if (!seen) {
		in_cursor = find_alternate_spell_message(mon->race,	spell->index,
												 MON_ALTMSG_UNSEEN);
		if (in_cursor == NULL) {
			if (smart && (level->smart_blind_silence_message ||
						  level->smart_blind_message)) {
				if (silence && level->smart_blind_silence_message) {
					in_cursor = level->smart_blind_silence_message;
				} else {
					in_cursor = level->smart_blind_message;
				}
			} else {
				if (silence && level->blind_silence_message) {
					in_cursor = level->blind_silence_message;
				} else {
					in_cursor = level->blind_message;
				}
			}
		} else if (in_cursor[0] == '\0') {
			return;
		}
	} else {
		in_cursor = find_alternate_spell_message(mon->race,	spell->index,
												 MON_ALTMSG_SEEN);
		if (in_cursor == NULL) {
			if (smart && (level->smart_silence_message ||
								level->smart_message)) {
				if (silence && level->smart_silence_message) {
					in_cursor = level->smart_silence_message;
				} else {
					in_cursor = level->smart_message;
				}
			} else {
				if (silence && level->silence_message) {
					in_cursor = level->silence_message;
				} else {
					in_cursor = level->message;
				}
			}
		} else if (in_cursor[0] == '\0') {
			return;
		}
	}

	next = strchr(in_cursor, '{');
	is_leading = (next == in_cursor);
	while (next) {
		/* Copy the text leading up to this { */
		strnfcat(buf, 1024, &end, "%.*s", next - in_cursor, in_cursor);

		s = next + 1;
		while (*s && isalpha((unsigned char) *s)) s++;

		/* Valid tag */
		if (*s == '}') {
			/* Start the tag after the { */
			tag = next + 1;
			in_cursor = s + 1;

			switch (spell_tag_lookup(tag)) {
				case SPELL_TAG_NAME: {
					char m_name[80];
					int mdesc_mode = (MDESC_IND_HID |
						MDESC_PRO_HID);

					if (is_leading) {
						mdesc_mode |= MDESC_CAPITAL;
					}
					if (!strchr(punct, *in_cursor)) {
						mdesc_mode |= MDESC_COMMA;
					}
					monster_desc(m_name, sizeof(m_name),
						mon, mdesc_mode);

					strnfcat(buf, sizeof(buf), &end, m_name);
					break;
				}

				case SPELL_TAG_PRONOUN: {
					char m_poss[80];

					/* Get the monster possessive ("his"/"her"/"its") */
					monster_desc(m_poss, sizeof(m_poss), mon, MDESC_PRO_VIS | MDESC_POSS);

					strnfcat(buf, sizeof(buf), &end, m_poss);
					break;
				}

				default: {
					break;
				}
			}
		} else {
			/* An invalid tag, skip it */
			in_cursor = next + 1;
		}

		next = strchr(in_cursor, '{');
		is_leading = false;
	}
	strnfcat(buf, 1024, &end, in_cursor);

	msgt(spell->msgt, "%s", buf);
}

/**
 * Return the chance of a monster casting a spell this turn
 */
int monster_cast_chance(struct monster *mon)
{
	int chance = mon->race->freq_ranged;

	/* Not allowed to cast spells */
	if (!chance) return 0;

	/* Certain conditions always cause a monster to always cast */
	if (mflag_has(mon->mflag, MFLAG_ALWAYS_CAST)) chance = 100;

	/* Cannot use ranged attacks when confused. */
	if (mon->m_timed[MON_TMD_CONF]) chance = 0;

	/* Cannot use ranged attacks during the truce. */
	if (player->truce) chance = 0;
	
	/* Stunned monsters use ranged attacks half as often. */
	if (mon->m_timed[MON_TMD_STUN]) chance /= 2;

	return chance;
}

const struct monster_spell *monster_spell_by_index(int index)
{
	const struct monster_spell *spell = monster_spells;
	while (spell) {
		if (spell->index == index)
			break;
		spell = spell->next;
	}
	return spell;
}

/**
 * Check if a spell effect has been saved against, learn any object property
 * that may have helped
 */
static bool spell_check_for_save(const struct monster_spell *spell)
{
	struct effect *effect = spell->effect;
	bool save = false;
	while (effect) {
		if (effect->index == EF_TIMED_INC) {
			/* Timed effects */
			save = player_inc_check(player, effect->subtype, false);
		} else {
			/* Direct call to player_saving_throw() */
			struct monster *mon = cave->mon_current > 0 ?
				cave_monster(cave, cave->mon_current) : NULL;
			save = player_saving_throw(player, mon, 0);
		}
		effect = effect->next;
	}
	return save;
}


/**
 * Process a monster spell 
 *
 * \param index is the monster spell flag (RSF_FOO)
 * \param mon is the attacking monster
 * \param seen is whether the player can see the monster at this moment
 */
void do_mon_spell(int index, struct monster *mon, bool seen)
{
	const struct monster_spell *spell = monster_spell_by_index(index);
	struct monster_spell_level *level = spell->level;
	bool ident = false;

	/* Tell the player what's going on */
	disturb(player, spell->disturb_stealth);
	spell_message(mon, spell, seen);

	/* Get the right level of save message */
	while (level->next && mon->race->spell_power >= level->next->power) {
		level = level->next;
	}

	/* Try a saving throw if available */
	if (level->save_message && spell_check_for_save(spell)) {
		msg("%s", level->save_message);
	} else {
		if (level->no_save_message) {
			msg("%s", level->no_save_message);
		}
		effect_do(spell->effect, source_monster(mon->midx), NULL, &ident,
				  true, 0, NULL);
	}
}

/**
 * ------------------------------------------------------------------------
 * Spell selection
 * ------------------------------------------------------------------------ */
/**
 * Types of monster spells used for spell selection.
 */
static const struct mon_spell_info {
	uint16_t index;				/* Numerical index (RSF_FOO) */
	int type;				/* Type bitflag */
} mon_spell_types[] = {
    #define RSF(a, b)	{ RSF_##a, b },
    #include "list-mon-spells.h"
    #undef RSF
};


static bool mon_spell_is_valid(int index)
{
	return index > RSF_NONE && index < RSF_MAX;
}

static bool mon_spell_is_archery(int index)
{
	return (mon_spell_types[index].type & RST_ARCHERY) ? true : false;
}

static bool mon_spell_is_breath(int index)
{
	return (mon_spell_types[index].type & RST_BREATH) ? true : false;
}

static bool mon_spell_is_innate(int index)
{
	return (mon_spell_types[index].type & RST_INNATE) ? true : false;
}

static bool mon_spell_is_distant(int index)
{
	return (mon_spell_types[index].type & RST_DISTANT) ? true : false;
}

static bool mon_spell_is_song(int index)
{
	return (mon_spell_types[index].type & RST_SONG) ? true : false;
}

/**
 * Given the monster, *mon, and cave *c, set *dist to the distance to the
 * monster's target and *grid to the target's location.  Either dist or grid
 * may be NULL if that value is not needed.
 */
static void monster_get_target_dist_grid(struct monster *mon, int *dist,
										 struct loc *grid)
{
	if (dist) {
		*dist = mon->cdis;
	}
	if (grid) {
		*grid = player->grid;
	}
}

/**
 * Remove the "bad" spells from a spell list.
 *
 * This includes spells which are too expensive for the monster to cast and
 * spells which have no benefit.
 */
void remove_bad_spells(struct monster *mon, bitflag f[RSF_SIZE])
{
	int tdist;
	struct loc tgrid;
	int path, i;

	/* Get distance from the player */
	monster_get_target_dist_grid(mon, &tdist, &tgrid);

	/* Do we have the player in sight at all? */
	path = projectable(cave, mon->grid, tgrid, PROJECT_STOP);
	if (path == PROJECT_PATH_NO) {
		rsf_wipe(f);
		return;
	}

	/* Iterate through the spells */
	for (i = FLAG_START; i != FLAG_END; i = rsf_next(f, i + 1)) {
		const struct monster_spell *spell;

		/* Do we even have this spell ? */
		if (!rsf_has(f, i)) continue;

		spell = monster_spell_by_index(i);

		/* Check for a clean bolt shot */
		if (mon_spell_is_archery(i) && (path == PROJECT_PATH_NOT_CLEAR)) {
			rsf_off(f, i);
		}

		/* Remove unaffordable spells */
		if (spell->mana > mon->mana) {
			rsf_off(f, i);
		}

		/*
		 * A mindless monster does no further checking if this spell
		 * should be cast.
		 */
		if (rf_has(mon->race->flags, RF_MINDLESS)) {
			continue;
		}

		/* Some attacks have limited range */
		if (tdist > spell->max_range) {
			rsf_off(f, i);
		}

		/* Make sure that missile attacks are never done at melee range or
		 * when afraid */
		if (((tdist == 1) || (mon->stance == STANCE_FLEEING) || player->truce)
				&& (mon_spell_is_distant(i))) {
			rsf_off(f, i);
		}

		/* Make sure that fleeing monsters never use breath attacks */
		if ((mon->stance == STANCE_FLEEING) && mon_spell_is_breath(i)) {
			rsf_off(f, i);
		}

		/* No songs during the truce, or by Morgoth until uncrowned */
		if (mon_spell_is_song(i)) {
			const struct artifact *crown = lookup_artifact_name("of Morgoth");
			if (player->truce) {
				rsf_off(f, i);
			}
			if (rf_has(mon->race->flags, RF_QUESTOR) &&
					!is_artifact_created(crown)) {
				rsf_off(f, i);
			}
		}

		/* Earthquake is only useful if there is no monster in the
		 * smashed square */
		if (i == RSF_EARTHQUAKE) {
			struct loc grid = player->grid;
			if (mon->grid.y > grid.y) {
				grid.y--;
			} else if (mon->grid.y < grid.y) {
				grid.y++;
			}
			if (mon->grid.x > grid.x) {
				grid.x--;
			} else if (mon->grid.x < grid.x) {
				grid.x++;
			}
			if (square_monster(cave, grid)) {
				rsf_off(f, i);
			}
		}

		/* Darkness is only useful if the player's square is lit */
		if ((i == RSF_DARKNESS) && !square_islit(cave, player->grid)) {
			rsf_off(f, i);
		}
	}
}

/**
 * Create a mask of monster spell flags of a specific type.
 *
 * \param f is the flag array we're filling
 * \param ... is the list of flags we're looking for
 *
 * N.B. RST_NONE must be the last item in the ... list
 */
void create_mon_spell_mask(bitflag *f, ...)
{
	const struct mon_spell_info *rs;
	int i;
	va_list args;

	rsf_wipe(f);

	va_start(args, f);

	/* Process each type in the va_args */
    for (i = va_arg(args, int); i != RST_NONE; i = va_arg(args, int)) {
		for (rs = mon_spell_types; rs->index < RSF_MAX; rs++) {
			if (rs->type & i) {
				rsf_on(f, rs->index);
			}
		}
	}

	va_end(args);

	return;
}

const char *mon_spell_lore_description(int index,
									   const struct monster_race *race)
{
	if (mon_spell_is_valid(index)) {
		const struct monster_spell *spell = monster_spell_by_index(index);

		/* Get the right level of description */
		struct monster_spell_level *level = spell->level;
		while (level->next && race->spell_power >= level->next->power) {
			level = level->next;
		}
		return level->lore_desc;
	} else {
		return "";
	}
}

random_value mon_spell_lore_damage(int index)
{
	random_value val = { 0, 0, 0, 0 };
	if (mon_spell_is_valid(index) &&
		(mon_spell_is_innate(index) || mon_spell_is_breath(index))) {
		const struct monster_spell *spell = monster_spell_by_index(index);
		
		if (spell->effect->dice != NULL) {
			(void) dice_roll(spell->effect->dice, &val);
		}
	}
	return val;
}
