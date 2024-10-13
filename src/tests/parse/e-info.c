/* parse/e-info */

#include "unit-test.h"
#include "unit-test-data.h"
#include "init.h"
#include "obj-init.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "object.h"
#include "player-abilities.h"
#include "z-form.h"
#include "z-virt.h"

static char dummy_cloak_name[16] = "& Cloak~";
static char dummy_fur_cloak_name[16] = "& Fur Cloak~";
static char dummy_dagger_name[16] = "& Dagger~";
static char dummy_rapier_name[16] = "& Rapier~";
static char dummy_skullcap_name[16] = "& Skullcap~";
static char dummy_helm_name[16] = "& Steel Helm~";
static struct object_kind dummy_kinds[] = {
	{ .name = dummy_cloak_name, .kidx = 0, .tval = TV_CLOAK, .sval = 1 },
	{ .name = dummy_fur_cloak_name, .kidx = 1, .tval = TV_CLOAK, .sval = 2 },
	{ .name = dummy_dagger_name, .kidx = 2, .tval = TV_SWORD, .sval = 1 },
	{ .name = dummy_rapier_name, .kidx = 3, .tval = TV_SWORD, .sval = 2 },
	{ .name = dummy_skullcap_name, .kidx = 4, .tval = TV_HELM, .sval = 1 },
	{ .name = dummy_helm_name, .kidx = 5, .tval = TV_HELM, .sval = 2 },
};
static char dummy_slay_1[16] = "ORC_1";
static char dummy_slay_2[16] = "SPIDER_1";
static struct slay dummy_slays[] = {
	{ .code = NULL },
	{ .code = dummy_slay_1 },
	{ .code = dummy_slay_2 },
};
static char dummy_brand_1[16] = "COLD_1";
static char dummy_brand_2[16] = "POIS_1";
static struct brand dummy_brands[] = {
	{ .code = NULL },
	{ .code = dummy_brand_1 },
	{ .code = dummy_brand_2 },
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

	*state = ego_parser.init();
	/*
	 * Do minimal setup for adding of slays and brands and for kind lookup.
	 * z_info is also used by ego_parser.finish.
	 */
	z_info = mem_zalloc(sizeof(*z_info));
	z_info->k_max = (uint16_t) N_ELEMENTS(dummy_kinds);
	k_info = dummy_kinds;
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
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (ego_parser.finish(p)) {
		r = 1;
	}
	ego_parser.cleanup();
	mem_free(z_info);
	return r;
}

static bool has_all_of_tval(const struct poss_item *p, int tval, bool only) {
	bool *marked = mem_zalloc(z_info->k_max * sizeof(*marked));
	bool valid = true;
	unsigned int i;

	while (p) {
		if (p->kidx >= z_info->k_max) {
			valid = false;
			break;
		}
		if (k_info[p->kidx].tval == tval) {
			marked[p->kidx] = true;
		} else if (only) {
			valid = false;
			break;
		}
		p = p->next;
	}
	for (i = 0; i < z_info->k_max; ++i) {
		if (k_info[i].tval == tval && !marked[i]) {
			valid = false;
			break;
		}
	}
	mem_free(marked);
	return valid;
}

static int test_missing_record_header0(void *state) {
	struct parser *p = (struct parser*) state;
	struct ego_item *e = (struct ego_item*) parser_priv(p);
	enum parser_error r;

	null(e);
	r = parser_parse(p, "alloc:40:10 to 100");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "cost:1000");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "max-attack:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "dam-dice:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "dam-sides:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "max-evasion:2");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "prot-dice:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "prot-sides:2");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "max-pval:3");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "type:sword");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "item:helm:Skullcap");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "flags:SUST_STR | IGNORE_FIRE");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "values:GRA[1] | DEX[1]");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "slay:ORC_1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "brand:POIS_1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "ability:Will:Majesty");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:of Resist Lightning");
	struct ego_item *e;
	int i;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	notnull(e->name);
	require(streq(e->name, "of Resist Lightning"));
	eq(e->cost, 0);
	require(of_is_empty(e->flags));
	require(kf_is_empty(e->kind_flags));
	for (i = 0; i < OBJ_MOD_MAX; ++i) {
		eq(e->modifiers[i], 0);
	}
	for (i = 0; i < ELEM_MAX; ++i) {
		eq(e->el_info[i].flags, 0);
		eq(e->el_info[i].res_level, 0);
	}
	null(e->brands);
	null(e->slays);
	eq(e->rarity, 0);
	eq(e->level, 0);
	eq(e->alloc_max, 0);
	null(e->poss_items);
	null(e->abilities);
	eq(e->att, 0);
	eq(e->dd, 0);
	eq(e->ds, 0);
	eq(e->evn, 0);
	eq(e->pd, 0);
	eq(e->ps, 0);
	eq(e->pval, 0);
	eq(e->aware, false);
	eq(e->everseen, false);
	ok;
}

static int test_alloc0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "alloc:40:10 to 100");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	eq(e->rarity, 40);
	eq(e->level, 10);
	eq(e->alloc_max, 100);
	ok;
}

static int test_alloc_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try with a mismatching string as the second parameter. */
	enum parser_error r = parser_parse(p, "alloc:40:10 100");

	eq(r, PARSE_ERROR_INVALID_ALLOCATION);
	/* Try with allocation ranges that are out of bounds. */
	r = parser_parse(p, "alloc:40:-1 to 100");
	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	r = parser_parse(p, "alloc:40:0 to 290");
	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	r = parser_parse(p, "alloc:40:370 to 40");
	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	r = parser_parse(p, "alloc:40:30 to -7");
	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	r = parser_parse(p, "alloc:40:-70 to -3");
	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	r = parser_parse(p, "alloc:40:-10 to 371");
	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	r = parser_parse(p, "alloc:40:268 to 500");
	eq(r, PARSE_ERROR_OUT_OF_BOUNDS);
	/* Check missing whitespace. */
	r = parser_parse(p, "alloc:40:2to 7");
	eq(r, PARSE_ERROR_INVALID_ALLOCATION);
	r = parser_parse(p, "alloc:40:2 to7");
	eq(r, PARSE_ERROR_INVALID_ALLOCATION);
	/* Check when either integer is invalid or out of range. */
	r = parser_parse(p, "alloc:40:a to 7");
	eq(r, PARSE_ERROR_INVALID_ALLOCATION);
	r = parser_parse(p, "alloc:40:2 to b");
	eq(r, PARSE_ERROR_INVALID_ALLOCATION);
	r = parser_parse(p, "alloc:40:-989999988989898889389 to 1");
	eq(r, PARSE_ERROR_INVALID_ALLOCATION);
	r = parser_parse(p, "alloc:40:1 to 3892867393957396729696739023");
	eq(r, PARSE_ERROR_INVALID_ALLOCATION);
	/* Check an invalid separating string. */
	r = parser_parse(p, "alloc:40:2 x 7");
	eq(r, PARSE_ERROR_INVALID_ALLOCATION);
	r = parser_parse(p, "alloc:40:2 sto 7");
	eq(r, PARSE_ERROR_INVALID_ALLOCATION);
	r = parser_parse(p, "alloc:40:2 top 7");
	eq(r, PARSE_ERROR_INVALID_ALLOCATION);
	ok;
}

static int test_cost0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "cost:1000");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	eq(e->cost, 1000);
	ok;
}

static int test_attack0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "max-attack:6");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	eq(e->att, 6);
	ok;
}

static int test_dam_dice0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "dam-dice:1");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	eq(e->dd, 1);
	ok;
}

static int test_dam_sides0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "dam-sides:2");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	eq(e->ds, 2);
	ok;
}

static int test_evasion0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "max-evasion:3");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	eq(e->evn, 3);
	ok;
}

static int test_prot_dice0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "prot-dice:1");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	eq(e->pd, 1);
	ok;
}

static int test_prot_sides0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "prot-sides:4");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	eq(e->ps, 4);
	ok;
}

static int test_max_pval0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "max-pval:2");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	eq(e->pval, 2);
	ok;
}

static int test_type0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "type:sword");
	struct ego_item *e;
	bool has_all;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	has_all = has_all_of_tval(e->poss_items, TV_SWORD, false);
	require(has_all);
	ok;
}

static int test_item0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "item:helm:Skullcap");
	char buffer[40];
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	notnull(e->poss_items);
	require(e->poss_items->kidx < z_info->k_max);
	eq(k_info[e->poss_items->kidx].tval, TV_HELM);
	eq(k_info[e->poss_items->kidx].sval, 1);
	/* Check that lookup by index works. */
	strnfmt(buffer, sizeof(buffer), "item:%d:Rapier", TV_SWORD);
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	notnull(e->poss_items);
	require(e->poss_items->kidx < z_info->k_max);
	eq(k_info[e->poss_items->kidx].tval, TV_SWORD);
	eq(k_info[e->poss_items->kidx].sval, 2);
	r = parser_parse(p, "item:helm:1");
	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	notnull(e->poss_items);
	require(e->poss_items->kidx < z_info->k_max);
	eq(k_info[e->poss_items->kidx].tval, TV_HELM);
	eq(k_info[e->poss_items->kidx].sval, 1);
	strnfmt(buffer, sizeof(buffer), "item:%d:2", TV_SWORD);
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	notnull(e->poss_items);
	require(e->poss_items->kidx < z_info->k_max);
	eq(k_info[e->poss_items->kidx].tval, TV_SWORD);
	eq(k_info[e->poss_items->kidx].sval, 2);
	ok;
}

static int test_item_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an unrecognized tval. */
	enum parser_error r = parser_parse(p, "item:xyzzy:Dagger");

	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	/* Try a valid tval but with an sval that isn't in it. */
	r = parser_parse(p, "item:sword:Skullcap");
	eq(r, PARSE_ERROR_UNRECOGNISED_SVAL);
	ok;
}

static int test_type_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check for an unrecognized tval. */
	enum parser_error r = parser_parse(p, "type:xyzzy");

	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	r = parser_parse(p, "type:light");
	eq(r, PARSE_ERROR_NO_KIND_FOR_EGO_TYPE);
	ok;
}

static int test_flags0(void *state) {
	struct parser *p = (struct parser*) state;
	struct ego_item *e = (struct ego_item*) parser_priv(p);
	bitflag eflags[MAX(OF_SIZE, KF_SIZE)];
	enum parser_error r;
	int i;

	notnull(e);
	of_wipe(e->flags);
	kf_wipe(e->kind_flags);
	for (i = 0; i < ELEM_MAX; ++i) {
		e->el_info[i].flags = 0;
	}
	/* Verify that no flags works. */
	r = parser_parse(p, "flags:");
	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	require(of_is_empty(e->flags));
	require(kf_is_empty(e->flags));
	for (i = 0; i < ELEM_MAX; ++i) {
		eq(e->el_info[i].flags, 0);
	}
	/* Try an object flag. */
	r = parser_parse(p, "flags:SEE_INVIS");
	eq(r, PARSE_ERROR_NONE);
	/* Try a kind flag and an element flag. */
	r = parser_parse(p, "flags:GOOD | IGNORE_FIRE");
	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	of_wipe(eflags);
	of_on(eflags, OF_SEE_INVIS);
	require(of_is_equal(e->flags, eflags));
	kf_wipe(eflags);
	kf_on(eflags, KF_GOOD);
	require(kf_is_equal(e->kind_flags, eflags));
	for (i = 0; i < ELEM_MAX; ++i) {
		eq(e->el_info[i].flags, (i == ELEM_FIRE) ? EL_INFO_IGNORE : 0);
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
	struct ego_item *e = (struct ego_item*) parser_priv(p);
	enum parser_error r;
	int i;

	notnull(e);
	/* Clear any prior settings. */
	for (i = 0; i < OBJ_MOD_MAX; ++i) {
		e->modifiers[i] = 0;
	}
	for (i = 0; i < ELEM_MAX; ++i) {
		e->el_info[i].res_level = 0;
	}
	/* Try setting one object modifier. */
	r = parser_parse(p, "values:STEALTH[2]");
	eq(r, PARSE_ERROR_NONE);
	/* Try setting an object modifier and a resistance. */
	r = parser_parse(p, "values:GRA[1] RES_POIS[-1]");
	eq(r, PARSE_ERROR_NONE);
	/* Check the state. */
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	for (i = 0; i < OBJ_MOD_MAX; ++i) {
		if (i == OBJ_MOD_GRA) {
			eq(e->modifiers[i], 1);
		} else if (i == OBJ_MOD_STEALTH) {
			eq(e->modifiers[i], 2);
		} else {
			eq(e->modifiers[i], 0);
		}
	}
	for (i = 0; i < ELEM_MAX; ++i) {
		eq(e->el_info[i].res_level, ((i == ELEM_POIS) ? -1 : 0));
	}
	ok;
}

static int test_values_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an unrecognized object modifier. */
	enum parser_error r = parser_parse(p, "values:XYZZY[2]");

	eq(r, PARSE_ERROR_INVALID_VALUE);
	/* Try an unrecognized element. */
	r = parser_parse(p, "values:RES_XYZZY[3]");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	/* Check handling of a missing opening bracket. */
	r = parser_parse(p, "values:STEALTH1]");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	r = parser_parse(p, "values:RES_POIS1]");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	/* Check handling of missing closing bracket. */
	r = parser_parse(p, "values:STEALTH[1");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	r = parser_parse(p, "values:RES_POIS[1");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	ok;
}

static int test_slay0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "slay:SPIDER_1");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	notnull(e->slays);
	eq(e->slays[0], false);
	eq(e->slays[1], false);
	eq(e->slays[2], true);
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
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	notnull(e->brands);
	eq(e->brands[0], false);
	eq(e->brands[1], true);
	eq(e->brands[2], false);
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
	enum parser_error r = parser_parse(p, "ability:Melee:Charge");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "ability:Archery:Precision");
	e = (struct ego_item*) parser_priv(p);
	notnull(e);
	notnull(e->abilities);
	notnull(e->abilities->name);
	require(streq(e->abilities->name, "Precision"));
	eq(e->abilities->skill, SKILL_ARCHERY);
	notnull(e->abilities->next);
	notnull(e->abilities->next->name);
	require(streq(e->abilities->next->name, "Charge"));
	eq(e->abilities->next->skill, SKILL_MELEE);
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

const char *suite_name = "parse/e-info";
/*
 * test_missing_record_header0() has to be before test_name0().
 * All others except test_name0() have to be after test_name0().
 */
struct test tests[] = {
	{ "missing_record_header0", test_missing_record_header0 },
	{ "name0", test_name0 },
	{ "alloc0", test_alloc0 },
	{ "alloc_bad0", test_alloc_bad0 },
	{ "cost0", test_cost0 },
	{ "attack0", test_attack0 },
	{ "dam_dice0", test_dam_dice0 },
	{ "dam_sides0", test_dam_sides0 },
	{ "evasion0", test_evasion0 },
	{ "prot_dice0", test_prot_dice0 },
	{ "prot_sides0", test_prot_sides0 },
	{ "max_pval", test_max_pval0 },
	{ "type0", test_type0 },
	{ "type_bad0", test_type_bad0 },
	{ "item0", test_item0 },
	{ "item_bad0", test_item_bad0 },
	{ "flags0", test_flags0 },
	{ "flags_bad0", test_flags_bad0 },
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
