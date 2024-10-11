/* parse/brand */
/* Exercise parsing used for brand.txt. */

#include "unit-test.h"
#include "datafile.h"
#include "init.h"
#include "monster.h"
#include "object.h"
#include "obj-init.h"
#include "z-virt.h"

int setup_tests(void **state) {
	*state = brand_parser.init();
	/* Needed by brand_parser.finish. */
	z_info = mem_zalloc(sizeof(*z_info));
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (brand_parser.finish(p)) {
		r = 1;
	}
	brand_parser.cleanup();
	mem_free(z_info);
	return r;
}

static int test_missing_record_header0(void *state) {
	struct parser *p = (struct parser*) state;
	struct brand *b = (struct brand*) parser_priv(p);
	enum parser_error r;

	null(b);
	r = parser_parse(p, "name:fire");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "desc:burns {name} with an inner fire");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "dice:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "vuln-dice:2");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "smith-difficulty:24");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "resist-flag:RES_FIRE");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "vuln-flag:HURT_FIRE");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_code0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "code:FIRE_1");
	struct brand *b;

	eq(r, PARSE_ERROR_NONE);
	b = (struct brand*) parser_priv(p);
	notnull(b);
	notnull(b->code);
	require(streq(b->code, "FIRE_1"));
	null(b->name);
	null(b->desc);
	eq(b->resist_flag, 0);
	eq(b->vuln_flag, 0);
	eq(b->dice, 0);
	eq(b->vuln_dice, 0);
	eq(b->smith_difficulty, 0);
	ok;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:fire");
	struct brand *b;

	eq(r, PARSE_ERROR_NONE);
	b = (struct brand*) parser_priv(p);
	notnull(b);
	notnull(b->name);
	require(streq(b->name, "fire"));
	/* Try setting it again to see if memory is leaked. */
	r = parser_parse(p, "name:flame");
	eq(r, PARSE_ERROR_NONE);
	notnull(b->name);
	require(streq(b->name, "flame"));
	ok;
}

static int test_desc0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r =
		parser_parse(p, "desc:burns {name} with an inner fire");
	struct brand *b;

	eq(r, PARSE_ERROR_NONE);
	b = (struct brand*) parser_priv(p);
	notnull(b);
	notnull(b->desc);
	require(streq(b->desc, "burns {name} with an inner fire"));
	/* Try settinng it again to see if memory is leaked. */
	r = parser_parse(p, "desc:freezes {name}");
	eq(r, PARSE_ERROR_NONE);
	notnull(b->desc);
	require(streq(b->desc, "freezes {name}"));
	ok;
}

static int test_dice0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "dice:1");
	struct brand *b;

	eq(r, PARSE_ERROR_NONE);
	b = (struct brand*) parser_priv(p);
	notnull(b);
	eq(b->dice, 1);
	ok;
}

static int test_vuln_dice0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "vuln-dice:2");
	struct brand *b;

	eq(r, PARSE_ERROR_NONE);
	b = (struct brand*) parser_priv(p);
	notnull(b);
	eq(b->vuln_dice, 2);
	ok;
}

static int test_smith_difficulty0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "smith-difficulty:24");
	struct brand *b;

	eq(r, PARSE_ERROR_NONE);
	b = (struct brand*) parser_priv(p);
	notnull(b);
	eq(b->smith_difficulty, 24);
	ok;
}

static int test_resist_flag0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "resist-flag:RES_FIRE");
	struct brand *b;

	eq(r, PARSE_ERROR_NONE);
	b = (struct brand*) parser_priv(p);
	notnull(b);
	eq(b->resist_flag, RF_RES_FIRE);
	ok;
}

static int test_resist_flag_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "resist-flag:XYZZY");

	eq(r, PARSE_ERROR_INVALID_FLAG);
	ok;
}

static int test_vuln_flag0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "vuln-flag:HURT_FIRE");
	struct brand *b;

	eq(r, PARSE_ERROR_NONE);
	b = (struct brand*) parser_priv(p);
	notnull(b);
	eq(b->vuln_flag, RF_HURT_FIRE);
	ok;
}

static int test_vuln_flag_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "vuln-flag:XYZZY");

	eq(r, PARSE_ERROR_INVALID_FLAG);
	ok;
}

static int test_combined0(void *state) {
	struct parser *p = (struct parser*) state;
	const char *lines[] = {
		"code:COLD_1",
		"name:cold",
		"dice:1",
		"vuln-dice:2",
		"desc:freezes {name}",
		"smith-difficulty:20",
		"resist-flag:RES_COLD",
		"vuln-flag:HURT_COLD"
	};
	struct brand *b;
	int i;

	for (i = 0; i < (int) N_ELEMENTS(lines); ++i) {
		enum parser_error r = parser_parse(p, lines[i]);

		eq(r, PARSE_ERROR_NONE);
	}
	b = (struct brand*) parser_priv(p);
	notnull(b);
	notnull(b->code);
	require(streq(b->code, "COLD_1"));
	notnull(b->name);
	require(streq(b->name, "cold"));
	notnull(b->desc);
	require(streq(b->desc, "freezes {name}"));
	eq(b->dice, 1);
	eq(b->vuln_dice, 2);
	eq(b->smith_difficulty, 20);
	eq(b->resist_flag, RF_RES_COLD);
	eq(b->vuln_flag, RF_HURT_COLD);
	ok;
}

const char *suite_name = "parse/brand";
/*
 * test_missing_record_header0() has to be before test_code0() and
 * test_combined0().
 * All others except test_code0() and test_combined0() have to be after
 * test_code0().
 */
struct test tests[] = {
	{ "missing_record_header0", test_missing_record_header0 },
	{ "code0", test_code0 },
	{ "name0", test_name0 },
	{ "desc0", test_desc0 },
	{ "dice0", test_dice0 },
	{ "vuln_dice0", test_vuln_dice0 },
	{ "smith_difficulty0", test_smith_difficulty0 },
	{ "resist_flag0", test_resist_flag0 },
	{ "resist_flag_bad0", test_resist_flag_bad0 },
	{ "vuln_flag0", test_vuln_flag0 },
	{ "vuln_flag_bad0", test_vuln_flag_bad0 },
	{ "combined0", test_combined0 },
	{ NULL, NULL }
};
