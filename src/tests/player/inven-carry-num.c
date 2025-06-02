/* player/inven-carry-num.c */
/* Exercise inven_carry_num() and inven_carry_okay(). */

#include "test-utils.h"
#include "unit-test.h"
#include "init.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "player-birth.h"
#include "player-calcs.h"

struct carry_num_state {
	struct player *p;
	/*
	 * Want something that is neither ammunition nor good for throwing
	 * (torch), ammunition but not good for throwing (arrow), ammunition
	 * and good for throwing (shot), and good for throwing but not
	 * ammunition (flask of oil) when testing how the quiver fills.
	 */
	struct object *torch;
	struct object *arrow;
	struct object *shot;
	struct object *flask;
	struct object *inscribed_flask;
	struct object *inscribed_flask_alt;
	struct object *treasure;
};

int setup_tests(void **state) {
	struct carry_num_state *cns;

	set_file_paths();
	init_angband();
#ifdef UNIX
	/* Necessary for creating the randart file. */
	create_needed_dirs();
#endif

	/*
	 * Use a smaller than normal pack and quiver so it is less tedious to
	 * fill them up.  The tests are structured to assume that pack_size is
	 * at least two larger than the quiver size.  Use a quiver size of
	 * three so it is possible to fill it up with one stack of arrows, one
	 * stack of shots, and one stack of flasks.
	 */
	z_info->pack_size = 5;

	/* Set up the player. */
	if (!player_make_simple(NULL, NULL, NULL, "Tester")) {
		cleanup_angband();
		return 1;
	}

	cns = mem_zalloc(sizeof *cns);
	cns->p = player;
	cns->torch = object_new();
	object_prep(cns->torch, lookup_kind(TV_LIGHT, 1), 0, RANDOMISE);
	cns->torch->known = object_new();
	object_set_base_known(cns->p, cns->torch);
	object_touch(cns->p, cns->torch);
	cns->arrow = object_new();
	object_prep(cns->arrow, lookup_kind(TV_ARROW, 1), 0, RANDOMISE);
	cns->arrow->known = object_new();
	object_set_base_known(cns->p, cns->arrow);
	object_touch(cns->p, cns->arrow);
	cns->flask = object_new();
	object_prep(cns->flask, lookup_kind(TV_FLASK, 1), 0, RANDOMISE);
	cns->flask->known = object_new();
	object_set_base_known(cns->p, cns->flask);
	object_touch(cns->p, cns->flask);
	*state = cns;

	return 0;
}

int teardown_tests(void *state) {
	struct carry_num_state *cns = state;

	object_free(cns->torch);
	object_free(cns->arrow);
	object_free(cns->flask);
	mem_free(state);

	cleanup_angband();

	return 0;
}

static bool fill_pack(struct carry_num_state *cns, int n_pack,
		int n_arrow, int n_shot, int n_flask) {
	struct object *curr = cns->p->gear;
	int i;

	/* Empty out the pack and quiver. */
	while (curr != NULL) {
		if (! object_is_equipped(cns->p->body, curr)) {
			struct object *next = curr->next;
			bool none_left = false;

			curr = gear_object_for_use(cns->p, curr, curr->number,
				false, &none_left);
			object_free(curr);
			curr = next;
			if (!none_left) {
				return false;
			}
		} else {
			curr = curr->next;
		}
	}

	/* Add to pack. */
	for (i = 0; i < n_pack; ++i) {
		if (pack_is_full()) {
			return false;
		}
		curr = object_new();
		object_copy(curr, cns->torch);
		/* Vary inscriptions so they won't stack. */
		curr->note = quark_add(format("dummy%d", i));
		if (cns->torch->known) {
			curr->known = object_new();
			object_copy(curr->known, cns->torch->known);
		}
		inven_carry(cns->p, curr, false, false);
		calc_inventory(cns->p);
		if (! object_is_carried(cns->p, curr) ||
				object_is_equipped(cns->p->body, curr)) {
			return false;
		}
	}

	/* Add flasks. */
	i = 0;
	while (i < n_flask) {
		int n = n_flask - i;

		if (pack_is_full()) {
			return false;
		}
		curr = object_new();
		object_copy(curr, cns->flask);
		curr->number = n;
		if (cns->flask->known) {
			curr->known = object_new();
			object_copy(curr->known, cns->flask->known);
			curr->known->number = n;
			curr->known->note = curr->note;
		}
		inven_carry(cns->p, curr, false, false);
		calc_inventory(cns->p);
		if (! object_is_carried(cns->p, curr) ||
				object_is_equipped(cns->p->body, curr)) {
			return false;
		}
		i += n;
	}

	return true;
}

/* Try inven_carry() and inven_carry_okay() for one specific object. */
static bool perform_one_test(struct carry_num_state *cns, struct object *obj,
		int n_try, int n_expected) {
	int n_old = obj->number;
	bool success = true;

	obj->number = n_try;
	if (inven_carry_num(cns->p, obj) != n_expected) {
		success = false;
	}
	if (inven_carry_okay(obj)) {
		if (n_expected == 0) {
			success = false;
		}
	} else {
		if (n_expected > 0) {
			success = false;
		}
	}
	obj->number = n_old;
	return success;
}

static int test_carry_num_empty_pack_empty(void *state) {
	struct carry_num_state *cns = state;
	require(fill_pack(cns, 0, 0, 0, 0));
	require(perform_one_test(cns, cns->torch, 3, 3));
	require(perform_one_test(cns, cns->flask, 3, 3));
	ok;
}

static int test_carry_num_partial_pack_empty(void *state) {
	struct carry_num_state *cns = state;
	require(fill_pack(cns, z_info->pack_size - 1, 0, 0, 0));
	require(perform_one_test(cns, cns->torch, 3, 3));
	require(perform_one_test(cns, cns->arrow, 3, 3));
	/* Since it is not inscribed, it goes into the remaining pack slot. */
	require(perform_one_test(cns, cns->flask, 3, 3));
	ok;
}

const char *suite_name = "player/inven-carry-num";
struct test tests[] = {
	{ "carry num empty", test_carry_num_empty_pack_empty },
	{ "carry num partial", test_carry_num_partial_pack_empty },
	{ NULL, NULL }
};

