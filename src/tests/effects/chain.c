/*
 * effects/chain
 * Test handling of effect chains and the container (RANDOM and SELECT) or
 * logic operation effects (BREAK, SKIP, BREAK_IF, and SKIP_IF) that are used
 * in chains.
 */

#include "unit-test.h"
#include "test-utils.h"
#include "cave.h"
#include "effects.h"
#include "effects-info.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "player.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "source.h"
#include "z-dice.h"

struct simple_effect {
	int t_index, radius, other;
	const char *st_str;
	const char *d_str;
};

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
#ifdef UNIX
	/* Necessary for creating the randart file. */
	create_needed_dirs();
#endif
	/* Set up the player so there's a target available for the effects. */
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

static void restore_to_full_health(void)
{
	player->chp = player->mhp;
	if (player->upkeep) player->upkeep->redraw |= (PR_HP);
}

static struct effect *build_effect_chain(const struct simple_effect *earr,
	int count)
{
	struct effect *prev = NULL;
	int i = count;

	/* Work backwards to make building the linked list easier. */
	while (i > 0) {
		struct effect *curr = mem_zalloc(sizeof(*curr));

		--i;
		curr->next = prev;
		curr->index = earr[i].t_index;
		if (earr[i].d_str) {
			curr->dice = dice_new();
			if (!dice_parse_string(curr->dice, earr[i].d_str)) {
				free_effect(curr);
				return NULL;
			}
		}
		curr->subtype = effect_subtype(curr->index, earr[i].st_str);
		if (curr->subtype == -1) {
			free_effect(curr);
			return NULL;
		}
		curr->radius = earr[i].radius;
		curr->other = earr[i].other;
		prev = curr;
	}
	return prev;
}

static int test_chain1_execute(void *state) {
	struct simple_effect ea[] = {
		{ EF_DAMAGE, 0, 0, "NONE", "1" },
	};
	struct effect *ec = build_effect_chain(ea, (int)N_ELEMENTS(ea));
	bool completed = false, ident = true;

	if (ec) {
		restore_to_full_health();
		completed = effect_do(ec, source_player(), NULL, &ident, true,
			0, NULL);
		free_effect(ec);
	}
	notnull(ec);
	require(completed);
	require(ident);
	eq(player->chp, player->mhp - 1);
	ok;
}

static int test_chain2_execute(void *state) {
	struct simple_effect ea[] = {
		{ EF_DAMAGE, 0, 0, "NONE", "2" },
		{ EF_HEAL_HP, 0, 0, "NONE", "1" },
	};
	struct effect *ec = build_effect_chain(ea, (int)N_ELEMENTS(ea));
	bool completed = false, ident = true;

	if (ec) {
		restore_to_full_health();
		completed = effect_do(ec, source_player(), NULL, &ident, true,
			0, NULL);
		free_effect(ec);
	}
	notnull(ec);
	require(completed);
	require(ident);
	eq(player->chp, player->mhp - 1);
	ok;
}

static int test_chain3_execute(void *state) {
	struct simple_effect ea[] = {
		{ EF_DAMAGE, 0, 0, "NONE", "5" },
		{ EF_HEAL_HP, 0, 0, "NONE", "4" },
		{ EF_DAMAGE, 0, 0, "NONE", "2" },
	};
	struct effect *ec = build_effect_chain(ea, (int)N_ELEMENTS(ea));
	bool completed = false, ident = true;

	if (ec) {
		restore_to_full_health();
		completed = effect_do(ec, source_player(), NULL, &ident, true,
			0, NULL);
		free_effect(ec);
	}
	notnull(ec);
	require(completed);
	require(ident);
	eq(player->chp, player->mhp - 3);
	ok;
}

const char *suite_name = "effects/chain";
struct test tests[] = {
	{ "chain1_execute", test_chain1_execute },
	{ "chain2_execute", test_chain2_execute },
	{ "chain3_execute", test_chain3_execute },
	{ NULL, NULL }
};
