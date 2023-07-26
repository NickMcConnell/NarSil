/**
 * \file ui-score.c
 * \brief Highscore display for Angband
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
#include "score.h"
#include "ui-input.h"
#include "ui-output.h"
#include "ui-score.h"
#include "ui-term.h"

/**
 * Prints a nice comma spaced natural number
 */
static void comma_number(char *output, int number, int len)
{
	if (number >= 1000000) {
		strnfmt(output, len, "%d,%03d,%03d", number / 1000000,
				(number % 1000000) / 1000, number % 1000);
	} else if (number >= 1000) {
		strnfmt(output, len, "%d,%03d", number / 1000, number % 1000);
	} else {
		strnfmt(output, len, "%d", number);
	}
}

/*
 * Converts a number into the three letter code of a month
 */
static void atomonth(int number, char *output, int len)
{
	switch (number)
	{
		case 1:
			strnfmt(output, len, "Jan");
			break;
		case 2:
			strnfmt(output, len, "Feb");
			break;
		case 3:
			strnfmt(output, len, "Mar");
			break;
		case 4:
			strnfmt(output, len, "Apr");
			break;
		case 5:
			strnfmt(output, len, "May");
			break;
		case 6:
			strnfmt(output, len, "Jun");
			break;
		case 7:
			strnfmt(output, len, "Jul");
			break;
		case 8:
			strnfmt(output, len, "Aug");
			break;
		case 9:
			strnfmt(output, len, "Sep");
			break;
		case 10:
			strnfmt(output, len, "Oct");
			break;
		case 11:
			strnfmt(output, len, "Nov");
			break;
		case 12:
			strnfmt(output, len, "Dec");
			break;
		default:
			break;
	}
}

/**
 * Display a page of scores
 */
void display_single_score(const struct high_score *score, int row, int place,
						  int attr)
{
	int aged, depth;
	const char *user, *when;
	struct player_house *h;
	struct player_sex *s;
	char out_val[160];
	char tmp_val[160];
	char aged_commas[15];
	char depth_commas[15];
	bool alive = streq(score->how, "(alive and well)");

	h = player_id2house(atoi(score->p_h));
	s = player_id2sex(atoi(score->p_s));

	/* Hack -- extract the gold and such */
	for (user = score->uid; isspace((unsigned char)*user); user++)
		/* loop */;
	for (when = score->day; isspace((unsigned char)*when); when++)
		/* loop */;

	aged = atoi(score->turns);
	depth = atoi(score->cur_dun) * 50;

	comma_number(aged_commas, aged, sizeof(aged_commas));
	comma_number(depth_commas, depth, sizeof(depth_commas));

	/* Clean up standard encoded form of "when" */
	if ((*when == '@') && strlen(when) == 9) {
		char month[4];

		strnfmt(month, sizeof(month), "%.2s", when + 5);
		atomonth(atoi(month), month, sizeof(month));

		if (*(when + 7) == '0') {
			strnfmt(tmp_val, sizeof(tmp_val), "%.1s %.3s %.4s",
				when + 8, month, when + 1);
		} else {
			strnfmt(tmp_val, sizeof(tmp_val), "%.2s %.3s %.4s",
				when + 7, month, when + 1);
		}	
		when = tmp_val;
	}

	/* If not displayed in a place, then don't write the place number */
	if (place == 0) {
		/* Prepare the first line, with the house only */
		strnfmt(out_val, sizeof(out_val),
				"     %5s ft  %s of %s",
				depth_commas, score->who, h->alt_name);
	} else {
		/* Prepare the first line, with the house only */
		strnfmt(out_val, sizeof(out_val),
				"%3d. %5s ft  %s of %s",
				place, depth_commas, score->who, h->alt_name);
	}

	/* Possibly amend the first line */
	if (score->morgoth_slain[0] == 't') {
		my_strcat(out_val,     ", who defeated Morgoth in his dark halls",
				  sizeof(out_val));
	} else {
		if (score->silmarils[0] == '1') {
			my_strcat(out_val, ", who freed a Silmaril", sizeof(out_val));
		}
		if (score->silmarils[0] == '2') {
			my_strcat(out_val, ", who freed two Silmarils",
					  sizeof(out_val));
		}
		if (score->silmarils[0] == '3') {
			my_strcat(out_val, ", who freed all three Silmarils",
					  sizeof(out_val));
		}
		if (score->silmarils[0] > '3') {
			my_strcat(out_val, ", who freed suspiciously many Silmarils",
					  sizeof(out_val));
		}
	}

	/* Dump the first line */
	c_put_str(attr, out_val, row + 3, 0);

	/* Prepare the second line for escapees */
	if (score->escaped[0] == 't') {
		strnfmt(out_val, sizeof(out_val),
				"               Escaped the iron hells");

		if ((score->morgoth_slain[0] == 't') ||
			(score->silmarils[0] > '0')) {
			my_strcat(out_val, " and brought back the light of Valinor",
					  sizeof(out_val));
		} else {
			my_strcat(out_val, format(" with %s task unfulfilled",
									  s->possessive), sizeof(out_val));
		}
	} else if (alive) {
		/* If character is still alive, display differently */
		strnfmt(out_val, sizeof(out_val),
				"               Lives still, deep within Angband's vaults");
	} else {
		/* Prepare the second line for those slain */
		strnfmt(out_val, sizeof(out_val),
				"               Slain by %s",
				score->how);

		/* Mark those with a silmaril */
		if (score->silmarils[0] > '0') {
			my_strcat(out_val, format(" during %s escape", s->possessive),
					  sizeof(out_val));
		}
	}

	/* Dump the info */
	c_put_str(attr, out_val, row + 4, 0);

	/* Don't print date for living characters */
	if (alive) {
		strnfmt(out_val, sizeof(out_val),
				"               after %s turns.",
				aged_commas);
		c_put_str(attr, out_val, row + 5, 0);
	} else {
		strnfmt(out_val, sizeof(out_val),
				"               after %s turns.  (%s)",
				aged_commas, when);
		c_put_str(attr, out_val, row + 5, 0);
	}

	/* Print symbols for silmarils / slaying Morgoth */
	if (score->escaped[0] == 't') {
		c_put_str(attr, "  escaped", row + 3, 4);
	}
	if (score->silmarils[0] == '1') {
		c_put_str(attr, "         *", row + 5, 0);
	}
	if (score->silmarils[0] == '2') {
		c_put_str(attr, "        * *", row + 5, 0);
	}
	if (score->silmarils[0] > '2') {
		c_put_str(attr, "       * * *", row + 5, 0);
	}
	if (score->morgoth_slain[0] == 't') {
		c_put_str(COLOUR_L_DARK, "         V", row + 4, 0);
	}
}

/**
 * Display a page of scores
 */
static void display_score_page(const struct high_score scores[], int start,
							   int count, int highlight)
{
	int n;

	/* Dump 5 entries */
	for (n = 0; start < count && n < 5; start++, n++) {
		const struct high_score *score = &scores[start];
		bool alive = streq(score->how, "(alive and well)");
		uint8_t attr = alive ? COLOUR_WHITE : COLOUR_SLATE;

		display_single_score(score, n * 4, start, attr);
	}
}

/**
 * Display the scores in a given range.
 */
static void display_scores_aux(const struct high_score scores[], int from,
							   int to, int highlight, bool allow_scrolling)
{
	struct keypress ch;
	int k, count;

	/* Assume we will show the first 10 */
	if (from < 0) from = 0;
	if (to < 0) to = allow_scrolling ? 5 : 10;
	if (to > MAX_HISCORES) to = MAX_HISCORES;

	/* Hack -- Count the high scores */
	for (count = 0; count < MAX_HISCORES; count++)
		if (!scores[count].what[0])
			break;

	/* Forget about the last entries */
	if ((count > to) && !allow_scrolling) count = to;

	/*
	 * Move 5 entries at a time.  Unless scrolling is allowed, only
	 * move forward and stop once the end is reached.
	 */
	k = from;
	while (1) {
		/* Clear screen */
		Term_clear();

		/* Title */
		if (k > 0) {
			put_str(format("Names of the Fallen (from position %d)",
				k + 1), 0, 21);
		} else {
			put_str("Names of the Fallen", 0, 30);
		}

		display_score_page(scores, k, count, highlight);

		/* Wait for response; prompt centered on 80 character line */
		if (allow_scrolling) {
			prt("[Press ESC to exit, up for prior page, any other key for next page.]", 23, 6);
		} else {
			prt("[Press ESC to exit, any other key to page forward till done.]", 23, 9);
		}
		ch = inkey();
		prt("", 23, 0);

		if (ch.code == ESCAPE) {
			break;
		} else if (ch.code == ARROW_UP && allow_scrolling) {
			if (k == 0) {
				k = count - 5;
				while (k % 5) k++;
			} else if (k < 5) {
				k = 0;
			} else {
				k = k - 5;
			}
		} else {
			k += 5;
			if (k >= count) {
				if (allow_scrolling) {
					k = 0;
				} else {
					break;
				}
			}
		}
	}

	return;
}

/**
 * Predict the players location, and display it.
 */
void predict_score(bool allow_scrolling)
{
	int j;
	struct high_score the_score;
	struct high_score scores[MAX_HISCORES];


	/* Read scores, place current score */
	highscore_read(scores, N_ELEMENTS(scores));
	build_score(&the_score, player, "nobody (yet!)", NULL);

	if (player->is_dead)
		j = highscore_where(&the_score, scores, N_ELEMENTS(scores));
	else
		j = highscore_add(&the_score, scores, N_ELEMENTS(scores));

	/* Top fifteen scores if on the top ten, otherwise ten surrounding */
	if (j < 10) {
		display_scores_aux(scores, 0, 15, j, allow_scrolling);
	} else {
		display_scores_aux(scores, j - 2, j + 7, j, allow_scrolling);
	}
}


/**
 * Show scores.
 */
void show_scores(void)
{
	screen_save();

	/* Display the scores */
	if (character_generated) {
		predict_score(true);
	} else {
		/* Currently unused, but leaving in in case we re-implement looking
		 * at the scores without loading a character */
		struct high_score scores[MAX_HISCORES];
		highscore_read(scores, N_ELEMENTS(scores));
		display_scores_aux(scores, 0, MAX_HISCORES, -1, true);
	}

	screen_load();

	/* Hack - Flush it */
	Term_fresh();
}

