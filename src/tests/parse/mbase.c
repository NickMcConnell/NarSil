/* parse/mbase */
/* Exercise parsing used for monster_base.txt. */

#include "unit-test.h"
#include "datafile.h"
#include "init.h"
#include "monster.h"
#include "mon-init.h"
#include "mon-spell.h"
#include "z-form.h"
#include "z-virt.h"
#ifndef WINDOWS
#include <locale.h>
#include <langinfo.h>
#endif

extern struct monster_pursuit *pursuit_messages;
extern struct monster_warning *warning_messages;

static struct monster_pain dummy_pain_messages[] = {
	{ { NULL, NULL, NULL }, 0, NULL },
	{
		{
			"You hear a snarl.",
			"You hear a yelp.",
			"You hear a feeble yelp."
		},
		1, NULL
	},
	{
		{
			"You hear a grunt.",
			"You hear a cry of pain.",
			"You hear a feeble cry."
		},
		2, NULL
	}
};

static struct monster_pursuit dummy_pursuit_messages[] = {
	{ 0, NULL, NULL, NULL, NULL },
	{
		1,
		"shouts excitedly.",
		"You hear a shout.",
		"You hear a distant shout.",
		NULL
	},
	{
		2,
		"roars.",
		"You hear a loud roar.",
		"You hear a distant roar.",
		NULL
	}
};

static struct monster_warning dummy_warning_messages[] = {
	{ 0, NULL, NULL, NULL, NULL, NULL },
	{
		1,
		"shouts a warning.",
		"You hear a warning shout.",
		"shouts a muffled warning.",
		"You hear a muffled warning shout.",
		NULL
	},
	{
		2,
		"roars in anger.",
		"You hear a loud roar.",
		"lets out a muffled roar.",
		"You hear a muffled roar.",
		NULL
	}
};

int setup_tests(void **state) {
	int i;

	*state = mon_base_parser.init();
	/*
	 * Set up enough messages to exercise the pain, pursuit, and warning
	 * directives.
	 */
	for (i = (int) N_ELEMENTS(dummy_pain_messages) - 2; i >= 0; --i) {
		dummy_pain_messages[i].next = dummy_pain_messages + i + 1;
	}
	pain_messages = dummy_pain_messages;
	for (i = (int) N_ELEMENTS(dummy_pursuit_messages) - 2; i >= 0; --i) {
		dummy_pursuit_messages[i].next = dummy_pursuit_messages + i + 1;
	}
	pursuit_messages = dummy_pursuit_messages;
	for (i = (int) N_ELEMENTS(dummy_warning_messages) - 2; i >= 0; --i) {
		dummy_warning_messages[i].next = dummy_warning_messages + i + 1;
	}
	warning_messages = dummy_warning_messages;
	z_info = mem_zalloc(sizeof(*z_info));
	z_info->pain_max = (uint16_t) N_ELEMENTS(dummy_pain_messages);
	z_info->pursuit_max = (uint16_t) N_ELEMENTS(dummy_pursuit_messages);
	z_info->warning_max = (uint16_t) N_ELEMENTS(dummy_warning_messages);
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (mon_base_parser.finish(p)) {
		r = 1;
	}
	mon_base_parser.cleanup();
	mem_free(z_info);
	return r;
}

static int test_missing_record_header0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_base *rb = (struct monster_base*) parser_priv(p);
	enum parser_error r;

	null(rb);
	r = parser_parse(p, "glyph:D");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "pain:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "pursuit:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "warning:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "flags:DRAGON | NO_CONF");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "desc:Ancient Dragon/Wyrm");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:ancient dragon");
	struct monster_base *rb;

	eq(r, PARSE_ERROR_NONE);
	rb = (struct monster_base*) parser_priv(p);
	notnull(rb);
	notnull(rb->name);
	require(streq(rb->name, "ancient dragon"));
	null(rb->text);
	eq(rb->d_char, 0);
	null(rb->pain);
	null(rb->pursuit);
	null(rb->warning);
	require(rf_is_empty(rb->flags));
	ok;
}

static int test_glyph0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "glyph:D");
	struct monster_base *rb;

	eq(r, PARSE_ERROR_NONE);
	rb = (struct monster_base*) parser_priv(p);
	notnull(rb);
	eq(rb->d_char, L'D');
#ifndef WINDOWS
	if (setlocale(LC_CTYPE, "") && streq(nl_langinfo(CODESET), "UTF-8")) {
		/*
		 * Check that a glyph outside of the ASCII range works.  Using
		 * the Yen sign, U+00A5 or C2 A5 as UTF-8.
		 */
		wchar_t wcs[3];
		int nc;

		r = parser_parse(p, "glyph:¥");
		eq(r, PARSE_ERROR_NONE);
		nc = text_mbstowcs(wcs, "¥", (int) N_ELEMENTS(wcs));
		eq(nc, 1);
		eq(rb->d_char, wcs[0]);
	}
#endif
	ok;
}

static int test_pain0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "pain:1");
	struct monster_base *rb;

	eq(r, PARSE_ERROR_NONE);
	rb = (struct monster_base*) parser_priv(p);
	notnull(rb);
	ptreq(rb->pain, dummy_pain_messages + 1);
	ok;
}

static int test_pain_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	char buffer[80];
	enum parser_error r;
	size_t np;

	np = strnfmt(buffer, sizeof(buffer), "pain:%d",
		(int) N_ELEMENTS(dummy_pain_messages));
	require(np < sizeof(buffer));
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	ok;
}

static int test_pursuit0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "pursuit:2");
	struct monster_base *rb;

	eq(r, PARSE_ERROR_NONE);
	rb = (struct monster_base*) parser_priv(p);
	notnull(rb);
	ptreq(rb->pursuit, dummy_pursuit_messages + 2);
	ok;
}

static int test_pursuit_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	char buffer[80];
	enum parser_error r;
	size_t np;

	np = strnfmt(buffer, sizeof(buffer), "pursuit:%d",
		(int) N_ELEMENTS(dummy_pursuit_messages));
	require(np < sizeof(buffer));
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	ok;
}

static int test_warning0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "warning:1");
	struct monster_base *rb;

	eq(r, PARSE_ERROR_NONE);
	rb = (struct monster_base*) parser_priv(p);
	notnull(rb);
	ptreq(rb->warning, dummy_warning_messages + 1);
	ok;
	ok;
}

static int test_warning_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	char buffer[80];
	enum parser_error r;
	size_t np;

	np = strnfmt(buffer, sizeof(buffer), "warning:%d",
		(int) N_ELEMENTS(dummy_warning_messages));
	require(np < sizeof(buffer));
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	ok;
}

static int test_flags0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_base *rb = (struct monster_base*) parser_priv(p);
	bitflag expected[RF_SIZE];
	enum parser_error r;

	notnull(rb);
	rf_wipe(rb->flags);
	/* Check that specifying an empty set of flags works. */
	r = parser_parse(p, "flags:");
	eq(r, PARSE_ERROR_NONE);
	require(rf_is_empty(rb->flags));
	/* Try setting one flag. */
	r = parser_parse(p, "flags:UNIQUE");
	eq(r, PARSE_ERROR_NONE);
	/* Try setting more than one flag. */
	r = parser_parse(p, "flags:MALE | SHORT_SIGHTED");
	eq(r, PARSE_ERROR_NONE);
	rf_wipe(expected);
	rf_on(expected, RF_UNIQUE);
	rf_on(expected, RF_MALE);
	rf_on(expected, RF_SHORT_SIGHTED);
	require(rf_is_equal(rb->flags, expected));
	ok;
}

static int test_flags_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check that an unknown flag generates an appropriate error. */
	enum parser_error r = parser_parse(p, "flags:XYZZY");

	eq(r, PARSE_ERROR_INVALID_FLAG);
	ok;
}

static int test_desc0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "desc:something");
	struct monster_base *rb;

	eq(r, PARSE_ERROR_NONE);
	rb = (struct monster_base*) parser_priv(p);
	notnull(rb);
	notnull(rb->text);
	require(streq(rb->text, "something"));
	/* Check that another directive appends to the first. */
	r = parser_parse(p, "desc: nasty");
	eq(r, PARSE_ERROR_NONE);
	notnull(rb->text);
	require(streq(rb->text, "something nasty"));
	ok;
}

static int test_combined0(void *state) {
	const char *lines[] = {
		"name:dragon",
		"glyph:d",
		"pain:2",
		"pursuit:1",
		"warning:2",
		"flags:TERRITORIAL",
		"flags:BASH_DOOR | DRAGON",
		"desc:Dragon"
	};
	struct parser *p = (struct parser*) state;
	struct monster_base *rb;
	bitflag eflags[RF_SIZE];
	int i;

	for (i = 0; i < (int) N_ELEMENTS(lines); ++i) {
		enum parser_error r = parser_parse(p, lines[i]);

		eq(r, PARSE_ERROR_NONE);
	}
	rb = (struct monster_base*) parser_priv(p);
	notnull(rb);
	notnull(rb->name);
	require(streq(rb->name, "dragon"));
	eq(rb->d_char, L'd');
	ptreq(rb->pain, dummy_pain_messages + 2);
	ptreq(rb->pursuit, dummy_pursuit_messages + 1);
	ptreq(rb->warning, dummy_warning_messages + 2);
	rf_wipe(eflags);
	rf_on(eflags, RF_TERRITORIAL);
	rf_on(eflags, RF_BASH_DOOR);
	rf_on(eflags, RF_DRAGON);
	require(rf_is_equal(rb->flags, eflags));
	notnull(rb->text);
	require(streq(rb->text, "Dragon"));
	ok;
}

const char *suite_name = "parse/mbase";
/*
 * test_missing_record_header0() has to be before test_name0() and
 * test_combined0().
 * All others, unless otherwise noted, have to be after test_name0().
 */
struct test tests[] = {
	{ "missing_record_header0", test_missing_record_header0 },
	{ "name0", test_name0 },
	{ "glyph0", test_glyph0 },
	{ "pain0", test_pain0 },
	{ "pain_bad0", test_pain_bad0 },
	{ "pursuit0", test_pursuit0 },
	{ "pursuit_bad0", test_pursuit_bad0 },
	{ "warning0", test_warning0 },
	{ "warning_bad0", test_warning_bad0 },
	{ "flags0", test_flags0 },
	{ "flags_bad0", test_flags_bad0 },
	{ "desc0", test_desc0 },
	{ "combined0", test_combined0 },
	{ NULL, NULL }
};
