/* parse/pain */
/* Exercise parsing used for pain.txt. */

#include "unit-test.h"
#include "datafile.h"
#include "init.h"
#include "monster.h"
#include "mon-init.h"
#include "z-form.h"
#include "z-virt.h"

int setup_tests(void **state) {
	*state = pain_parser.init();
	/* pain_parser.finish needs z_info. */
	z_info = mem_zalloc(sizeof(*z_info));
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (pain_parser.finish(p)) {
		r = 1;
	}
	pain_parser.cleanup();
	mem_free(z_info);
	return r;
}

static int test_missing_record_header(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_pain *mp = (struct monster_pain*) parser_priv(p);
	enum parser_error r;

	null(mp);
	r = parser_parse(p, "message:You hear a snarl.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_type0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "type:1");
	struct monster_pain *mp;
	int i;

	eq(r, PARSE_ERROR_NONE);
	mp = (struct monster_pain*) parser_priv(p);
	notnull(mp);
	eq(mp->pain_idx, 1);
	for (i = 0; i < (int) N_ELEMENTS(mp->messages); ++i) {
		null(mp->messages[i]);
	}
	ok;
}

static int test_message0(void *state) {
	struct parser *p = (struct parser*) state;
	const char *messages[] = {
		"You hear a snarl.",
		"You hear a yelp.",
		"You hear a feeble yelp."
	};
	/* Set up an empty type. */
	enum parser_error r = parser_parse(p, "type:2");
	char buffer[80];
	struct monster_pain *mp;
	size_t np;
	int i;

	eq(r, PARSE_ERROR_NONE);
	for (i = 0; i < (int) N_ELEMENTS(messages); ++i) {
		np = strnfmt(buffer, sizeof(buffer), "message:%s",
			messages[i]);
		require(np < sizeof(buffer));
		r = parser_parse(p, buffer);
		eq(r, PARSE_ERROR_NONE);
	}
	mp = (struct monster_pain*) parser_priv(p);
	notnull(mp);
	for (i = 0; i < (int) N_ELEMENTS(messages); ++i) {
		notnull(mp->messages[i]);
		require(streq(mp->messages[i], messages[i]));
	}
	ok;
}

static int test_message_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up an empty type. */
	enum parser_error r = parser_parse(p, "type:3");
	char buffer[80];
	struct monster_pain *mp;
	size_t np;
	int i;

	eq(r, PARSE_ERROR_NONE);
	mp = (struct monster_pain*) parser_priv(p);
	notnull(mp);
	for (i = 0; i < (int) N_ELEMENTS(mp->messages); ++i) {
		np = strnfmt(buffer, sizeof(buffer), "message:pain %d", i);
		require(np < sizeof(buffer));
		r = parser_parse(p, buffer);
		eq(r, PARSE_ERROR_NONE);
	}
	np = strnfmt(buffer, sizeof(buffer), "message:pain %d",
		(int) N_ELEMENTS(mp->messages));
	require(np < sizeof(buffer));
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_TOO_MANY_ENTRIES);
	ok;
}

static int test_combined0(void *state) {
	const char *lines[] = {
		"type:4",
		"message:You hear an angry droning.",
		"message:You hear a scuttling noise.",
		"message:You hear a skittering sound.",
	};
	struct parser *p = (struct parser*) state;
	struct monster_pain *mp;
	int i;

	for (i = 0; i < (int) N_ELEMENTS(lines); ++i) {
		enum parser_error r = parser_parse(p, lines[i]);

		eq(r, PARSE_ERROR_NONE);
	}
	mp = (struct monster_pain*) parser_priv(p);
	notnull(mp);
	eq(mp->pain_idx, 4);
	notnull(mp->messages[0]);
	require(streq(mp->messages[0], "You hear an angry droning."));
	notnull(mp->messages[1]);
	require(streq(mp->messages[1], "You hear a scuttling noise."));
	notnull(mp->messages[2]);
	require(streq(mp->messages[2], "You hear a skittering sound."));
	ok;
}

const char *suite_name = "parse/pain";
/*
 * test_missing_record_header() has to be before test_type0(),
 * test_message0(), test_message_bad0(), and test_combined0().
 */
struct test tests[] = {
	{ "missing_record_header", test_missing_record_header },
	{ "type0", test_type0 },
	{ "message0", test_message0 },
	{ "message_bad0", test_message_bad0 },
	{ "combined0", test_combined0 },
	{ NULL, NULL }
};
