/* parse/drop */
/* Exercise parsing used for drop.txt. */

#include "unit-test.h"
#include "datafile.h"
#include "init.h"
#include "object.h"
#include "obj-init.h"
#include "z-form.h"
#include "z-virt.h"

static char dummy_boots_1[24] = "& Pair~ of Shoes";
static char dummy_boots_2[24] = "& Pair~ of Boots";
static char dummy_boots_3[24] = "& Pair~ of Greaves";
static char dummy_sword_1[24] = "& Broken Sword~";
struct object_kind dummy_kinds[] = {
	{ .name = NULL, .kidx = 0, .tval = 0, .sval = 0 },
	{ .name = dummy_boots_1, .kidx = 1, .tval = TV_BOOTS, .sval = 1 },
	{ .name = dummy_boots_2, .kidx = 2, .tval = TV_BOOTS, .sval = 2 },
	{ .name = dummy_boots_3, .kidx = 3, .tval = TV_BOOTS, .sval = 3 },
	{ .name = dummy_sword_1, .kidx = 4, .tval = TV_SWORD, .sval = 1 }
};

int setup_tests(void **state) {
	*state = drop_parser.init();
	/* Needed by drop_parser.finish and kind lookups. */
	z_info = mem_zalloc(sizeof(*z_info));
	/* Do minimal setup for kind lookups. */
	z_info->k_max = (uint16_t) N_ELEMENTS(dummy_kinds);
	z_info->ordinary_kind_max = z_info->k_max;
	k_info = dummy_kinds;
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	int r = 0;

	if (drop_parser.finish(p)) {
		r = 1;
	}
	drop_parser.cleanup();
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
	struct drop *d = (struct drop*) parser_priv(p);
	enum parser_error r;

	null(d);
	r = parser_parse(p, "chest:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "base:boots");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "not-base:boots");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "item:boots:Pair of Shoes");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:hands");
	struct drop *d;

	eq(r, PARSE_ERROR_NONE);
	d = (struct drop*) parser_priv(p);
	notnull(d);
	notnull(d->name);
	require(streq(d->name, "hands"));
	require(!d->chest);
	null(d->poss);
	null(d->imposs);
	ok;
}

static int test_base0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "base:boots");
	char buffer[40];
	struct drop *d;
	bool has_all;

	eq(r, PARSE_ERROR_NONE);
	d = (struct drop*) parser_priv(p);
	notnull(d);
	has_all = has_all_of_tval(d->poss, TV_BOOTS, false);
	require(has_all);
	/* Check that lookup by index works. */
	strnfmt(buffer, sizeof(buffer), "base:%d", TV_SWORD);
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_NONE);
	d = (struct drop*) parser_priv(p);
	notnull(d);
	has_all = has_all_of_tval(d->poss, TV_SWORD, false);
	ok;
}

static int test_base_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an invalid base. */
	enum parser_error r = parser_parse(p, "base:xyzzy");

	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	/* Try a base with no kinds. */
	r = parser_parse(p, "base:light");
	eq(r, PARSE_ERROR_NO_KIND_FOR_DROP_TYPE);
	ok;
}

static int test_notbase0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "not-base:boots");
	char buffer[40];
	struct drop *d;
	bool has_all;

	eq(r, PARSE_ERROR_NONE);
	d = (struct drop*) parser_priv(p);
	notnull(d);
	has_all = has_all_of_tval(d->imposs, TV_BOOTS, false);
	require(has_all);
	/* Check that lookup by index works. */
	strnfmt(buffer, sizeof(buffer), "not-base:%d", TV_SWORD);
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_NONE);
	d = (struct drop*) parser_priv(p);
	notnull(d);
	has_all = has_all_of_tval(d->imposs, TV_SWORD, false);
	require(has_all);
	ok;
}

static int test_notbase_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try an invalid base. */
	enum parser_error r = parser_parse(p, "not-base:xyzzy");

	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	/* Try a base with no kinds. */
	r = parser_parse(p, "base:light");
	eq(r, PARSE_ERROR_NO_KIND_FOR_DROP_TYPE);
	ok;
}

static int test_item0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "item:boots:Pair of Shoes");
	char buffer[40];
	struct drop *d;

	eq(r, PARSE_ERROR_NONE);
	d = (struct drop*) parser_priv(p);
	notnull(d);
	notnull(d->poss);
	eq(d->poss->kidx, 1);
	/* Check that lookup by index works. */
	strnfmt(buffer, sizeof(buffer), "item:%d:Broken Sword", TV_SWORD);
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_NONE);
	d = (struct drop*) parser_priv(p);
	notnull(d);
	notnull(d->poss);
	eq(d->poss->kidx, 4);
	r = parser_parse(p, "item:boots:1");
	eq(r, PARSE_ERROR_NONE);
	d = (struct drop*) parser_priv(p);
	notnull(d);
	notnull(d->poss);
	eq(d->poss->kidx, 1);
	strnfmt(buffer, sizeof(buffer), "item:%d:1", TV_SWORD);
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_NONE);
	d = (struct drop*) parser_priv(p);
	notnull(d);
	notnull(d->poss);
	eq(d->poss->kidx, 4);
	ok;
}

static int test_item_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try with an invalid tval but valid sval for some tval */
	enum parser_error r = parser_parse(p, "item:xyzzy:Pair of Shoes");

	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	/* Try with a valid tval but invalid sval. */
	r = parser_parse(p, "item:boots:xyzzy");
	eq(r, PARSE_ERROR_UNRECOGNISED_SVAL);
	/* Try with an invalid tval and sval. */
	r = parser_parse(p, "item:xyzzy:xyzzy");
	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	ok;
}

static int test_combined0(void *state) {
	struct parser *p = (struct parser*) state;
	const char *lines[] = {
		"name:footwear",
		"chest:1",
		"base:boots"
	};
	struct drop *d;
	int i;
	bool has_all;

	for (i = 0; i < (int) N_ELEMENTS(lines); ++i) {
		enum parser_error r = parser_parse(p, lines[i]);

		eq(r, PARSE_ERROR_NONE);
	}
	d = (struct drop*) parser_priv(p);
	notnull(d);
	notnull(d->name);
	require(streq(d->name, "footwear"));
	require(d->chest);
	has_all = has_all_of_tval(d->poss, TV_BOOTS, true);
	require(has_all);
	null(d->imposs);
	ok;
}

const char *suite_name = "parse/drop";
/*
 * test_missing_record_header0() has to be before test_name0() and
 * test_combined0().
 * All others except test_name0() and test_combined0() have to be after
 * test_name0().
 */
struct test tests[] = {
	{ "missing_record_header0", test_missing_record_header0 },
	{ "name0", test_name0 },
	{ "base0", test_base0 },
	{ "base_bad0", test_base_bad0 },
	{ "notbase0", test_notbase0 },
	{ "notbase_bad0", test_notbase_bad0 },
	{ "item0", test_item0 },
	{ "item_bad0", test_item_bad0 },
	{ "combined0", test_combined0 },
	{ NULL, NULL }
};
