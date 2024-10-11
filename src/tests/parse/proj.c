/* parse/proj */
/* Exercise parsing used for projection.txt. */

struct chunk;
struct player;
#include "unit-test.h"
#include "datafile.h"
#include "message.h"
#include "obj-init.h"
#include "project.h"
#include "z-color.h"
#include "z-virt.h"

int setup_tests(void **state) {
	*state = projection_parser.init();
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	struct projection *proj = (struct projection*) parser_priv(p);

	while (proj) {
		struct projection *tgt = proj;

		proj = proj->next;
		string_free(tgt->name);
		string_free(tgt->type);
		string_free(tgt->desc);
		string_free(tgt->player_desc);
		string_free(tgt->blind_desc);
		mem_free(tgt);
	}
	parser_destroy(p);
	return 0;
}

static int test_missing_record_header0(void *state) {
	struct parser *p = (struct parser*) state;
	struct projection *proj = (struct projection*) parser_priv(p);
	enum parser_error r;

	null(proj);
	r = parser_parse(p, "name:acid");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "type:element");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "desc:acid");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "player-desc:acid");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "blind-desc:acid");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "msgt:BR_ACID");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "damaging:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "evade:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "obvious:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "wake:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "color:Slate");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_code0(void *state)
{
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "code:ACID");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	eq(proj->index, ELEM_ACID);
	null(proj->name);
	null(proj->type);
	null(proj->desc);
	null(proj->player_desc);
	null(proj->blind_desc);
	eq(proj->msgt, 0);
	eq(proj->damaging, false);
	eq(proj->evade, false);
	eq(proj->obvious, false);
	eq(proj->wake, false);
	eq(proj->color, 0);
	ok;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:acid");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	require(streq(proj->name, "acid"));
	/* Try setting it again to see if memory is leaked. */
	r = parser_parse(p, "name:caustic substance");
	eq(r, PARSE_ERROR_NONE);
	require(streq(proj->name, "caustic substance"));
	ok;
}

static int test_type0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "type:element");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	require(streq(proj->type, "element"));
	/* Try setting it again to see if memory is leaked. */
	r = parser_parse(p, "type:monster");
	eq(r, PARSE_ERROR_NONE);
	require(streq(proj->type, "monster"));
	ok;
}

static int test_desc0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "desc:acid");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	require(streq(proj->desc, "acid"));
	/* Try setting it again to see if memory is leaked. */
	r = parser_parse(p, "desc:caustic substance");
	eq(r, PARSE_ERROR_NONE);
	require(streq(proj->desc, "caustic substance"));
	ok;
}

static int test_player_desc0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "player-desc:acidic mist");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	require(streq(proj->player_desc, "acidic mist"));
	/* Try setting it again to see if memory is leaked. */
	r = parser_parse(p, "player-desc:acid");
	eq(r, PARSE_ERROR_NONE);
	require(streq(proj->player_desc, "acid"));
	ok;
}

static int test_blind_desc0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "blind-desc:something acrid");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	require(streq(proj->blind_desc, "something acrid"));
	/* Try setting it again to see if memory is leaked. */
	r = parser_parse(p, "blind-desc:acid");
	eq(r, PARSE_ERROR_NONE);
	require(streq(proj->blind_desc, "acid"));
	ok;
}

static int test_msgt0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "msgt:HIT");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	eq(proj->msgt, MSG_HIT);
	ok;
}

static int test_msgt_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "msgt:XYZZY");

	eq(r, PARSE_ERROR_INVALID_MESSAGE);
	ok;
}

static int test_damaging0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "damaging:1");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	eq(proj->damaging, true);
	r = parser_parse(p, "damaging:0");
	eq(r, PARSE_ERROR_NONE);
	eq(proj->damaging, false);
	proj->damaging = true;
	/* Non-zero values other than one are false. */
	r = parser_parse(p, "damaging:5");
	eq(r, PARSE_ERROR_NONE);
	eq(proj->damaging, false);
	ok;
}

static int test_evade0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "evade:1");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	eq(proj->evade, true);
	r = parser_parse(p, "evade:0");
	eq(r, PARSE_ERROR_NONE);
	eq(proj->evade, false);
	proj->evade = true;
	/* Non-zero values other than one are false. */
	r = parser_parse(p, "evade:3");
	eq(r, PARSE_ERROR_NONE);
	eq(proj->evade, false);
	ok;
}

static int test_obvious0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "obvious:1");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	eq(proj->obvious, true);
	r = parser_parse(p, "obvious:0");
	eq(r, PARSE_ERROR_NONE);
	eq(proj->obvious, false);
	proj->obvious = true;
	/* Non-zero values other than one are false. */
	r = parser_parse(p, "obvious:2");
	eq(r, PARSE_ERROR_NONE);
	eq(proj->obvious, false);
	ok;
}

static int test_wake0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "wake:1");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	eq(proj->wake, true);
	r = parser_parse(p, "wake:0");
	eq(r, PARSE_ERROR_NONE);
	eq(proj->wake, false);
	proj->wake = true;
	/* Non-zero values other than one are false. */
	r = parser_parse(p, "wake:7");
	eq(r, PARSE_ERROR_NONE);
	eq(proj->wake, false);
	ok;
}

static int test_color0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "color:u");
	struct projection *proj;

	eq(r, PARSE_ERROR_NONE);
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	eq(proj->color, COLOUR_UMBER);
	r = parser_parse(p, "color:Light Red");
	eq(r, PARSE_ERROR_NONE);
	eq(proj->color, COLOUR_L_RED);
	/* The name matching for colors is insensitive to case. */
	r = parser_parse(p, "color:light green");
	eq(r, PARSE_ERROR_NONE);
	eq(proj->color, COLOUR_L_GREEN);
	ok;
}

static int test_combined0(void *state) {
	const char * lines[] = {
		"code:FIRE",
		"name:fire",
		"type:element",
		"desc:flames",
		"player-desc:swirling flames",
		"blind-desc:something hot",
		"msgt:BR_FIRE",
		"damaging:1",
		"obvious:1",
		"wake:1",
		"color:red"
	};
	struct parser *p = (struct parser*) state;
	struct projection *proj;
	int i;

	for (i = 0; i < (int) N_ELEMENTS(lines); ++i) {
		enum parser_error r = parser_parse(p, lines[i]);

		eq(r, PARSE_ERROR_NONE);
	}
	proj = (struct projection*) parser_priv(p);
	notnull(proj);
	eq(proj->index, ELEM_FIRE);
	notnull(proj->name);
	require(streq(proj->name, "fire"));
	notnull(proj->type);
	require(streq(proj->type, "element"));
	notnull(proj->desc);
	require(streq(proj->desc, "flames"));
	notnull(proj->player_desc);
	require(streq(proj->player_desc, "swirling flames"));
	notnull(proj->blind_desc);
	require(streq(proj->blind_desc, "something hot"));
	eq(proj->msgt, MSG_BR_FIRE);
	eq(proj->damaging, true);
	eq(proj->evade, false);
	eq(proj->obvious, true);
	eq(proj->wake, true);
	eq(proj->color, COLOUR_RED);
	ok;
}

static int test_code_mismatch0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "code:POIS");

	eq(r, PARSE_ERROR_ELEMENT_NAME_MISMATCH);
	ok;
}

const char *suite_name = "parse/proj";
/*
 * test_missing_record_header0() has to be before test_code0() and
 * test_combined0().  All others have to be after test_code().
 * test_code_mismatch0() has to be last.
 */
struct test tests[] = {
	{ "missing_record_header0", test_missing_record_header0 },
	{ "code0", test_code0 },
	{ "name0", test_name0 },
	{ "type0", test_type0 },
	{ "desc0", test_desc0 },
	{ "player_desc0", test_player_desc0 },
	{ "blind_desc0", test_blind_desc0 },
	{ "msgt0", test_msgt0 },
	{ "msgt_bad0", test_msgt_bad0 },
	{ "damaging0", test_damaging0 },
	{ "evade0", test_evade0 },
	{ "obvious0", test_obvious0 },
	{ "wake0", test_wake0 },
	{ "color0", test_color0 },
	{ "combined0", test_combined0 },
	{ "code_mismatch0", test_code_mismatch0 },
	{ NULL, NULL }
};
