/* parse/objbase */
/* Exercise parsing used for object_base.txt. */

struct chunk;
struct player;
#include "unit-test.h"
#include "datafile.h"
#include "init.h"
#include "object.h"
#include "obj-init.h"
#include "obj-slays.h"
#include "project.h"
#include "z-color.h"
#include "z-form.h"

#define DEFAULT_BREAK_CHANCE (15)
#define DEFAULT_MAX_STACK (35)
static char dummy_chest_name[16] = "Chest~";
static const struct object_base test_tv1 = {
	.name = dummy_chest_name,
	.tval = TV_CHEST,
	.attr = COLOUR_GREEN,
	.smith_attack_valid = true,
	.smith_attack_artistry = 3,
	.smith_attack_artefact = 9,
	.break_perc = 8,
	.max_stack = 20,
	.num_svals = 0
};
static const struct object_base test_tv2 = {
	.name = NULL,
	.tval = TV_LIGHT,
	.attr = 0,
	.smith_attack_valid = false,
	.smith_attack_artistry = 0,
	.smith_attack_artefact = 0,
	.break_perc = DEFAULT_BREAK_CHANCE,
	.max_stack = DEFAULT_MAX_STACK,
	.num_svals = 0
};
static const char *tv_names[] = {
	#define TV(a, b) b,
	#include "list-tvals.h"
	#undef TV
};

static char dummy_orc_slay[16] = "ORC_1";
static char dummy_spider_slay[16] = "SPIDER_1";
static struct slay dummy_slays[] = {
	{ .code = NULL },
	{ .code = dummy_orc_slay },
	{ .code = dummy_spider_slay },
};
static char dummy_cold_brand[16] = "COLD_1";
static char dummy_fire_brand[16] = "FIRE_1";
static struct brand dummy_brands[] = {
	{ .code = NULL },
	{ .code = dummy_cold_brand },
	{ .code = dummy_fire_brand },
};

int setup_tests(void **state) {
	*state = object_base_parser.init();
	/* Do minimal setup so manipulation of slays and brands works. */
	z_info = mem_zalloc(sizeof(*z_info));
	z_info->slay_max = (uint8_t) N_ELEMENTS(dummy_slays);
	slays = dummy_slays;
	z_info->brand_max = (uint8_t) N_ELEMENTS(dummy_brands);
	brands = dummy_brands;
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser*) state;
	errr r = object_base_parser.finish(p);
	int i;

	if (kb_info[test_tv1.tval].tval != test_tv1.tval) {
		if (verbose) {
			(void) printf("%s: delayed check failed at "
				"line %d\n", suite_name, __LINE__ - 3);
		}
		r = 1;
	}
	if (test_tv1.name) {
		if (!streq(kb_info[test_tv1.tval].name, test_tv1.name)) {
			if (verbose) {
				(void) printf("%s: delayed check failed at "
					"line %d\n", suite_name, __LINE__ - 3);
			}
			r = 1;
		}
	} else if (kb_info[test_tv1.tval].name) {
		if (verbose) {
			(void) printf("%s: delayed check failed at "
				"line %d\n", suite_name, __LINE__ - 3);
		}
		r = 1;
	}
	if (kb_info[test_tv1.tval].attr != test_tv1.attr) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 3);
		}
		r = 1;
	}
	if (kb_info[test_tv1.tval].smith_attack_valid
			!= test_tv1.smith_attack_valid) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 4);
		}
		r = 1;
	}
	if (kb_info[test_tv1.tval].smith_attack_artistry
			!= test_tv1.smith_attack_artistry) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 4);
		}
		r = 1;
	}
	if (kb_info[test_tv1.tval].smith_attack_artefact
			!= test_tv1.smith_attack_artefact) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 4);
		}
		r = 1;
	}
	if (!kb_info[test_tv1.tval].smith_slays
			|| kb_info[test_tv1.tval].smith_slays[0]
			|| kb_info[test_tv1.tval].smith_slays[1]
			|| !kb_info[test_tv1.tval].smith_slays[2]) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 6);
		}
		r = 1;
	}
	if (!kb_info[test_tv1.tval].smith_brands
			|| kb_info[test_tv1.tval].smith_brands[0]
			|| !kb_info[test_tv1.tval].smith_brands[1]
			|| kb_info[test_tv1.tval].smith_brands[2]) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 6);
		}
		r = 1;
	}
	if (kb_info[test_tv1.tval].break_perc != test_tv1.break_perc) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 3);
		}
		r = 1;
	}
	if (kb_info[test_tv1.tval].max_stack != test_tv1.max_stack) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 3);
		}
		r = 1;
	}
	if (kb_info[test_tv1.tval].num_svals != test_tv1.num_svals) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 3);
		}
		r = 1;
	}
	if (kb_info[test_tv2.tval].tval != test_tv2.tval) {
		if (verbose) {
			(void) printf("%s: delayed check failed at "
				"line %d\n", suite_name, __LINE__ - 3);
		}
		r = 1;
	}
	if (test_tv2.name) {
		if (!streq(kb_info[test_tv2.tval].name, test_tv2.name)) {
			if (verbose) {
				(void) printf("%s: delayed check failed at "
					"line %d\n", suite_name, __LINE__ - 3);
			}
			r = 1;
		}
	} else {
		if (kb_info[test_tv2.tval].name) {
			if (verbose) {
				(void) printf("%s: delayed check failed at "
					"line %d\n", suite_name, __LINE__ - 3);
			}
			r = 1;
		}
	}
	if (kb_info[test_tv2.tval].attr != test_tv2.attr) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 3);
		}
		r = 1;
	}
	if (kb_info[test_tv2.tval].smith_attack_valid
			!= test_tv2.smith_attack_valid) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 4);
		}
		r = 1;
	}
	if (kb_info[test_tv2.tval].smith_attack_artistry
			!= test_tv2.smith_attack_artistry) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 4);
		}
		r = 1;
	}
	if (kb_info[test_tv2.tval].smith_attack_artefact
			!= test_tv2.smith_attack_artefact) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 4);
		}
		r = 1;
	}
	if (kb_info[test_tv2.tval].smith_slays
			&& (kb_info[test_tv2.tval].smith_slays[0]
			|| kb_info[test_tv2.tval].smith_slays[1]
			|| kb_info[test_tv2.tval].smith_slays[2])) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 6);
		}
		r = 1;
	}
	if (kb_info[test_tv2.tval].smith_brands
			&& (kb_info[test_tv2.tval].smith_brands[0]
			|| kb_info[test_tv2.tval].smith_brands[1]
			|| kb_info[test_tv2.tval].smith_brands[2])) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 6);
		}
		r = 1;
	}
	if (kb_info[test_tv2.tval].break_perc != test_tv2.break_perc) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 3);
		}
		r = 1;
	}
	if (kb_info[test_tv2.tval].max_stack != test_tv2.max_stack) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 3);
		}
		r = 1;
	}
	if (kb_info[test_tv2.tval].num_svals != test_tv2.num_svals) {
		if (verbose) {
			(void) printf("%s: delayed check failed at line %d\n",
				suite_name, __LINE__ - 3);
		}
		r = 1;
	}
	for (i = 1; i < OF_MAX; ++i) {
		if (i == OF_PROT_FEAR) {
			if (!of_has(kb_info[test_tv1.tval].flags, i)) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"object flag %d failed at "
						"line %d\n", suite_name, i,
						__LINE__ - 5);
				}
				r = 1;
			}
		} else {
			if (of_has(kb_info[test_tv1.tval].flags, i)) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"object flag %d failed at "
						"line %d\n", suite_name, i,
						__LINE__ - 5);
				}
				r = 1;
			}
		}
		if (i == OF_RADIANCE || i == OF_HAUNTED) {
			if (!of_has(kb_info[test_tv1.tval].smith_flags, i)) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"smithing flag %d failed at "
						"line %d\n", suite_name, i,
						__LINE__ - 5);
				}
				r = 1;
			}
		} else {
			if (of_has(kb_info[test_tv1.tval].smith_flags, i)) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"smithing flag %d failed at "
						"line %d\n", suite_name, i,
						__LINE__ - 5);
				}
				r = 1;
			}
		}
		if (of_has(kb_info[test_tv2.tval].flags, i)) {
			if (verbose) {
				(void) printf("%s: delayed check for object "
					"flag %d failed at line %d\n",
					suite_name, i, __LINE__ - 4);
			}
			r = 1;
		}
		if (of_has(kb_info[test_tv2.tval].smith_flags, i)) {
			if (verbose) {
				(void) printf("%s: delayed check for smithing "
					"flag %d failed at line %d\n",
					suite_name, i, __LINE__ - 4);
			}
			r = 1;
		}
	}
	for (i = 1; i < KF_MAX; ++i) {
		if (i == KF_EASY_KNOW) {
			if (!kf_has(kb_info[test_tv1.tval].kind_flags, i)) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"kind flag %d failed at "
						"line %d\n", suite_name, i,
						__LINE__ - 5);
				}
				r = 1;
			}
		} else {
			if (kf_has(kb_info[test_tv1.tval].kind_flags, i)) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"kind flag %d failed at "
						"line %d\n", suite_name, i,
						__LINE__ - 5);
				}
				r = 1;
			}
		}
		if (kf_has(kb_info[test_tv2.tval].kind_flags, i)) {
			if (verbose) {
				(void) printf("%s: delayed check for kind "
					"flag %d failed at line %d\n",
					suite_name, i, __LINE__ - 4);
			}
			r = 1;
		}
	}
	for (i = 0; i < ELEM_MAX; ++i) {
		if (kb_info[test_tv1.tval].el_info[i].res_level != 0) {
			if (verbose) {
				(void) printf("%s: delayed check for element "
					"%d failed at line %d\n", suite_name, i,
					__LINE__ - 4);
			}
			r = 1;
		}
		if (i == ELEM_ACID) {
			if (kb_info[test_tv1.tval].el_info[i].flags
					!= (EL_INFO_HATES)) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"element %d failed at line "
						"%d\n", suite_name, i,
						__LINE__ - 6);
				}
				r = 1;
			}
		} else {
			if (kb_info[test_tv1.tval].el_info[i].flags) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"element %d failed at line "
						"%d\n", suite_name, i,
						__LINE__ - 5);
				}
				r = 1;
			}
		}
		if (i == PROJ_COLD) {
			if (kb_info[test_tv1.tval].smith_el_info[i].res_level
					!= 2) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"smithing element %d failed at "
						"line %d\n", suite_name, i,
						__LINE__ - 6);
				}
				r = 1;
			}
		} else if (i == PROJ_FIRE) {
			if (kb_info[test_tv1.tval].smith_el_info[i].res_level
					!= -1) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"smithing element %d failed at "
						"line %d\n", suite_name, i,
						__LINE__ - 6);
				}
				r = 1;
			}
		} else {
			if (kb_info[test_tv1.tval].smith_el_info[i].res_level) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"smithing element %d failed at "
						"line %d\n", suite_name, i,
						__LINE__ - 6);
				}
				r = 1;
			}
		}
		if (i == PROJ_FIRE) {
			if (kb_info[test_tv1.tval].smith_el_info[i].flags
					!= (EL_INFO_IGNORE)) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"smithing element %d failed at "
						"line %d\n", suite_name, i,
						__LINE__ - 6);
				}
				r = 1;
			}
		} else {
			if (kb_info[test_tv1.tval].smith_el_info[i].flags) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"smithing element %d failed at "
						"line %d\n", suite_name, i,
						__LINE__ - 5);
				}
				r = 1;
			}
		}
		if (kb_info[test_tv2.tval].el_info[i].res_level != 0) {
			if (verbose) {
				(void) printf("%s: delayed check for element "
					"%d failed at line %d\n", suite_name, i,
					__LINE__ - 4);
			}
			r = 1;
		}
		if (kb_info[test_tv2.tval].el_info[i].flags) {
			if (verbose) {
				(void) printf("%s: delayed check for element "
					"%d failed at line %d\n", suite_name, i,
					__LINE__ - 4);
			}
			r = 1;
		}
		if (kb_info[test_tv2.tval].smith_el_info[i].res_level) {
			if (verbose) {
				(void) printf("%s: delayed check for smithing "
					"element %d failed at line %d\n",
					suite_name, i, __LINE__ - 4);
			}
			r = 1;
		}
		if (kb_info[test_tv2.tval].smith_el_info[i].flags) {
			if (verbose) {
				(void) printf("%s: delayed check for smithing "
					"element %d failed at line %d\n",
					suite_name, i, __LINE__ - 4);
			}
			r = 1;
		}
	}
	for (i = 0; i < OBJ_MOD_MAX; ++i) {
		if (i == OBJ_MOD_WILL) {
			if (kb_info[test_tv1.tval].smith_modifiers[i] != 1) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"smithing modifier %d failed "
						"at line %d\n", suite_name, i,
						__LINE__ - 5);
				}
				r = 1;
			}
		} else {
			if (kb_info[test_tv1.tval].smith_modifiers[i]) {
				if (verbose) {
					(void) printf("%s: delayed check for "
						"smithing modifier %d failed "
						"at line %d\n", suite_name, i,
						__LINE__ - 5);
				}
				r = 1;
			}
		}
		if (kb_info[test_tv2.tval].smith_modifiers[i]) {
			if (verbose) {
				(void) printf("%s: delayed check for smithing "
					"modifier %d failed at line %d\n",
					suite_name, i, __LINE__ - 4);
			}
			r = 1;
		}
	}
	object_base_parser.cleanup();
	mem_free(z_info);
	return (r != 0) ? 1 : 0;
}

static int test_default0(void *state) {
	struct parser *p = (struct parser*) state;
	char buffer[80];
	enum parser_error r;

	require(strnfmt(buffer, sizeof(buffer), "default:break-chance:%d",
		DEFAULT_BREAK_CHANCE) < sizeof(buffer));
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_NONE);
	require(strnfmt(buffer, sizeof(buffer), "default:max-stack:%d",
		DEFAULT_MAX_STACK) < sizeof(buffer));
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_default_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "default:xyzzy:8");

	eq(r, PARSE_ERROR_UNDEFINED_DIRECTIVE);
	ok;
}

static int test_missing_record_header0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "graphics:Red");

	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "break:3");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "max-stack:10");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "smith-attack:3:9");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "flags:EASY_KNOW");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p,
		"smith-values:RES_FIRE[-1] RES_COLD[-1] RES_POIS[-1]");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "smith-flags:SHARPNESS");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "slay:ORC_1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "brand:COLD_1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	ok;
}

static int test_name_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:xyzzy:Something~");

	eq(r, PARSE_ERROR_UNRECOGNISED_TVAL);
	ok;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	char buffer[80];
	enum parser_error r;

	require(strnfmt(buffer, sizeof(buffer), "name:%s%s%s",
		tv_names[test_tv1.tval], (test_tv1.name) ? ":" : "",
		(test_tv1.name) ? test_tv1.name : "") < sizeof(buffer));
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_graphics0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "graphics:g");

	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_break0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "break:8");

	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_stack0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "max-stack:20");

	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_flags0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p,
		"flags:EASY_KNOW | PROT_FEAR | HATES_ACID");

	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_flags_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "flags:XYZZY");

	eq(r, PARSE_ERROR_INVALID_FLAG);
	ok;
}

static int test_smith_attack0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "smith-attack:3:9");

	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_smith_flags0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Check one object flag. */
	enum parser_error r = parser_parse(p, "smith-flags:RADIANCE");

	eq(r, PARSE_ERROR_NONE);
	/* Check one object flag and an element flag. */
	r = parser_parse(p, "smith-flags:HAUNTED | IGNORE_FIRE");
	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_smith_flags_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "smith-flags:XYZZY");

	eq(r, PARSE_ERROR_INVALID_FLAG);
	ok;
}

static int test_smith_values0(void *state) {
	struct parser *p = (struct parser*) state;
	/* Try one object modifier. */
	enum parser_error r = parser_parse(p, "smith-values:WILL[1]");

	eq(r, PARSE_ERROR_NONE);
	/* Try two vulnerabilities. */
	r = parser_parse(p, "smith-values:RES_COLD[-1] RES_FIRE[-1]");
	eq(r, PARSE_ERROR_NONE);
	/* Specify a resistance where there is also a vulnerability. */
	r = parser_parse(p, "smith-values:RES_COLD[1]");
	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_smith_values_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "smith-values:XYZZY[1]");

	eq(r, PARSE_ERROR_INVALID_VALUE);
	ok;
}

static int test_smith_slay0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "slay:SPIDER_1");

	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_smith_slay_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "slay:XYZZY");

	eq(r, PARSE_ERROR_UNRECOGNISED_SLAY);
	ok;
}

static int test_smith_brand0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "brand:COLD_1");

	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_smith_brand_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "brand:XYZZY");

	eq(r, PARSE_ERROR_UNRECOGNISED_BRAND);
	ok;
}

static int test_default_passthrough0(void *state) {
	struct parser *p = (struct parser*) state;
	char buffer[80];
	enum parser_error r;

	require(strnfmt(buffer, sizeof(buffer), "name:%s%s%s",
		tv_names[test_tv2.tval], (test_tv2.name) ? ":" : "",
		(test_tv2.name) ? test_tv2.name : "") < sizeof(buffer));
	r = parser_parse(p, buffer);
	eq(r, PARSE_ERROR_NONE);
	ok;
}

const char *suite_name = "parse/objbase";
/*
 * test_default0() must be before test_name0().
 * test_missing_record_header0() and test_name_bad0() must be
 * before test_name0().  All others must be after test_name0().
 * test_default_passthrough0() must appear last.
 */
struct test tests[] = {
	{ "default0", test_default0 },
	{ "default_bad0", test_default_bad0 },
	{ "missing_record_header0", test_missing_record_header0 },
	{ "name_bad0", test_name_bad0 },
	{ "name0", test_name0 },
	{ "graphics0", test_graphics0 },
	{ "break0", test_break0 },
	{ "stack0", test_stack0 },
	{ "flags0", test_flags0 },
	{ "flags_bad0", test_flags_bad0 },
	{ "smith_attack0", test_smith_attack0 },
	{ "smith_flags0", test_smith_flags0 },
	{ "smith_flags_bad0", test_smith_flags_bad0 },
	{ "smith_values0", test_smith_values0 },
	{ "smith_values_bad0", test_smith_values_bad0 },
	{ "smith_slay0", test_smith_slay0 },
	{ "smith_slay_bad0", test_smith_slay_bad0 },
	{ "smith_brand0", test_smith_brand0 },
	{ "smith_brand_bad0", test_smith_brand_bad0 },
	{ "default_passthrough0", test_default_passthrough0 },
	{ NULL, NULL }
};
