/* parse/r-info */

#include "unit-test.h"
#include "unit-test-data.h"
#include "init.h"
#include "monster.h"

int setup_tests(void **state) {
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
	*state = init_parse_monster();
	return !*state;
}

int teardown_tests(void *state) {
	struct monster_race *mr = parser_priv(state);
	struct monster_blow *mb;
	struct monster_altmsg *ma;
	struct blow_method *meth;
	struct blow_effect *eff;

	string_free(mr->name);
	string_free(mr->text);
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


static int test_name0(void *state) {
	enum parser_error r = parser_parse(state, "name:Carcharoth, the Jaws of Thirst");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	require(streq(mr->name, "Carcharoth, the Jaws of Thirst"));
	ok;
}

static int test_base0(void *state) {
	enum parser_error r;
	struct monster_race *mr;

	rb_info = &test_rb_info;
	r = parser_parse(state, "base:townsfolk");
	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	require(streq(mr->base->name, "townsfolk"));
	ok;
}

static int test_color0(void *state) {
	enum parser_error r = parser_parse(state, "color:v");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->d_attr, COLOUR_VIOLET);
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
	enum parser_error r = parser_parse(state, "prot:2:1d4");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
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

static int test_flags0(void *state) {
	enum parser_error r = parser_parse(state, "flags:UNIQUE | MALE");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	require(mr->flags);
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

static int test_spell_power0(void *state) {
	enum parser_error r = parser_parse(state, "spell-power:4");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	eq(mr->spell_power, 4);
	ok;
}

static int test_spells0(void *state) {
	enum parser_error r = parser_parse(state, "spells:BR_DARK | SNG_BIND");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	require(mr->spell_flags);
	ok;
}

static int test_messagevis0(void *state) {
	enum parser_error r = parser_parse(state,
		"message-vis:CONF:{name} grins mischieviously.");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	require(has_alternate_message(mr, RSF_CONF, MON_ALTMSG_SEEN,
		"{name} grins mischieviously."));
	ok;
}

static int test_messagevis_bad0(void *state) {
	enum parser_error r = parser_parse(state,
		"message-vis:XYZZY:{name} waves its tentacles menacingly.");

	eq(r, PARSE_ERROR_INVALID_SPELL_NAME);
	ok;
}

static int test_messageinvis0(void *state) {
	enum parser_error r = parser_parse(state,
		"message-invis:ARROW1");
	struct monster_race *mr;

	eq(r, PARSE_ERROR_NONE);
	mr = parser_priv(state);
	require(mr);
	require(has_alternate_message(mr, RSF_ARROW1, MON_ALTMSG_UNSEEN, ""));
	ok;
}

static int test_messageinvis_bad0(void *state) {
	enum parser_error r = parser_parse(state,
		"message-invis:XYZZY:Something whispers.");

	eq(r, PARSE_ERROR_INVALID_SPELL_NAME);
	ok;
}

const char *suite_name = "parse/r-info";
struct test tests[] = {
	{ "name0", test_name0 },
	{ "color0", test_color0 },
	{ "base0", test_base0 },
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
	{ "flags0", test_flags0 },
	{ "desc0", test_desc0 },
	{ "ranged-freq0", test_ranged_freq0 },
	{ "spell-power0", test_spell_power0 },
	{ "spells0", test_spells0 },
	{ "message-vis0", test_messagevis0 },
	{ "message-vis-bad0", test_messagevis_bad0 },
	{ "message-invis0", test_messageinvis0 },
	{ "message-invis-bad0", test_messageinvis_bad0 },
	{ NULL, NULL }
};
