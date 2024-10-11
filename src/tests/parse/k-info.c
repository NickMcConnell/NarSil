/* parse/k-info */

#include "unit-test.h"
#include "unit-test-data.h"

#include "effects.h"
#include "init.h"
#include "object.h"
#include "obj-init.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "player-timed.h"
#include "project.h"


static char dummy_orc_slay[16] = "ORC_1";
static struct slay dummy_slays[] = {
	{ .code = NULL },
	{ .code = dummy_orc_slay },
};
static char dummy_cold_brand[16] = "COLD_1";
static struct brand dummy_brands[] = {
	{ .code = NULL },
	{ .code = dummy_cold_brand },
};

int setup_tests(void **state) {
	*state = object_parser.init();
	/* Do the bare minimum so the sval assignment will work. */
	kb_info = mem_zalloc(TV_MAX * sizeof(*kb_info));
	kb_info[TV_FOOD].tval = TV_FOOD;
	/* Do minimal setup for adding of slays and brands. */
	z_info = mem_zalloc(sizeof(*z_info));
	z_info->slay_max = (uint8_t) N_ELEMENTS(dummy_slays);
	slays = dummy_slays;
	z_info->brand_max = (uint8_t) N_ELEMENTS(dummy_brands);
	brands = dummy_brands;
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (object_parser.finish(p)) {
		r = 1;
	}
	object_parser.cleanup();
	mem_free(kb_info);
	mem_free(z_info);
	return r;
}

static int test_missing_record_header0(void *state) {
	struct parser *p = (struct parser*) state;
	struct object_kind *k = (struct object_kind*) parser_priv(p);
	enum parser_error r;

	null(k);
	r = parser_parse(p, "type:TV_FOOD");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "graphics:~:blue");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "depth:10");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "weight:2");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "cost:50");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "attack:-7:6d5");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "defence:-4:2d5");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "alloc:7:3");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "charges:4d2");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "flags:IGNORE_ACID");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "effect:DAMAGE");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "dice:$B+5d8");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "expr:B:PLAYER_WILL: + 0");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "thrown-effect:DAMAGE");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "thrown-dice:-1+1d4");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "thrown-expr:B:PLAYER_WILL: + 0");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "msg:That tastes awful.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "pval:0");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "values:RES_FIRE[-1]");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "desc:This is a pair of well-worn wooden clogs.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "slay:ORC_1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "brand:FIRE_1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "special:-1+m5:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_name0(void *state) {
	errr r = parser_parse(state, "name:Test Object Kind");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	require(streq(k->name, "Test Object Kind"));
	ok;
}

static int test_graphics0(void *state) {
	errr r = parser_parse(state, "graphics:~:red");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	eq(k->d_char, L'~');
	eq(k->d_attr, COLOUR_RED);
	ok;
}

static int test_graphics1(void *state) {
	errr r = parser_parse(state, "graphics:!:W");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	eq(k->d_char, L'!');
	eq(k->d_attr, COLOUR_L_WHITE);
	ok;
}

static int test_type_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "type:xyzzy");
	struct object_kind *k;

	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	k = (struct object_kind*) parser_priv(p);
	notnull(k);
	eq(k->tval, 0);
	eq(k->sval, 0);
	ok;
}

static int test_type0(void *state) {
	int previous_sval = kb_info[TV_FOOD].num_svals;
	errr r = parser_parse(state, "type:food");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	eq(k->tval, TV_FOOD);
	eq(k->sval, previous_sval + 1);
	ok;
}

static int test_level0(void *state) {
	errr r = parser_parse(state, "depth:10");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	eq(k->level, 10);
	ok;
}

static int test_weight0(void *state) {
	errr r = parser_parse(state, "weight:5");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	eq(k->weight, 5);
	ok;
}

static int test_cost0(void *state) {
	errr r = parser_parse(state, "cost:120");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	eq(k->cost, 120);
	ok;
}

static int test_alloc0(void *state) {
	errr r = parser_parse(state, "alloc:3:4");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	eq(k->alloc->locale, 3);
	eq(k->alloc->chance, 4);
	ok;
}

static int test_attack0(void *state) {
	errr r = parser_parse(state, "attack:2:4d8");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	eq(k->att, 2);
	eq(k->dd, 4);
	eq(k->ds, 8);
	ok;
}

static int test_defence0(void *state) {
	errr r = parser_parse(state, "defence:3:7d6");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	eq(k->evn, 3);
	eq(k->pd, 7);
	eq(k->ps, 6);
	ok;
}

static int test_charges0(void *state) {
	errr r = parser_parse(state, "charges:2d8");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	eq(k->charge.dice, 2);
	eq(k->charge.sides, 8);
	ok;
}

static int test_flags0(void *state) {
	errr r = parser_parse(state, "flags:DANGER | GOOD");
	struct object_kind *k;
	int i;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	require(k->flags);
	require(k->kind_flags);
	eq(of_has(k->flags, OF_DANGER), 1);
	eq(of_has(k->flags, OF_SLOW_DIGEST), 0);
	eq(kf_has(k->kind_flags, KF_GOOD), 1);
	eq(kf_has(k->kind_flags, KF_INSTA_ART), 0);
	r = parser_parse(state, "flags:IGNORE_COLD");
	eq(r, 0);
	for (i = 0; i < ELEM_MAX; ++i) {
		eq(k->el_info[i].flags,
			((i == ELEM_COLD) ? EL_INFO_IGNORE : 0));
	}
	ok;
}

static int test_flags_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "flags:XYZZY");

	eq(r, PARSE_ERROR_INVALID_FLAG);
	ok;
}

static int test_missing_effect0(void *state) {
	struct parser *p = (struct parser*) state;
	struct object_kind *k = (struct object_kind*) parser_priv(p);
	enum parser_error r;

	notnull(k);
	null(k->effect);
	/*
	 * Specifying dice without a preceding effect should do nothing and
	 * not flag an error.
	 */
	r = parser_parse(p, "dice:d$S");
	eq(r, PARSE_ERROR_NONE);
	null(k->effect);
	/*
	 * Specifying an expression without a preceding effect should do
	 * nothing and not flag an error.
	 */
	r = parser_parse(p, "expr:S:PLAYER_WILL:+ 0");
	eq(r, PARSE_ERROR_NONE);
	null(k->effect);
	ok;
}

static int test_effect0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check for an effect without subtype, radius, or other. */
	enum parser_error r = parser_parse(p, "effect:DAMAGE");
	struct object_kind *k;
	struct effect *e;

	eq(r, PARSE_ERROR_NONE);
	k = (struct object_kind*) parser_priv(p);
	notnull(k);
	notnull(k->effect);
	e = k->effect;
	while (e->next) e = e->next;
	eq(e->index, EF_DAMAGE);
	null(e->dice);
	eq(e->subtype, 0);
	eq(e->radius, 0);
	eq(e->other, 0);
	null(e->msg);
	/* Check for an effect with a subtype but without a radius or other. */
	r = parser_parse(p, "effect:TIMED_INC:CUT");
	eq(r, PARSE_ERROR_NONE);
	notnull(k->effect);
	e = k->effect;
	while (e->next) e = e->next;
	eq(e->index, EF_TIMED_INC);
	null(e->dice);
	eq(e->subtype, TMD_CUT);
	eq(e->radius, 0);
	eq(e->other, 0);
	null(e->msg);
	/* Check for an effect with a subtype and radius but no other. */
	r = parser_parse(p, "effect:SPOT:ACID:2");
	eq(r, PARSE_ERROR_NONE);
	notnull(k->effect);
	e = k->effect;
	while (e->next) e = e->next;
	eq(e->index, EF_SPOT);
	null(e->dice);
	eq(e->subtype, PROJ_ACID);
	eq(e->radius, 2);
	eq(e->other, 0);
	null(e->msg);
	/* Check for an effect with a subtype, radius, and other. */
	r = parser_parse(p, "effect:BREATH:FIRE:5:30");
	eq(r, PARSE_ERROR_NONE);
	notnull(k->effect);
	e = k->effect;
	while (e->next) e = e->next;
	eq(e->index, EF_BREATH);
	eq(e->subtype, PROJ_FIRE);
	eq(e->radius, 5);
	eq(e->other, 30);
	null(e->msg);
	ok;
}

static int test_effect_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check with unrecognized effect. */
	enum parser_error r = parser_parse(p, "effect:XYZT");

	eq(r, PARSE_ERROR_INVALID_EFFECT);
	/* Check with bad subtype. */
	r = parser_parse(p, "effect:SPHERE:XYZT:3");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	ok;
}

static int test_dice0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up effect. */
	enum parser_error r = parser_parse(p, "effect:BOLT:FIRE");
	struct object_kind *k;
	struct effect *e;

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "dice:5d8");
	eq(r, PARSE_ERROR_NONE);
	k = (struct object_kind*) parser_priv(p);
	notnull(k);
	notnull(k->effect);
	e = k->effect;
	while (e->next) e = e->next;
	eq(dice_test_values(e->dice, 0, 5, 8, 0), true);
	/* Try setting again to see if memory is leaked. */
	r = parser_parse(p, "dice:3+4d6");
	eq(r, PARSE_ERROR_NONE);
	notnull(e->dice);
	eq(dice_test_values(e->dice, 3, 4, 6, 0), true);
	ok;
}

static int test_dice_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up effect. */
	enum parser_error r = parser_parse(p, "effect:SPOT:DARK:3:10");

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "dice:d6+d8");
	eq(r, PARSE_ERROR_INVALID_DICE);
	ok;
}

static int test_missing_dice0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up effect without dice. */
	enum parser_error r = parser_parse(p, "effect:TIMED_INC:SINVIS");

	eq(r, PARSE_ERROR_NONE);
	/*
	 * Specifying an expression without preceding dice should do nothing
	 * and not flag an error.
	 */
	r = parser_parse(p, "expr:B:DUNGEON_LEVEL:/ 6 + 1");
	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_expr0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up effect with dice. */
	enum parser_error r = parser_parse(p, "effect:BREATH:40");

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "dice:$B");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "expr:B:PLAYER_HP:/ 50 + 15");
	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_expr_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up effet with dice. */
	enum parser_error r = parser_parse(p, "effect:TIMED_INC:OPP_FIRE");

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "dice:20+$Ad4");
	eq(r, PARSE_ERROR_NONE);
	/* Try an expression with an invalid operations string. */
	r = parser_parse(p, "expr:A:DUNGEON_LEVEL:+ ( PLAYER_HP / 100 )");
	eq(r, PARSE_ERROR_BAD_EXPRESSION_STRING);
	/* Try to bind an expression to a variable that isn't in the dice. */
	r = parser_parse(p, "expr:B:DUNGEON_LEVEL:/ 8 + 1");
	eq(r, PARSE_ERROR_UNBOUND_EXPRESSION);
	ok;
}

static int test_thrown_missing_effect0(void *state) {
	struct parser *p = (struct parser*) state;
	struct object_kind *k = (struct object_kind*) parser_priv(p);
	enum parser_error r;

	notnull(k);
	null(k->thrown_effect);
	/*
	 * Specifying dice without a preceding effect should do nothing and
	 * not flag an error.
	 */
	r = parser_parse(p, "thrown-dice:d$T");
	eq(r, PARSE_ERROR_NONE);
	null(k->thrown_effect);
	/*
	 * Specifying an expression without a preceding effect should do
	 * nothing and not flag an error.
	 */
	r = parser_parse(p, "thrown-expr:S:PLAYER_WILL:* 2");
	eq(r, PARSE_ERROR_NONE);
	null(k->thrown_effect);
	ok;
}

static int test_thrown_effect0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check for an effect without subtype, radius, or other. */
	enum parser_error r = parser_parse(p, "thrown-effect:DAMAGE");
	struct object_kind *k;
	struct effect *e;

	eq(r, PARSE_ERROR_NONE);
	k = (struct object_kind*) parser_priv(p);
	notnull(k);
	notnull(k->thrown_effect);
	e = k->thrown_effect;
	while (e->next) e = e->next;
	eq(e->index, EF_DAMAGE);
	null(e->dice);
	eq(e->subtype, 0);
	eq(e->radius, 0);
	eq(e->other, 0);
	null(e->msg);
	/* Check for an effect with a subtype but without a radius or other. */
	r = parser_parse(p, "thrown-effect:TIMED_INC:CUT");
	eq(r, PARSE_ERROR_NONE);
	notnull(k->thrown_effect);
	e = k->thrown_effect;
	while (e->next) e = e->next;
	eq(e->index, EF_TIMED_INC);
	null(e->dice);
	eq(e->subtype, TMD_CUT);
	eq(e->radius, 0);
	eq(e->other, 0);
	null(e->msg);
	/* Check for an effect with a subtype and radius but no other. */
	r = parser_parse(p, "thrown-effect:SPOT:ACID:2");
	eq(r, PARSE_ERROR_NONE);
	notnull(k->thrown_effect);
	e = k->thrown_effect;
	while (e->next) e = e->next;
	eq(e->index, EF_SPOT);
	null(e->dice);
	eq(e->subtype, PROJ_ACID);
	eq(e->radius, 2);
	eq(e->other, 0);
	null(e->msg);
	/* Check for an effect with a subtype, radius, and other. */
	r = parser_parse(p, "thrown-effect:BREATH:FIRE:5:30");
	eq(r, PARSE_ERROR_NONE);
	notnull(k->thrown_effect);
	e = k->thrown_effect;
	while (e->next) e = e->next;
	eq(e->index, EF_BREATH);
	eq(e->subtype, PROJ_FIRE);
	eq(e->radius, 5);
	eq(e->other, 30);
	null(e->msg);
	ok;
}

static int test_thrown_effect_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check with unrecognized effect. */
	enum parser_error r = parser_parse(p, "thrown-effect:XYZT");

	eq(r, PARSE_ERROR_INVALID_EFFECT);
	/* Check with bad subtype. */
	r = parser_parse(p, "thrown-effect:SPOT:XYZT:3");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	ok;
}

static int test_thrown_dice0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up effect. */
	enum parser_error r = parser_parse(p, "thrown-effect:BOLT:FIRE");
	struct object_kind *k;
	struct effect *e;

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "thrown-dice:6d7");
	eq(r, PARSE_ERROR_NONE);
	k = (struct object_kind*) parser_priv(p);
	notnull(k);
	notnull(k->thrown_effect);
	e = k->thrown_effect;
	while (e->next) e = e->next;
	eq(dice_test_values(e->dice, 0, 6, 7, 0), true);
	/* Try setting again to see if memory is leaked. */
	r = parser_parse(p, "thrown-dice:2+5d3");
	eq(r, PARSE_ERROR_NONE);
	notnull(e->dice);
	eq(dice_test_values(e->dice, 2, 5, 3, 0), true);
	ok;
}

static int test_thrown_dice_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up effect. */
	enum parser_error r = parser_parse(p, "thrown-effect:SPOT:DARK:3:10");

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "thrown-dice:d4+d2");
	eq(r, PARSE_ERROR_INVALID_DICE);
	ok;
}

static int test_thrown_missing_dice0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up effect without dice. */
	enum parser_error r = parser_parse(p, "thrown-effect:TIMED_INC:SINVIS");

	eq(r, PARSE_ERROR_NONE);
	/*
	 * Specifying an expression without preceding dice should do nothing
	 * and not flag an error.
	 */
	r = parser_parse(p, "thrown-expr:B:DUNGEON_LEVEL:/ 6 + 1");
	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_thrown_expr0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up effect with dice. */
	enum parser_error r = parser_parse(p, "thrown-effect:BREATH:40");

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "thrown-dice:$B");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "thrown-expr:B:PLAYER_HP:/ 50 + 15");
	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_thrown_expr_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Set up effet with dice. */
	enum parser_error r =
		parser_parse(p, "thrown-effect:TIMED_INC:OPP_FIRE");

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "thrown-dice:20+$Ad4");
	eq(r, PARSE_ERROR_NONE);
	/* Try an expression with an invalid operations string. */
	r = parser_parse(p,
		"thrown-expr:A:DUNGEON_LEVEL:+ ( PLAYER_HP / 100 )");
	eq(r, PARSE_ERROR_BAD_EXPRESSION_STRING);
	/* Try to bind an expression to a variable that isn't in the dice. */
	r = parser_parse(p, "thrown-expr:T:DUNGEON_LEVEL:/ 8 + 1");
	eq(r, PARSE_ERROR_UNBOUND_EXPRESSION);
	ok;
}

static int test_msg0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r =
		parser_parse(p, "msg:It feels warm to the touch.");
	struct object_kind *k;

	eq(r, PARSE_ERROR_NONE);
	k = (struct object_kind*) parser_priv(p);
	notnull(k);
	notnull(k->effect_msg);
	require(streq(k->effect_msg, "It feels warm to the touch."));
	/* Check that multiple directives are concatenated. */
	r = parser_parse(p, "msg: And gives off an incredible stench.");
	eq(r, PARSE_ERROR_NONE);
	notnull(k->effect_msg);
	require(streq(k->effect_msg, "It feels warm to the touch. And gives "
		"off an incredible stench."));
	ok;
}

static int test_pval0(void *state) {
	errr r = parser_parse(state, "pval:1");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	eq(k->pval, 1);
	ok;
}

static int test_values0(void *state) {
	struct parser *p = (struct parser*) state;
	struct object_kind *k = (struct object_kind*) parser_priv(p);
	enum parser_error r;
	int i;

	notnull(k);
	/* Clear anything previously set. */
	for (i = 0; i < OBJ_MOD_MAX; ++i) {
		k->modifiers[i].base = 0;
		k->modifiers[i].dice = 0;
		k->modifiers[i].sides = 0;
		k->modifiers[i].m_bonus = 0;
	}
	for (i = 0; i < ELEM_MAX; ++i) {
		k->el_info[i].res_level = 0;
	}
	/* Try setting an object modifier. */
	r = parser_parse(p, "values:STEALTH[-5]");
	eq(r, PARSE_ERROR_NONE);
	/* Try setting both a resistance and an object modifier. */
	r = parser_parse(p, "values:RES_FIRE[1] | DEX[1+1d2]");
	eq(r, PARSE_ERROR_NONE);
	for (i = 0; i < OBJ_MOD_MAX; ++i) {
		if (i == OBJ_MOD_STEALTH) {
			eq(k->modifiers[i].base, -5);
			eq(k->modifiers[i].dice, 0);
			eq(k->modifiers[i].sides, 0);
			eq(k->modifiers[i].m_bonus, 0);
		} else if (i == OBJ_MOD_DEX) {
			eq(k->modifiers[i].base, 1);
			eq(k->modifiers[i].dice, 1);
			eq(k->modifiers[i].sides, 2);
			eq(k->modifiers[i].m_bonus, 0);
		} else {
			eq(k->modifiers[i].base, 0);
			eq(k->modifiers[i].dice, 0);
			eq(k->modifiers[i].sides, 0);
			eq(k->modifiers[i].m_bonus, 0);
		}
	}
	for (i = 0; i < ELEM_MAX; ++i) {
		eq(k->el_info[i].res_level, ((i == ELEM_FIRE) ? 1 : 0));
	}
	ok;
}

static int test_values_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check for an invalid object modifier. */
	enum parser_error r = parser_parse(p, "values:XYZZY[8]");

	eq(r, PARSE_ERROR_INVALID_VALUE);
	/* Check for invalid resistance. */
	r = parser_parse(p, "values:RES_XYZZY[-1]");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	ok;
}

static int test_desc0(void *state) {
	errr r = parser_parse(state, "desc:foo bar");
	struct object_kind *k;

	eq(r, 0);
	k = parser_priv(state);
	require(k);
	require(k->text);
	require(streq(k->text, "foo bar"));
	r = parser_parse(state, "desc: baz");
	eq(r, 0);
	ptreq(k, parser_priv(state));
	require(streq(k->text, "foo bar baz"));
	ok;
}

static int test_slay0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "slay:ORC_1");
	struct object_kind *k;

	eq(r, PARSE_ERROR_NONE);
	k = (struct object_kind*) parser_priv(p);
	notnull(k);
	notnull(k->slays);
	eq(k->slays[0], false);
	eq(k->slays[1], true);
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
	struct object_kind *k;

	eq(r, PARSE_ERROR_NONE);
	k = (struct object_kind*) parser_priv(p);
	notnull(k);
	notnull(k->brands);
	eq(k->brands[0], false);
	eq(k->brands[1], true);
	ok;
}

static int test_brand_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "brand:XYZZY");

	eq(r, PARSE_ERROR_UNRECOGNISED_BRAND);
	ok;
}

static int test_special0(void *state) {
	struct parser *p = (struct parser*) state;
	struct object_kind *k = (struct object_kind*) parser_priv(p);
	enum parser_error r;

	notnull(k);
	/* Clear prior values. */
	k->special1.base = 0;
	k->special1.dice = 0;
	k->special1.sides = 0;
	k->special1.m_bonus = 0;
	k->special2 = 0;
	/* Set the random value but not the minimum. */
	r = parser_parse(p, "special:1+m14");
	eq(r, PARSE_ERROR_NONE);
	k = (struct object_kind*) parser_priv(p);
	notnull(k);
	eq(k->special1.base, 1);
	eq(k->special1.dice, 0);
	eq(k->special1.sides, 0);
	eq(k->special1.m_bonus, 14);
	eq(k->special2, 0);
	/* Set the random value and the minimum. */
	r = parser_parse(p, "special:-3+2d3:1");
	k = (struct object_kind*) parser_priv(p);
	notnull(k);
	eq(k->special1.base, -3);
	eq(k->special1.dice, 2);
	eq(k->special1.sides, 3);
	eq(k->special1.m_bonus, 0);
	eq(k->special2, 1);
	ok;
}

static int test_special_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try with an invalid dice string. */
	enum parser_error r = parser_parse(p, "special:5+$A*2");

	eq(r, PARSE_ERROR_NOT_RANDOM);
	ok;
}

const char *suite_name = "parse/k-info";
/*
 * test_missing_record_header0() has to be before test_name0().
 * test_type_bad0() has to be after test_name0() and before test_type0().
 * test_missing_effect0() has to be after test_name0() and before
 * test_effect0(), test_effect_bad0(). test_dice0(), test_dice_bad0(),
 * test_missing_dice0(), test_expr0(), and test_expr_bad0().
 * test_thrown_missing_effect0() has to be after test_name0(), and before
 * test_thrown_effect0(), test_thrown_effect_bad0(), test_thrown_dice0(),
 * test_thrown_dice_bad0(), test_thrown_missing_dice0(), test_thrown_expr0(),
 * and test_thrown_expr_bad0().
 */
struct test tests[] = {
	{ "missing_record_header0", test_missing_record_header0 },
	{ "name0", test_name0 },
	{ "graphics0", test_graphics0 },
	{ "graphics1", test_graphics1 },
	{ "type_bad0", test_type_bad0 },
	{ "type0", test_type0 },
	{ "level0", test_level0 },
	{ "weight0", test_weight0 },
	{ "cost0", test_cost0 },
	{ "alloc0", test_alloc0 },
	{ "attack0", test_attack0 },
	{ "defence0", test_defence0 },
	{ "charges0", test_charges0 },
	{ "flags0", test_flags0 },
	{ "flags_bad0", test_flags_bad0 },
	{ "missing_effect0", test_missing_effect0 },
	{ "effect0", test_effect0 },
	{ "effect_bad0", test_effect_bad0 },
	{ "dice0", test_dice0 },
	{ "dice_bad0", test_dice_bad0 },
	{ "missing_dice0", test_missing_dice0 },
	{ "expr0", test_expr0 },
	{ "expr_bad0", test_expr_bad0 },
	{ "thrown_missing_effect0", test_thrown_missing_effect0 },
	{ "thrown_effect0", test_thrown_effect0 },
	{ "thrown_effect_bad0", test_thrown_effect_bad0 },
	{ "thrown_dice0", test_thrown_dice0 },
	{ "thrown_dice_bad0", test_thrown_dice_bad0 },
	{ "thrown_missing_dice0", test_thrown_missing_dice0 },
	{ "thrown_expr0", test_thrown_expr0 },
	{ "thrown_expr_bad0", test_thrown_expr_bad0 },
	{ "msg0", test_msg0 },
	{ "values0", test_values0 },
	{ "values_bad0", test_values_bad0 },
	{ "slay0", test_slay0 },
	{ "slay_bad0", test_slay_bad0 },
	{ "brand0", test_brand0 },
	{ "brand_bad0", test_brand_bad0 },
	{ "desc0", test_desc0 },
	{ "pval0", test_pval0 },
	{ "special0", test_special0 },
	{ "special_bad0", test_special_bad0 },
	{ NULL, NULL }
};
