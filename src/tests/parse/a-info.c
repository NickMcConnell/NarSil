/* parse/a-info */

#include "unit-test.h"
#include "unit-test-data.h"
#include "effects.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-abilities.h"
#include "init.h"
#include "z-color.h"
#include "z-virt.h"

static char orc_slay_name[16] = "ORC_1";
static char spider_slay_name[16] = "SPIDER_1";
static struct slay dummy_slays[] = {
	{ .code = NULL },
	{ .code = orc_slay_name },
	{ .code = spider_slay_name },
};
static char cold_brand_name[16] = "COLD_1";
static char poison_brand_name[16] = "POIS_1";
static struct brand dummy_brands[] = {
	{ .code = NULL },
	{ .code = cold_brand_name },
	{ .code = poison_brand_name },
};
static char dummy_ability_names[5][16] = {
	"Power",
	"Charge",
	"Precision",
	"Versatility",
	"Rapid Fire"
};
static struct ability dummy_abilities[5] = {
	{ .name = dummy_ability_names[0], .skill = SKILL_MELEE },
	{ .name = dummy_ability_names[1], .skill = SKILL_MELEE },
	{ .name = dummy_ability_names[2], .skill = SKILL_ARCHERY },
	{ .name = dummy_ability_names[3], .skill = SKILL_ARCHERY },
	{ .name = dummy_ability_names[4], .skill = SKILL_ARCHERY },
};

int setup_tests(void **state) {
	int i;

	*state = init_parse_artifact();
	/* Do the bare minimum so kind lookups work. */
	z_info = mem_zalloc(sizeof(*z_info));
	z_info->k_max = 1;
	z_info->ordinary_kind_max = 1;
	k_info = mem_zalloc(z_info->k_max * sizeof(*k_info));
	kb_info = mem_zalloc(TV_MAX * sizeof(*kb_info));
	kb_info[TV_LIGHT].tval = TV_LIGHT;
	/* Do minimal setup for testing slay and brand directives. */
	z_info->slay_max = (uint8_t) N_ELEMENTS(dummy_slays);
	slays = dummy_slays;
	z_info->brand_max = (uint8_t) N_ELEMENTS(dummy_brands);
	brands = dummy_brands;
	/* Do minimal setup for testing of the ability directive. */
	for (i = 0; i < (int) N_ELEMENTS(dummy_abilities) - 1; ++i) {
		dummy_abilities[i].next = &dummy_abilities[i + 1];
	}
	dummy_abilities[N_ELEMENTS(dummy_abilities) - 1].next = NULL;
	abilities = dummy_abilities;
	return !*state;
}

int teardown_tests(void *state) {
	struct artifact *a = parser_priv(state);
	int k;

	string_free(a->name);
	string_free(a->text);
	mem_free(a->slays);
	mem_free(a->brands);
	while (a->abilities) {
		struct ability *tgt = a->abilities;

		a->abilities = tgt->next;
		mem_free(tgt);
	}
	mem_free(a);
	for (k = 1; k < z_info->k_max; ++k) {
		struct object_kind *kind = &k_info[k];

		string_free(kind->name);
		string_free(kind->text);
		string_free(kind->effect_msg);
		mem_free(kind->brands);
		mem_free(kind->slays);
		free_effect(kind->effect);
	}
	mem_free(k_info);
	mem_free(kb_info);
	mem_free(z_info);
	parser_destroy(state);
	return 0;
}

static int test_missing_record_header0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "base-object:light:Arkenstone");

	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "color:y");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "graphics:~:y");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "pval:3");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "depth:10");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "rarity:20");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "weight:5");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "cost:50000");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "attack:1:1d5");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "defence:-1:2d4");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "flags:SEE_INVIS | PROT_BLIND | NO_FUEL");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "values:CON[1] | GRA[1]");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "desc:It is a highly magical McGuffin.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "slay:ORC_1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "brand:POIS_1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "ability:Stealth:Disguise");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_name0(void *state) {
	enum parser_error r = parser_parse(state, "name:of Thrain");
	struct artifact *a;

	eq(r, PARSE_ERROR_NONE);
	a = parser_priv(state);
	require(a);
	require(streq(a->name, "of Thrain"));
	ok;
}

static int test_badtval0(void *state) {
	enum parser_error r = parser_parse(state, "base-object:badtval:Junk");
	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	ok;
}

static int test_badtval1(void *state) {
	enum parser_error r = parser_parse(state, "base-object:-1:Junk");
	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	ok;
}

static int test_base_object0(void *state) {
	enum parser_error r = parser_parse(state, "base-object:light:Arkenstone");
	struct artifact *a;

	eq(r, PARSE_ERROR_NONE);
	a = parser_priv(state);
	require(a);
	eq(a->tval, TV_LIGHT);
	eq(a->sval, z_info->ordinary_kind_max);
	ok;
}

static int test_color0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "color:y");
	struct artifact *a;
	struct object_kind *k;

	eq(r, PARSE_ERROR_NONE);
	a = (struct artifact*) parser_priv(p);
	notnull(a);
	k = lookup_kind(a->tval, a->sval);
	notnull(k);
	eq(k->d_attr, COLOUR_YELLOW);
	/*
	 * Try with the full name for the color (matching is supposed to be
	 * case insensitive).
	 */
	r = parser_parse(p, "color:White");
	eq(r, PARSE_ERROR_NONE);
	a = (struct artifact*) parser_priv(p);
	notnull(a);
	k = lookup_kind(a->tval, a->sval);
	notnull(k);
	eq(k->d_attr, COLOUR_WHITE);
	r = parser_parse(p, "color:light green");
	eq(r, PARSE_ERROR_NONE);
	a = (struct artifact*) parser_priv(p);
	notnull(a);
	k = lookup_kind(a->tval, a->sval);
	notnull(k);
	eq(k->d_attr, COLOUR_L_GREEN);
	ok;
}

static int test_graphics0(void *state) {
	struct parser *p = (struct parser*) state;
	struct artifact *a = (struct artifact*) parser_priv(p);
	struct object_kind *k;
	enum parser_error r;
	bool kind_changed;

	notnull(a);
	k = lookup_kind(a->tval, a->sval);
	notnull(k);
	if (!kf_has(k->kind_flags, KF_INSTA_ART)) {
		kf_on(k->kind_flags, KF_INSTA_ART);
		kind_changed = true;
	} else {
		kind_changed = false;
	}
	/* Try with a single letter code for the color. */
	r = parser_parse(p, "graphics:&:b");
	eq(r, PARSE_ERROR_NONE);
	eq(k->d_char, L'&');
	eq(k->d_attr, COLOUR_BLUE);
	/*
	 * Try with the full name for the color (matching is supposed to be
	 * case insensitive).
	 */
	r = parser_parse(p, "graphics:~:Yellow");
	eq(r, PARSE_ERROR_NONE);
	eq(k->d_char, L'~');
	eq(k->d_attr, COLOUR_YELLOW);
	r = parser_parse(p, "graphics:+:light green");
	eq(r, PARSE_ERROR_NONE);
	eq(k->d_char, L'+');
	eq(k->d_attr, COLOUR_L_GREEN);
	if (kind_changed) {
		kf_off(k->kind_flags, KF_INSTA_ART);
	}
	ok;
}

static int test_graphics_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	struct artifact *a = (struct artifact*) parser_priv(p);
	struct object_kind *k;
	enum parser_error r;
	bool kind_changed;

	notnull(a);
	k = lookup_kind(a->tval, a->sval);
	notnull(k);
	if (kf_has(k->kind_flags, KF_INSTA_ART)) {
		kf_off(k->kind_flags, KF_INSTA_ART);
		kind_changed = true;
	} else {
		kind_changed = false;
	}
	r = parser_parse(p, "graphics:~:y");
	eq(r, PARSE_ERROR_NOT_SPECIAL_ARTIFACT);
	if (kind_changed) {
		kf_on(k->kind_flags, KF_INSTA_ART);
	}
	ok;
}

static int test_level0(void *state) {
	enum parser_error r = parser_parse(state, "depth:3");
	struct artifact *a;

	eq(r, PARSE_ERROR_NONE);
	a = parser_priv(state);
	require(a);
	eq(a->level, 3);
	ok;
}

static int test_weight0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "weight:8");
	struct artifact *a;
	struct object_kind *k;

	eq(r, PARSE_ERROR_NONE);
	a = (struct artifact*) parser_priv(p);
	notnull(a);
	eq(a->weight, 8);
	k = lookup_kind(a->tval, a->sval);
	notnull(k);
	if (k->kidx >= z_info->ordinary_kind_max) {
		eq(k->weight, 8);
	}
	ok;
}

static int test_cost0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "cost:200");
	struct artifact *a;
	struct object_kind *k;

	eq(r, PARSE_ERROR_NONE);
	a = (struct artifact*) parser_priv(p);
	notnull(a);
	eq(a->cost, 200);
	k = lookup_kind(a->tval, a->sval);
	notnull(k);
	if (k->kidx >= z_info->ordinary_kind_max) {
		eq(k->cost, 200);
	}
	ok;
}

static int test_attack0(void *state) {
	enum parser_error r = parser_parse(state, "attack:2:4d5");
	struct artifact *a;

	eq(r, PARSE_ERROR_NONE);
	a = parser_priv(state);
	require(a);
	eq(a->att, 2);
	eq(a->dd, 4);
	eq(a->ds, 5);
	ok;
}

static int test_defence0(void *state) {
	enum parser_error r = parser_parse(state, "defence:-3:1d7");
	struct artifact *a;

	eq(r, PARSE_ERROR_NONE);
	a = parser_priv(state);
	require(a);
	eq(a->evn, -3);
	eq(a->pd, 1);
	eq(a->ps, 7);
	ok;
}

static int test_flags0(void *state) {
	struct parser *p = (struct parser*) state;
	struct artifact *a = (struct artifact*) parser_priv(p);
	bitflag expflags[OF_SIZE];
	enum parser_error r;
	int i;

	/* Wipe the slate. */
	a = (struct artifact*) parser_priv(p);
	notnull(a);
	of_wipe(a->flags);
	for (i = 0; i < ELEM_MAX; ++i) {
		a->el_info[i].flags = 0;
	}
	/* Try nothing at all. */
	r = parser_parse(p, "flags:");
	eq(r, PARSE_ERROR_NONE);
	/* Try two object flags. */
	r = parser_parse(p, "flags:SEE_INVIS | FREE_ACT");
	eq(r, PARSE_ERROR_NONE);
	/* Try adding a single element flag. */
	r =  parser_parse(p, "flags:HATES_FIRE");
	eq(r, PARSE_ERROR_NONE);
	/* Check that state is correct. */
	of_wipe(expflags);
	of_on(expflags, OF_SEE_INVIS);
	of_on(expflags, OF_FREE_ACT);
	require(of_is_equal(a->flags, expflags));
	for (i = 0; i < ELEM_MAX; ++i) {
		eq(a->el_info[i].flags, ((i == ELEM_FIRE) ? EL_INFO_HATES : 0));
	}
	ok;
}

static int test_flags_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an unrecognized flag. */
	enum parser_error r = parser_parse(p, "flags:XYZZY");

	eq(r, PARSE_ERROR_INVALID_FLAG);
	/* Try an unrecognized element. */
	r = parser_parse(p, "flags:HATES_XYZZY");
	eq(r, PARSE_ERROR_INVALID_FLAG);
	r = parser_parse(p, "flags:IGNORE_XYZZY");
	eq(r, PARSE_ERROR_INVALID_FLAG);
	ok;
}

static int test_values0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "values:STR[1] | CON[1]");
	struct artifact *a;

	eq(r, PARSE_ERROR_NONE);
	a = (struct artifact*) parser_priv(p);
	notnull(a);
	eq(a->modifiers[0], 1);
	eq(a->modifiers[2], 1);
	ok;
}

static int test_values_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an unrecognized object modifier. */
	enum parser_error r = parser_parse(p, "values:XYZZY[-4]");

	eq(r, PARSE_ERROR_INVALID_VALUE);
	ok;
}

static int test_desc0(void *state) {
	enum parser_error r = parser_parse(state, "desc:baz");
	struct artifact *a;

	eq(r, 0);
	r = parser_parse(state, "desc: quxx");
	eq(r, 0);
	a = parser_priv(state);
	notnull(a);
	notnull(a->text);
	require(streq(a->text, "baz quxx"));
	ok;
}

static int test_slay0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "slay:SPIDER_1");
	struct artifact *a;

	eq(r, PARSE_ERROR_NONE);
	a = (struct artifact*) parser_priv(p);
	notnull(a);
	notnull(a->slays);
	eq(a->slays[0], false);
	eq(a->slays[1], false);
	eq(a->slays[2], true);
	ok;
}

static int test_slay_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "slay:XYZZY");

	eq(r, PARSE_ERROR_UNRECOGNISED_SLAY);
	ok;
}

static int test_brand0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "brand:COLD_1");
	struct artifact *a;

	eq(r, PARSE_ERROR_NONE);
	a = (struct artifact*) parser_priv(p);
	notnull(a);
	notnull(a->brands);
	eq(a->brands[0], false);
	eq(a->brands[1], true);
	eq(a->brands[2], false);
	ok;
}

static int test_brand_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "brand:XYZZY");

	eq(r, PARSE_ERROR_UNRECOGNISED_BRAND);
	ok;
}

static int test_ability0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "ability:Melee:Power");
	struct artifact *a;

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "ability:Archery:Rapid Fire");
	eq(r, PARSE_ERROR_NONE);
	a = (struct artifact*) parser_priv(p);
	notnull(a);
	notnull(a->abilities);
	notnull(a->abilities->name);
	require(streq(a->abilities->name, "Rapid Fire"));
	eq(a->abilities->skill, SKILL_ARCHERY);
	notnull(a->abilities->next);
	notnull(a->abilities->next->name);
	require(streq(a->abilities->next->name, "Power"));
	eq(a->abilities->next->skill, SKILL_MELEE);
	ok;
}

static int test_ability_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try with a valid skill but invalid ability. */
	enum parser_error r = parser_parse(p, "ability:Melee:Xyzzy");

	eq(r, PARSE_ERROR_INVALID_ABILITY);
	/* Try with an invalid skill but valid ability. */
	r = parser_parse(p, "ability:Xyzzy:Charge");
	eq(r, PARSE_ERROR_INVALID_SKILL);
	/* Try with an invalid skill and invalid ability. */
	r = parser_parse(p, "ability:Xyzzy:Xyzzy");
	eq(r, PARSE_ERROR_INVALID_SKILL);
	ok;
}

const char *suite_name = "parse/a-info";
/* test_missing_record_header0() has to be before test_name0(). */
struct test tests[] = {
	{ "missing_record_header0", test_missing_record_header0 },
	{ "name0", test_name0 },
	{ "badtval0", test_badtval0 },
	{ "badtval1", test_badtval1 },
	{ "base-object0", test_base_object0 },
	{ "color0", test_color0 },
	{ "graphics0", test_graphics0 },
	{ "graphics_bad0", test_graphics_bad0 },
	{ "level0", test_level0 },
	{ "weight0", test_weight0 },
	{ "cost0", test_cost0 },
	{ "attack0", test_attack0 },
	{ "defence0", test_defence0 },
	{ "flags0", test_flags0 },
	{ "flags_bad0", test_flags_bad0 },
	{ "desc0", test_desc0 },
	{ "values0", test_values0 },
	{ "values_bad0", test_values_bad0 },
	{ "slay0", test_slay0 },
	{ "slay_bad0", test_slay_bad0 },
	{ "brand0", test_brand0 },
	{ "brand_bad0", test_brand_bad0 },
	{ "ability0", test_ability0 },
	{ "ability_bad0", test_ability_bad0 },
	{ NULL, NULL }
};
