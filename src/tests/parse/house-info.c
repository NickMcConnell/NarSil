/* parse/house-info */
/* Exercise parsing use for house.txt. */

#include "unit-test.h"

#include "init.h"
#include "player.h"
#include "z-form.h"

/* Set up a minimal set of races to test race lookups. */
static char dummy_race_name_1[16] = "Noldor";
static char dummy_race_name_2[16] = "Sindar";
static char dummy_race_name_3[16] = "Naugrim";
static char dummy_race_name_4[16] = "Edain";
static struct player_race dummy_races[4];

int setup_tests(void **state) {
	*state = house_parser.init();
	dummy_races[0].name = dummy_race_name_1;
	dummy_races[0].next = &dummy_races[1];
	dummy_races[1].name = dummy_race_name_2;
	dummy_races[1].next = &dummy_races[2];
	dummy_races[2].name = dummy_race_name_3;
	dummy_races[2].next = &dummy_races[3];
	dummy_races[3].name = dummy_race_name_4;
	dummy_races[3].next = NULL;
	races = dummy_races;
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (house_parser.finish(p)) {
		r = 1;
	}
	house_parser.cleanup();
	return r;
}

static int test_missing_header_record0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r;

	null(parser_priv(p));
	r = parser_parse(p, "alt-name:Feanor's House");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "short-name:Feanor");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "race:Noldor");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "stats:0:1:0:0");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "skills:0:0:0:0:0:0:1:0");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "player-flags:BLADE_PROFICIENCY");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "desc:Feanor was the greatest of the Noldor, ");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:House of Fingolfin");
	struct player_house *h;
	int i;

	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	notnull(h->name);
	require(streq(h->name, "House of Fingolfin"));
	null(h->race);
	null(h->alt_name);
	null(h->short_name);
	null(h->desc);
	for (i = 0; i < STAT_MAX; ++i) {
		eq(h->stat_adj[i], 0);
	}
	for (i = 0; i < SKILL_MAX; ++i) {
		eq(h->skill_adj[i], 0);
	}
	require(pf_is_empty(h->pflags));
	ok;
}

static int test_alt_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "alt-name:Fingolfin's house");
	struct player_house *h;

	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	notnull(h->alt_name);
	require(streq(h->alt_name, "Fingolfin's house"));
	/*
	 * Specifying multiple times fo the same house should not leak
	 * memory.
	 */
	r = parser_parse(p, "alt-name:Feanor's house");
	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	notnull(h->alt_name);
	require(streq(h->alt_name, "Feanor's house"));
	ok;
}

static int test_short_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "short-name:Fingolfin");
	struct player_house *h;

	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	notnull(h->short_name);
	require(streq(h->short_name, "Fingolfin"));
	/*
	 * Specifying multiple times for the same house should not leak
	 * memory.
	 */
	r = parser_parse(p, "short-name:Feanor");
	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	notnull(h->short_name);
	require(streq(h->short_name, "Feanor"));
	ok;
}

static int test_race0(void *state) {
	struct parser *p = (struct parser*) state;
	char buffer[80];
	int i;

	for (i = 0; i < (int) N_ELEMENTS(dummy_races); ++i) {
		struct player_house *h;
		enum parser_error r;

		strnfmt(buffer, sizeof(buffer), "race:%s", dummy_races[i].name);
		r = parser_parse(p, buffer);
		eq(r, PARSE_ERROR_NONE);
		h = (struct player_house*) parser_priv(p);
		notnull(h);
		ptreq(h->race, &dummy_races[i]);
	}
	ok;
}

static int test_race_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an unrecognized race. */
	enum parser_error r = parser_parse(p, "race:Xyzzy");

	eq(r, PARSE_ERROR_INVALID_PLAYER_RACE);
	ok;
}

static int test_stats0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "stats:3:-3:2:-2");
	struct player_house *h;

	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	eq(h->stat_adj[STAT_STR], 3);
	eq(h->stat_adj[STAT_DEX], -3);
	eq(h->stat_adj[STAT_CON], 2);
	eq(h->stat_adj[STAT_GRA], -2);
	ok;
}

static int test_skills0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "skills:1:2:-1:-2:3:4:-4:-3");
	struct player_house *h;

	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	eq(h->skill_adj[SKILL_MELEE], 1);
	eq(h->skill_adj[SKILL_ARCHERY], 2);
	eq(h->skill_adj[SKILL_EVASION], -1);
	eq(h->skill_adj[SKILL_STEALTH], -2);
	eq(h->skill_adj[SKILL_PERCEPTION], 3);
	eq(h->skill_adj[SKILL_WILL], 4);
	eq(h->skill_adj[SKILL_SMITHING], -4);
	eq(h->skill_adj[SKILL_SONG], -3);
	ok;
}

static int test_flags0(void *state) {
	struct parser *p = (struct parser*) state;
	struct player_house *h = (struct player_house*) parser_priv(p);
	enum parser_error r;
	bitflag eflags[PF_SIZE];

	notnull(h);
	pf_wipe(h->pflags);
	/* Try with no flags. */
	r = parser_parse(p, "player-flags:");
	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	require(pf_is_empty(h->pflags));
	/* Try with a single flag. */
	r = parser_parse(p, "player-flags:BLADE_PROFICIENCY");
	eq(r, PARSE_ERROR_NONE);
	/* Check that multiple directives append the flags. */
	r = parser_parse(p, "player-flags:AXE_PROFICIENCY");
	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	pf_wipe(eflags);
	pf_on(eflags, PF_BLADE_PROFICIENCY);
	pf_on(eflags, PF_AXE_PROFICIENCY);
	require(pf_is_equal(h->pflags, eflags));
	/* Try with multiple flags at once. */
	pf_wipe(h->pflags);
	r = parser_parse(p, "player-flags:AXE_PROFICIENCY | BLADE_PROFICIENCY");
	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	require(pf_is_equal(h->pflags, eflags));
	ok;
}

static int test_flags_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try with an unrecognized flag. */
	enum parser_error r = parser_parse(p, "player-flags:XYZZY");

	eq(r, PARSE_ERROR_INVALID_FLAG);
	ok;
}

static int test_desc0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p,
		"desc:Fingolfin led his house into Beleriand ");
	struct player_house *h;

	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	notnull(h->desc);
	require(streq(h->desc, "Fingolfin led his house into Beleriand "));
	/* Check that a second directive is appended to the first. */
	r = parser_parse(p,
		"desc:to protect its peoples from the shadow of Morgoth.");
	eq(r, PARSE_ERROR_NONE);
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	notnull(h->desc);
	require(streq(h->desc, "Fingolfin led his house into Beleriand"
		"to protect its peoples from the shadow of Morgoth."));
	ok;
}

static int test_complete0(void *state) {
	const char *lines[] = {
		"name:Of the Falas",
		"alt-name:the Falas",
		"short-name:Falathrim",
		"race:Sindar",
		"stats:0:1:0:0",
		"skills:0:1:0:0:0:0:0:0",
		"desc:When Thingol met with Melian under the wheeling stars, ",
		"desc:many of his folk despaired of finding him again and ",
		"desc:journeyed to the shore, the Falas, to set sail to ",
		"desc:Valinor. Some tarried there and dwelt in the havens ",
		"desc:on the edge of Middle-Earth with their lord, Cirdan, ",
		"desc:the shipbuilder."
	};
	struct parser *p = (struct parser*) state;
	struct player_house *h;
	int i;

	for (i = 0; i < (int) N_ELEMENTS(lines); ++i) {
		enum parser_error r = parser_parse(p, lines[i]);

		eq(r, PARSE_ERROR_NONE);
	}
	h = (struct player_house*) parser_priv(p);
	notnull(h);
	notnull(h->name);
	require(streq(h->name, "Of the Falas"));
	notnull(h->alt_name);
	require(streq(h->alt_name, "the Falas"));
	notnull(h->short_name);
	require(streq(h->short_name, "Falathrim"));
	notnull(h->desc);
	require(streq(h->desc, "When Thingol met with Meial under the wheeling "
		"stars, many of his folk despaired of finding him again and "
		"journeyed to the shore, the Falas, to set sail to Valinor. "
		"Some tarried there and dwelt in the havens on the edge of "
		"Middle-Earth with their lord, Cirdan, the shipbuilder."));
	for (i = 0; i < STAT_MAX; ++i) {
		eq(h->stat_adj[i], (i == STAT_DEX) ? 1 : 0);
	}
	for (i = 0; i < SKILL_MAX; ++i) {
		eq(h->skill_adj[i], (i == SKILL_ARCHERY) ? 1 : 0);
	}
	pf_is_empty(h->pflags);
	ok;
}

const char *suite_name = "parse/house-info";
/*
 * test_missing_header_record0() has to be before test_name0() and
 * test_complete0().
 * Unless otherwise indicated, all other functions have to be after
 * test_name0().
 */
struct test tests[] = {
	{ "missing_header_record0", test_missing_header_record0 },
	{ "name0", test_name0 },
	{ "alt_name0", test_alt_name0 },
	{ "short_name0", test_short_name0 },
	{ "race0", test_race0 },
	{ "race_bad0", test_race_bad0 },
	{ "stats0", test_stats0 },
	{ "skills0", test_skills0 },
	{ "flags0", test_flags0 },
	{ "flags_bad0", test_flags_bad0 },
	{ "desc0", test_desc0 },
	{ "complete0", test_complete0 },
	{ NULL, NULL }
};
