/* parse/slay */
/* Exercise parsing used for slay.txt. */

#include "unit-test.h"
#include "datafile.h"
#include "init.h"
#include "monster.h"
#include "object.h"
#include "obj-init.h"
#include "z-virt.h"

int setup_tests(void **state) {
	*state = slay_parser.init();
	/* Needed by slay_parser.finish. */
	z_info = mem_zalloc(sizeof(*z_info));
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (slay_parser.finish(p)) {
		r = 1;
	}
	slay_parser.cleanup();
	mem_free(z_info);
	return r;
}

static int test_missing_record_header0(void *state)
{
	struct parser *p = (struct parser*) state;
	struct slay *s = (struct slay*) parser_priv(p);
	enum parser_error r;

	null(s);
	r = parser_parse(p, "name:orcs");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "race-flag:ORC");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "dice:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_code0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "code:ORC_1");
	struct slay *s;

	eq(r, PARSE_ERROR_NONE);
	s = (struct slay*) parser_priv(p);
	notnull(s);
	notnull(s->code);
	require(streq(s->code, "ORC_1"));
	null(s->name);
	eq(s->race_flag, 0);
	eq(s->dice, 0);
	ok;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:orcs");
	struct slay *s;

	eq(r, PARSE_ERROR_NONE);
	s = (struct slay*) parser_priv(p);
	notnull(s);
	notnull(s->name);
	require(streq(s->name, "orcs"));
	/* Try setting it again to see if memory is leaked. */
	r = parser_parse(p, "name:uruk-hai");
	eq(r, PARSE_ERROR_NONE);
	require(streq(s->name, "uruk-hai"));
	ok;
}

static int test_race_flag0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "race-flag:UNIQUE");
	struct slay *s;

	eq(r, PARSE_ERROR_NONE);
	s = (struct slay*) parser_priv(p);
	notnull(s);
	eq(s->race_flag, RF_UNIQUE);
	ok;
}

static int test_race_flag_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "race-flag:XYZZY");

	eq(r, PARSE_ERROR_INVALID_FLAG);
	ok;
}

static int test_dice0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "dice:1");
	struct slay *s;

	eq(r, PARSE_ERROR_NONE);
	s = (struct slay*) parser_priv(p);
	notnull(s);
	eq(s->dice, 1);
	ok;
}

static int test_combined0(void *state) {
	struct parser *p = (struct parser*) state;
	const char *lines[] = {
		"code:SPIDER_1",
		"name:spiders",
		"race-flag:SPIDER",
		"dice:1"
	};
	struct slay *s;
	int i;

	for (i =0; i < (int) N_ELEMENTS(lines); ++i) {
		enum parser_error r = parser_parse(p, lines[i]);

		eq(r, PARSE_ERROR_NONE);
	}
	s = (struct slay*) parser_priv(p);
	notnull(s);
	notnull(s->code);
	require(streq(s->code, "SPIDER_1"));
	notnull(s->name);
	require(streq(s->name, "spiders"));
	eq(s->race_flag, RF_SPIDER);
	eq(s->dice, 1);
	ok;
}

const char *suite_name = "parse/slay";
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
	{ "race_flag0", test_race_flag0 },
	{ "race_flag_bad0", test_race_flag_bad0 },
	{ "dice0", test_dice0 },
	{ "combined0", test_combined0 },
	{ NULL, NULL }
};
