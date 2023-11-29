/**
 * \file score.c
 * \brief Highscore handling for Angband
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
#include "buildid.h"
#include "game-world.h"
#include "init.h"
#include "player-quest.h"
#include "score.h"


/**
 * An integer value representing the player's "points".
 *
 * In reality it isn't so much a score as a number that has the same ordering
 * as the scores.
 *
 * It ranges from 100,000 to 141,399,999
 */
static int score_points(const struct high_score *score)
{
	int points = 0;
	int silmarils;
	
	int maxturns = 100000;
	int silmarils_factor = maxturns;
	int depth_factor = silmarils_factor * 10;
	int morgoth_factor = depth_factor * 100;

	/* Points from turns taken (00000 to 99999)  */
	points = maxturns - atoi(score->turns);
	if (points < 0) {
		points = 0;
	}
	if (points >= maxturns)	{
		points = maxturns - 1;
	}

	/* Points from silmarils (0 00000 to 3 00000) */
	silmarils = atoi(score->silmarils);
	points += silmarils_factor * silmarils;

	/* Points from depth (01 0 00000 to 40 0 00000) */
	if (silmarils == 0) {
		points += depth_factor * atoi(score->cur_dun);
	} else {
		points += depth_factor * (40 - atoi(score->cur_dun));
	}

	/* Points for escaping (changes 40 0 00000 to 41 0 00000) */
	if (score->escaped[0] == 't') {
		points += depth_factor;
	}

	/* points slaying Morgoth  (0 00 0 00000 to 1 00 0 00000) */
	if (score->morgoth_slain[0] == 't') {
		points += morgoth_factor;
	}

	return points;
}

/**
 * Read in a highscore file.
 */
size_t highscore_read(struct high_score scores[], size_t sz)
{
	char fname[1024];
	ang_file *scorefile;
	size_t i;

	/* Wipe current scores */
	memset(scores, 0, sz * sizeof(struct high_score));

	path_build(fname, sizeof(fname), ANGBAND_DIR_SCORES, "scores.raw");
	safe_setuid_grab();
	scorefile = file_open(fname, MODE_READ, FTYPE_TEXT);
	safe_setuid_drop();

	if (!scorefile) return 0;

	for (i = 0; i < sz; i++)
		if (file_read(scorefile, (char *)&scores[i],
					  sizeof(struct high_score)) <= 0)
			break;

	file_close(scorefile);

	return i;
}


/**
 * Just determine where a new score *would* be placed
 * Return the location (0 is best) or -1 on failure
 */
size_t highscore_where(const struct high_score *entry,
					   const struct high_score scores[], size_t sz)
{
	size_t i;

	/* Read until we get to a higher score */
	for (i = 0; i < sz; i++) {
		int entry_pts = score_points(entry);
		int score_pts = score_points(&scores[i]);

		if (entry_pts >= score_pts)
			return i;

		if (scores[i].what[0] == '\0')
			return i;
	}

	/* The last entry is always usable */
	return sz - 1;
}

/**
 * Place an entry into a high score array
 */
size_t highscore_add(const struct high_score *entry, struct high_score scores[],
					 size_t sz)
{
	size_t slot = highscore_where(entry, scores, sz);

	memmove(&scores[slot + 1], &scores[slot],
			sizeof(struct high_score) * (sz - 1 - slot));
	memcpy(&scores[slot], entry, sizeof(struct high_score));

	return slot;
}

static size_t highscore_count(const struct high_score scores[], size_t sz)
{
	size_t i;
	for (i = 0; i < sz; i++)
		if (scores[i].what[0] == '\0')
			break;

	return i;
}


/**
 * Actually place an entry into the high score file
 */
static void highscore_write(const struct high_score scores[], size_t sz)
{
	size_t n;

	ang_file *lok;
	ang_file *scorefile;

	char old_name[1024];
	char cur_name[1024];
	char new_name[1024];
	char lok_name[1024];
	bool exists;

	path_build(old_name, sizeof(old_name), ANGBAND_DIR_SCORES, "scores.old");
	path_build(cur_name, sizeof(cur_name), ANGBAND_DIR_SCORES, "scores.raw");
	path_build(new_name, sizeof(new_name), ANGBAND_DIR_SCORES, "scores.new");
	path_build(lok_name, sizeof(lok_name), ANGBAND_DIR_SCORES, "scores.lok");


	/* Read in and add new score */
	n = highscore_count(scores, sz);


	/* Lock scores */
	safe_setuid_grab();
	exists = file_exists(lok_name);
	safe_setuid_drop();
	if (exists) {
		msg("Lock file in place for scorefile; not writing.");
		return;
	}

	safe_setuid_grab();
	lok = file_open(lok_name, MODE_WRITE, FTYPE_RAW);
	if (!lok) {
		safe_setuid_drop();
		msg("Failed to create lock for scorefile; not writing.");
		return;
	} else {
		file_lock(lok);
		safe_setuid_drop();
	}

	/* Open the new file for writing */
	safe_setuid_grab();
	scorefile = file_open(new_name, MODE_WRITE, FTYPE_RAW);
	safe_setuid_drop();

	if (!scorefile) {
		msg("Failed to open new scorefile for writing.");

		file_close(lok);
		safe_setuid_grab();
		file_delete(lok_name);
		safe_setuid_drop();
		return;
	}

	file_write(scorefile, (const char *)scores, sizeof(struct high_score)*n);
	file_close(scorefile);

	/* Now move things around */
	safe_setuid_grab();

	if (file_exists(old_name) && !file_delete(old_name))
		msg("Couldn't delete old scorefile");

	if (file_exists(cur_name) && !file_move(cur_name, old_name))
		msg("Couldn't move old scores.raw out of the way");

	if (!file_move(new_name, cur_name))
		msg("Couldn't rename new scorefile to scores.raw");

	/* Remove the lock */
	file_close(lok);
	file_delete(lok_name);

	safe_setuid_drop();
}



/**
 * Fill in a score record for the given player.
 *
 * \param entry points to the record to fill in.
 * \param p is the player whose score should be recorded.
 * \param died_from is the reason for death.  In typical use, that will be
 * p->died_from, but when the player isn't dead yet, the caller may want to
 * use something else:  "nobody (yet!)" is traditional.
 * \param death_time points to the time at which the player died.  May be NULL
 * when the player isn't dead.
 *
 * Bug:  takes a player argument, but still accesses a bit of global state,
 * player_uid, referring to the player
 */
void build_score(struct high_score *entry, const struct player *p,
		const char *died_from, const time_t *death_time)
{
	memset(entry, 0, sizeof(struct high_score));

	/* Save the version */
	strnfmt(entry->what, sizeof(entry->what), "%s", buildid);

	/* Save the current turn */
	strnfmt(entry->turns, sizeof(entry->turns), "%9ld", (long)p->turn);

	/* Time of death */
	if (death_time)
		strftime(entry->day, sizeof(entry->day), "@%Y%m%d",
				 localtime(death_time));
	else
		my_strcpy(entry->day, "TODAY", sizeof(entry->day));

	/* Save the player name (15 chars) */
	strnfmt(entry->who, sizeof(entry->who), "%-.15s", p->full_name);

	/* Save the player info XXX XXX XXX */
	strnfmt(entry->uid, sizeof(entry->uid), "%7u", player_uid);
	strnfmt(entry->p_s, sizeof(entry->p_s), "%2d", p->sex->sidx);
	strnfmt(entry->p_r, sizeof(entry->p_r), "%2d", p->race->ridx);
	strnfmt(entry->p_h, sizeof(entry->p_h), "%2d", p->house->hidx);

	/* Save the level and such */
	strnfmt(entry->cur_dun, sizeof(entry->cur_dun), "%3d", p->depth);
	strnfmt(entry->max_dun, sizeof(entry->max_dun), "%3d", p->max_depth);

	/* No cause of death */
	my_strcpy(entry->how, died_from, sizeof(entry->how));

	/* Save the number of silmarils, whether morgoth is slain,
	 * whether the player has escaped */
	strnfmt(entry->silmarils, sizeof(entry->silmarils), "%1d",
			silmarils_possessed((struct player *)p));

	if (p->morgoth_slain) {
		strnfmt(entry->morgoth_slain, sizeof(entry->morgoth_slain), "t");
	} else {
		strnfmt(entry->morgoth_slain, sizeof(entry->morgoth_slain), "f");
	}
	if (p->escaped) {
		strnfmt(entry->escaped, sizeof(entry->escaped), "t");
	} else {
		strnfmt(entry->escaped, sizeof(entry->escaped), "f");
	}
}



/**
 * Enter a player's name on a hi-score table, if "legal".
 *
 * \param p is the player to enter
 * \param death_time points to the time at which the player died; may be NULL
 * for a player that's not dead yet
 * Assumes "signals_ignore_tstp()" has been called.
 */
void enter_score(const struct player *p, const time_t *death_time)
{
	int j;

	/* Cheaters are not scored */
	for (j = 0; j < OPT_MAX; ++j) {
		if (option_type(j) != OP_SCORE)
			continue;
		if (!p->opts.opt[j])
			continue;

		msg("Score not registered for cheaters.");
		event_signal(EVENT_MESSAGE_FLUSH);
		return;
	}

	/* Add a new entry, if allowed */
	if (p->noscore & (NOSCORE_WIZARD | NOSCORE_DEBUG)) {
		msg("Score not registered for wizards.");
		event_signal(EVENT_MESSAGE_FLUSH);
	} else if (streq(p->died_from, "Interrupting")) {
		msg("Score not registered due to interruption.");
		event_signal(EVENT_MESSAGE_FLUSH);
	} else if (streq(p->died_from, "Retiring")) {
		msg("Score not registered due to retiring.");
		event_signal(EVENT_MESSAGE_FLUSH);
	} else {
		struct high_score entry;
		struct high_score scores[MAX_HISCORES];

		build_score(&entry, p, p->died_from, death_time);

		highscore_read(scores, N_ELEMENTS(scores));
		highscore_add(&entry, scores, N_ELEMENTS(scores));
		highscore_write(scores, N_ELEMENTS(scores));
	}

	/* Success */
	return;
}


