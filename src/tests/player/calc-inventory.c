/* player/calc-inventory.c */
/* Exercise calc_inventory(). */

#include "unit-test.h"
#include "test-utils.h"
#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-properties.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "z-quark.h"

/*
 * This is the maximum number of things (one of which will be a sentinel
 * element) to put in the gear for a test.
 */
#define TEST_SLOT_COUNT (40)

struct in_slot_desc { int tval, sval, num; bool known; bool equipped; };
struct out_slot_desc { int tval, sval, num; };
struct simple_test_case {
	struct in_slot_desc gear_in[TEST_SLOT_COUNT];
	struct out_slot_desc pack_out[TEST_SLOT_COUNT];
	struct out_slot_desc quiv_out[TEST_SLOT_COUNT];
};

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
#ifdef UNIX
	/* Necessary for creating the randart file. */
	create_needed_dirs();
#endif

	/* Set up the player. */
	if (!player_make_simple(NULL, NULL, NULL, "Tester")) {
		cleanup_angband();
		return 1;
	}

	prepare_next_level(player);
	on_new_level();

	return 0;
}

int teardown_tests(void *state) {
	wipe_mon_list(cave, player);
	cleanup_angband();

	return 0;
}

/* Remove all of the gear. */
static bool flush_gear(void) {
	struct object *curr = player->gear;

	while (curr != NULL) {
		struct object *next = curr->next;
		bool none_left = false;

		if (object_is_equipped(player->body, curr)) {
			inven_takeoff(curr);
		}
		curr = gear_object_for_use(player, curr, curr->number, false,
			&none_left);
		object_free(curr);
		curr = next;
		if (!none_left) {
			return false;
		}
	}
	return true;
}

/* Fill the gear with specified, simple, items. */
static bool populate_gear(const struct in_slot_desc *slots) {
	while (slots->tval > 0) {
		struct object_kind *kind =
			lookup_kind(slots->tval, slots->sval);
		struct object *obj;

		if (!kind) {
			return false;
		}
		obj = object_new();
		object_prep(obj, kind, 0, RANDOMISE);
		obj->number = slots->num;
		obj->known = object_new();
		object_set_base_known(player, obj);
		object_touch(player, obj);
		if (slots->known && ! object_flavor_is_aware(obj)) {
			object_know(obj);
		}
		gear_insert_end(player, obj);
		if (!object_is_carried(player, obj)) {
			return false;
		}
		if (slots->equipped) {
			inven_wield(obj, wield_slot(obj));
			if (!object_is_equipped(player->body, obj)) {
				return false;
			}
		}

		++slots;
	}

	return true;
}

/* Verify that the pack matches a given layout. */
static bool verify_pack(struct player *p, const struct out_slot_desc *slots,
		int slots_for_quiver) {
	int curr_slot = 0;
	int n_slots_used;

	if (!p->upkeep || !p->upkeep->inven) {
		return false;
	}
	n_slots_used = pack_slots_used(p);
	while (slots->tval > 0) {
		struct object_kind *kind =
			lookup_kind(slots->tval, slots->sval);

		if (curr_slot >= n_slots_used) {
			return false;
		}
		if (!p->upkeep->inven[curr_slot]) {
			return false;
		}
		if (p->upkeep->inven[curr_slot]->kind != kind) {
			return false;
		}
		if (p->upkeep->inven[curr_slot]->number != slots->num) {
			return false;
		}
		if (!object_is_carried(p, p->upkeep->inven[curr_slot])) {
			return false;
		}
		if (object_is_equipped(p->body, p->upkeep->inven[curr_slot])) {
			return false;
		}
		++curr_slot;
		++slots;
	}
	if (curr_slot + slots_for_quiver != n_slots_used) {
		return false;
	}
	return true;
}

/*
 * Verify that another call to calc_inventory() with the gear unchanged gives
 * the same result.
 */
static bool verify_stability(struct player *p) {
	struct object **old_pack =
		mem_alloc(z_info->pack_size * sizeof(*old_pack));
	bool result = true;
	int i;

	for (i = 0; i < z_info->pack_size; ++i) {
		old_pack[i] = p->upkeep->inven[i];
	}
	calc_inventory(p);
	for (i = 0; i < z_info->pack_size; ++i) {
		if (old_pack[i] != p->upkeep->inven[i]) {
			result = false;
		}
	}
	mem_free(old_pack);
	return result;
}

static int test_calc_inventory_empty(void *state) {
	struct out_slot_desc empty = { -1, -1, -1 };

	require(flush_gear());
	calc_inventory(player);
	require(verify_pack(player, &empty, 0));
	require(verify_stability(player));
	ok;
}

static int test_calc_inventory_only_equipped(void *state) {
	struct simple_test_case only_equipped_case = {
		{
			{ TV_SWORD, 1, 1, true, true },
			{ TV_BOW, 2, 1, true, true },
			{ TV_SHIELD, 1, 1, true, true },
			{ TV_CLOAK, 1, 1, true, true },
			{ TV_SOFT_ARMOR, 2, 1, true, true },
			{ -1, -1, -1, false, false, }
		},
		{ { -1, -1, -1 }, },
		{ { -1, -1, -1 }, }
	};

	require(flush_gear());
	require(populate_gear(only_equipped_case.gear_in));
	calc_inventory(player);
	require(verify_pack(player, only_equipped_case.pack_out, 0));
	require(verify_stability(player));
	ok;
}

static int test_calc_inventory_only_pack(void *state) {
	struct simple_test_case only_pack_case = {
		{
			{ TV_FOOD, 2, 4, true, false },
			{ TV_HERB, 3, 1, true, false },
			{ TV_HORN, 2, 2, true, false },
			{ TV_POTION, 4, 5, true, false },
			{ TV_LIGHT, 1, 6, true, false },
			{ TV_DIGGING, 1, 1, true, false },
			{ TV_FLASK, 1, 1, true, false },
			{ TV_STAFF, 3, 1, true, false },
			{ -1, -1, -1, false, false }
		},
		/*
		 * Usable book is first; then appear in order of decreasing
		 * tval.
		 */
		{
			{ TV_HERB, 3, 1 },
			{ TV_FOOD, 2, 4 },
			{ TV_FLASK, 1, 1 },
			{ TV_POTION, 4, 5 },
			{ TV_HORN, 2, 2 },
			{ TV_STAFF, 3, 1 },
			{ TV_LIGHT, 1, 6 },
			{ TV_DIGGING, 1, 1 },
			{ -1, -1, -1 }
		},
		{ { -1, -1, -1 } }
	};

	require(flush_gear());
	require(populate_gear(only_pack_case.gear_in));
	calc_inventory(player);
	require(verify_pack(player, only_pack_case.pack_out, 0));
	require(verify_stability(player));
	ok;
}

static int test_calc_inventory_equipped_throwing_inscribed(void *state) {
	struct simple_test_case this_test_case = {
		{
			{ TV_SWORD, 1, 1, true, true },
			{ -1, -1, -1, false, false }
		},
		{
			{ -1, -1, -1 }
		},
		{
			{ -1, -1, -1 }
		}
	};

	require(flush_gear());
	require(populate_gear(this_test_case.gear_in));
	/* Inscribe the dagger so it would go to the quiver if not equipped. */
	player->gear->note = quark_add("@v1");
	calc_inventory(player);
	require(verify_pack(player, this_test_case.pack_out, 0));
	require(verify_stability(player));
	ok;
}

const char *suite_name = "player/calc-inventory";
struct test tests[] = {
	{ "calc_inventory empty", test_calc_inventory_empty },
	{ "calc_inventory only equipped", test_calc_inventory_only_equipped },
	{ "calc_inventory only pack", test_calc_inventory_only_pack },
	{ "calc_inventory equipped throwing inscribed", test_calc_inventory_equipped_throwing_inscribed },
	{ NULL, NULL }
};
