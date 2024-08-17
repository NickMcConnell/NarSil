/* parse/f-info */

#include "unit-test.h"
#include "unit-test-data.h"
#include "cave.h"
#include "init.h"
#include "monster.h"
#include "player.h"
#include "z-form.h"
#ifndef WINDOWS
#include <locale.h>
#include <langinfo.h>
#endif


int setup_tests(void **state) {
	*state = init_parse_feat();
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	struct feature *f = (struct feature*) parser_priv(p);

	string_free(f->look_in_preposition);
	string_free(f->look_prefix);
	string_free(f->confused_msg);
	string_free(f->die_msg);
	string_free(f->hurt_msg);
	string_free(f->run_msg);
	string_free(f->walk_msg);
	string_free(f->desc);
	string_free(f->name);
	mem_free(f_info);
	parser_destroy(state);
	return 0;
}

static int test_missing_header_record0(void *state) {
	struct parser *p = (struct parser*) state;
	struct feature *f = (struct feature*) parser_priv(p);
	enum parser_error r;

	null(f);
	r = parser_parse(p, "name:Test Feature");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "graphics: :w");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "priority:2");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "mimic:GRANITE");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "flags:LOS | PASSABLE");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "desc:A door that is already open.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "walk-msg:It looks dangerous.  Really enter? ");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "run-msg:Lava blocks your path.  Step into it? ");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "hurt-msg:The lava burns you!");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "die-msg:burning to a cinder in lava");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "confused-msg:bangs into a door");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "look-prefix:the entrance to the");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "look-in-preposition:at");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_code_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "code:XYZZY");

	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	ok;
}

static int test_code0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "code:FLOOR");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = (struct feature*) parser_priv(p);
	ptreq(f, &f_info[FEAT_FLOOR]);
	null(f->name);
	null(f->desc);
	null(f->mimic);
	eq(f->fidx, FEAT_FLOOR);
	eq(f->priority, 0);
	eq(f->dig, 0);
	require(flag_is_empty(f->flags, TF_SIZE));
	eq(f->d_attr, 0);
	eq(f->d_char, 0);
	null(f->walk_msg);
	null(f->run_msg);
	null(f->hurt_msg);
	null(f->die_msg);
	null(f->confused_msg);
	null(f->look_prefix);
	null(f->look_in_preposition);
	ok;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:Test Feature");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = (struct feature*) parser_priv(p);
	require(f);
	require(streq(f->name, "Test Feature"));
	ok;
}

static int test_name_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Specifying a name when there is another one should raise an error. */
	enum parser_error r = parser_parse(p, "name:Another Name");

	eq(r, PARSE_ERROR_REPEATED_DIRECTIVE);
	ok;
}

static int test_graphics0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(state, "graphics:::red");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = (struct feature*) parser_priv(p);
	require(f);
	eq(f->d_char, L':');
	eq(f->d_attr, COLOUR_RED);
	ok;
}

static int test_mimic0(void *state) {
	enum parser_error r = parser_parse(state, "mimic:FLOOR");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	ptreq(f->mimic, &f_info[FEAT_FLOOR]);
	ok;
}

static int test_mimic_bad0(void *state) {
	enum parser_error r = parser_parse(state, "mimic:XYZZY");

	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	ok;
}

static int test_priority0(void *state) {
	enum parser_error r = parser_parse(state, "priority:2");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	eq(f->priority, 2);
	ok;
}

static int test_flags0(void *state) {
	enum parser_error r = parser_parse(state, "flags:LOS | PERMANENT | DOWNSTAIR");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	require(f->flags);
	ok;
}

static int test_info0(void *state) {
	enum parser_error r = parser_parse(state, "info:9:2:0");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	eq(f->forge_bonus, 9);
	eq(f->dig, 2);
	eq(f->pit_difficulty, 0);
	ok;
}

static int test_walk_msg0(void *state) {
	enum parser_error r = parser_parse(state, "walk-msg:lookout ");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	require(streq(f->walk_msg, "lookout "));
	ok;
}

static int test_run_msg0(void *state) {
	enum parser_error r = parser_parse(state, "run-msg:lookout! ");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	require(streq(f->run_msg, "lookout! "));
	ok;
}

static int test_hurt_msg0(void *state) {
	enum parser_error r = parser_parse(state, "hurt-msg:ow!");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	require(streq(f->hurt_msg, "ow!"));
	ok;
}

static int test_die_msg0(void *state) {
	enum parser_error r = parser_parse(state, "die-msg:aargh");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	require(streq(f->die_msg, "aargh"));
	ok;
}

static int test_dig_msg0(void *state) {
	enum parser_error r = parser_parse(state, "dig-msg:urgh");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	require(streq(f->dig_msg, "urgh"));
	ok;
}

static int test_fail_msg0(void *state) {
	enum parser_error r = parser_parse(state, "fail-msg:D'oh");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	require(streq(f->fail_msg, "D'oh"));
	ok;
}

static int test_str_msg0(void *state) {
	enum parser_error r = parser_parse(state, "str-msg:hnnnh");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	require(streq(f->str_msg, "hnnnh"));
	ok;
}

static int test_confused_msg0(void *state) {
	enum parser_error r = parser_parse(state, "confused-msg:huh?");
	struct feature *f;

	eq(r, PARSE_ERROR_NONE);
	f = parser_priv(state);
	require(f);
	require(streq(f->confused_msg, "huh?"));
	ok;
}

const char *suite_name = "parse/f-info";
struct test tests[] = {
	{ "missing_header_record0", test_missing_header_record0 },
	{ "code_bad0", test_code_bad0 },
	{ "code0", test_code0 },
	{ "name0", test_name0 },
	{ "name_bad0", test_name_bad0 },
	{ "graphics0", test_graphics0 },
	{ "mimic0", test_mimic0 },
	{ "mimic_bad0", test_mimic_bad0 },
	{ "priority0", test_priority0 },
	{ "flags0", test_flags0 },
	{ "info0", test_info0 },
	{ "walk_msg0", test_walk_msg0 },
	{ "run_msg0", test_run_msg0 },
	{ "hurt_msg0", test_hurt_msg0 },
	{ "die_msg0", test_die_msg0 },
	{ "dig_msg0", test_dig_msg0 },
	{ "fail_msg0", test_fail_msg0 },
	{ "str_msg0", test_str_msg0 },
	{ "confused_msg0", test_confused_msg0 },
	{ NULL, NULL }
};
