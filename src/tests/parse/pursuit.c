/* parse/pursuit */
/* Exercise parsing used for pursuit.txt. */

#include "unit-test.h"
#include "datafile.h"
#include "init.h"
#include "monster.h"
#include "mon-init.h"
#include "z-virt.h"

int setup_tests(void **state) {
	*state = pursuit_parser.init();
	/* pursuit_parser.finish needs z_info. */
	z_info = mem_zalloc(sizeof(*z_info));
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (pursuit_parser.finish(p)) {
		r = 1;
	}
	pursuit_parser.cleanup();
	mem_free(z_info);
	return r;
}

static int test_missing_record_header(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_pursuit *mp = (struct monster_pursuit*) parser_priv(p);
	enum parser_error r;

	null(mp);
	r = parser_parse(p, "visible:shouts excitedly.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "close:You hear a shout.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "far:You hear a distant shout.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_type0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "type:1");
	struct monster_pursuit *mp;

	eq(r, PARSE_ERROR_NONE);
	mp = (struct monster_pursuit*) parser_priv(p);
	notnull(mp);
	eq(mp->idx, 1);
	null(mp->msg_vis);
	null(mp->msg_close);
	null(mp->msg_far);
	ok;
}

static int test_visible0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "visible:shouts excitedly.");
	struct monster_pursuit *mp;

	eq(r, PARSE_ERROR_NONE);
	mp = (struct monster_pursuit*) parser_priv(p);
	notnull(mp);
	notnull(mp->msg_vis);
	require(streq(mp->msg_vis, "shouts excitedly."));
	ok;
}

static int test_close0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "close:You hear a shout.");
	struct monster_pursuit *mp;

	eq(r, PARSE_ERROR_NONE);
	mp = (struct monster_pursuit*) parser_priv(p);
	notnull(mp);
	notnull(mp->msg_close);
	require(streq(mp->msg_close, "You hear a shout."));
	ok;
}

static int test_far0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "far:You hear a distant shout.");
	struct monster_pursuit *mp;

	eq(r, PARSE_ERROR_NONE);
	mp = (struct monster_pursuit*) parser_priv(p);
	notnull(mp);
	notnull(mp->msg_far);
	require(streq(mp->msg_far, "You hear a distant shout."));
	ok;
}

static int test_combined0(void *state) {
	const char *lines[] = {
		"type:2",
		"visible:roars.",
		"close:You hear a loud roar.",
		"far:You hear a distant roar."
	};
	struct parser *p = (struct parser*) state;
	struct monster_pursuit *mp;
	int i;

	for (i = 0; i < (int) N_ELEMENTS(lines); ++i) {
		enum parser_error r = parser_parse(p, lines[i]);

		eq(r, PARSE_ERROR_NONE);
	}
	mp = (struct monster_pursuit*) parser_priv(p);
	notnull(mp);
	eq(mp->idx, 2);
	notnull(mp->msg_vis);
	require(streq(mp->msg_vis, "roars."));
	notnull(mp->msg_close);
	require(streq(mp->msg_close, "You hear a loud roar."));
	notnull(mp->msg_far);
	require(streq(mp->msg_far, "You hear a distant roar."));
	ok;
}

const char *suite_name = "parse/pursuit";
/*
 * test_missing_record_header() has to be before test_type0() and
 * test_combined0().
 * test_combined0() should be last.
 * All others, unless otherwise indicated, have to be after test_type0().
 */
struct test tests[] = {
	{ "missing_record_header", test_missing_record_header },
	{ "type0", test_type0 },
	{ "visible0", test_visible0 },
	{ "close0", test_close0 },
	{ "far0", test_far0 },
	{ "combined0", test_combined0 },
	{ NULL, NULL }
};
