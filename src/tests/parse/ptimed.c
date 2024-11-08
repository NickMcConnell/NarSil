/* parse/ptimed.c */
/* Exercise the parser used for player_timed.txt. */

#include "unit-test.h"
#include "init.h"
#include "message.h"
#include "object.h"
#include "parser.h"
#include "player-timed.h"
#include "z-color.h"
#include "z-rand.h"
#include "z-virt.h"
#include <ctype.h>

int setup_tests(void **state) {
	struct parser *p = (*player_timed_parser.init)();

	if (!p) {
		(void) teardown_tests(p);
		return 1;
	}
	Rand_init();
	*state = p;
	return 0;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;

	(void) (*player_timed_parser.finish)(p);
	(*player_timed_parser.cleanup)();
	return 0;
}

static void clear_changeinc(struct timed_effect_data *t) {
	struct timed_change *tc = t->increase;

	while (tc) {
		struct timed_change *tgt = tc;

		tc = tc->next;
		string_free(tgt->msg);
		string_free(tgt->inc_msg);
		mem_free(tgt);
	}
	t->increase = NULL;
}

static void clear_grades(struct timed_effect_data* t) {
	struct timed_grade *g = t->grade;

	while (g) {
		struct timed_grade *tgt = g;

		g = g->next;
		string_free(tgt->name);
		string_free(tgt->up_msg);
		string_free(tgt->down_msg);
		mem_free(tgt);
	}
	t->grade = NULL;
}

static void clear_change_grades(struct timed_effect_data *t) {
	struct timed_change_grade *cg = t->c_grade;

	while (cg) {
		struct timed_change_grade *tgt = cg;

		cg = cg->next;
		string_free(tgt->name);
		mem_free(tgt);
	}
	t->c_grade = NULL;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:FOOD");
	struct timed_effect_data *t;

	eq(r, PARSE_ERROR_NONE);
	t = (struct timed_effect_data*) parser_priv(p);
	notnull(t);
	notnull(t->name);
	require(streq(t->name, "FOOD"));
	require(t->index == (int) (t - timed_effects));
	null(t->desc);
	null(t->on_end);
	null(t->on_increase);
	null(t->on_decrease);
	eq(t->msgt, 0);
	eq(t->fail, -1);
	null(t->c_grade);
	null(t->increase);
	null(t->decrease.msg);
	null(t->decrease.inc_msg);
	require(!t->este);
	require(!t->save);
	ok;
}

static int test_badname0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:XYZZY");

	noteq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_desc0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "desc:nourishment");
	struct timed_effect_data *t;

	eq(r, PARSE_ERROR_NONE);
	t = (struct timed_effect_data*) parser_priv(p);
	notnull(t);
	notnull(t->desc);
	require(streq(t->desc, "nourishment"));
	r = parser_parse(p, "desc: (i.e. food)");
	eq(r, PARSE_ERROR_NONE);
	notnull(t->desc);
	require(streq(t->desc, "nourishment (i.e. food)"));
	ok;
}

static int test_endmsg0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p,
		"on-end:You no longer feel safe from evil!");
	struct timed_effect_data *t;

	eq(r, PARSE_ERROR_NONE);
	t = (struct timed_effect_data*) parser_priv(p);
	notnull(t);
	notnull(t->on_end);
	require(streq(t->on_end, "You no longer feel safe from evil!"));
	r = parser_parse(p, "on-end:  They'll be after you soon.");
	eq(r, PARSE_ERROR_NONE);
	notnull(t->on_end);
	require(streq(t->on_end, "You no longer feel safe from evil!  "
		"They'll be after you soon."));
	ok;
}

static int test_incmsg0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p,
		"on-increase:You feel even safer from evil!");
	struct timed_effect_data *t;

	eq(r, PARSE_ERROR_NONE);
	t = (struct timed_effect_data*) parser_priv(p);
	notnull(t);
	notnull(t->on_increase);
	require(streq(t->on_increase, "You feel even safer from evil!"));
	r = parser_parse(p,
		"on-increase:  And the shadows seem to lighten and shrink.");
	eq(r, PARSE_ERROR_NONE);
	notnull(t->on_increase);
	require(streq(t->on_increase, "You feel even safer from evil!  "
		"And the shadows seem to lighten and shrink."));
	ok;
}

static int test_decmsg0(void *state) {
	struct parser *p = (struct parser*)state;
	enum parser_error r = parser_parse(p,
		"on-decrease:You feel less safe from evil!");
	struct timed_effect_data *t;

	eq(r, PARSE_ERROR_NONE);
	t = (struct timed_effect_data*)parser_priv(p);
	notnull(t);
	notnull(t->on_decrease);
	require(streq(t->on_decrease, "You feel less safe from evil!"));
	r = parser_parse(p,
		"on-decrease:  And the shadows seem to lengthen and darken.");
	eq(r, PARSE_ERROR_NONE);
	notnull(t->on_decrease);
	require(streq(t->on_decrease, "You feel less safe from evil!  "
		"And the shadows seem to lengthen and darken."));
	ok;
}

static int test_changeinc0(void *state) {
	struct {
		const char *msg; const char *inc_msg; int max;
	} test_lvls[] = {
		{
			"You have been poisoned.",
			"You have been further poisoned.",
			10
		},
		{
			"You have been badly poisoned.",
			NULL,
			20
		},
		{
			"You have been severely poisoned.",
			NULL,
			100
		}
	};
	struct parser *p = (struct parser*)state;
	struct timed_effect_data *t;
	const struct timed_change *tc;
	char buffer[160];
	enum parser_error r;
	int i;

	t = (struct timed_effect_data*)parser_priv(p);
	notnull(t);
	clear_changeinc(t);
	for (i = 0; i < (int)N_ELEMENTS(test_lvls); ++i) {
		if (test_lvls[i].inc_msg) {
			strnfmt(buffer, sizeof(buffer),
				"change-inc:%d:%s:%s", test_lvls[i].max,
				test_lvls[i].msg, test_lvls[i].inc_msg);
		} else {
			strnfmt(buffer, sizeof(buffer),
				"change-inc:%d:%s", test_lvls[i].max,
				test_lvls[i].msg);
		}
		r = parser_parse(p, buffer);
		eq(r, PARSE_ERROR_NONE);
	}
	tc = t->increase;
	for (i = 0; i < (int)N_ELEMENTS(test_lvls); ++i, tc = tc->next) {
		notnull(tc);
		eq(tc->max, test_lvls[i].max);
		notnull(tc->msg);
		require(streq(tc->msg, test_lvls[i].msg));
		if (test_lvls[i].inc_msg) {
			notnull(tc->inc_msg);
			require(streq(tc->inc_msg, test_lvls[i].inc_msg));
		} else {
			null(tc->inc_msg);
		}
	}
	ok;
}

static int test_changedec0(void *state) {
	struct parser *p = (struct parser*)state;
	enum parser_error r = parser_parse(p,
		"change-dec:5:The bleeding slows.");
	struct timed_effect_data *t;

	eq(r, PARSE_ERROR_NONE);
	t = (struct timed_effect_data*)parser_priv(p);
	notnull(t);
	eq(t->decrease.max, 5);
	notnull(t->decrease.msg);
	require(streq(t->decrease.msg, "The bleeding slows."));
	ok;
}

static int test_msgt0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "msgt:HUNGRY");
	struct timed_effect_data *t;

	eq(r, PARSE_ERROR_NONE);
	t = (struct timed_effect_data*) parser_priv(p);
	notnull(t);
	eq(t->msgt, MSG_HUNGRY);
	ok;
}

static int test_badmsgt0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "msgt:XYZZY");

	eq(r, PARSE_ERROR_INVALID_MESSAGE);
	ok;
}

static int test_fail0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "fail:FREE_ACT");
	struct timed_effect_data *t;

	eq(r, PARSE_ERROR_NONE);
	t = (struct timed_effect_data*) parser_priv(p);
	notnull(t);
	eq(t->fail, OF_FREE_ACT);
	ok;
}

static int test_badfail0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "fail:XYZZY");

	eq(r, PARSE_ERROR_INVALID_FLAG);
	ok;
}

static int test_grade0(void *state) {
	struct parser *p = (struct parser*) state;
	struct {
		const char *name;
		const char *up_msg;
		const char *down_msg;
		int *p_py_food;
		int max;
		int color;
	} test_grades[] = {
		{
			"Starving",
			NULL,
			"You are beginning to starve!",
			&PY_FOOD_STARVE,
			1,
			COLOUR_L_RED
		},
		{
			"Weak",
			"You are still weak.",
			"You are getting weak from hunger!",
			&PY_FOOD_WEAK,
			1000,
			COLOUR_ORANGE
		},
		{
			"Hungry",
			"You are still hungry.",
			"You are getting hungry.",
			&PY_FOOD_ALERT,
			2000,
			COLOUR_YELLOW
		},
		{
			"Fed",
			"You are no longer hungry.",
			"You are no longer full.",
			&PY_FOOD_FULL,
			5000,
			COLOUR_UMBER
		},
		{
			"Full",
			"You are full!",
			"You are no longer gorged.",
			&PY_FOOD_MAX,
			8000,
			COLOUR_L_GREEN
		},
		{
			"Gorged",
			"You have gorged yourself!  You can't eat or drink any more until you recover.",
			NULL,
			NULL,
			20000,
			COLOUR_GREEN
		},
		{
			NULL,
			NULL,
			NULL,
			NULL,
			25000,
			COLOUR_PURPLE
		},
	};
	char buffer[256], color[32];
	enum parser_error r;
	struct timed_effect_data *t;
	const struct timed_grade *last_grade;
	int i;

	t = (struct timed_effect_data*) parser_priv(p);
	notnull(t);
	clear_grades(t);

	for (i = 0; i < (int) N_ELEMENTS(test_grades); ++i) {
		/*
		 * Use the one letter code for the color half of the time.
		 * Otherwise, use either the mixed case, all lower case, or
		 * all upper case version of the full name.
		 */
		if (one_in_(2)) {
			(void) strnfmt(color, sizeof(color), "%c",
				color_table[test_grades[i].color].index_char);
		} else {
			int j;

			(void) my_strcpy(color,
				color_table[test_grades[i].color].name,
				sizeof(color));
			if (one_in_(3)) {
				for (j = 0; color[j]; ++j) {
					if (isupper(color[j])) {
						color[j] = tolower(color[j]);
					}
				}
			} else if (one_in_(3)) {
				for (j = 0; color[j]; ++j) {
					if (islower(color[j])) {
						color[j] = toupper(color[j]);
					}
				}
			}
		}
		if (test_grades[i].down_msg) {
			(void) strnfmt(buffer, sizeof(buffer),
				"grade:%s:%d:%s:%s:%s",
				color,
				test_grades[i].max,
				(test_grades[i].name) ? test_grades[i].name : " ",
				(test_grades[i].up_msg) ? test_grades[i].up_msg : " ",
				test_grades[i].down_msg);
		} else if (one_in_(2)) {
			/*
			 * Test that a trailing colon with nothing after it
			 * works for the optional down message.
			 */
			(void) strnfmt(buffer, sizeof(buffer),
				"grade:%s:%d:%s:%s:",
				color,
				test_grades[i].max,
				(test_grades[i].name) ? test_grades[i].name : " ",
				(test_grades[i].up_msg) ? test_grades[i].up_msg : " ");
		} else {
			/*
			 * Test that omitting the down message entirely works.
			 */
			(void) strnfmt(buffer, sizeof(buffer),
				"grade:%s:%d:%s:%s",
				color,
				test_grades[i].max,
				(test_grades[i].name) ? test_grades[i].name : " ",
				(test_grades[i].up_msg) ? test_grades[i].up_msg : " ");
		}

		r = parser_parse(p, buffer);
		eq(r, PARSE_ERROR_NONE);
	}

	t = (struct timed_effect_data*) parser_priv(p);
	notnull(t);
	last_grade = t->grade;
	notnull(last_grade);
	/* Skip the zero grade at the start. */
	last_grade = last_grade->next;
	for (i = 0; i < (int) N_ELEMENTS(test_grades); ++i) {
		notnull(last_grade);
		eq(i + 1, last_grade->grade);
		eq(test_grades[i].color, last_grade->color);
		eq(test_grades[i].max, last_grade->max);
		if (test_grades[i].name) {
			require(streq(test_grades[i].name, last_grade->name));
		} else {
			null(last_grade->name);
		}
		if (test_grades[i].up_msg) {
			require(streq(test_grades[i].up_msg,
				last_grade->up_msg));
		} else {
			null(last_grade->up_msg);
		}
		if (test_grades[i].down_msg) {
			require(streq(test_grades[i].down_msg,
				last_grade->down_msg));
		} else {
			null(last_grade->down_msg);
		}
		last_grade = last_grade->next;
		if (test_grades[i].p_py_food) {
			eq(*test_grades[i].p_py_food, test_grades[i].max);
		}
	}

	ok;
}

static int test_changegrade0(void *state) {
	struct parser *p = (struct parser*)state;
	struct {
		const char *name;
		int max;
		int digits;
		int color;
	} test_cgs[] = {
		{ "Trickle", 9, 1, COLOUR_L_BLUE },
		{ "Stream", 99, 2, COLOUR_BLUE },
		{ "Flood", 999, 3, COLOUR_L_PURPLE },
		{ "Deluge", 1000, 0, COLOUR_PURPLE },
		{ NULL, 2000, 0, COLOUR_PURPLE }
	};
	char buffer[256], color[32];
	enum parser_error r;
	struct timed_effect_data *t;
	const struct timed_change_grade *last_cg;
	int i;

	t = (struct timed_effect_data*)parser_priv(p);
	notnull(t);
	clear_change_grades(t);

	for (i = 0; i < (int) N_ELEMENTS(test_cgs); ++i) {
		/*
		 * Use the one letter code for the color half of the time.
		 * Otherwise, use either the mixed case, all lower case, or
		 * all upper case version of the full name.
		 */
		if (one_in_(2)) {
			(void) strnfmt(color, sizeof(color), "%c",
				color_table[test_cgs[i].color].index_char);
		} else {
			int j;

			(void) my_strcpy(color,
				color_table[test_cgs[i].color].name,
				sizeof(color));
			if (one_in_(3)) {
				for (j = 0; color[j]; ++j) {
					if (isupper(color[j])) {
						color[j] = tolower(color[j]);
					}
				}
			} else if (one_in_(2)) {
				for (j = 0; color[j]; ++j) {
					if (islower(color[j])) {
						color[j] = toupper(color[j]);
					}
				}
			}
		}
		(void) strnfmt(buffer, sizeof(buffer),
			"change-grade:%s:%d:%d:%s",
				color,
				test_cgs[i].max,
				test_cgs[i].digits,
				(test_cgs[i].name) ? test_cgs[i].name : " ");

		r = parser_parse(p, buffer);
		eq(r, PARSE_ERROR_NONE);
	}

	t = (struct timed_effect_data*) parser_priv(p);
	notnull(t);
	last_cg = t->c_grade;
	notnull(last_cg);
	/* Check the zero grade at the start. */
	eq(last_cg->c_grade, 0);
	eq(last_cg->color, 0);
	eq(last_cg->max, 0);
	eq(last_cg->digits, 0);
	null(last_cg->name);
	last_cg = last_cg->next;
	for (i = 0; i < (int) N_ELEMENTS(test_cgs); ++i) {
		notnull(last_cg);
		eq(i + 1, last_cg->c_grade);
		eq(test_cgs[i].color, last_cg->color);
		eq(test_cgs[i].max, last_cg->max);
		eq(test_cgs[i].digits, last_cg->digits);
		if (test_cgs[i].name) {
			require(streq(test_cgs[i].name, last_cg->name));
		} else {
			null(last_cg->name);
		}
		last_cg = last_cg->next;
	}
	ok;
}

static int test_badchangegrade0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:POISONED");

	eq(r, PARSE_ERROR_NONE);
	/* Try with out of bounds values for the grade maximum. */
	r = parser_parse(p, "change-grade:G:-1:2:Grade maximum below zero");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	r = parser_parse(p, "change-grade:G:32768:5:Grade maximum too large");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	ok;
}

static int test_resist0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "resist:COLD");
	struct timed_effect_data *t;

	eq(r, PARSE_ERROR_NONE);
	t = (struct timed_effect_data*) parser_priv(p);
	notnull(t);
	eq(t->temp_resist, ELEM_COLD);
	ok;
}

static int test_badresist0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "resist:XYZZY");

	noteq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_este0(void *state) {
	struct parser *p = (struct parser*)state;
	enum parser_error r = parser_parse(p, "este:1");
	struct timed_effect_data *t;

	eq(r, PARSE_ERROR_NONE);
	t = (struct timed_effect_data*)parser_priv(p);
	notnull(t);
	require(t->este);
	r = parser_parse(p, "este:0");
	eq(r, PARSE_ERROR_NONE);
	require(!t->este);
	ok;
}

static int test_save0(void *state) {
	struct parser *p = (struct parser*)state;
	enum parser_error r = parser_parse(p, "save:1");
	struct timed_effect_data *t;

	eq(r, PARSE_ERROR_NONE);
	t = (struct timed_effect_data*)parser_priv(p);
	notnull(t);
	require(t->save);
	r = parser_parse(p, "save:0");
	eq(r, PARSE_ERROR_NONE);
	require(!t->save);
	ok;
}

const char *suite_name = "parse/ptimed";

/*
 * test_name0() has to be before any of the other tests besides test_badname0().
 */
struct test tests[] = {
	{ "name0", test_name0 },
	{ "badname0", test_badname0 },
	{ "desc0", test_desc0 },
	{ "endmsg0", test_endmsg0 },
	{ "incmsg0", test_incmsg0 },
	{ "decmsg0", test_decmsg0 },
	{ "changeinc0", test_changeinc0 },
	{ "changedec0", test_changedec0 },
	{ "msgt0", test_msgt0 },
	{ "badmsgt0", test_badmsgt0 },
	{ "fail0", test_fail0 },
	{ "badfail0", test_badfail0 },
	{ "grade0", test_grade0 },
	{ "changegrade0", test_changegrade0 },
	{ "badchangegrade0", test_badchangegrade0 },
	{ "resist0", test_resist0 },
	{ "badresist0", test_badresist0 },
	{ "este0", test_este0 },
	{ "save0", test_save0 },
	{ NULL, NULL }
};
