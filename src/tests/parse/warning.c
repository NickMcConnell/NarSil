/* parse/warning */
/* Exercise parsing used for warning.txt. */

#include "unit-test.h"
#include "datafile.h"
#include "init.h"
#include "monster.h"
#include "mon-init.h"
#include "z-virt.h"

int setup_tests(void **state) {
	*state = warning_parser.init();
	/* warning_parser.finish needs z_info. */
	z_info = mem_zalloc(sizeof(*z_info));
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (warning_parser.finish(p)) {
		r = 1;
	}
	warning_parser.cleanup();
	mem_free(z_info);
	return r;
}

static int test_missing_record_header(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_warning *mw = (struct monster_warning*) parser_priv(p);
	enum parser_error r;

	null(mw);
	r = parser_parse(p, "vis:roars in anger.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "invis:You hear a loud roar.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "vis-silence:lets out a muffled roar.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "invis-silence:You hear a muffled roar.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_type0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "type:1");
	struct monster_warning *mw;

	eq(r, PARSE_ERROR_NONE);
	mw = (struct monster_warning*) parser_priv(p);
	notnull(mw);
	eq(mw->idx, 1);
	null(mw->msg_vis);
	null(mw->msg_invis);
	null(mw->msg_vis_silence);
	null(mw->msg_invis_silence);
	ok;
}

static int test_vis0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "vis:roars in anger.");
	struct monster_warning *mw;

	eq(r, PARSE_ERROR_NONE);
	mw = (struct monster_warning*) parser_priv(p);
	notnull(mw);
	notnull(mw->msg_vis);
	require(streq(mw->msg_vis, "roars in anger."));
	ok;
}

static int test_invis0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "invis:You hear a loud roar.");
	struct monster_warning *mw;

	eq(r, PARSE_ERROR_NONE);
	mw = (struct monster_warning*) parser_priv(p);
	notnull(mw);
	notnull(mw->msg_invis);
	require(streq(mw->msg_invis, "You hear a loud roar."));
	ok;
}

static int test_vis_silence0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p,
		"vis-silence:lets out a muffled roar.");
	struct monster_warning *mw;

	eq(r, PARSE_ERROR_NONE);
	mw = (struct monster_warning*) parser_priv(p);
	notnull(mw);
	notnull(mw->msg_vis_silence);
	require(streq(mw->msg_vis_silence, "lets out a muffled roar."));
	ok;
}

static int test_invis_silence0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p,
		"invis-silence:You hear a muffled roar.");
	struct monster_warning *mw;

	eq(r, PARSE_ERROR_NONE);
	mw = (struct monster_warning*) parser_priv(p);
	notnull(mw);
	notnull(mw->msg_invis_silence);
	require(streq(mw->msg_invis_silence, "You hear a muffled roar."));
	ok;
}

static int test_combined0(void *state) {
	const char *lines[] = {
		"type:3",
		"vis:grunts in anger.",
		"invis:You hear a loud grunt.",
		"vis-silence:lets out a muffled grunt.",
		"invis-silence:You hear a muffled grunt.",
	};
	struct parser *p = (struct parser*) state;
	struct monster_warning *mw;
	int i;

	for (i = 0; i < (int) N_ELEMENTS(lines); ++i) {
		enum parser_error r = parser_parse(p, lines[i]);

		eq(r, PARSE_ERROR_NONE);
	}
	mw = (struct monster_warning*) parser_priv(p);
	notnull(mw);
	eq(mw->idx, 3);
	notnull(mw->msg_vis);
	require(streq(mw->msg_vis, "grunts in anger."));
	notnull(mw->msg_invis);
	require(streq(mw->msg_invis, "You hear a loud grunt."));
	notnull(mw->msg_vis_silence);
	require(streq(mw->msg_vis_silence, "lets out a muffled grunt."));
	notnull(mw->msg_invis_silence);
	require(streq(mw->msg_invis_silence, "You hear a muffled grunt."));
	ok;
}

const char *suite_name = "parse/warning";
/*
 * test_missing_record_header() has to be before test_type0() and
 * test_combined0.
 * test_combined0() should be last.
 * All others, unless otherwise indicated, have to be after test_type0().
 */
struct test tests[] = {
	{ "missing_record_header", test_missing_record_header },
	{ "type0", test_type0 },
	{ "vis0", test_vis0 },
	{ "invis", test_invis0 },
	{ "vis_silence0", test_vis_silence0 },
	{ "invis_silence0", test_invis_silence0 },
	{ "combined0", test_combined0 },
	{ NULL, NULL }
};
