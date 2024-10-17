/* parse/r-info */

#include "unit-test.h"
#include "unit-test-data.h"
#include "init.h"
#include "monster.h"
#include "mon-spell.h"
#include "object.h"
#include "obj-util.h"
#include <locale.h>
#include <langinfo.h>

static char dummy_chest_1[24] = "& Small wooden chest~";
static char dummy_chest_2[24] = "& Small iron chest~";
static char dummy_torch[24] = "& Wooden Torch~";
static struct object_kind dummy_kinds[] = {
	{ .name = NULL, .kidx = 0, .tval = 0 },
	{ .name = dummy_chest_1, .kidx = 1, .tval = TV_CHEST, .sval = 1 },
	{ .name = dummy_chest_2, .kidx = 2, .tval = TV_CHEST, .sval = 2 },
	{ .name = dummy_torch, .kidx = 3, .tval = TV_LIGHT, .sval = 1, .next = NULL }
};

static char of_boldog[24] = "of Boldog";
static char of_the_dwarves[24] = "of the Dwarves";
static struct artifact dummy_artifacts[] = {
	{ .name = NULL, .aidx = 0 },
	{ .name = of_boldog, .aidx = 1 },
	{ .name = of_the_dwarves, .aidx = 2, .next = NULL }
};

int setup_tests(void **state) {
	int i;

	z_info = mem_zalloc(sizeof(struct angband_constants));
	z_info->max_sight = 20;
	/*
	 * Initialize just enough of the blow methods and effects so the tests
	 * will work.
	 */
	z_info->blow_methods_max = 3;
	blow_methods = mem_zalloc(z_info->blow_methods_max * sizeof(*blow_methods));
	blow_methods[1].name = string_make("CLAW");
	blow_methods[1].next = &blow_methods[2];
	blow_methods[2].name = string_make("BITE");
	blow_methods[2].next = NULL;
	z_info->blow_effects_max = 2;
	blow_effects = mem_zalloc(z_info->blow_effects_max * sizeof(*blow_effects));
	blow_effects[0].name = string_make("NONE");
	blow_effects[0].next = &blow_effects[1];
	blow_effects[1].name = string_make("FIRE");
	blow_effects[1].next = NULL;
	/* Set up so monster base lookups work. */
	rb_info = &test_rb_info;
	/*
	 * Set up just enough so object and artifact lookups work for the tests.
	 */
	k_info = dummy_kinds;
	z_info->k_max = (uint16_t) N_ELEMENTS(dummy_kinds);
	z_info->ordinary_kind_max = z_info->k_max;
	for (i = (int) N_ELEMENTS(dummy_kinds) - 2; i >= 0; --i) {
		dummy_kinds[i].next = dummy_kinds + i + 1;
	}
	a_info = dummy_artifacts;
	z_info->a_max= (uint16_t) N_ELEMENTS(dummy_artifacts);
	for (i = (int) N_ELEMENTS(dummy_artifacts) - 3; i >= 0; --i) {
		dummy_artifacts[i].next = dummy_artifacts + i + 1;
	}
	*state = init_parse_monster();
	return !*state;
}

int teardown_tests(void *state) {
	struct monster_race *mr = parser_priv(state);
	struct monster_blow *mb;
	struct monster_altmsg *ma;
	struct blow_method *meth;
	struct blow_effect *eff;
	struct monster_drop *md;

	string_free(mr->name);
	string_free(mr->text);
	string_free(mr->plural);
	mb = mr->blow;
	while (mb) {
		struct monster_blow *mbn = mb->next;

		mem_free(mb);
		mb = mbn;
	}
	ma = mr->spell_msgs;
	while (ma) {
		struct monster_altmsg *man = ma->next;

		string_free(ma->message);
		mem_free(ma);
		ma = man;
	}
	md = mr->drops;
	while (md) {
		struct monster_drop *mdn = md->next;

		mem_free(md);
		md = mdn;
	}
	mem_free(mr);
	parser_destroy(state);
	for (eff = blow_effects; eff; eff = eff->next) {
		string_free(eff->effect_type);
		string_free(eff->desc);
		string_free(eff->name);
	}
	mem_free(blow_effects);
	for (meth = &blow_methods[1]; meth; meth = meth->next) {
		struct blow_message *msg = meth->messages;
		string_free(meth->desc);
		while (msg) {
			struct blow_message *next = msg->next;
			string_free(msg->act_msg);
			mem_free(msg);
			msg = next;
		}
		string_free(meth->name);
	}
	mem_free(blow_methods);
	mem_free(z_info);
	return 0;
}

static bool has_alternate_message(const struct monster_race *r, uint16_t s_idx,
		enum monster_altmsg_type msg_type, const char *message)
{
	struct monster_altmsg *am = r->spell_msgs;

	while (1) {
		if (!am) return false;
		if (am->index == s_idx && am->msg_type == msg_type
				&& streq(am->message, message)) return true;
		am = am->next;
	}
}


static int test_missing_header_record0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_race *mr = (struct monster_race*) parser_priv(p);
	enum parser_error r;

	null(mr);
	r = parser_parse(p, "plural:red-hatted elves");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "depth:8");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "rarity:2");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "color:r");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "speed:3");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "health:6d4");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "light:-2");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "sleep:5");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "percept:4");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "stealth:3");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "will:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "song:21");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "prot:2:1d4");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "flags:FRIEND");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "desc:He looks squalid and thoroughly revolting.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "ranged-freq:20");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "spell-power:4");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "spells:SCARE | FORGET");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-vis:FORGET:{name} rings its bell.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-invis:SCARE:Something incants terribly.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "drop:chest:small wooden chest:20:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "drop-artifact:of Boldog");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "color-cycle:fancy:crystal");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_name0(void *state) {
	enum parser_error r = parser_parse(state, "name:Carcharoth, the Jaws of Thirst");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	require(streq(mr->name, "Carcharoth, the Jaws of Thirst"));
	ok;
}

static int test_plural0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check that specifying no plural (i.e. use default) works. */
	enum parser_error r = parser_parse(p, "plural:");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	null(mr->plural);
	/* Check that supplying a plural works. */
	r = parser_parse(p, "plural:red-hatted elves");
	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	notnull(mr->plural);
	require(streq(mr->plural, "red-hatted elves"));
	ok;
}

static int test_base0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "base:townsfolk");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	notnull(mr->base);
	require(streq(mr->base->name, "townsfolk"));
	ok;
}

static int test_base_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an unrecognized monster base. */
	enum parser_error r = parser_parse(p, "base:xyzzy");

	eq(r, PARSE_ERROR_INVALID_MONSTER_BASE);
	ok;
}

static int test_glyph0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "glyph:!");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	eq(mr->d_char, L'!');
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
		eq(mr->d_char, wcs[0]);
	}
	ok;
}

static int test_color0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "color:v");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(p);
	notnull(mr);
	eq(mr->d_attr, COLOUR_VIOLET);
	/* Check that color can be set by the full name. */
	r = parser_parse(p, "color:Light Green");
	eq(r, PARSE_ERROR_NONE);
	eq(mr->d_attr, COLOUR_L_GREEN);
	/* Check that full name matching is case insensitive. */
	r = parser_parse(p, "color:light red");
	eq(r, PARSE_ERROR_NONE);
	eq(mr->d_attr, COLOUR_L_RED);
	ok;
}

static int test_speed0(void *state) {
	enum parser_error r = parser_parse(state, "speed:7");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->speed, 7);
	ok;
}

static int test_hp0(void *state) {
	enum parser_error r = parser_parse(state, "health:5d4");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->hdice, 5);
	eq(mr->hside, 4);
	ok;
}

static int test_percept0(void *state) {
	enum parser_error r = parser_parse(state, "percept:8");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->per, 8);
	ok;
}

static int test_stealth0(void *state) {
	enum parser_error r = parser_parse(state, "stealth:3");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->stl, 3);
	ok;
}

static int test_will0(void *state) {
	enum parser_error r = parser_parse(state, "will:2");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->wil, 2);
	ok;
}

static int test_prot0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try with only an evasion value. */
	enum parser_error r = parser_parse(p, "prot:5");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	eq(mr->evn, 5);
	eq(mr->pd, 0);
	eq(mr->ps, 0);
	/* Try with both evasion and protection dice */
	r = parser_parse(state, "prot:2:1d4");
	eq(r, PARSE_ERROR_NONE);
	eq(mr->evn, 2);
	eq(mr->pd, 1);
	eq(mr->ps, 4);
	ok;
}

static int test_sleep0(void *state) {
	enum parser_error r = parser_parse(state, "sleep:3");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->sleep, 3);
	ok;
}

static int test_song0(void *state) {
	enum parser_error r = parser_parse(state, "song:15");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->song, 15);
	ok;
}

static int test_depth0(void *state) {
	enum parser_error r = parser_parse(state, "depth:42");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->level, 42);
	ok;
}

static int test_rarity0(void *state) {
	enum parser_error r = parser_parse(state, "rarity:11");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->rarity, 11);
	ok;
}

static int test_blow0(void *state) {
	enum parser_error r = parser_parse(state, "blow:CLAW:FIRE:5:9d12");
	struct monster_race *mr;
	struct monster_blow *mb;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	require(mr->blow);
	mb = mr->blow;
	while (mb->next) {
		mb = mb->next;
	}
	require(mb->method && streq(mb->method->name, "CLAW"));
	require(mb->effect && streq(mb->effect->name, "FIRE"));
	eq(mb->dice.base, 5);
	eq(mb->dice.dice, 9);
	eq(mb->dice.sides, 12);
	ok;
}

static int test_blow1(void *state) {
	enum parser_error r = parser_parse(state, "blow:BITE:FIRE:1:6d8");
	struct monster_race *mr;
	struct monster_blow *mb;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	require(mr->blow);
	mb = mr->blow;
	while (mb->next) {
		mb = mb->next;
	}
	require(mb->method && streq(mb->method->name, "BITE"));
	require(mb->effect && streq(mb->effect->name, "FIRE"));
	eq(mb->dice.base, 1);
	eq(mb->dice.dice, 6);
	eq(mb->dice.sides, 8);
	ok;
}

static int test_blow_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an unrecognized type of blow. */
	enum parser_error r = parser_parse(p, "blow:XYZZY:HURT:1:2d4");

	eq(r, PARSE_ERROR_UNRECOGNISED_BLOW);
	/* Try an unrecognized effect. */
	r = parser_parse(p, "blow:BITE:XYZZY:1:2d4");
	eq(r, PARSE_ERROR_INVALID_EFFECT);
	ok;
}

static int test_flags0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_race *mr = (struct monster_race*) parser_priv(p);
	enum parser_error r;
	bitflag eflags[RF_SIZE];

	notnull(mr);
	rf_wipe(mr->flags);
	/* Check that using an empty set of flags works. */
	r = parser_parse(p, "flags:");
	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	require(rf_is_empty(mr->flags));
	/* Check that supplying a single flag works. */
	r = parser_parse(p, "flags:SHORT_SIGHTED");
	eq(r, PARSE_ERROR_NONE);
	/* Check that supplying multiple flags works. */
	r = parser_parse(state, "flags:UNIQUE | MALE");
	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	rf_wipe(eflags);
	rf_on(eflags, RF_SHORT_SIGHTED);
	rf_on(eflags, RF_UNIQUE);
	rf_on(eflags, RF_MALE);
	require(rf_is_equal(mr->flags, eflags));
	ok;
}

static int test_desc0(void *state) {
	enum parser_error r = parser_parse(state, "desc:foo bar ");
	enum parser_error s = parser_parse(state, "desc: baz");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	eq(s, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	require(streq(mr->text, "foo bar  baz"));
	ok;
}

static int test_ranged_freq0(void *state) {
	enum parser_error r = parser_parse(state, "ranged-freq:10");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->freq_ranged, 10);
	ok;
}

static int test_ranged_freq_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check that values outside of 1 to 100 are rejected. */
	enum parser_error r = parser_parse(p, "ranged-freq:0");

	eq(r, PARSE_ERROR_INVALID_SPELL_FREQ);
	r = parser_parse(p, "ranged-freq:-2");
	eq(r, PARSE_ERROR_INVALID_SPELL_FREQ);
	r = parser_parse(p, "ranged-freq:101");
	eq(r, PARSE_ERROR_INVALID_SPELL_FREQ);
	ok;
}

static int test_spell_power0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "spell-power:4");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	eq(mr->spell_power, 4);
	ok;
}

static int test_spells0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_race *mr = (struct monster_race*) parser_priv(p);
	enum parser_error r;
	bitflag eflags[RSF_SIZE];

	notnull(mr);
	rsf_wipe(mr->spell_flags);
	/* Check that one spell works. */
	r = parser_parse(p, "spells:SCARE");
	eq(r, PARSE_ERROR_NONE);
	/* Check that setting multiple spells works. */
	r = parser_parse(state, "spells:BR_DARK | SNG_BIND");
	eq(r, PARSE_ERROR_NONE);
	rsf_wipe(eflags);
	rsf_on(eflags, RSF_SCARE);
	rsf_on(eflags, RSF_BR_DARK);
	rsf_on(eflags, RSF_SNG_BIND);
	require(rsf_is_equal(mr->spell_flags, eflags));
	ok;
}

static int test_messagevis0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check that an empty message works. */
	enum parser_error r = parser_parse(state, "message-vis:CONF");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	require(has_alternate_message(mr, RSF_CONF, MON_ALTMSG_SEEN, ""));
	/* Check with a non-empty message. */
	r = parser_parse(state,
		"message-vis:HOLD:{name} curses malevolently.");
	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	require(has_alternate_message(mr, RSF_HOLD, MON_ALTMSG_SEEN,
		"{name} curses malevolently."));
	ok;
}

static int test_messagevis_bad0(void *state) {
	enum parser_error r = parser_parse(state,
		"message-vis:XYZZY:{name} waves its tentacles menacingly.");

	eq(r, PARSE_ERROR_INVALID_SPELL_NAME);
	ok;
}

static int test_messageinvis0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check that an empty message works. */
	enum parser_error r = parser_parse(p, "message-invis:ARROW1");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	require(has_alternate_message(mr, RSF_ARROW1, MON_ALTMSG_UNSEEN, ""));
	/* Check with a non-empty message. */
	r = parser_parse(p,
		"message-invis:BOULDER:Something grunts forcefully.");
	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	require(has_alternate_message(mr, RSF_BOULDER, MON_ALTMSG_UNSEEN,
		"Something grunts forcefully."));
	ok;
}

static int test_messageinvis_bad0(void *state) {
	enum parser_error r = parser_parse(state,
		"message-invis:XYZZY:Something whispers.");

	eq(r, PARSE_ERROR_INVALID_SPELL_NAME);
	ok;
}

static int test_drop0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "drop:light:wooden torch:10:1d3");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	notnull(mr->drops);
	null(mr->drops->art);
	notnull(mr->drops->kind);
	eq(mr->drops->kind->tval, TV_LIGHT);
	eq(mr->drops->kind->sval, lookup_sval(TV_LIGHT, "wooden torch"));
	eq(mr->drops->percent_chance, 10);
	eq(mr->drops->dice.base, 0);
	eq(mr->drops->dice.dice, 1);
	eq(mr->drops->dice.sides, 3);
	eq(mr->drops->dice.m_bonus, 0);
	ok;
}

static int test_drop_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an unrecognized tval. */
	enum parser_error r =
		parser_parse(p, "drop:xyzzy:small wooden chest:5:1");

	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	/* Try an unrecognized object. */
	r = parser_parse(p, "drop:light:xyzzy:10:1+1d2");
	eq(r, PARSE_ERROR_UNRECOGNISED_SVAL);
	ok;
}

static int test_drop_artifact0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "drop-artifact:of the Dwarves");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = (struct monster_race*) parser_priv(p);
	notnull(mr);
	notnull(mr->drops);
	null(mr->drops->kind);
	notnull(mr->drops->art);
	notnull(mr->drops->art->name);
	require(streq(mr->drops->art->name, "of the Dwarves"));
	eq(mr->drops->percent_chance, 100);
	eq(mr->drops->dice.base, 0);
	eq(mr->drops->dice.dice, 0);
	eq(mr->drops->dice.sides, 0);
	eq(mr->drops->dice.m_bonus, 0);
	ok;
}

static int test_drop_artifact_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "drop-artifact:xyzzy");

	eq(r, PARSE_ERROR_NO_ARTIFACT_NAME);
	ok;
}

const char *suite_name = "parse/r-info";
/*
 * test_missing_header_record0() has to be before test_name0().
 * All others, except test_name0(), have to be after test_name0().
 */
struct test tests[] = {
	{ "missing_header_record0", test_missing_header_record0 },
	{ "name0", test_name0 },
	{ "plural0", test_plural0 },
	{ "base0", test_base0 },
	{ "base_bad0", test_base_bad0 },
	{ "glyph0", test_glyph0 },
	{ "color0", test_color0 },
	{ "speed0", test_speed0 },
	{ "hp0", test_hp0 },
	{ "percept0", test_percept0 },
	{ "stealth0", test_stealth0 },
	{ "will0", test_will0 },
	{ "prot0", test_prot0 },
	{ "sleep0", test_sleep0 },
	{ "song0", test_song0 },
	{ "depth0", test_depth0 },
	{ "rarity0", test_rarity0 },
	{ "blow0", test_blow0 },
	{ "blow1", test_blow1 },
	{ "blow_bad0", test_blow_bad0 },
	{ "flags0", test_flags0 },
	{ "desc0", test_desc0 },
	{ "ranged-freq0", test_ranged_freq0 },
	{ "ranged-freq_bad0", test_ranged_freq_bad0 },
	{ "spell-power0", test_spell_power0 },
	{ "spells0", test_spells0 },
	{ "message-vis0", test_messagevis0 },
	{ "message-vis-bad0", test_messagevis_bad0 },
	{ "message-invis0", test_messageinvis0 },
	{ "message-invis-bad0", test_messageinvis_bad0 },
	{ "drop0", test_drop0 },
	{ "drop_bad0", test_drop_bad0 },
	{ "drop_artifact0", test_drop_artifact0 },
	{ "drop_artifact_bad0", test_drop_artifact_bad0 },
	{ NULL, NULL }
};
