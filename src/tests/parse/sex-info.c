/* parse/sex-info */
/* Exercise parsing used for sex.txt. */

#include "unit-test.h"
#include "init.h"
#include "player.h"

int setup_tests(void **state) {
	*state = sex_parser.init();
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (sex_parser.finish(p)) {
		r = 1;
	}
	sex_parser.cleanup();
	return r;
}

static int test_missing_header_record0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r;

	null(parser_priv(p));
	r = parser_parse(p, "possess:her");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "poetry:female_entry_poetry");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:Female");
	struct player_sex *s;

	eq(r, PARSE_ERROR_NONE);
	s = (struct player_sex*) parser_priv(p);
	notnull(s);
	notnull(s->name);
	require(streq(s->name, "Female"));
	null(s->possessive);
	null(s->poetry_name);
	ok;
}

static int test_possessive0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "possess:her");
	struct player_sex *s;

	eq(r, PARSE_ERROR_NONE);
	s = (struct player_sex*) parser_priv(p);
	notnull(s);
	notnull(s->possessive);
	require(streq(s->possessive, "her"));
	/*
	 * Specifying multiple times for the same sex should not leak
	 * memory.
	 */
	r = parser_parse(p, "possess:his");
	eq(r, PARSE_ERROR_NONE);
	s = (struct player_sex*) parser_priv(p);
	notnull(s);
	notnull(s->possessive);
	require(streq(s->possessive, "his"));
	ok;
}

static int test_poetry0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "poetry:female_entry_poetry");
	struct player_sex *s;

	eq(r, PARSE_ERROR_NONE);
	s = (struct player_sex*) parser_priv(p);
	notnull(s);
	notnull(s->poetry_name);
	require(streq(s->poetry_name, "female_entry_poetry"));
	/*
	 * Specifying multiple times for the same sex should not leak
	 * memory.
	 */
	r = parser_parse(p, "poetry:male_entry_poetry");
	eq(r, PARSE_ERROR_NONE);
	s = (struct player_sex*) parser_priv(p);
	notnull(s);
	notnull(s->poetry_name);
	require(streq(s->poetry_name, "male_entry_poetry"));
	ok;
}

static int test_complete0(void *state) {
	const char *lines[] = {
		"name:Male",
		"possess:his",
		"poetry:male_entry_poetry"
	};
	struct parser *p = (struct parser*) state;
	struct player_sex *s;
	int i;

	for (i = 0; i < (int) N_ELEMENTS(lines); ++i) {
		enum parser_error r = parser_parse(p, lines[i]);

		eq(r, PARSE_ERROR_NONE);
	}
	s = (struct player_sex*) parser_priv(p);
	notnull(s);
	notnull(s->name);
	require(streq(s->name, "Male"));
	notnull(s->possessive);
	require(streq(s->possessive, "his"));
	notnull(s->poetry_name);
	require(streq(s->poetry_name, "male_entry_poetry"));
	ok;
}

const char *suite_name = "parse/parsex";
/*
 * test_missing_header_record0() has to be before test_name0() and
 * test_complete0().
 * Unless otherwise indicated, all other functions have to be after
 * test_name0().
 */
struct test tests[] = {
	{ "missing_header_record0", test_missing_header_record0 },
	{ "name0", test_name0 },
	{ "possessive0", test_possessive0 },
	{ "poetry0", test_poetry0 },
	{ "complete0", test_complete0 },
	{ NULL, NULL }
};
