/* parse/p-info */
/* Exercise parsing used for race.txt. */

#include "unit-test.h"
#include "init.h"
#include "object.h"
#include "player.h"


int setup_tests(void **state) {
	*state = init_parse_race();
	return !*state;
}

int teardown_tests(void *state) {
	struct player_race *pr = parser_priv(state);
	struct start_item *si = pr->start_items;

	while (si) {
		struct start_item *nsi = si->next;

		mem_free(si);
		si = nsi;
	}
	string_free((char *)pr->desc);
	string_free((char *)pr->name);
	mem_free(pr);
	parser_destroy(state);
	return 0;
}

static int test_missing_record_header0(void *state) {
	struct parser *p = (struct parser*) state;
	struct player_race *pr = (struct player_race*) parser_priv(p);
	enum parser_error r;

	null(pr);
	r = parser_parse(p, "stats:0:1:2:2");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "skills:0:1:0:0:0:0:0:0");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "history:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "age:20:4865");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "height:76:3");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "weight:159:10");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "player-flags:BLADE_PROFICIENCY");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "equip:food:Fragment of Lembas:3:3");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "desc:The dwarves are stone-hard and stubborn, ");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_name0(void *state) {
	enum parser_error r = parser_parse(state, "name:Half-Elf");
	struct player_race *pr;

	eq(r, PARSE_ERROR_NONE);
	pr = parser_priv(state);
	require(pr);
	require(streq(pr->name, "Half-Elf"));
	ok;
}

static int test_stats0(void *state) {
	enum parser_error r = parser_parse(state, "stats:1:-1:2:-2");
	struct player_race *pr;

	eq(r, PARSE_ERROR_NONE);
	pr = parser_priv(state);
	require(pr);
	eq(pr->stat_adj[STAT_STR], 1);
	eq(pr->stat_adj[STAT_DEX], -1);
	eq(pr->stat_adj[STAT_CON], 2);
	eq(pr->stat_adj[STAT_GRA], -2);
	ok;
}

static int test_skills0(void *state) {
	enum parser_error r = parser_parse(state, "skills:1:2:-1:0:1:0:-1:0");
	struct player_race *pr;

	eq(r, PARSE_ERROR_NONE);
	pr = parser_priv(state);
	require(pr);
	eq(pr->skill_adj[SKILL_MELEE], 1);
	eq(pr->skill_adj[SKILL_ARCHERY], 2);
	eq(pr->skill_adj[SKILL_EVASION], -1);
	eq(pr->skill_adj[SKILL_STEALTH], 0);
	eq(pr->skill_adj[SKILL_PERCEPTION], 1);
	eq(pr->skill_adj[SKILL_WILL], 0);
	eq(pr->skill_adj[SKILL_SMITHING], -1);
	eq(pr->skill_adj[SKILL_SONG], 0);
	ok;
}

static int test_history0(void *state) {
	enum parser_error r = parser_parse(state, "history:0");
	struct player_race *pr;

	eq(r, PARSE_ERROR_NONE);
	pr = parser_priv(state);
	require(pr);
	null(pr->history);
	ok;
}

static int test_age0(void *state) {
	enum parser_error r = parser_parse(state, "age:10:3");
	struct player_race *pr;

	eq(r, PARSE_ERROR_NONE);
	pr = parser_priv(state);
	require(pr);
	eq(pr->b_age, 10);
	eq(pr->m_age, 3);
	ok;
}

static int test_height0(void *state) {
	enum parser_error r = parser_parse(state, "height:10:2");
	struct player_race *pr;

	eq(r, PARSE_ERROR_NONE);
	pr = parser_priv(state);
	require(pr);
	eq(pr->base_hgt, 10);
	eq(pr->mod_hgt, 2);
	ok;
}

static int test_weight0(void *state) {
	enum parser_error r = parser_parse(state, "weight:80:10");
	struct player_race *pr;

	eq(r, PARSE_ERROR_NONE);
	pr = parser_priv(state);
	require(pr);
	eq(pr->base_wgt, 80);
	eq(pr->mod_wgt, 10);
	ok;
}

static int test_play_flags0(void *state) {
	struct parser *p = (struct parser*) state;
	struct player_race *pr = (struct player_race*) parser_priv(p);
	enum parser_error r;
	bitflag eflags[PF_SIZE];

	notnull(pr);
	pf_wipe(pr->pflags);
	/* Check that specifying no flags works. */
	r = parser_parse(p, "player-flags:");
	eq(r, PARSE_ERROR_NONE);
	pr = (struct player_race*) parser_priv(p);
	notnull(pr);
	require(pf_is_empty(pr->pflags));
	/* Try one flag. */
	r = parser_parse(p, "player-flags:BLADE_PROFICIENCY");
	eq(r, PARSE_ERROR_NONE);
	pf_wipe(eflags);
	pf_on(eflags, PF_BLADE_PROFICIENCY);
	pr = (struct player_race*) parser_priv(p);
	notnull(pr);
	require(pf_is_equal(pr->pflags, eflags));
	/* Check that multiple player-flags lines concatenate the flags. */
	r = parser_parse(p, "player-flags:AXE_PROFICIENCY");
	eq(r, PARSE_ERROR_NONE);
	pf_on(eflags, PF_AXE_PROFICIENCY);
	pr = (struct player_race*) parser_priv(p);
	notnull(pr);
	require(pf_is_equal(pr->pflags, eflags));
	/* Try multiple flags at once. */
	pf_wipe(pr->pflags);
	r = parser_parse(p, "player-flags:AXE_PROFICIENCY | BLADE_PROFICIENCY");
	eq(r, PARSE_ERROR_NONE);
	pf_wipe(eflags);
	pf_on(eflags, PF_AXE_PROFICIENCY);
	pf_on(eflags, PF_BLADE_PROFICIENCY);
	pr = (struct player_race*) parser_priv(p);
	notnull(pr);
	require(pf_is_equal(pr->pflags, eflags));
	ok;
}

static int test_play_flags_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an unrecognized flag. */
	enum parser_error r = parser_parse(p, "player-flags:XYZZY");

	eq(r, PARSE_ERROR_INVALID_FLAG);
	ok;
}

static int test_equip0(void *state) {
	enum parser_error r = parser_parse(state, "equip:sword:1:2:5");
	struct player_race *pr;

	eq(r, PARSE_ERROR_NONE);
	pr = parser_priv(state);
	require(pr);
	eq(pr->start_items[0].tval, TV_SWORD);
	eq(pr->start_items[0].sval, 1);
	eq(pr->start_items[0].min, 2);
	eq(pr->start_items[0].max, 5);
	ok;
}

static int test_equip_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an unrecognized tval name. */
	enum parser_error r = parser_parse(p, "equip:xyzzy:1:1:1");

	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	/* Try invalid minimums or maximums. */
	r = parser_parse(p, "equip:sword:1:1:105");
	eq(r, PARSE_ERROR_INVALID_ITEM_NUMBER);
	r = parser_parse(p, "equip:sword:1:120:1");
	eq(r, PARSE_ERROR_INVALID_ITEM_NUMBER);
	r = parser_parse(p, "equip:sword:1:700:800");
	eq(r, PARSE_ERROR_INVALID_ITEM_NUMBER);

	ok;
}

static int test_desc0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p,
		"desc: The dwarves are stone-hard and stubborn, ");
	struct player_race *pr;

	eq(r, PARSE_ERROR_NONE);
	/* Check that multiple directives are appended. */
	r = parser_parse(p, "desc:fast in friendship and in enmity.");
	eq(r, PARSE_ERROR_NONE);
	pr = (struct player_race*) parser_priv(p);
	notnull(pr);
	notnull(pr->desc);
	require(streq(pr->desc, "The dwarves are stone-hard and stubborn, "
		"fast in friendship and in enmity."));
	ok;
}

const char *suite_name = "parse/p-info";
/*
 * test_missing_record_header0() has to be before test_name0().  All others,
 * except test_name0(), have to be after test_name0().
 */
struct test tests[] = {
	{ "missing_record_header0", test_missing_record_header0 },
	{ "name0", test_name0 },
	{ "stats0", test_stats0 },
	{ "skills0", test_skills0 },
	{ "history0", test_history0 },
	{ "age0", test_age0 },
	{ "height0", test_height0 },
	{ "weight0", test_weight0 },
	{ "player_flags0", test_play_flags0 },
	{ "player_flags_bad0", test_play_flags_bad0 },
	{ "equip0", test_equip0 },
	{ "equip_bad0", test_equip_bad0 },
	{ "desc0", test_desc0 },
	{ NULL, NULL }
};
