/* parse/blowe */
/* Exercise parsing used for blow_effects.txt. */

#include "unit-test.h"
#include "datafile.h"
#include "init.h"
#include "mon-blows.h"
#include "mon-init.h"
#include "object.h"
#include "project.h"
#include "z-color.h"
#include "z-virt.h"

int setup_tests(void **state) {
	*state = eff_parser.init();
	/* eff_parser.finish needs z_info. */
	z_info = mem_zalloc(sizeof(*z_info));
	return !state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (eff_parser.finish(p)) {
		r = 1;
	}
	eff_parser.cleanup();
	mem_free(z_info);
	return r;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:HURT");
	struct blow_effect *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct blow_effect*) parser_priv(p);
	notnull(e);
	notnull(e->name);
	require(streq(e->name, "HURT"));
	eq(e->power, 0);
	eq(e->eval, 0);
	null(e->desc);
	null(e->effect_type);
	eq(e->resist, 0);
	eq(e->dam_type, 0);
	ok;
}

static int test_power0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "power:30");
	struct blow_effect *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct blow_effect*) parser_priv(p);
	notnull(e);
	eq(e->power, 30);
	ok;
}

static int test_eval0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "eval:10");
	struct blow_effect *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct blow_effect*) parser_priv(p);
	notnull(e);
	eq(e->eval, 10);
	ok;
}

static int test_desc0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "desc:attack");
	struct blow_effect *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct blow_effect*) parser_priv(p);
	notnull(e);
	notnull(e->desc);
	require(streq(e->desc, "attack"));
	/* Check that a second directive appends to the first. */
	r = parser_parse(p, "desc: something");
	eq(r, PARSE_ERROR_NONE);
	notnull(e->desc);
	require(streq(e->desc, "attack something"));
	ok;
}

static int test_effect_type0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "effect-type:element");
	struct blow_effect *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct blow_effect*) parser_priv(p);
	notnull(e);
	require(streq(e->effect_type, "element"));
	ok;
}

static int test_effect_type_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up an effect with an unrecognized effect-type. */
	enum parser_error r = parser_parse(p, "name:TEST_BAD_EFFECT_TYPE0");

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "effect-type:XYZZY");
	/*
	 * The unrecognized effect-type is detected when trying to use the
	 * resist directive.
	 */
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "resist:POIS");
	eq(r, PARSE_ERROR_MISSING_BLOW_EFF_TYPE);
	ok;
}

static int test_resist0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up a new effect with an effect-type of eleemnt. */
	enum parser_error r = parser_parse(p, "name:POISON");
	struct blow_effect *e;

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "effect-type:element");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "resist:POIS");
	eq(r, PARSE_ERROR_NONE);
	e = (struct blow_effect*) parser_priv(p);
	notnull(e);
	eq(e->resist, PROJ_POIS);
	/* Set up a new effect with an effect-type of flag. */
	r = parser_parse(p, "name:BLIND");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "effect-type:flag");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "resist:PROT_BLIND");
	eq(r, PARSE_ERROR_NONE);
	e = (struct blow_effect*) parser_priv(p);
	notnull(e);
	eq(e->resist, OF_PROT_BLIND);
	ok;
}

static int test_resist_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up a new effect with an effect-type of eleemnt. */
	enum parser_error r = parser_parse(p, "name:BAD_ELEMENT");
	struct blow_effect *e;

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "effect-type:element");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "resist:XYZZY");
	/* Doesn't signal an error, but the resist field is set to -1. */
	eq(r, PARSE_ERROR_NONE);
	e = (struct blow_effect*) parser_priv(p);
	notnull(e);
	eq(e->resist, -1);
	/* Set up a new effect with an effect-type of flag. */
	r = parser_parse(p, "name:BAD_FLAG");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "effect-type:flag");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "resist:XYZZY");
	eq(r, PARSE_ERROR_NONE);
	e = (struct blow_effect*) parser_priv(p);
	notnull(e);
	eq(e->resist, -1);
	ok;
}

static int test_dam_type0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "dam-type:FIRE");
	struct blow_effect *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct blow_effect*) parser_priv(p);
	notnull(e);
	eq(e->dam_type, PROJ_FIRE);
	ok;
}

static int test_dam_type_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "dam-type:XYZZY");
	struct blow_effect *e;

	/*
	 * Doesn't flag an error and the dam_type field is set to PROJ_HURT.
	 */
	eq(r, PARSE_ERROR_NONE);
	e = (struct blow_effect*) parser_priv(p);
	notnull(e);
	eq(e->dam_type, PROJ_HURT);
	ok;
}

const char *suite_name = "parse/blowe";
/*
 * test_power0(), test_eval0(), test_desc0(), test_effect_type0(),
 * test_resist0(), test_dam_type0(), test_dam_type_bad0() have to be after
 * test_name0().
 */
struct test tests[] = {
	{ "name0", test_name0 },
	{ "power0", test_power0 },
	{ "eval0", test_eval0 },
	{ "desc0", test_desc0 },
	{ "effect_type0", test_effect_type0 },
	{ "effect_type_bad0", test_effect_type_bad0 },
	{ "resist0", test_resist0 },
	{ "resist_bad0", test_resist_bad0 },
	{ "dam_type0", test_dam_type0 },
	{ "dam_type_bad0", test_dam_type_bad0 },
	{ NULL, NULL }
};
