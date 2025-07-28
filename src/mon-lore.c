/**
 * \file mon-lore.c
 * \brief Monster memory code.
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
#include "effects.h"
#include "game-world.h"
#include "init.h"
#include "mon-attack.h"
#include "mon-blows.h"
#include "mon-init.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "obj-gear.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "project.h"
#include "z-textblock.h"

/**
 * Monster genders
 */
enum monster_sex {
	MON_SEX_NEUTER = 0,
	MON_SEX_MALE,
	MON_SEX_FEMALE,
	MON_SEX_MAX,
};

typedef enum monster_sex monster_sex_t;

void lore_learn_flag_if_visible(struct monster_lore *lore, const struct monster *mon, int flag)
{
	if (monster_is_visible(mon)) {
		rf_on(lore->flags, flag);
	}
}

/**
 * Update which bits of lore are known
 */
void lore_update(const struct monster_race *race, struct monster_lore *lore)
{
	int i;
	bitflag mask[RF_SIZE];

	if (!race || !lore) return;

	/* Assume some "obvious" flags */
	create_mon_flag_mask(mask, RFT_OBV, RFT_ABIL_OBV, RFT_MAX);
	rf_union(lore->flags, mask);

	/* Blows */
	for (i = 0; i < z_info->mon_blows_max; i++) {
		if (!race->blow) break;
		if (lore->blow_known[i] || (lore->blows[i].times_seen) ||
			lore->all_known) {
			lore->blow_known[i] = true;
			lore->blows[i].method = race->blow[i].method;
			lore->blows[i].effect = race->blow[i].effect;
			lore->blows[i].dice = race->blow[i].dice;
		}
	}

	/* Killing a monster reveals some properties */
	if ((lore->tkills > 0) || lore->all_known) {
		lore->armour_known = true;
		lore->drop_known = true;
		create_mon_flag_mask(mask, RFT_RACE_A, RFT_RACE_N, RFT_DROP, RFT_MAX);
		rf_union(lore->flags, mask);
		rf_on(lore->flags, RF_FORCE_DEPTH);
	}

	/* Awareness */
	if ((lore->ranged == UCHAR_MAX) || lore->all_known ||
	    ((player && lore->tsights > 1) &&
		 (10 - lore->tsights < player->state.skill_use[SKILL_PERCEPTION])))
		lore->sleep_known = true;

	/* Spellcasting frequency */
	if (lore->ranged == UCHAR_MAX || lore->all_known) {
		lore->ranged_freq_known = true;
	}

	/* Flags for probing and cheating */
	if (lore->all_known) {
		rf_setall(lore->flags);
		rsf_copy(lore->spell_flags, race->spell_flags);
	}
}

/**
 * Learn everything about a monster.
 *
 * Sets the all_known variable, all flags and all relevant spell flags.
 */
void cheat_monster_lore(const struct monster_race *race, struct monster_lore *lore)
{
	assert(race);
	assert(lore);

	/* Full knowledge */
	lore->all_known = true;
	lore_update(race, lore);
}

/**
 * Forget everything about a monster.
 */
void wipe_monster_lore(const struct monster_race *race, struct monster_lore *lore)
{
	struct monster_blow *blows;
	bool *blow_known;
	struct monster_drop *d;

	assert(race);
	assert(lore);

	d = lore->drops;
	while (d) {
		struct monster_drop *dn = d->next;
		mem_free(d);
		d = dn;
	}
	/*
	 * Keep the blows and blow_known pointers - other code assumes they
	 * are not NULL.  Wipe the pointed to memory.
	 */
	blows = lore->blows;
	memset(blows, 0, z_info->mon_blows_max * sizeof(*blows));
	blow_known = lore->blow_known;
	memset(blow_known, 0, z_info->mon_blows_max * sizeof(*blow_known));
	memset(lore, 0, sizeof(*lore));
	lore->blows = blows;
	lore->blow_known = blow_known;
}

/**
 * Learn about a monster (by "probing" it)
 */
void lore_do_probe(struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);
	
	lore->all_known = true;
	lore_update(mon->race, lore);

	/* Update monster recall window */
	if (player->upkeep->monster_race == mon->race)
		player->upkeep->redraw |= (PR_MONSTER);
}

/**
 * Determine whether the monster is fully known
 */
bool lore_is_fully_known(const struct monster_race *race)
{
	unsigned i;
	struct monster_lore *lore = get_lore(race);

	/* Check if already known */
	if (lore->all_known)
		return true;

	if (!lore->armour_known)
		return false;
	/* Only check spells if the monster can cast them */
	if (!lore->ranged_freq_known && race->freq_ranged)
		return false;
	if (!lore->drop_known)
		return false;
	if (!lore->sleep_known)
		return false;
		
	/* Check if blows are known */
	for (i = 0; i < z_info->mon_blows_max; i++){
		/* Only check if the blow exists */
		if (!race->blow[i].method)
			break;
		if (!lore->blow_known[i])
			return false;
		
	}
		
	/* Check all the flags */
	for (i = 0; i < RF_SIZE; i++)
		if (!lore->flags[i])
			return false;
		
		
	/* Check spell flags */
	for (i = 0; i < RSF_SIZE; i++)
		if (lore->spell_flags[i] != race->spell_flags[i])			
			return false;
	
	/* The player knows everything */
	lore->all_known = true;
	lore_update(race, lore);
	return true;
}
	
	
/**
 * Take note that the given monster just dropped some treasure
 *
 * Note that learning the "GOOD"/"GREAT" flags gives information
 * about the treasure (even when the monster is killed for the first
 * time, such as uniques, and the treasure has not been examined yet).
 *
 * This "indirect" method was used to prevent the player from learning
 * exactly how much treasure a monster can drop from observing only
 * a single example of a drop.  This method actually observes how many
 * items are dropped, and remembers that information to be described later
 * by the monster recall code.
 */
void lore_treasure(struct monster *mon, int num_item)
{
	struct monster_lore *lore = get_lore(mon->race);

	assert(num_item >= 0);

	/* Note the number of things dropped */
	if (num_item > lore->drop_item) {
		lore->drop_item = num_item;
	}

	/* Learn about drop quality */
	rf_on(lore->flags, RF_DROP_GOOD);
	rf_on(lore->flags, RF_DROP_GREAT);

	/* Update monster recall window */
	if (player->upkeep->monster_race == mon->race) {
		player->upkeep->redraw |= (PR_MONSTER);
	}
}

/**
 * Copies into `flags` the flags of the given monster race that are known
 * to the given lore structure (usually the player's knowledge).
 *
 * Known flags will be 1 for present, or 0 for not present. Unknown flags
 * will always be 0.
 */
void monster_flags_known(const struct monster_race *race,
						 const struct monster_lore *lore,
						 bitflag flags[RF_SIZE])
{
	rf_copy(flags, race->flags);
	rf_inter(flags, lore->flags);
}

/**
 * Return a description for the given monster race awareness value.
 *
 * Descriptions are in a table within the function. Returns a sensible string
 * for values not in the table.
 *
 * \param awareness is the inactivity counter of the race (monster_race.sleep).
 */
static const char *lore_describe_awareness(int16_t awareness)
{
	/* Value table ordered descending, for priority. Terminator is
	 * {SHRT_MAX, NULL}. */
	static const struct lore_awareness {
		int16_t threshold;
		const char *description;
	} lore_awareness_description[] = {
		{20,	"is usually found asleep"},
		{15,	"is often found asleep"},
		{10,	"is sometimes found asleep"},
		{5,		"is never found asleep"},
		{1,		"is quick to notice intruders"},
		{0,		"is very quick to notice intruders"},
		{SHRT_MAX,	NULL},
	};
	const struct lore_awareness *current = lore_awareness_description;

	while (current->threshold != SHRT_MAX && current->description != NULL) {
		if (awareness > current->threshold)
			return current->description;

		current++;
	}

	/* Values zero and less are the most vigilant */
	return "is ever vigilant";
}

/**
 * Return a description for the given monster race speed value.
 *
 * Descriptions are in a table within the function. Returns a sensible string
 * for values not in the table.
 *
 * \param speed is the speed rating of the race (monster_race.speed).
 */
static const char *lore_describe_speed(uint8_t speed)
{
	/* Value table ordered descending, for priority. Terminator is
	 * {UCHAR_MAX, NULL}. */
	static const struct lore_speed {
		uint8_t threshold;
		const char *description;
	} lore_speed_description[] = {
		{5,	"incredibly quickly"},
		{4,	"extremely quickly"},
		{3,	"very quickly"},
		{2,	"quickly"},
		{1,	"normal speed"}, /* 1 is normal speed */
		{0,	"slowly"},
		{UCHAR_MAX,	NULL},
	};
	const struct lore_speed *current = lore_speed_description;

	while (current->threshold != UCHAR_MAX && current->description != NULL) {
		if (speed > current->threshold)
			return current->description;

		current++;
	}

	/* Return a weird description, since the value wasn't found in the table */
	return "erroneously";
}

/**
 * Append the monster speed, in words, to a textblock.
 *
 * \param tb is the textblock we are adding to.
 * \param race is the monster race we are describing.
 */
static void lore_adjective_speed(textblock *tb, const struct monster_race *race)
{
	/* "at" is separate from the normal speed description in order to use the
	 * normal text colour */
	if (race->speed == 2)
		textblock_append(tb, "at ");

	textblock_append_c(tb, COLOUR_GREEN, "%s", lore_describe_speed(race->speed));
}

/**
 * Return a value describing the sex of the provided monster race.
 */
static monster_sex_t lore_monster_sex(const struct monster_race *race)
{
	if (rf_has(race->flags, RF_FEMALE))
		return MON_SEX_FEMALE;
	else if (rf_has(race->flags, RF_MALE))
		return MON_SEX_MALE;

	return MON_SEX_NEUTER;
}

/**
 * Return a pronoun for a monster; used as the subject of a sentence.
 *
 * Descriptions are in a table within the function. Table must match
 * monster_sex_t values.
 *
 * \param sex is the gender value (as provided by `lore_monster_sex()`.
 * \param title_case indicates whether the initial letter should be
 * capitalized; `true` is capitalized, `false` is not.
 */
static const char *lore_pronoun_nominative(monster_sex_t sex, bool title_case)
{
	static const char *lore_pronouns[MON_SEX_MAX][2] = {
		{"it", "It"},
		{"he", "He"},
		{"she", "She"},
	};

	int pronoun_index = MON_SEX_NEUTER, case_index = 0;

	if (sex < MON_SEX_MAX)
		pronoun_index = sex;

	if (title_case)
		case_index = 1;

	return lore_pronouns[pronoun_index][case_index];
}

/**
 * Return a pronoun for a monster; used as the object of a sentence.
 *
 * Descriptions are in a table within the function. Table must match
 * monster_sex_t values.
 *
 * \param sex is the gender value (as provided by `lore_monster_sex()`.
 * \param title_case indicates whether the initial letter should be
 * capitalized; `true` is capitalized, `false` is not.
 */
static const char *lore_pronoun_accusative(monster_sex_t sex, bool title_case)
{
	static const char *lore_pronouns[MON_SEX_MAX][2] = {
		{"it", "It"},
		{"him", "Him"},
		{"her", "Her"},
	};

	int pronoun_index = MON_SEX_NEUTER, case_index = 0;

	if (sex < MON_SEX_MAX)
		pronoun_index = sex;

	if (title_case)
		case_index = 1;

	return lore_pronouns[pronoun_index][case_index];
}

/**
 * Return a possessive pronoun for a monster.
 *
 * Descriptions are in a table within the function. Table must match
 * monster_sex_t values.
 *
 * \param sex is the gender value (as provided by `lore_monster_sex()`.
 * \param title_case indicates whether the initial letter should be
 * capitalized; `true` is capitalized, `false` is not.
 */
static const char *lore_pronoun_possessive(monster_sex_t sex, bool title_case)
{
	static const char *lore_pronouns[MON_SEX_MAX][2] = {
		{"its", "Its"},
		{"his", "His"},
		{"her", "Her"},
	};

	int pronoun_index = MON_SEX_NEUTER, case_index = 0;

	if (sex < MON_SEX_MAX)
		pronoun_index = sex;

	if (title_case)
		case_index = 1;

	return lore_pronouns[pronoun_index][case_index];
}

/**
 * Append a clause containing a list of descriptions of monster flags from
 * list-mon-race-flags.h to a textblock.
 *
 * The text that joins the list is drawn using the default attributes. The list
 * uses a serial comma ("a, b, c, and d").
 *
 * \param tb is the textblock we are adding to.
 * \param f is the set of flags to be described.
 * \param attr is the attribute each list item will be drawn with.
 * \param start is a string to start the clause.
 * \param conjunction is a string that is added before the last item.
 * \param end is a string that is added after the last item.
 */
static void lore_append_clause(textblock *tb, bitflag *f, int attr,
							   const char *start, const char *conjunction,
							   const char *end)
{
	int count = rf_count(f);
	bool comma = count > 2;

	if (count) {
		int flag;
		textblock_append(tb, "%s", start);
		for (flag = rf_next(f, FLAG_START); flag; flag = rf_next(f, flag + 1)) {
			/* First entry starts immediately */
			if (flag != rf_next(f, FLAG_START)) {
				if (comma) {
					textblock_append(tb, ",");
				}
				/* Last entry */
				if (rf_next(f, flag + 1) == FLAG_END) {
					textblock_append(tb, " ");
					textblock_append(tb, "%s", conjunction);
				}
				textblock_append(tb, " ");
			}
			textblock_append_c(tb, attr, "%s", describe_race_flag(flag));
		}
		textblock_append(tb, "%s", end);
	}
}


/**
 * Append a list of spell descriptions.
 *
 * This is a modified version of `lore_append_clause()` to format spells.
 *
 * \param tb is the textblock we are adding to.
 * \param f is the set of flags to be described.
 * \param race is the monster race.
 * \param attr is the color index to use for the text from monster_spell.txt's
 * lore directive for each spell
 * \param dam_attr is the color index to use for the text describing each
 * spell's damage
 */
static void lore_append_spell_clause(textblock *tb, bitflag *f,
									 const struct monster_race *race,
									 int attr, int dam_attr)
{
	int count = rsf_count(f);
	bool comma = count > 2;

	if (count) {
		int spell;
		for (spell = rsf_next(f, FLAG_START); spell;
			 spell = rsf_next(f, spell + 1)) {
			random_value damage = mon_spell_lore_damage(spell);
            int archery_bonus = 0;
            archery_bonus = mon_spell_lore_archery_bonus(spell, race);

			/* First entry starts immediately */
			if (spell != rsf_next(f, FLAG_START)) {
				if (comma) {
					textblock_append(tb, ",");
				}
				/* Last entry */
				if (rsf_next(f, spell + 1) == FLAG_END) {
					textblock_append(tb, " or");
				}
				textblock_append(tb, " ");
			}
			textblock_append_c(tb, attr, "%s",
							   mon_spell_lore_description(spell, race));

            /* If it's not an archery spell, archery_bonus is 0 */
            if (damage.dice && damage.sides && archery_bonus) {
                textblock_append_c(tb, dam_attr, " (+%d, %dd%d)",
								   archery_bonus, damage.dice, damage.sides);
            } else if (damage.dice && damage.sides) {
				textblock_append_c(tb, dam_attr, " (%dd%d)", damage.dice,
								   damage.sides);
			}
		}
		textblock_append(tb, ".  ");
	}
}

/**
 * Append the kill history to a texblock for a given monster race.
 *
 * Known race flags are passed in for simplicity/efficiency.
 *
 * \param tb is the textblock we are adding to.
 * \param race is the monster race we are describing.
 * \param lore is the known information about the monster race.
 * \param known_flags is the preprocessed bitfield of race flags known to the
 *        player.
 */
void lore_append_kills(textblock *tb, const struct monster_race *race,
					   const struct monster_lore *lore,
					   const bitflag known_flags[RF_SIZE])
{
	monster_sex_t msex = MON_SEX_NEUTER;
	bool out = true;

	assert(tb && race && lore);

	/* Extract a gender (if applicable) */
	msex = lore_monster_sex(race);

	/* Treat by whether unique, then by whether they have any player kills */
	if (rf_has(known_flags, RF_UNIQUE)) {
		/* Hack -- Determine if the unique is "dead" */
		bool dead = (race->max_num == 0) ? true : false;

		/* We've been killed... */
		if (lore->deaths) {
			/* Killed ancestors */
			textblock_append(tb, "%s has slain %d of your ancestors",
							 lore_pronoun_nominative(msex, true), lore->deaths);

			/* But we've also killed it */
			if (dead)
				textblock_append(tb, ", but you have taken revenge!  ");

			/* Unavenged (ever) */
			else
				textblock_append(tb, ", who %s unavenged.  ",
								 VERB_AGREEMENT(lore->deaths, "remains",
												"remain"));
		} else if (dead) { /* Dead unique who never hurt us */
			textblock_append(tb, "You have slain this foe.  ");
		} else {
			/* Alive and never killed us */
			out = false;
		}
		if (!dead) {
			if (lore->psights) {
				textblock_append(tb, "You have encountered this foe.  ");
			} else {
				textblock_append(tb, "You are yet to encounter this foe.  ");
			}
		}
	} else if (lore->deaths) {
		/* Dead ancestors */
		textblock_append(tb, "%d of your predecessors %s been killed by this creature, ", lore->deaths, VERB_AGREEMENT(lore->deaths, "has", "have"));

		if (lore->pkills) {
			/* Some kills this life */
			textblock_append(tb, "and you have slain %d of the %d you have encountered.  ", lore->pkills, lore->psights);
		} else if (lore->tkills) {
			/* Some kills past lives */
			textblock_append(tb, "and your predecessors have slain %d in return.  ", lore->tkills);
		} else {
			/* No kills */
			textblock_append_c(tb, COLOUR_RED, "and %s is not ever known to have been defeated.  ", lore_pronoun_nominative(msex, false));
			if (lore->psights) {
				textblock_append(tb, "You have encountered %d.  ",
								 lore->psights);
			} else {
				textblock_append(tb, "You are yet to encounter one.  ");
			}
		}
	} else {
		/* Encountered some this life */
		if (lore->psights && !lore->pkills) {
			textblock_append(tb, "You have encountered %d of these creatures, ",
							 lore->psights);

			/* Killed some la.  st life */
			if (lore->tkills) {
				textblock_append(tb, "and your predecessors have slain %d.  ",
								 lore->tkills);
			} else {
				/* Killed none */
				textblock_append(tb,
								 "but no battles to the death are recalled.  ");
			}
		} else if (lore->pkills) {
			/* Killed some this life */
			textblock_append(tb, "You have slain %d of the %d you have encountered.  ", lore->pkills, lore->psights);
		} else {
			textblock_append(tb,
							 "You have encountered none of these creatures, ");
			if (lore->tkills) {
				/* Killed some last life */
				textblock_append(tb, "but your predecessors have slain %d.  ",
								 lore->tkills);
			} else {
				/* Killed none */
				textblock_append(tb,
								 "and no battles to the death are recalled.  ");
			}
		}
	}

	/* Separate */
	if (out)
		textblock_append(tb, "\n");
}

/**
 * Append the monster race description to a textblock.
 *
 * \param tb is the textblock we are adding to.
 * \param race is the monster race we are describing.
 */
void lore_append_flavor(textblock *tb, const struct monster_race *race)
{
	assert(tb && race);

	textblock_append(tb, "%s\n", race->text);
}

/**
 * Append the monster type, location, and movement patterns to a textblock.
 *
 * Known race flags are passed in for simplicity/efficiency.
 *
 * \param tb is the textblock we are adding to.
 * \param race is the monster race we are describing.
 * \param lore is the known information about the monster race.
 * \param known_flags is the preprocessed bitfield of race flags known to the
 *        player.
 */
void lore_append_movement(textblock *tb, const struct monster_race *race,
						  const struct monster_lore *lore,
						  bitflag known_flags[RF_SIZE])
{
	int f;
	bitflag flags[RF_SIZE];

	assert(tb && race && lore);

	textblock_append(tb, "This");

	/* Get adjectives */
	create_mon_flag_mask(flags, RFT_RACE_A, RFT_MAX);
	rf_inter(flags, race->flags);
	for (f = rf_next(flags, FLAG_START); f; f = rf_next(flags, f + 1)) {
		textblock_append_c(tb, COLOUR_L_BLUE, " %s", describe_race_flag(f));
	}

	/* Get noun */
	create_mon_flag_mask(flags, RFT_RACE_N, RFT_MAX);
	rf_inter(flags, race->flags);
	f = rf_next(flags, FLAG_START);
	if (f) {
		textblock_append_c(tb, COLOUR_L_BLUE, " %s", describe_race_flag(f));
	} else {
		textblock_append_c(tb, COLOUR_L_BLUE, " creature");
	}

	/* Describe location */
	if (race->level == 0) {
		textblock_append_c(tb, COLOUR_YELLOW,
						   " dwells at the gates of Angband");
	} else {
		if (rf_has(known_flags, RF_FORCE_DEPTH))
			textblock_append(tb, " is found ");
		else
			textblock_append(tb, " is normally found ");

		if (race == lookup_monster("Carcharoth")) {
			textblock_append_c(tb, COLOUR_YELLOW,
							   "guarding the gates of Angband");
		} else if (race->level < z_info->dun_depth) {
			textblock_append(tb, "at depths of ");
			textblock_append_c(tb, COLOUR_YELLOW, "%d", race->level * 50);
			textblock_append(tb, " feet");
		} else {
			textblock_append(tb, "at depths of ");
			textblock_append_c(tb, COLOUR_YELLOW, "%d", z_info->dun_depth * 50);
			textblock_append(tb, " feet");
		}
	}

	textblock_append(tb, ", and");

	if (rf_has(known_flags, RF_NEVER_MOVE)) {
		textblock_append(tb, " cannot move");
	} else if (rf_has(known_flags, RF_HIDDEN_MOVE)) {
		textblock_append(tb, " never moves when you are looking");
	} else {
		textblock_append(tb, " moves");
	}

	/* Random-ness */
	if (flags_test(known_flags, RF_SIZE, RF_RAND_50, RF_RAND_25, FLAG_END)){
		/* Adverb */
		if (rf_has(known_flags, RF_RAND_50) &&
			rf_has(known_flags, RF_RAND_25))
			textblock_append(tb, " extremely");
		else if (rf_has(known_flags, RF_RAND_50))
			textblock_append(tb, " somewhat");
		else if (rf_has(known_flags, RF_RAND_25))
			textblock_append(tb, " a bit");

		/* Adjective */
		textblock_append(tb, " erratically");

		/* Hack -- Occasional conjunction */
		if (race->speed != 2) textblock_append(tb, ", and");
	}

	/* Speed */
	textblock_append(tb, " ");
	lore_adjective_speed(tb, race);

	/* End this sentence */
	textblock_append(tb, ".  ");

	/* Note if this monster does not pursue you */
	if (rf_has(known_flags, RF_TERRITORIAL)) {
		monster_sex_t msex = lore_monster_sex(race);
		const char *initial_pronoun = lore_pronoun_nominative(msex, true);
		textblock_append(tb, "%s does not deign to pursue you.  ",
						 initial_pronoun);
	}
}

/**
 * Append the monster AC, HP, and hit chance to a textblock.
 *
 * Known race flags are passed in for simplicity/efficiency.
 *
 * \param tb is the textblock we are adding to.
 * \param race is the monster race we are describing.
 * \param lore is the known information about the monster race.
 * \param known_flags is the preprocessed bitfield of race flags known to the
 *        player.
 */
void lore_append_toughness(textblock *tb, const struct monster_race *race,
						   const struct monster_lore *lore,
						   bitflag known_flags[RF_SIZE])
{
	monster_sex_t msex = MON_SEX_NEUTER;

	assert(tb && race && lore);

	/* Extract a gender (if applicable) */
	msex = lore_monster_sex(race);

	/* Describe monster "toughness" */
	if (lore->armour_known) {
		/* Hitpoints */
		textblock_append(tb, "%s has ", lore_pronoun_nominative(msex, true));

		if (rf_has(known_flags, RF_UNIQUE)) {
			textblock_append_c(tb, COLOUR_GREEN, "%d ",
							   race->hdice * (1 + race->hside) / 2);
		} else {
			textblock_append_c(tb, COLOUR_GREEN, "%dd%d ", race->hdice,
							   race->hside);
		}

		textblock_append(tb, "health");

		/* Armor */
		textblock_append(tb, ", and a defence of ");
		if ((race->pd > 0) && (race->ps > 0)) {
			textblock_append_c(tb, COLOUR_SLATE, "[%+d, %dd%d]", race->evn,
							   race->pd, race->ps);
		} else {
			textblock_append_c(tb, COLOUR_SLATE, "[%+d]", race->evn);
		}
		textblock_append(tb, ".  ");
	}
}

/**
 * Append the experience value description to a textblock.
 *
 * Known race flags are passed in for simplicity/efficiency.
 *
 * \param tb is the textblock we are adding to.
 * \param race is the monster race we are describing.
 * \param lore is the known information about the monster race.
 * \param known_flags is the preprocessed bitfield of race flags known to the
 *        player.
 */
void lore_append_exp(textblock *tb, const struct monster_race *race,
					 const struct monster_lore *lore,
					 bitflag known_flags[RF_SIZE])
{
	long exp;
	monster_sex_t msex = MON_SEX_NEUTER;

	/* Check legality and that this is a placeable monster */
	assert(tb && race && lore);
	if (!race->rarity) return;

	/* Must have a kill or sighting */
	if (!lore->tkills && !lore->tsights) return;

	/* Extract a gender (if applicable) */
	msex = lore_monster_sex(race);

	/* Introduction for Encounters */
	if (lore->psights) {
		if (rf_has(known_flags, RF_UNIQUE)) {
			textblock_append(tb, "Encountering %s was worth",
							 lore_pronoun_accusative(msex, false));
		} else {
			textblock_append(tb, "Encountering another would be worth");
		}
	} else {
		if (rf_has(known_flags, RF_UNIQUE)) {
			textblock_append(tb, "Encountering %s would be worth",
							 lore_pronoun_accusative(msex, false));
		} else {
			textblock_append(tb, "Encountering one would be worth");
		}
	}

	/* Calculate the integer exp part */
	exp = adjusted_mon_exp(race, false);

	/* Mention the experience */
	textblock_append(tb, " %ld experience.  ", (long) exp);

	/* Introduction for Kills */
	if (lore->pkills) {
		if (rf_has(known_flags, RF_UNIQUE)) {
			textblock_append(tb, "Killing %s was worth",
							 lore_pronoun_accusative(msex, false));
		} else {
			textblock_append(tb, "Killing another would be worth");
		}
	} else {
		if (rf_has(known_flags, RF_UNIQUE)) {
			textblock_append(tb, "Killing %s would be worth",
							 lore_pronoun_accusative(msex, false));
		} else {
			textblock_append(tb, "Killing one would be worth");
		}
	}

	/* Calculate the integer exp part */
	exp = adjusted_mon_exp(race, true);

	/* Mention the experience */
	textblock_append(tb, " %ld.  ", (long) exp);
}

/**
 * Append the monster drop description to a textblock.
 *
 * Known race flags are passed in for simplicity/efficiency.
 *
 * \param tb is the textblock we are adding to.
 * \param race is the monster race we are describing.
 * \param lore is the known information about the monster race.
 * \param known_flags is the preprocessed bitfield of race flags known to the
 *        player.
 */
void lore_append_drop(textblock *tb, const struct monster_race *race,
					  const struct monster_lore *lore,
					  bitflag known_flags[RF_SIZE])
{
	int n = 0;
	monster_sex_t msex = MON_SEX_NEUTER;

	assert(tb && race && lore);
	if (!lore->drop_known) return;

	/* Extract a gender (if applicable) */
	msex = lore_monster_sex(race);

	/* Count maximum drop */
	n = mon_create_drop_count(race, true);

	/* Drops gold and/or items */
	if (n > 0) {
		if (rf_has(race->flags, RF_TERRITORIAL)) {
			textblock_append(tb, "%s may be found with",
							 lore_pronoun_nominative(msex, true));
		} else {
			textblock_append(tb, "%s may carry",
							 lore_pronoun_nominative(msex, true));
		}

		/* Report general drops */
		if (n == 1) {
			if (rf_has(known_flags, RF_DROP_GOOD) &&
				!rf_has(known_flags, RF_DROP_GREAT)) {
				textblock_append(tb, " a ");
			} else {
				textblock_append(tb, " an ");
			}
		} else if (n == 2) {
			textblock_append(tb, " one or two ");
		} else {
			textblock_append(tb, " up to %d ", n);
		}

		/* Quality */
		if (rf_has(known_flags, RF_DROP_GREAT)) {
			textblock_append_c(tb, COLOUR_BLUE,
							   "exceptional ");
		} else if (rf_has(known_flags, RF_DROP_GOOD)) {
			textblock_append_c(tb, COLOUR_BLUE, "good ");
		}

		/* Objects */
		textblock_append(tb, "object%s.  ", PLURAL(n));
	}
}

/**
 * Append the monster abilities (resists, weaknesses, other traits) to a
 * textblock.
 *
 * Known race flags are passed in for simplicity/efficiency. Note the macros
 * that are used to simplify the code.
 *
 * \param tb is the textblock we are adding to.
 * \param race is the monster race we are describing.
 * \param lore is the known information about the monster race.
 * \param known_flags is the preprocessed bitfield of race flags known to the
 *        player.
 */
void lore_append_abilities(textblock *tb, const struct monster_race *race,
						   const struct monster_lore *lore,
						   bitflag known_flags[RF_SIZE])
{
	int flag;
	char start[40];
	const char *initial_pronoun;
	bitflag current_flags[RF_SIZE];
	monster_sex_t msex = MON_SEX_NEUTER;

	assert(tb && race && lore);

	/* Extract a gender (if applicable) and get a pronoun for the start of
	 * sentences */
	msex = lore_monster_sex(race);
	initial_pronoun = lore_pronoun_nominative(msex, true);

	/* Describe abilities. */
	create_mon_flag_mask(current_flags, RFT_ABIL, RFT_ABIL_OBV, RFT_MAX);
	rf_inter(current_flags, known_flags);
	strnfmt(start, sizeof(start), "%s has the abilities: ",
		initial_pronoun);
	lore_append_clause(tb, current_flags, COLOUR_RED, start, "and", ".  ");

	/* Describe light */
	if (race->light > 0) {
		/* Humanoids carry torches, others glow */
		if (streq(race->base->name, "person") ||
			streq(race->base->name, "giant")) {
			textblock_append(tb, "%s can use a light source.  ",
							 initial_pronoun);
		} else {
			textblock_append(tb, "%s radiate light.  ", initial_pronoun);
		}
	} else if (race->light < 0) {
		textblock_append(tb, "%s can produce an unnatural darkness.  ",
						 initial_pronoun);
	}

	/* Describe movement abilities. */
	create_mon_flag_mask(current_flags, RFT_MOVE, RFT_MAX);
	rf_inter(current_flags, known_flags);
	strnfmt(start, sizeof(start), "%s can ", initial_pronoun);
	lore_append_clause(tb, current_flags, COLOUR_WHITE, start, "and", ".  ");

	/* Describe special things */
	create_mon_flag_mask(current_flags, RFT_NOTE, RFT_MAX);
	rf_inter(current_flags, known_flags);
	for (flag = rf_next(current_flags, FLAG_START); flag;
		 flag = rf_next(current_flags, flag + 1)) {
		textblock_append(tb, "%s %s.  ", initial_pronoun,
						 describe_race_flag(flag));
	}

	/* Describe detection traits */
	create_mon_flag_mask(current_flags, RFT_MIND, RFT_MAX);
	rf_inter(current_flags, known_flags);
	strnfmt(start, sizeof(start), "%s is ", initial_pronoun);
	lore_append_clause(tb, current_flags, COLOUR_WHITE, start, "and", ".  ");

	/* Describe susceptibilities */
	create_mon_flag_mask(current_flags, RFT_VULN, RFT_VULN_I, RFT_MAX);
	rf_inter(current_flags, known_flags);
	strnfmt(start, sizeof(start), "%s is vulnerable to ", initial_pronoun);
	lore_append_clause(tb, current_flags, COLOUR_L_BLUE, start, "and", ".  ");

	/* Describe resistances */
	create_mon_flag_mask(current_flags, RFT_RES, RFT_MAX);
	rf_inter(current_flags, known_flags);
	strnfmt(start, sizeof(start), "%s resists ", initial_pronoun);
	lore_append_clause(tb, current_flags, COLOUR_WHITE, start, "and", ".  ");

	/* Describe non-effects */
	create_mon_flag_mask(current_flags, RFT_PROT, RFT_MAX);
	rf_inter(current_flags, known_flags);
	strnfmt(start, sizeof(start), "%s cannot be ", initial_pronoun);
	lore_append_clause(tb, current_flags, COLOUR_YELLOW, start, "or", ".  ");

	/* Describe groups */
	create_mon_flag_mask(current_flags, RFT_GROUP, RFT_MAX);
	rf_inter(current_flags, known_flags);
	for (flag = rf_next(current_flags, FLAG_START); flag;
		 flag = rf_next(current_flags, flag + 1)) {
		textblock_append(tb, "%s %s.  ", initial_pronoun,
						 describe_race_flag(flag));
	}
}

/**
 * Append how the monster reacts to intruders and at what distance it does so.
 *
 * \param tb is the textblock we are adding to.
 * \param race is the monster race we are describing.
 * \param lore is the known information about the monster race.
 * \param known_flags is the preprocessed bitfield of race flags known to the
 *        player.
 */
void lore_append_skills(textblock *tb, const struct monster_race *race,
						   const struct monster_lore *lore,
						   bitflag known_flags[RF_SIZE])
{
	monster_sex_t msex = MON_SEX_NEUTER;

	assert(tb && race && lore);

	/* Extract a gender (if applicable) */
	msex = lore_monster_sex(race);

	/* Do we know how aware it is? */
	if (lore->sleep_known) {
		const char *aware = lore_describe_awareness(race->sleep);
		textblock_append(tb, "%s has %d Will,",
						 lore_pronoun_nominative(msex, true), race->wil);
		if (player_active_ability(player, "Listen")) {
			textblock_append(tb, " %d Stealth,", race->stl);
		}
		textblock_append(tb, " %d Perception", race->per);
		if (rf_has(race->flags, RF_MINDLESS)) {
			textblock_append(tb, ".  ");
		} else {
			textblock_append(tb, ", and %s.  ", aware);
		}
	}
}

/**
 * Append the monster's attack spells to a textblock.
 *
 * Known race flags are passed in for simplicity/efficiency. Note the macros
 * that are used to simplify the code.
 *
 * \param tb is the textblock we are adding to.
 * \param race is the monster race we are describing.
 * \param lore is the known information about the monster race.
 * \param known_flags is the preprocessed bitfield of race flags known to the
 *        player.
 */
void lore_append_spells(textblock *tb, const struct monster_race *race,
						const struct monster_lore *lore,
						bitflag known_flags[RF_SIZE])
{
	monster_sex_t msex = MON_SEX_NEUTER;
	const char *initial_pronoun;
	bitflag current_flags[RSF_SIZE];
	const struct monster_race *old_ref;

	assert(tb && race && lore);

	/* Set the race for expressions in the spells. */
	old_ref = ref_race;
	ref_race = race;

	/* Extract a gender (if applicable) and get a pronoun for the start of
	 * sentences */
	msex = lore_monster_sex(race);
	initial_pronoun = lore_pronoun_nominative(msex, true);

	/* Collect innate attacks */
	create_mon_spell_mask(current_flags, RST_INNATE, RST_NONE);
	rsf_inter(current_flags, lore->spell_flags);
	if (!rsf_is_empty(current_flags)) {
		textblock_append(tb, "%s may ", initial_pronoun);
		lore_append_spell_clause(tb, current_flags, race, COLOUR_L_RED,
								 COLOUR_UMBER);
	}

	/* Collect breaths */
	create_mon_spell_mask(current_flags, RST_BREATH, RST_NONE);
	rsf_inter(current_flags, lore->spell_flags);
	if (!rsf_is_empty(current_flags)) {
		textblock_append(tb, "%s may breathe ", initial_pronoun);
		lore_append_spell_clause(tb, current_flags, race, COLOUR_L_RED,
								 COLOUR_WHITE);
	}

	/* Collect spell information */
	create_mon_spell_mask(current_flags, RST_SPELL, RST_NONE);
	rsf_inter(current_flags, lore->spell_flags);
	if (!rsf_is_empty(current_flags)) {
		textblock_append(tb, "%s may attempt to ", initial_pronoun);
		lore_append_spell_clause(tb, current_flags, race, COLOUR_ORANGE,
								 COLOUR_WHITE);
	}

	/* Restore the previous reference. */
	ref_race = old_ref;
}

/**
 * Append the monster's melee attacks to a textblock.
 *
 * Known race flags are passed in for simplicity/efficiency.
 *
 * \param tb is the textblock we are adding to.
 * \param race is the monster race we are describing.
 * \param lore is the known information about the monster race.
 * \param known_flags is the preprocessed bitfield of race flags known to the
 *        player.
 */
void lore_append_attack(textblock *tb, const struct monster_race *race,
						const struct monster_lore *lore,
						bitflag known_flags[RF_SIZE])
{
	int i, known_attacks = 0, described_count = 0;
	monster_sex_t msex = MON_SEX_NEUTER;

	assert(tb && race && lore);

	/* Extract a gender (if applicable) */
	msex = lore_monster_sex(race);

	/* Count the number of defined and known attacks */
	for (i = 0; i < z_info->mon_blows_max; i++) {
		/* Skip non-attacks */
		if (!race->blow[i].method) continue;

		if (lore->blow_known[i])
			known_attacks++;
	}

	/* Describe the lack of knowledge */
	if (known_attacks == 0) {
		textblock_append(tb, "Nothing is known about %s attack.  ",
						 lore_pronoun_possessive(msex, false));
		return;
	}

	/* Describe each melee attack */
	for (i = 0; i < z_info->mon_blows_max; i++) {
		random_value dice;
		const char *effect_str = NULL;

		/* Skip unknown and undefined attacks */
		if (!race->blow[i].method || !lore->blow_known[i]) continue;

		/* Extract the attack info */
		dice = race->blow[i].dice;
		effect_str = race->blow[i].effect->desc;

		/* Introduce the attack description */
		if (described_count == 0)
			textblock_append(tb, "%s can ",
							 lore_pronoun_nominative(msex, true));
		else if (described_count < known_attacks - 1)
			textblock_append(tb, ", ");
		else
			textblock_append(tb, ", or ");

		/* Describe the method */
		textblock_append(tb, "%s", race->blow[i].method->desc);

		/* Describe the effect (if any) */
		if (effect_str && strlen(effect_str) > 0) {
			/* Describe the attack type */
			textblock_append(tb, " to ");
			textblock_append_c(tb, COLOUR_L_RED, "%s", effect_str);

			textblock_append(tb, " (");
			/* Describe damage */
			if (dice.base || (dice.dice && dice.sides)) {
				textblock_append_c(tb, COLOUR_L_WHITE, "%+d", dice.base);
				if (dice.dice && dice.sides) {
					textblock_append(tb, ", %dd%d", dice.dice, dice.sides);
				}
				textblock_append(tb, ")");
			}
		}

		described_count++;
	}
	assert(described_count == known_attacks);
	textblock_append(tb, ".  ");
}

/**
 * Get the lore record for this monster race.
 */
struct monster_lore *get_lore(const struct monster_race *race)
{
	assert(race);
	return &l_list[race->ridx];
}


/**
 * Write the monster lore
 */
static void write_lore_entries(ang_file *fff)
{
	int i, n;

	for (i = 0; i < z_info->r_max; i++) {
		/* Current entry */
		struct monster_race *race = &r_info[i];
		struct monster_lore *lore = &l_list[i];

		/* Ignore non-existent or unseen monsters */
		if (!race->name) continue;
		if (!lore->tsights && !lore->all_known) continue;

		/* Output 'name' */
		file_putf(fff, "name:%s\n", race->name);

		/* Output base if we're remembering everything */
		if (lore->all_known)
			file_putf(fff, "base:%s\n", race->base->name);

		/* Output counts */
		file_putf(fff, "counts:%d:%d:%d:%d:%d:%d\n", lore->tsights,
				  lore->deaths, lore->tkills, lore->notice, lore->ignore,
				  lore->ranged);

		/* Output blow (up to max blows) */
		for (n = 0; n < z_info->mon_blows_max; n++) {
			/* End of blows */
			if (!lore->blow_known[n] && !lore->all_known) continue;
			if (!lore->blows[n].method) continue;

			/* Output blow method */
			file_putf(fff, "blow:%s", lore->blows[n].method->name);

			/* Output blow effect (may be none) */
			file_putf(fff, ":%s", lore->blows[n].effect->name);

			/* Output blow damage (may be 0) */
			file_putf(fff, ":%d+%dd%dM%d", lore->blows[n].dice.base,
					lore->blows[n].dice.dice,
					lore->blows[n].dice.sides,
					lore->blows[n].dice.m_bonus);

			/* Output number of times that blow has been seen */
			file_putf(fff, ":%d", lore->blows[n].times_seen);

			/* Output blow index */
			file_putf(fff, ":%d", n);

			/* End line */
			file_putf(fff, "\n");
		}

		/* Output flags */
		write_flags(fff, "flags:", lore->flags, RF_SIZE, r_info_flags);

		/* Output spell flags (multiple lines) */
		rsf_inter(lore->spell_flags, race->spell_flags);
		write_flags(fff, "spells:", lore->spell_flags, RSF_SIZE,
					r_info_spell_flags);

		/* Output 'drop' */
		if (lore->drops) {
			struct monster_drop *drop = lore->drops;
			char name[120] = "";

			while (drop) {
				struct object_kind *kind = drop->kind;

				if (kind) {
					object_short_name(name, sizeof name, kind->name);
					file_putf(fff, "drop:%s:%s:%d:%d:%d\n",
							  tval_find_name(kind->tval), name,
							  drop->percent_chance, drop->dice.dice,
							  drop->dice.sides);
				} else {
					assert(drop->art);
					file_putf(fff, "drop-artifact:%s\n", drop->art->name);
				}
				drop = drop->next;
			}
		}

		file_putf(fff, "\n");
	}
}


/**
 * Save the lore to a file in the user directory.
 *
 * \param name is the filename
 *
 * \returns true on success, false otherwise.
 */
bool lore_save(const char *name)
{
	char path[1024];

	/* Write to the user directory */
	path_build(path, sizeof(path), ANGBAND_DIR_USER, name);

	if (text_lines_to_file(path, write_lore_entries)) {
		msg("Failed to create file %s.new", path);
		return false;
	}

	return true;
}
