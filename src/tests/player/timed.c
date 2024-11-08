/* player/timed.c */
/* Exercise functions in player-timed.c. */

#include "unit-test.h"
#include "unit-test-data.h"
#include "test-utils.h"
#include "cave.h"
#include "game-event.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "object.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "player.h"
#include "player-birth.h"
#include "player-timed.h"
#include "z-color.h"
#include "z-rand.h"
#include "z-virt.h"
#include <limits.h>

struct test_timed_state {
	struct object *weapon;
	char *last_tracked_msg;
	char *last_recover_msg;
	int tracked_type;
	unsigned int n_tracked;
	unsigned int n_recover;
	unsigned int n_untracked;
	bool input_flushed;
};

static void test_timed_event_handler(game_event_type type,
	game_event_data *data, void *user);
static struct test_timed_state *setup_event_handlers(void);
static void reset_event_counters(struct test_timed_state *st,
	int tracked_type);
static void cleanup_event_handlers(struct test_timed_state *st);

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

	/*
	 * Set event handlers so some of the side effects of the functions in
	 * player-timed.h can be tracked.
	 */
	*state = setup_event_handlers();
	if (!*state) {
		cleanup_angband();
	}

	return 0;
}

int teardown_tests(void *state) {
	struct test_timed_state *st = (struct test_timed_state*) state;

	cleanup_event_handlers(st);
	wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

static void test_timed_event_handler(game_event_type type,
		game_event_data *data, void *user)
{
	struct test_timed_state *st = (struct test_timed_state*) user;

	switch (type) {
	case EVENT_MESSAGE:
		if (data->message.type == st->tracked_type) {
			string_free(st->last_tracked_msg);
			st->last_tracked_msg = (data->message.msg) ?
				string_make(data->message.msg) : NULL;
			if (st->n_tracked < UINT_MAX) {
				++st->n_tracked;
			}
		} else if (data->message.type == MSG_RECOVER) {
			string_free(st->last_recover_msg);
			st->last_recover_msg = (data->message.msg) ?
				string_make(data->message.msg) : NULL;
			if (st->n_recover < UINT_MAX) {
				++st->n_recover;
			}
		} else {
			if (st->n_untracked < UINT_MAX) {
				++st->n_untracked;
			}
		}
		break;

	case EVENT_INPUT_FLUSH:
		st->input_flushed = true;
		break;

	default:
		/* We got an event that we didn't signal an interest in. */
		assert(0);
	}
}

static struct test_timed_state *setup_event_handlers(void)
{
	struct test_timed_state *st = mem_alloc(sizeof(*st));

	st->last_tracked_msg = NULL;
	st->last_recover_msg = NULL;
	reset_event_counters(st, -1);
	event_add_handler(EVENT_MESSAGE, test_timed_event_handler, st);
	/*
	 * disturb() has the side effect of flushing the input.  So, monitor
	 * EVENT_INPUT_FLUSH to track whether disturb was called.
	 */
	event_add_handler(EVENT_INPUT_FLUSH, test_timed_event_handler, st);
	/* Set up a basic object that tests can apply flags to. */
	st->weapon = object_new();
	object_prep(st->weapon, lookup_kind(TV_SWORD, 1), 1, AVERAGE);
	st->weapon->known = object_new();
	object_touch(player, st->weapon);

	return st;
}

static void reset_event_counters(struct test_timed_state *st, int tracked_type)
{
	string_free(st->last_tracked_msg);
	st->last_tracked_msg = NULL;
	string_free(st->last_recover_msg);
	st->last_recover_msg = NULL;
	st->tracked_type = tracked_type;
	st->n_tracked = 0;
	st->n_recover = 0;
	st->n_untracked = 0;
	st->input_flushed = false;
}

static void cleanup_event_handlers(struct test_timed_state *st)
{
	event_remove_handler(EVENT_INPUT_FLUSH, test_timed_event_handler, st);
	event_remove_handler(EVENT_MESSAGE, test_timed_event_handler, st);
	if (st) {
		string_free(st->last_tracked_msg);
		string_free(st->last_recover_msg);
		object_free(st->weapon);
		mem_free(st);
	}
}

static int test_name2idx0(void *state) {
	eq(timed_name_to_idx("FAST"), TMD_FAST);
	eq(timed_name_to_idx("FOOD"), TMD_FOOD);
	require(timed_name_to_idx("XYZZY") < 0);
	ok;
}

static int test_timed_grade_eq0(void *state) {
	const struct timed_grade *g, *last, *tgt;

	/* Check on/off timed effect. */
	player->timed[TMD_SLOW] = 0;
	eq(player_timed_grade_eq(player, TMD_SLOW,
		timed_effects[TMD_SLOW].grade->next->name), false);
	player->timed[TMD_SLOW] = 500;
	eq(player_timed_grade_eq(player, TMD_SLOW,
		timed_effects[TMD_SLOW].grade->next->name), true);
	/* Check one with multiple grades. */
	player->timed[TMD_STUN] = 0;
	for (g = timed_effects[TMD_STUN].grade->next; g; g = g->next) {
		eq(player_timed_grade_eq(player, TMD_STUN, g->name),
			false);
	}
	for (last = timed_effects[TMD_STUN].grade, tgt = last->next; tgt;
			last = tgt, tgt = tgt->next) {
		require(last->max + 1 <= tgt->max);
		player->timed[TMD_STUN] =
			rand_range(last->max + 1, tgt->max);
		for (g = timed_effects[TMD_STUN].grade->next; g; g = g->next) {
			eq(player_timed_grade_eq(player, TMD_STUN,
				g->name), (g == tgt));
		}
		player->timed[TMD_STUN] = tgt->max;
		for (g = timed_effects[TMD_STUN].grade->next; g; g = g->next) {
			eq(player_timed_grade_eq(player, TMD_STUN,
				g->name), (g == tgt));
		}
	}
	ok;
}

/*
 * Test player_set_timed() with on/off state and messages for upward change
 * of grade and overall end message.
 */
static int test_set_timed0(void *state) {
	struct test_timed_state *st = (struct test_timed_state*) state;

	struct {
		int16_t in, new;
		bool notify;
		bool disturb;
		int16_t out;
		bool notified;
		const char *change_msg;
		const char *recover_msg;
	} test_cases[] = {
		/*
		 * No change from zero should never notify or issue a message,
		 * regardess of notify or disturb.
		 */
		{ 0, 0, true, true, 0, false, NULL, NULL },
		{ 0, 0, true, false, 0, false, NULL, NULL },
		{ 0, 0, false, true, 0, false, NULL, NULL },
		{ 0, 0, false, false, 0, false, NULL, NULL },
		/*
		 * Going from zero to a negative value is coerced to no change.
		 * So, no notification or messages.
		 */
		{ 0, -1, true, true, 0, false, NULL, NULL },
		{ 0, -83, false, true, 0, false, NULL, NULL },
		{ 0, -519, true, false, 0, false, NULL, NULL },
		{ 0, -1478, false, false, 0, false, NULL, NULL },
		/*
		 * No change from the current nonzero value should never
		 * notify or issue a message, regardless of notify or disturb.
		 */
		{ 1, 1, true, true, 1, false, NULL, NULL },
		{ 31, 31, true, false, 31, false, NULL, NULL },
		{ 198, 198, false, true, 198, false, NULL, NULL },
		{ 1024, 1024, false, false, 1024, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, timed_effects[TMD_SLOW].grade->next->max, true, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, timed_effects[TMD_SLOW].grade->next->max, true, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, timed_effects[TMD_SLOW].grade->next->max, false, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, timed_effects[TMD_SLOW].grade->next->max, false, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		/*
		 * Going up a grade will notify because the new grade has an
		 * up message.
		 */
		{ 0, 1, true, true, 1, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 53, true, false, 53, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 100, false, true, 100, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 5131, false, false, 5131, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_SLOW].grade->next->max, true, true, timed_effects[TMD_SLOW].grade->next->max, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_SLOW].grade->next->max, false, true, timed_effects[TMD_SLOW].grade->next->max, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_SLOW].grade->next->max, true, false, timed_effects[TMD_SLOW].grade->next->max, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_SLOW].grade->next->max, false, false, timed_effects[TMD_SLOW].grade->next->max, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_SLOW].grade->next->max + 1, true, true, timed_effects[TMD_SLOW].grade->next->max, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_SLOW].grade->next->max + 15, true, true, timed_effects[TMD_SLOW].grade->next->max, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_SLOW].grade->next->max + 307, true, true, timed_effects[TMD_SLOW].grade->next->max, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_SLOW].grade->next->max + 1008, true, true, timed_effects[TMD_SLOW].grade->next->max, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		/*
		 * Going down a grade will only notify if requested because the
		 * new grade does not have a down message; when notifying, a
		 * recover message will be issued.
		 */
		{ 1, 0, true, true, 0, true, NULL, timed_effects[TMD_SLOW].on_end },
		{ 90, 0, false, true, 0, false, NULL, NULL },
		{ 458, 0, true, false, 0, true, NULL, timed_effects[TMD_SLOW].on_end },
		{ 8192, 0, false, false, 0, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 0, true, true, 0, true, NULL, timed_effects[TMD_SLOW].on_end },
		{ timed_effects[TMD_SLOW].grade->next->max, 0, false, true, 0, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 0, true, false, 0, true, NULL, timed_effects[TMD_SLOW].on_end },
		{ timed_effects[TMD_SLOW].grade->next->max, 0, false, false, 0, false, NULL, NULL },
		{ 7, -1, true, true, 0, true, NULL, timed_effects[TMD_SLOW].on_end },
		{ 38, -125, false, true, 0, false, NULL, NULL },
		{ 428, -96, true, false, 0, true, NULL, timed_effects[TMD_SLOW].on_end },
		{ 2197, -1364, false, false, 0, false, NULL, NULL },
		/*
		 * Increasing within the same grade will only notify if
		 * requested; no messages will be generated because there
		 * isn't an on_increase message.
		 */
		{ 1, 2, true, true, 2, true, NULL, NULL },
		{ 10, 30, false, true, 30, false, NULL, NULL },
		{ 853, 901, true, false, 901, true, NULL, NULL },
		{ 2412, 2300, false, false, 2300, false, NULL, NULL },
		/*
		 * Decreasing within the same grade will only notify if
		 * requested; no messages will be generated because there
		 * isn't an on_decrease message.
		 */
		{ 2, 1, true, true, 1, true, NULL, NULL },
		{ 73, 60, false, true, 60, false, NULL, NULL },
		{ 345, 121, true, false, 121, true, NULL, NULL },
		{ 3890, 3883, false, false, 3883, false, NULL, NULL },
		/*
		 * Trying to increase beyond the maximum while already there
		 * should never notify or issue a message.
		 */
		{ timed_effects[TMD_SLOW].grade->next->max, timed_effects[TMD_SLOW].grade->next->max + 1, true, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, timed_effects[TMD_SLOW].grade->next->max + 81, false, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, timed_effects[TMD_SLOW].grade->next->max + 673, true, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, timed_effects[TMD_SLOW].grade->next->max + 2738, false, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
	};
	int i;

	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		bool result;

		reset_event_counters(st, timed_effects[TMD_SLOW].msgt);
		player->timed[TMD_SLOW] = test_cases[i].in;
		result = player_set_timed(player, TMD_SLOW,
			test_cases[i].new, test_cases[i].notify,
			test_cases[i].disturb);
		eq(result, test_cases[i].notified);
		eq(player->timed[TMD_SLOW], test_cases[i].out);
		if (test_cases[i].change_msg) {
			eq(st->n_tracked, 1);
			require(st->last_tracked_msg
				&& streq(st->last_tracked_msg,
				test_cases[i].change_msg));
		} else {
			eq(st->n_tracked, 0);
		}
		if (test_cases[i].recover_msg) {
			eq(st->n_recover, 1);
			require(st->last_recover_msg
				&& streq(st->last_recover_msg,
				test_cases[i].recover_msg));
		} else {
			eq(st->n_recover, 0);
		}
		eq(st->n_untracked, 0);
		eq(st->input_flushed, (test_cases[i].notified
			&& test_cases[i].disturb));
	}
	ok;
}

/*
 * Test player_set_timed() with on/off state, overlap with a resist, and
 * messages for upward change of grade, overall end, and increase.
 */
static int test_set_timed1(void *state) {
	struct test_timed_state *st = (struct test_timed_state*) state;

	struct {
		int16_t in, new;
		bool resist;
		bool notify;
		bool disturb;
		int16_t out;
		bool notified;
		const char *change_msg;
		const char *recover_msg;
	} test_cases[] = {
		/*
		 * No change from zero should never notify or issue a message,
		 * regardless of notify or disturb.
		 */
		{ 0, 0, false, true, true, 0, false, NULL, NULL },
		{ 0, 0, false, true, false, 0, false, NULL, NULL },
		{ 0, 0, false, false, true, 0, false, NULL, NULL },
		{ 0, 0, false, false, false, 0, false, NULL, NULL },
		{ 0, 0, true, true, true, 0, false, NULL, NULL },
		{ 0, 0, true, true, false, 0, false, NULL, NULL },
		{ 0, 0, true, false, true, 0, false, NULL, NULL },
		{ 0, 0, true, false, false, 0, false, NULL, NULL },
		/*
		 * Going from zero to a negative value is coerced to no change.
		 * So, no notification or messages.
		 */
		{ 0, -1, false, true, true, 0, false, NULL, NULL },
		{ 0, -62, false, false, true, 0, false, NULL, NULL },
		{ 0, -397, false, true, false, 0, false, NULL, NULL },
		{ 0, -1008, false, false, false, 0, false, NULL, NULL },
		{ 0, -1, true, true, true, 0, false, NULL, NULL },
		{ 0, -62, true, false, true, 0, false, NULL, NULL },
		{ 0, -397, true, true, false, 0, false, NULL, NULL },
		{ 0, -1008, true, false, false, 0, false, NULL, NULL },
		/*
		 * No change from the current nonzero value should never notify
		 * or issue a message, regardless of notify or disturb.
		 */
		{ 1, 1, false, true, true, 1, false, NULL, NULL },
		{ 23, 23, false, true, false, 23, false, NULL, NULL },
		{ 417, 417, false, false, true, 417, false, NULL, NULL },
		{ 3693, 3693, false, false, false, 3693, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max, false, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max, false, true, false, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max, false, false, true, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max, false, false, false, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ 1, 1, true, true, true, 1, false, NULL, NULL },
		{ 23, 23, true, true, false, 23, false, NULL, NULL },
		{ 417, 417, true, false, true, 417, false, NULL, NULL },
		{ 3693, 3693, true, false, false, 3693, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max, true, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max, true, true, false, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max, true, false, true, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max, true, false, false, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		/*
		 * Going up a grade will notify because the new grade has an
		 * up message.
		 */
		{ 0, 1, false, true, true, 1, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, 49, false, true, false, 49, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, 175, false, false, true, 175, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, 1467, false, false, false, 1467, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max, false, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max, false, false, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max, false, true, false, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max, false, false, false, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max + 1, false, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max + 15, false, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max + 307, false, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max + 1008, false, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].grade->next->up_msg, NULL },
		{ 0, 1, true, true, true, 1, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		{ 0, 49, true, true, false, 49, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		{ 0, 175, true, false, true, 175, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		{ 0, 1467, true, false, false, 1467, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max, true, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max, true, false, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max, true, true, false, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max, true, false, false, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max + 1, true, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max + 15, true, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max + 307, true, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		{ 0, timed_effects[TMD_OPP_FIRE].grade->next->max + 1008, true, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, true, timed_effects[TMD_OPP_FIRE].on_increase, NULL },
		/*
		 * Going down a grade will only notify if requested because the
		 * new grade does not have a down message; when notifying, a
		 * recover message will be issued.
		 */
		{ 1, 0, false, true, true, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ 52, 0, false, false, true, 0, false, NULL, NULL },
		{ 327, 0, false, true, false, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ 6718, 0, false, false, false, 0, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, 0, false, true, true, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, 0, false, false, true, 0, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, 0, false, true, false, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, 0, false, false, false, 0, false, NULL, NULL },
		{ 5, -1, false, true, true, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ 66, -138, false, false, true, 0, false, NULL, NULL},
		{ 274, -87, false, true, false, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ 1056, -1258, false, false, false, 0, false, NULL, NULL},
		{ 1, 0, true, true, true, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ 52, 0, true, false, true, 0, false, NULL, NULL },
		{ 327, 0, true, true, false, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ 6718, 0, true, false, false, 0, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, 0, true, true, true, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, 0, true, false, true, 0, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, 0, true, true, false, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, 0, true, false, false, 0, false, NULL, NULL },
		{ 5, -1, true, true, true, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ 66, -138, true, false, true, 0, false, NULL, NULL},
		{ 274, -87, true, true, false, 0, true, NULL, timed_effects[TMD_OPP_FIRE].on_end },
		{ 1056, -1258, true, false, false, 0, false, NULL, NULL},
		/*
		 * Increasing within the same grade will only notify if
		 * requested.  No message will be generated.
		 */
		{ 1, 3, false, true, true, 3, true, NULL, NULL },
		{ 12, 14, false, false, true, 14, false, NULL, NULL },
		{ 628, 671, false, true, false, 671, true, NULL, NULL },
		{ 1005, 1011, false, false, false, 1011, false, NULL, NULL },
		{ 1, 3, true, true, true, 3, true, NULL, NULL },
		{ 12, 14, true, false, true, 14, false, NULL, NULL },
		{ 628, 671, true, true, false, 671, true, NULL, NULL },
		{ 1005, 1011, true, false, false, 1011, false, NULL, NULL },
		/*
		 * Decreasing within the same grade will only notify if
		 * requested; there will be no messages, even if notifying,
		 * as there is no on_decrease message.
		 */
		{ 4, 1, false, true, true, 1, true, NULL, NULL },
		{ 58, 43, false, false, true, 43, false, NULL, NULL },
		{ 271, 248, false, true, false, 248, true, NULL, NULL },
		{ 1315, 1280, false, false, false, 1280, false, NULL, NULL },
		{ 4, 1, true, true, true, 1, true, NULL, NULL },
		{ 58, 43, true, false, true, 43, false, NULL, NULL },
		{ 271, 248, true, true, false, 248, true, NULL, NULL },
		{ 1315, 1280, true, false, false, 1280, false, NULL, NULL },
		/*
		 * Trying to increase beyond the maximum while already there
		 * should never notify or issue a message.
		 */
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max + 1, false, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max + 67, false, false, true, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max + 323, false, true, false, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max + 1141, false, false, false, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max + 1, true, true, true, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max + 67, true, false, true, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max + 323, true, true, false, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_OPP_FIRE].grade->next->max, timed_effects[TMD_OPP_FIRE].grade->next->max + 1141, true, false, false, timed_effects[TMD_OPP_FIRE].grade->next->max, false, NULL, NULL },
	};
	int old_resist = player->state.el_info[ELEM_FIRE].res_level;
	int i;

	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		bool result;

		reset_event_counters(st, timed_effects[TMD_OPP_FIRE].msgt);
		player->timed[TMD_OPP_FIRE] = test_cases[i].in;
		if (test_cases[i].resist) {
			player->state.el_info[ELEM_FIRE].res_level = 1;
		} else {
			player->state.el_info[ELEM_FIRE].res_level = 0;
		}
		result = player_set_timed(player, TMD_OPP_FIRE,
			test_cases[i].new, test_cases[i].notify,
			test_cases[i].disturb);
		eq(result, test_cases[i].notified);
		eq(player->timed[TMD_OPP_FIRE], test_cases[i].out);
		if (test_cases[i].change_msg) {
			eq(st->n_tracked, 1);
			require(st->last_tracked_msg
				&& streq(st->last_tracked_msg,
				test_cases[i].change_msg));
		} else {
			eq(st->n_tracked, 0);
		}
		if (test_cases[i].recover_msg) {
			eq(st->n_recover, 1);
			require(st->last_recover_msg
				&& streq(st->last_recover_msg,
				test_cases[i].recover_msg));
		} else {
			eq(st->n_recover, 0);
		}
		eq(st->n_untracked, 0);
		eq(st->input_flushed, (test_cases[i].notified
			&& test_cases[i].disturb));
	}
	player->state.el_info[ELEM_FIRE].res_level = old_resist;
	ok;
}

/*
 * Test player_set_timed() with multiple grades.  Each of the grades has an
 * up message; the middle grade as a down message.  There's an overall end
 * message, but no increase or decrease messages.
 */
static int test_set_timed2(void *state) {
	struct test_timed_state *st = (struct test_timed_state*) state;
	const struct timed_grade *s, *ls;

	for (ls = NULL, s = timed_effects[TMD_STUN].grade; s;
			ls = s, s = s->next) {
		/* This it the lower limit for the starting grade. */
		int s_l = (ls) ? ls->max + 1 : 0;
		const struct timed_grade *e, *le;

		require(s_l <= s->max);
		for (le = NULL, e = timed_effects[TMD_STUN].grade; e;
				le = e, e = e->next) {
			/* This is the lower limit for the ending grade. */
			int e_l = (le) ? le->max + 1 : 0;
			int oldv, newv, i;

			require(e_l <= e->max);
			if (s->grade == e->grade) {
				/*
				 * Test for no change of duration.
				 * player_set_timed() should not notify or
				 * generate messages.
				 */
				oldv = rand_range(s_l, s->max);
				for (i = 0; i < 4; ++i) {
					bool notify = i < 2;
					bool disturb = !(i % 2);
					bool result;

					reset_event_counters(st,
						timed_effects[TMD_STUN].msgt);
					player->timed[TMD_STUN] = oldv;
					result = player_set_timed(player,
						TMD_STUN, oldv, notify,
						disturb);
					eq(result, false);
					eq(player->timed[TMD_STUN], oldv);
					eq(st->n_tracked, 0);
					eq(st->n_recover, 0);
					eq(st->n_untracked, 0);
					eq(st->input_flushed, false);
				}

				if (!s->next) {
					/*
					 * Test for attempting to go above
					 * the maximum from the maximum.  Should
					 * behave the same as no change in
					 * value.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_STUN].msgt);
						player->timed[TMD_STUN] = s->max;
						newv = rand_range(
							MIN(s->max + 1, 32767),
							MAX(s->max + 10, 32767));
						result = player_set_timed(
							player, TMD_STUN,
							newv, notify, disturb);
						eq(result, false);
						eq(player->timed[TMD_STUN],
							s->max);
						eq(st->n_tracked, 0);
						eq(st->n_recover, 0);
						eq(st->input_flushed, false);
					}
				} else if (!s->grade) {
					/*
					 * Test for attempting to go below
					 * the minimum from the minimum.  Should
					 * behave the same as no change in
					 * value.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_STUN].msgt);
						player->timed[TMD_STUN] = s_l;
						newv = rand_range(s_l - 30,
							s_l - 1);
						result = player_set_timed(
							player, TMD_STUN,
							newv, notify,
							disturb);
						eq(result, false);
						eq(player->timed[TMD_STUN],
							s_l);
						eq(st->n_tracked, 0);
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed, 0);
					}
				}

				if (s_l < s->max) {
					/*
					 * Test for increase within the grade.
					 * Since there's no on_increase message,
					 * this should only notify if asked
					 * (with the special case that if in
					 * the highest grade of stun it will
					 * not notify and not increase the
					 * duration) and should not generate any
					 * messages.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_STUN].msgt);
						oldv = rand_range(s_l,
							s->max - 1);
						newv = rand_range(oldv + 1,
							s->max);
						player->timed[TMD_STUN] = oldv;
						result = player_set_timed(
							player, TMD_STUN,
							newv, notify,
							disturb);
						if (s->next) {
							eq(result, notify);
							eq(player->timed[TMD_STUN],
								newv);
						} else {
							eq(result, false);
							eq(player->timed[TMD_STUN],
								oldv);
						}
						eq(st->n_tracked, 0);
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed,
							(s->next && notify
							&& disturb));
					}
					/*
					 * Test for decrease within the grade.
					 * Since there's no on_decrease
					 * message, this should only notify if
					 * asked and should not generate any
					 * messages.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_STUN].msgt);
						oldv = rand_range(s_l + 1,
							s->max);
						newv = rand_range(s_l,
							oldv - 1);
						player->timed[TMD_STUN] = oldv;
						result = player_set_timed(
							player, TMD_STUN,
							newv, notify,
							disturb);
						eq(result, notify);
						eq(player->timed[TMD_STUN],
							newv);
						eq(st->n_tracked, 0);
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed,
							(notify && disturb));
					}

					if (!s->next) {
						/*
						 * Test attempting to go above
						 * the maximum from below the
						 * maximum.  Has same effect,
						 * except the coercion to the
						 * maximum, as a plain increase
						 * within a grade.
						 */
						for (i = 0; i < 4; ++i) {
							bool notify = i < 2;
							bool disturb = !(i % 2);
							bool result;

							reset_event_counters(st,
								timed_effects[TMD_STUN].msgt);
							oldv = rand_range(s_l,
								s->max - 1);
							newv = rand_range(
								MIN(s->max + 1, 32767),
								MIN(s->max + 20, 32767));
							player->timed[TMD_STUN] =
								oldv;
							result = player_set_timed(
								player,
								TMD_STUN, newv,
								notify,
								disturb);
							eq(result, false);
							eq(player->timed[TMD_STUN],
								oldv);
							eq(st->n_tracked, 0);
							eq(st->n_recover, 0);
							eq(st->n_untracked, 0);
							eq(st->input_flushed,
								false);
						}
					} else if (!s->grade) {
						/*
						 * Test attempting to go below
						 * the minimum from above the
						 * minimum.  Has same effect,
						 * except the coercion to the
						 * minimum, as a plain decrease
						 * within a grade.
						 */
						for (i = 0; i < 4; ++i) {
							bool notify = i < 2;
							bool disturb = !(i % 2);
							bool result;

							reset_event_counters(st,
								timed_effects[TMD_STUN].msgt);
							oldv = rand_range(
								s_l + 1,
								s->max);
							newv = rand_range(
								s_l - 50,
								s_l - 1);
							player->timed[TMD_STUN] =
								oldv;
							result = player_set_timed(
								player,
								TMD_STUN, newv,
								notify,
								disturb);
							eq(result, notify);
							eq(player->timed[TMD_STUN],
								s_l);
							eq(st->n_tracked, 0);
							eq(st->n_recover, 0);
							eq(st->n_untracked, 0);
							eq(st->input_flushed,
								(notify
								&& disturb));
						}
					}
				}
			} else {
				oldv = rand_range(s_l, s->max);
				newv = rand_range(e_l, e->max);
				/*
				 * An increase in grade will notify since
				 * the resulting grade has an up message and
				 * will have a message that is that up message.
				 * A decrease in grade only notifies if the
				 * destination grade has a down message or if
				 * requested and only has a message if
				 * the destination grade has a down message or
				 * if notifying and the effect lapses.
				 */
				for (i = 0; i < 4; ++i) {
					bool notify = i < 2;
					bool disturb = !(i % 2);
					bool notified = (e->grade > s->grade) ?
						true :
						notify || e->down_msg;
					bool result;

					reset_event_counters(st,
						timed_effects[TMD_STUN].msgt);
					player->timed[TMD_STUN] = oldv;
					result = player_set_timed(player,
						TMD_STUN, newv, notify,
						disturb);
					eq(result, notified);
					eq(player->timed[TMD_STUN], newv);
					if (e->grade > s->grade) {
						eq(st->n_tracked, 1);
						require(streq(
							st->last_tracked_msg,
							e->up_msg));
						eq(st->n_recover, 0);
					} else {
						if (e->down_msg) {
							eq(st->n_tracked, 1);
							require(streq(
								st->last_tracked_msg,
								e->down_msg));
						} else {
							eq(st->n_tracked, 0);
						}
						if (e->grade || !notified) {
							eq(st->n_recover, 0);
						} else {
							eq(st->n_recover, 1);
							require(streq(
								st->last_recover_msg,
								timed_effects[TMD_STUN].on_end));
						}
					}
					/*
					 * Handle special case of end of
					 * blackout message.
					 */
					if (!s->next && e->next) {
						require(st->n_untracked == 0
							|| st->n_untracked == 1);
					} else {
						eq(st->n_untracked, 0);
					}
					eq(st->input_flushed,
						(notified && disturb));
				}

				if (!e->next) {
					/* Test going above the maximum. */
					newv = rand_range(
						MIN(e->max + 1, 32767),
						MIN(e->max + 10, 32767));
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_STUN].msgt);
						player->timed[TMD_STUN] = oldv;
						result = player_set_timed(
							player, TMD_STUN,
							newv, notify,
							disturb);
						eq(result, true);
						eq(player->timed[TMD_STUN],
							e->max);
						eq(st->n_tracked, 1);
						require(streq(
							st->last_tracked_msg,
							e->up_msg));
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed, disturb);
					}
				} else if (!e->grade) {
					/* Test going below the minimum. */
					newv = rand_range(e_l - 1000, e_l - 1);
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_STUN].msgt);
						player->timed[TMD_STUN] = oldv;
						result = player_set_timed(
							player, TMD_STUN,
							newv, notify,
							disturb);
						eq(result, notify);
						eq(player->timed[TMD_STUN], e_l);
						eq(st->n_tracked, 0);
						if (notify) {
							eq(st->n_recover, 1);
							require(streq(
								st->last_recover_msg,
								timed_effects[TMD_STUN].on_end));
						} else {
							eq(st->n_recover, 0);
						}
						eq(st->n_untracked, 0);
						eq(st->input_flushed,
							(notify && disturb));
					}
				}
			}
		}
	}
	ok;
}

/*
 * Test player_set_timed() with TMD_FOOD's grades.  Intermediate grades have
 * both up and down messages.  The lowest grade only has a down message, and
 * the highest grade only has a up message.  There's no overall messages (end,
 * increase, or decrease).
 */
static int test_set_timed3(void *state) {
	struct test_timed_state *st = (struct test_timed_state*) state;
	const struct timed_grade *s, *ls;

	for (ls = NULL, s = timed_effects[TMD_FOOD].grade->next; s;
			ls = s, s = s->next) {
		/* This is the lower limit for the starting grade. */
		int s_l = (ls) ? ls->max + 1 : 1;
		const struct timed_grade *e, *le;

		require(s_l <= s->max);
		for (le = NULL, e = timed_effects[TMD_FOOD].grade->next; e;
				le = e, e = e->next) {
			/* This is the lower limit for the ending grade. */
			int e_l = (le) ? le->max + 1 : 1;
			int oldv, newv, i;

			require(e_l <= e->max);
			if (s->grade == e->grade) {
				/*
				 * Test for no change of duration.
				 * player_set_timed() should not notify or
				 * generate messages.
				 */
				oldv = rand_range(s_l, s->max);
				for (i = 0; i < 4; ++i) {
					bool notify = i < 2;
					bool disturb = !(i % 2);
					bool result;

					reset_event_counters(st,
						timed_effects[TMD_FOOD].msgt);
					player->timed[TMD_FOOD] = oldv;
					result = player_set_timed(player,
						TMD_FOOD, oldv, notify,
						disturb);
					eq(result, false);
					eq(player->timed[TMD_FOOD], oldv);
					eq(st->n_tracked, 0);
					eq(st->n_recover, 0);
					eq(st->n_untracked, 0);
					eq(st->input_flushed, false);
				}

				if (!s->next) {
					/*
					 * Test for attempting to go above
					 * the maximum from the maximum.  Should
					 * behave the same as no change in
					 * value.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_FOOD].msgt);
						player->timed[TMD_FOOD] =
							s->max;
						newv = rand_range(
							MIN(s->max + 1, 32767),
							MIN(s->max + 10, 32767));
						result = player_set_timed(
							player, TMD_FOOD,
							newv, notify,
							disturb);
						eq(result, false);
						eq(player->timed[TMD_FOOD],
							s->max);
						eq(st->n_tracked, 0);
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed, false);
					}
				} else if (!s->grade) {
					/*
					 * Test for attempting to go below
					 * the minimum from the minimum.  Should
					 * behave the same as no change in
					 * value.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_FOOD].msgt);
						player->timed[TMD_FOOD] = s_l;
						newv = rand_range(
							s_l - 30,
							s_l - 1);
						result = player_set_timed(
							player, TMD_FOOD,
							newv, notify,
							disturb);
						eq(result, false);
						eq(player->timed[TMD_FOOD],
							s_l);
						eq(st->n_tracked, 0);
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed, false);
					}
				}

				if (s_l < s->max) {
					/*
					 * Test for increase within the grade.
					 * Since there's no on_increase
					 * message, this should only notify
					 * if asked and should not generate
					 * any messages.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_FOOD].msgt);
						oldv = rand_range(s_l,
							s->max - 1);
						newv = rand_range(oldv + 1,
							s->max);
						player->timed[TMD_FOOD] = oldv;
						result = player_set_timed(
							player, TMD_FOOD,
							newv, notify,
							disturb);
						eq(result, notify);
						eq(player->timed[TMD_FOOD],
							newv);
						eq(st->n_tracked, 0);
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed,
							(notify && disturb));
					}
					/*
					 * Test for decrease within the grade.
					 * Since there's no on_decrease
					 * message, this should only notify
					 * if asked and should not generate
					 * any messages.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_FOOD].msgt);
						oldv = rand_range(s_l + 1,
							s->max);
						newv = rand_range(s_l,
							oldv - 1);
						player->timed[TMD_FOOD] = oldv;
						result = player_set_timed(
							player, TMD_FOOD,
							newv, notify,
							disturb);
						eq(result, notify);
						eq(player->timed[TMD_FOOD],
							newv);
						eq(st->n_tracked, 0);
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed,
							(notify && disturb));
					}

					if (!s->next) {
						/*
						 * Test attempting to go above
						 * the maximum from below the
						 * maximum.  Has same effect,
						 * except the coercion to the
						 * maximum, as a plain increase
						 * within a grade.
						 */
						for (i = 0; i < 4; ++i) {
							bool notify = i < 2;
							bool disturb = !(i % 2);
							bool result;

							reset_event_counters(st,
								timed_effects[TMD_FOOD].msgt);
							oldv = rand_range(s_l,
								s->max - 1);
							newv = rand_range(
								MIN(s->max + 1, 32767),
								MAX(s->max + 20, 32767));
							player->timed[TMD_FOOD] =
								oldv;
							result = player_set_timed(
								player,
								TMD_FOOD, newv,
								notify,
								disturb);
							eq(result, notify);
							eq(player->timed[TMD_FOOD],
								s->max);
							eq(st->n_tracked, 0);
							eq(st->n_recover, 0);
							eq(st->n_untracked, 0);
							eq(st->input_flushed,
								(notify
								&& disturb));
						}
					} else if (!s->grade) {
						/*
						 * Test attempting to go below
						 * the minimum from above the
						 * minimum.  Has same effect,
						 * except the coercion to the
						 * minimum, as a plain decrease
						 * within a grade.
						 */
						for (i = 0; i < 4; ++i) {
							bool notify = i < 2;
							bool disturb = !(i % 2);
							bool result;

							reset_event_counters(st,
								timed_effects[TMD_FOOD].msgt);
							oldv = rand_range(
								s_l + 1,
								s->max);
							newv = rand_range(
								s_l - 50,
								s_l - 1);
							player->timed[TMD_FOOD] =
								oldv;
							result = player_set_timed(
								player,
								TMD_FOOD, newv,
								notify,
								disturb);
							eq(result, notify);
							eq(player->timed[TMD_FOOD],
								s_l);
							eq(st->n_tracked, 0);
							eq(st->n_recover, 0);
							eq(st->n_untracked, 0);
							eq(st->input_flushed,
								(notify
								&& disturb));
						}
					}
				}
			} else {
				oldv = rand_range(s_l, s->max);
				newv = rand_range(e_l, e->max);
				/*
				 * An increase in grade will notify since
				 * the resulting grade has an up message and
				 * will have message that is that up message.
				 * A decrease in grade will notify since the
				 * resulting grade has a down message and
				 * will have a message that is that down
				 * message.
				 */
				for (i = 0; i < 4; ++i) {
					bool notify = i < 2;
					bool disturb = !(i % 2);
					bool result;

					reset_event_counters(st,
						timed_effects[TMD_FOOD].msgt);
					player->timed[TMD_FOOD] = oldv;
					result = player_set_timed(player,
						TMD_FOOD, newv, notify,
						disturb);
					eq(result, true);
					eq(player->timed[TMD_FOOD], newv);
					eq(st->n_tracked, 1);
					if (e->grade > s->grade) {
						require(streq(
							st->last_tracked_msg,
							e->up_msg));
					} else {
						require(streq(
							st->last_tracked_msg,
							e->down_msg));
					}
					eq(st->n_recover, 0);
					eq(st->n_untracked, 0);
					eq(st->input_flushed, disturb);
				}

				if (!e->next) {
					/* Test going above the maximum. */
					newv = rand_range(
						MIN(e->max + 1, 32767),
						MIN(e->max + 10, 32767));
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_FOOD].msgt);
						player->timed[TMD_FOOD] = oldv;
						result = player_set_timed(
							player, TMD_FOOD,
							newv, notify,
							disturb);
						eq(result, true);
						eq(player->timed[TMD_FOOD],
							e->max);
						eq(st->n_tracked, 1);
						require(streq(
							st->last_tracked_msg,
							e->up_msg));
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed, disturb);
					}
				} else if (!e->grade) {
					/* Test going below the minimum. */
					newv = rand_range(e_l - 1000, e_l - 1);
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_FOOD].msgt);
						player->timed[TMD_FOOD] = oldv;
						result = player_set_timed(
							player, TMD_FOOD,
							newv, notify,
							disturb);
						eq(result, notify);
						eq(player->timed[TMD_FOOD],
							e_l);
						eq(st->n_tracked, 1);
						require(streq(
							st->last_tracked_msg,
							e->down_msg));
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed, disturb);
					}
				}
			}
		}
	}
	ok;
}

/*
 * Test player_set_timed() with a timed effect that has multiple change grades,
 * multiple change-inc thresholds (only the first of those has an increase
 * message), a change-dec threshold, and an on-end message.
 */
static int test_set_timed4(void *state) {
	struct test_timed_state *st = (struct test_timed_state*) state;
	const struct timed_change_grade *s, *ls;

	for (ls = NULL, s = timed_effects[TMD_POISONED].c_grade; s;
			ls = s, s = s->next) {
		/* This is the lower limit for the starting grade. */
		int s_l = (ls) ? ls->max + 1 : 0;
		const struct timed_change_grade *e, *le;

		require(s_l <= s->max);
		for (le = NULL, e = timed_effects[TMD_POISONED].c_grade; e;
				le = e, e = e->next) {
			/* This is the lower limit for the ending grade. */
			int e_l = (le) ? le->max + 1 : 0;
			int oldv, newv, i;

			require(e_l <= e->max);
			if (s->c_grade == e->c_grade) {
				/*
				 * Test for no change of duration.
				 * player_set_timed() should not notify or
				 * generate messages.
				 */
				oldv = rand_range(s_l, s->max);
				for (i = 0; i < 4; ++i) {
					bool notify = i < 2;
					bool disturb = !(i % 2);
					bool result;

					reset_event_counters(st,
						timed_effects[TMD_POISONED].msgt);
					player->timed[TMD_POISONED] = oldv;
					result = player_set_timed(player,
						TMD_POISONED, oldv, notify,
						disturb);
					eq(result, false);
					eq(player->timed[TMD_POISONED], oldv);
					eq(st->n_tracked, 0);
					eq(st->n_recover, 0);
					eq(st->n_untracked, 0);
					eq(st->input_flushed, false);
				}

				if (!s->next) {
					/*
					 * Test for attempting to go above
					 * the maximum from the maximum.  Should
					 * behave the same as no change in
					 * value.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_POISONED].msgt);

						player->timed[TMD_POISONED] =
							s->max;
						newv = rand_range(
							MIN(s->max + 1, 32767),
							MIN(s->max + 10, 32767));

						result = player_set_timed(
							player, TMD_POISONED,
							newv, notify,
							disturb);
						eq(result, false);
						eq(player->timed[TMD_POISONED],
							s->max);
						eq(st->n_tracked, 0);
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed, false);
					}
				} else if (!ls) {
					/*
					 * Test for attempting to go below
					 * the minimum from the minimum.
					 * Should behave the same as no change
					 * in value.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						reset_event_counters(st,
							timed_effects[TMD_POISONED].msgt);

						player->timed[TMD_POISONED] = s_l;
						newv = rand_range(
							s_l - 30,
							s_l - 1);
						result = player_set_timed(
							player, TMD_POISONED,
							newv, notify,
							disturb);
						eq(result, false);
						eq(player->timed[TMD_POISONED],
							s_l);
						eq(st->n_tracked, 0);
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed, false);
					}
				}

				if (s_l < s->max) {
					const struct timed_change *inc, *inc_p;
					int div;

					/*
					 * Test for increase within the grade.
					 * This should always notify.
					 */
					for (inc_p = NULL, inc = timed_effects[TMD_POISONED].increase;
							inc;
							inc_p = inc,
							inc = inc->next) {
						int inc_min = (inc_p) ?
							MAX(1, inc_p->max + 1) :
							1;

						if (inc_min > s->max - s_l
								|| inc->max <= inc_min) {
							continue;
						}
						for (i = 0; i < 4; ++i) {
							bool notify = i < 2;
							bool disturb = !(i % 2);
							bool result;

							reset_event_counters(st,
								timed_effects[TMD_POISONED].msgt);
							oldv = rand_range(s_l,
								s->max - inc_min);
							newv = rand_range(oldv + inc_min,
								MIN(oldv + inc->max - 1, s->max));
							player->timed[TMD_POISONED] = oldv;
							result = player_set_timed(
								player,
								TMD_POISONED,
								newv,
								notify,
								disturb);
							eq(result, true);
							eq(player->timed[TMD_POISONED],
								newv);
							eq(st->n_tracked, 1);
							if (oldv && inc->inc_msg) {
								require(streq(
									st->last_tracked_msg,
									inc->inc_msg));
							} else {
								require(streq(
									st->last_tracked_msg,
									inc->msg));
							}
							eq(st->n_recover, 0);
							eq(st->n_untracked, 0);
							eq(st->input_flushed,
								disturb);
						}
					}

					/*
					 * Test for decrease within the grade.
					 * This should only notify if requested
					 * or the amount of change satisfies the
					 * change-dec threshold.
					 */
					div = timed_effects[TMD_POISONED].decrease.max;
					require(div > 0);
					notnull(timed_effects[TMD_POISONED].decrease.msg);
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result, big;

						reset_event_counters(st,
							timed_effects[TMD_POISONED].msgt);
						oldv = rand_range(s_l + 1,
							s->max);
						newv = rand_range(s_l,
							oldv - 1);
						player->timed[TMD_POISONED] = oldv;
						big = oldv - newv									> (oldv + div - 1)
							/ div;
						result = player_set_timed(
							player, TMD_POISONED,
							newv, notify, disturb);
						eq(result, notify || big);
						eq(player->timed[TMD_POISONED],
							newv);
						if (big) {
							eq(st->n_tracked, 1);
							require(streq(
								st->last_tracked_msg,
								timed_effects[TMD_POISONED].decrease.msg));
						} else {
							eq(st->n_tracked, 0);
						}
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->input_flushed,
							(notify || big)
							&& disturb);
					}
				}
			} else {
				/* Check changing grades. */
				for (i = 0; i < 4; ++i) {
					bool notify = i < 2;
					bool disturb = !(i % 2);
					bool result;

					oldv = rand_range(s_l, s->max);
					newv = rand_range(e_l, e->max);
					reset_event_counters(st,
						timed_effects[TMD_POISONED].msgt);
					player->timed[TMD_POISONED] = oldv;
					result = player_set_timed(
						player, TMD_POISONED,
						newv, notify, disturb);
					eq(player->timed[TMD_POISONED], newv);
					eq(st->n_recover, 0);
					eq(st->n_untracked, 0);
					if (oldv < newv) {
						const struct timed_change *inc =
							timed_effects[TMD_POISONED].increase;

						while (newv - oldv >= inc->max && inc->next) {
							inc = inc->next;
						}
						eq(result, true);
						eq(st->n_tracked, 1);
						if (oldv && inc->inc_msg) {
							require(streq(
								st->last_tracked_msg,
								inc->inc_msg));
						} else {
							require(streq(
								st->last_tracked_msg,
								inc->msg));
						}
						eq(st->input_flushed, disturb);
					} else {
						require(oldv != newv);
						if (newv) {
							int div = timed_effects[TMD_POISONED].decrease.max;
							bool big;

							require(div > 0);
							notnull(timed_effects[TMD_POISONED].decrease.msg);
							big = oldv - newv >
								(oldv + div - 1) / div;
							eq(result, notify || big);
							if (big) {
								eq(st->n_tracked, 1);
								require(streq(
									st->last_tracked_msg,
									timed_effects[TMD_POISONED].decrease.msg));
							} else {
								eq(st->n_tracked, 0);
							}
							eq(st->input_flushed,
								(notify || big)
								&& disturb);
						} else {
							eq(result, true);
							eq(st->n_tracked, 1);
							require(streq(
								st->last_tracked_msg,
								timed_effects[TMD_POISONED].on_end));
						}
					}
				}

				if (!e->next) {
					/*
					 * Try going above the maximum from
					 * another grade.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						const struct timed_change *inc =
							timed_effects[TMD_POISONED].increase;
						bool result;

						oldv = rand_range(s_l, s->max);
						newv = rand_range(e->max + 1, e->max + 20);
						while (e->max - oldv >= inc->max && inc->next) {
							inc = inc->next;
						}
						reset_event_counters(st,
							timed_effects[TMD_POISONED].msgt);
						player->timed[TMD_POISONED] = oldv;
						result = player_set_timed(
							player, TMD_POISONED,
							newv, notify, disturb);
						eq(result, true);
						eq(player->timed[TMD_POISONED],
							e->max);
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
						eq(st->n_tracked, 1);
						if (oldv && inc->inc_msg) {
							require(streq(
								st->last_tracked_msg,
								inc->inc_msg));
						} else {
							require(streq(
								st->last_tracked_msg,
								inc->msg));
						}
						eq(st->input_flushed, disturb);
					}
				}

				if (!le) {
					/*
					 * Try going below the minimum from
					 * another grade.
					 */
					for (i = 0; i < 4; ++i) {
						bool notify = i < 2;
						bool disturb = !(i % 2);
						bool result;

						oldv = rand_range(s_l, s->max);
						newv = rand_range(-20, -1);
						reset_event_counters(st,
							timed_effects[TMD_POISONED].msgt);
						player->timed[TMD_POISONED] = oldv;
						result = player_set_timed(
							player, TMD_POISONED,
							newv, notify, disturb);
						eq(result, true);
						eq(player->timed[TMD_POISONED], 0);
						eq(st->n_tracked, 1);
						require(streq(
							st->last_tracked_msg,
							timed_effects[TMD_POISONED].on_end));
						eq(st->n_recover, 0);
						eq(st->n_untracked, 0);
					}
				}
			}
		}
	}
	ok;
}

static int test_inc_check0(void *state) {
	struct test_timed_state *st = (struct test_timed_state*) state;
	int weapon_slot = wield_slot(st->weapon);
	int old_will;
	bool result;

	require(weapon_slot >= 0 && weapon_slot < player->body.count);

	/* Test for effect that has no protection. */
	eq(timed_effects[TMD_FOOD].fail, -1);
	result = player_inc_check(player, TMD_FOOD, false);
	eq(result, true);
	result = player_inc_check(player, TMD_FOOD, true);
	eq(result, true);
	eq(timed_effects[TMD_SINVIS].fail, -1);
	result = player_inc_check(player, TMD_SINVIS, false);
	eq(result, true);
	result = player_inc_check(player, TMD_SINVIS, true);
	eq(result, true);

	/*
	 * Test for effect that has object flag protection.  First without
	 * the object flag set and then with it set.
	 */
	require(timed_effects[TMD_SLOW].fail != -1);
	old_will = player->state.skill_use[SKILL_WILL];
	player->state.flags[timed_effects[TMD_SLOW].fail] = 0;
	player->known_state.flags[timed_effects[TMD_SLOW].fail] = 0;
	player->body.slots[weapon_slot].obj = NULL;
	/* Set the player's will so all saving throws will fail. */
	player->state.skill_use[SKILL_WILL] = 0;
	result = player_inc_check(player, TMD_SLOW, false);
	eq(result, true);
	/* Set the player's will so all saving throws will succeed. */
	player->state.skill_use[SKILL_WILL] = 10;
	player->state.flags[timed_effects[TMD_SLOW].fail] = 1;
	of_wipe(st->weapon->flags);
	of_on(st->weapon->flags, timed_effects[TMD_SLOW].fail);
	player->body.slots[weapon_slot].obj = st->weapon;
	result = player_inc_check(player, TMD_SLOW, false);
	eq(result, false);
	player->state.skill_use[SKILL_WILL] = old_will;

	/*
	 * Lore check use the known state, so this will say an increase is
	 * possible.
	 */
	result = player_inc_check(player, TMD_SLOW, true);
	eq(result, true);
	player->known_state.flags[timed_effects[TMD_SLOW].fail] = 1;
	player->state.skill_use[SKILL_WILL] = 10;
	result = player_inc_check(player, TMD_SLOW, false);
	eq(result, false);
	player->state.skill_use[SKILL_WILL] = old_will;
	result = player_inc_check(player, TMD_SLOW, true);
	eq(result, false);

	/* Check special cases. */
	if (timed_effects[TMD_AFRAID].fail != -1) {
		player->state.flags[timed_effects[TMD_AFRAID].fail] = 0;
		player->known_state.flags[timed_effects[TMD_AFRAID].fail] = 0;
	}
	player->timed[TMD_RAGE] = 1;
	result = player_inc_check(player, TMD_AFRAID, false);
	eq(result, false);

	ok;
}

/*
 * Test player_inc_timed() with on/off state, protection via an object flag
 * and messages for upward change of grade and overall end message.
 */
static int test_inc_timed0(void *state) {
	struct test_timed_state *st = (struct test_timed_state*) state;

	struct {
		int16_t in;
		int inc;
		bool notify;
		bool disturb;
		bool check;
		bool protected;
		int16_t out;
		bool notified;
		const char *change_msg;
		const char *recover_msg;
	} test_cases[] = {
		/*
		 * No change from zero should never notify or issue a message,
		 * regardless of notify, disturb, or check.
		 */
		{ 0, 0, true, true, false, false, 0, false, NULL, NULL },
		{ 0, 0, true, false, false, false, 0, false, NULL, NULL },
		{ 0, 0, false, true, false, false, 0, false, NULL, NULL },
		{ 0, 0, false, false, false, false, 0, false, NULL, NULL },
		{ 0, 0, true, true, true, false, 0, false, NULL, NULL },
		{ 0, 0, true, true, false, true, 0, false, NULL, NULL },
		{ 0, 0, true, true, true, true, 0, false, NULL, NULL },
		{ 0, 0, true, false, true, false, 0, false, NULL, NULL },
		{ 0, 0, true, false, false, true, 0, false, NULL, NULL },
		{ 0, 0, true, false, true, true, 0, false, NULL, NULL },
		{ 0, 0, false, true, true, false, 0, false, NULL, NULL },
		{ 0, 0, false, true, false, true, 0, false, NULL, NULL },
		{ 0, 0, false, true, true, true, 0, false, NULL, NULL },
		{ 0, 0, false, false, true, false, 0, false, NULL, NULL },
		{ 0, 0, false, false, false, true, 0, false, NULL, NULL },
		{ 0, 0, false, false, true, true, 0, false, NULL, NULL },
		/*
		 * No change from the current nonzero value should never
		 * notify or issue a message regardless of notify, disturb,
		 * or check.
		 */
		{ 1, 0, true, true, false, false, 1, false, NULL, NULL },
		{ 3, 0, true, false, false, false, 3, false, NULL, NULL },
		{ 12, 0, false, true, false, false, 12, false, NULL, NULL },
		{ 107, 0, false, false, false, false, 107, false, NULL, NULL },
		{ 1, 0, true, true, true, false, 1, false, NULL, NULL },
		{ 8, 0, true, true, false, true, 8, false, NULL, NULL },
		{ 234, 0, true, true, true, true, 234, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 0, true, false, true, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ 1, 0, true, false, false, true, 1, false, NULL, NULL },
		{ 6, 0, true, false, true, true, 6, false, NULL, NULL },
		{ 1317, 0, false, true, true, false, 1317, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 0, false, true, false, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ 1, 0, false, true, true, true, 1, false, NULL, NULL },
		{ 25, 0, false, false, true, false, 25, false, NULL, NULL },
		{ 176, 0, false, false, false, true, 176, false, NULL, NULL },
		{ 1864, 0, false, false, true, true, 1864, false, NULL, NULL },
		/*
		 * Going up a grade will notify, unless checking and protected
		 * by the object flag, because the new grade has an up message.
		 */
		{ 0, 1, true, true, false, false, 1, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 5, true, false, false, false, 5, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 13, false, true, false, false, 13, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 147, false, false, false, false, 147, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_SLOW].grade->next->max, true, true, true, false, timed_effects[TMD_SLOW].grade->next->max, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 1, true, true, false, true, 1, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 93, true, true, true, true, 0, false, NULL, NULL },
		{ 0, 1, true, false, true, false, 1, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 134, true, false, false, true, 134, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 1419, true, false, true, true, 0, false, NULL, NULL },
		{ 0, 10, false, true, false, true, 10, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 57, false, true, true, false, 57, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 1, false, true, true, true, 0, false, NULL, NULL },
		{ 0, timed_effects[TMD_SLOW].grade->next->max, false, false, false, true, timed_effects[TMD_SLOW].grade->next->max, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 8, false, false, true, false, 8, true, timed_effects[TMD_SLOW].grade->next->up_msg, NULL },
		{ 0, 1, false, false, true, true, 0, false, NULL, NULL },
		/*
		 * Increasing within the same grade will only notify if
		 * requested (and that will be prevented by checking when
		 * protected); no messages will be generated.
		 */
		{ 1, 35, true, true, false, false, 36, true, NULL, NULL },
		{ 10, 1, true, false, false, false, 11, true, NULL, NULL },
		{ 123, 8, false, true, false, false, 131, false, NULL, NULL },
		{ 1095, 10, false, false, false, false, 1105, false, NULL, NULL },
		{ 8, 9, true, true, true, false, 17, true, NULL, NULL },
		{ 17, 1, true, true, false, true, 18, true, NULL, NULL },
		{ 37, 6, true, true, true, true, 37, false, NULL, NULL },
		{ 133, 21, true, false, true, false, 154, true, NULL, NULL },
		{ 1067, 5, true, false, false, true, 1072, true, NULL, NULL },
		{ 2345, 2, true, false, true, true, 2345, false, NULL, NULL },
		{ 1, 18, false, true, true, false, 19, false, NULL, NULL },
		{ 184, 3, false, true, false, true, 187, false, NULL, NULL },
		{ 1137, 10, false, true, true, true, 1137, false, NULL, NULL },
		{ 5, 1, false, false, true, false, 6, false, NULL, NULL },
		{ 74, 3, false, false, false, true, 77, false, NULL, NULL },
		{ 153, 9, false, false, true, true, 153, false, NULL, NULL },
		/*
		 * Trying to go beyond the maximum while already there should
		 * not notify or generate a message).
		 */
		{ timed_effects[TMD_SLOW].grade->next->max, 1, true, true, false, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 11, true, false, false, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 139, false, true, false, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 1070, false, false, false, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 1, true, true, true, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 18, true, true, false, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 264, true, true, true, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 1, true, false, true, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 33, true, false, false, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 198, true, false, true, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 1, false, true, true, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 53, false, true, false, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 206, false, true, true, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 1, false, false, true, false, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 12, false, false, false, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_SLOW].grade->next->max, 1032, false, false, true, true, timed_effects[TMD_SLOW].grade->next->max, false, NULL, NULL },
	};
	int old_will, i;

	old_will = player->state.skill_use[SKILL_WILL];
	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		bool result;

		reset_event_counters(st, timed_effects[TMD_SLOW].msgt);
		if (test_cases[i].protected) {
			player->state.flags[OF_FREE_ACT] = 1;
			/*
			 * Set the player's will so that all saving throws
			 * will pass given the presence of resistance.
			 */
			player->state.skill_use[SKILL_WILL] = 10;
		} else {
			player->state.flags[OF_FREE_ACT] = 0;
			/*
			 * Set the player's will so that all saving throws
			 * will fail.
			 */
			player->state.skill_use[SKILL_WILL] = 0;
		}
		player->known_state.flags[OF_FREE_ACT] = 0;
		player->timed[TMD_SLOW] = test_cases[i].in;
		result = player_inc_timed(player, TMD_SLOW,
			test_cases[i].inc, test_cases[i].notify,
			test_cases[i].disturb, test_cases[i].check);
		eq(result, test_cases[i].notified);
		eq(player->timed[TMD_SLOW], test_cases[i].out);
		if (test_cases[i].change_msg) {
			eq(st->n_tracked, 1);
			require(st->last_tracked_msg
				&& streq(st->last_tracked_msg,
				test_cases[i].change_msg));
		} else {
			eq(st->n_tracked, 0);
		}
		if (test_cases[i].recover_msg) {
			eq(st->n_recover, 1);
			require(st->last_recover_msg
				&& streq(st->last_recover_msg,
				test_cases[i].recover_msg));
		} else {
			eq(st->n_recover, 0);
		}
		eq(st->n_untracked, 0);
		eq(st->input_flushed, (test_cases[i].notified
			&& test_cases[i].disturb));
	}
	player->state.skill_use[SKILL_WILL] = old_will;
	ok;
}

/*
 * Test special cases (non-stacking behavior of TMD_ENTRANCED) for
 * player_inc_timed().
 */
static int test_inc_timed1(void *state) {
	struct test_timed_state *st = (struct test_timed_state*) state;
	struct {
		int16_t in;
		int inc;
		bool notify;
		bool disturb;
		bool check;
		bool protected;
		int16_t out;
		bool notified;
		const char *change_msg;
		const char *recover_msg;
	} test_cases[] = {
		/*
		 * No change from zero should never notify or issue a message,
		 * regardless of notify, disturb, or check.
		 */
		{ 0, 0, true, true, false, false, 0, false, NULL, NULL },
		{ 0, 0, true, false, false, false, 0, false, NULL, NULL },
		{ 0, 0, false, true, false, false, 0, false, NULL, NULL },
		{ 0, 0, false, false, false, false, 0, false, NULL, NULL },
		{ 0, 0, true, true, true, false, 0, false, NULL, NULL },
		{ 0, 0, true, true, false, true, 0, false, NULL, NULL },
		{ 0, 0, true, true, true, true, 0, false, NULL, NULL },
		{ 0, 0, true, false, true, false, 0, false, NULL, NULL },
		{ 0, 0, true, false, false, true, 0, false, NULL, NULL },
		{ 0, 0, true, false, true, true, 0, false, NULL, NULL },
		{ 0, 0, false, true, true, false, 0, false, NULL, NULL },
		{ 0, 0, false, true, false, true, 0, false, NULL, NULL },
		{ 0, 0, false, true, true, true, 0, false, NULL, NULL },
		{ 0, 0, false, false, true, false, 0, false, NULL, NULL },
		{ 0, 0, false, false, false, true, 0, false, NULL, NULL },
		{ 0, 0, false, false, true, true, 0, false, NULL, NULL },
		/*
		 * No change from the current nonzero value should never
		 * notify or issue a message regardless of notify, disturb,
		 * or check.
		 */
		{ 1, 0, true, true, false, false, 1, false, NULL, NULL },
		{ 3, 0, true, false, false, false, 3, false, NULL, NULL },
		{ 12, 0, false, true, false, false, 12, false, NULL, NULL },
		{ 107, 0, false, false, false, false, 107, false, NULL, NULL },
		{ 1, 0, true, true, true, false, 1, false, NULL, NULL },
		{ 8, 0, true, true, false, true, 8, false, NULL, NULL },
		{ 234, 0, true, true, true, true, 234, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 0, true, false, true, false, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ 1, 0, true, false, false, true, 1, false, NULL, NULL },
		{ 6, 0, true, false, true, true, 6, false, NULL, NULL },
		{ 1317, 0, false, true, true, false, 1317, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 0, false, true, false, true, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ 1, 0, false, true, true, true, 1, false, NULL, NULL },
		{ 25, 0, false, false, true, false, 25, false, NULL, NULL },
		{ 176, 0, false, false, false, true, 176, false, NULL, NULL },
		{ 1864, 0, false, false, true, true, 1864, false, NULL, NULL },
		/*
		 * Going up a grade will notify, unless checking and protected
		 * by th object flag, because the new grade has an up message.
		 */
		{ 0, 1 , true, true, false, false, 1, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, 5, true, false, false, false, 5, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, 13, false, true, false, false, 13, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, 147, false, false, false, false, 147, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, timed_effects[TMD_ENTRANCED].grade->next->max, true, true, true, false, timed_effects[TMD_ENTRANCED].grade->next->max, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, 1, true, true, false, true, 1, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, 93, true, true, true, true, 0, false, NULL, NULL },
		{ 0, 1, true, false, true, false, 1, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, 134, true, false, false, true, 134, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, 1419, true, false, true, true, 0, false, NULL, NULL },
		{ 0, 10, false, true, false, true, 10, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, 57, false, true, true, false, 57, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, 1, false, true, true, true, 0, false, NULL, NULL },
		{ 0, timed_effects[TMD_ENTRANCED].grade->next->max, false, false, false, true, timed_effects[TMD_ENTRANCED].grade->next->max, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, 8, false, false, true, false, 8, true, timed_effects[TMD_ENTRANCED].grade->next->up_msg, NULL },
		{ 0, 1, false, false, true, true, 0, false, NULL, NULL },
		/*
		 * Increasing within the same grade should never notify as
		 * TMD_ENTRANCED does not stack.
		 */
		{ 1, 35, true, true, false, false, 1, false, NULL, NULL },
		{ 10, 1, true, false, false, false, 10, false, NULL, NULL },
		{ 123, 8, false, true, false, false, 123, false, NULL, NULL },
		{ 1095, 10, false, false, false, false, 1095, false, NULL, NULL },
		{ 8, 9, true, true, true, false, 8, false, NULL, NULL },
		{ 17, 1, true, true, false, true, 17, false, NULL, NULL },
		{ 37, 6, true, true, true, true, 37, false, NULL, NULL },
		{ 133, 21, true, false, true, false, 133, false, NULL, NULL },
		{ 1067, 5, true, false, false, true, 1067, false, NULL, NULL },
		{ 2345, 2, true, false, true, true, 2345, false, NULL, NULL },
		{ 1, 18, false, true, true, false, 1, false, NULL, NULL },
		{ 184, 3, false, true, false, true, 184, false, NULL, NULL },
		{ 1137, 10, false, true, true, true, 1137, false, NULL, NULL },
		{ 5, 1, false, false, true, false, 5, false, NULL, NULL },
		{ 74, 3, false, false, false, true, 74, false, NULL, NULL },
		{ 153, 9, false, false, true, true, 153, false, NULL, NULL },
		/*
		 * Trying to go beyond the maximum while already there should
		 * not notify or generate a message because TMD_ENTRANCED does
		 * not stack.
		 */
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 1, true, true, false, false, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 11, true, false, false, false, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 129, false, true, false, false, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 1070, false, false, false, false, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 1, true, true, true, false, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 18, true, true, false, true, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 264, true, true, true, true, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 1, true, false, true, false, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 33, true, false, false, true, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 198, true, false, true, true, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 1, false, true, true, false, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 53, false, true, false, true, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 206, false, true, true, true, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 1, false, false, true, false, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 12, false, false, false, true, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_ENTRANCED].grade->next->max, 1032, false, false, true, true, timed_effects[TMD_ENTRANCED].grade->next->max, false, NULL, NULL },
	};
	int old_will, i;

	old_will = player->state.skill_use[SKILL_WILL];
	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		bool result;

		reset_event_counters(st, timed_effects[TMD_ENTRANCED].msgt);
		if (test_cases[i].protected) {
			player->state.flags[OF_FREE_ACT] = 1;
			/*
			 * Set the player's will so all saving throws will
			 * succeed.
			 */
			player->state.skill_use[SKILL_WILL] = 10;
		} else {
			player->state.flags[OF_FREE_ACT] = 0;
			/*
			 * Set the player's will so all saving throws will fail.
			 */
			player->state.skill_use[SKILL_WILL] = 0;
		}
		player->known_state.flags[OF_FREE_ACT] = 0;
		player->timed[TMD_ENTRANCED] = test_cases[i].in;
		result = player_inc_timed(player, TMD_ENTRANCED,
			test_cases[i].inc, test_cases[i].notify,
			test_cases[i].disturb, test_cases[i].check);
		eq(result, test_cases[i].notified);
		eq(player->timed[TMD_ENTRANCED], test_cases[i].out);
		if (test_cases[i].change_msg) {
			eq(st->n_tracked, 1);
			require(st->last_tracked_msg
				&& streq(st->last_tracked_msg,
				test_cases[i].change_msg));
		} else {
			eq(st->n_tracked, 0);
		}
		if (test_cases[i].recover_msg) {
			eq(st->n_recover, 1);
			require(st->last_recover_msg
				&& streq(st->last_recover_msg,
				test_cases[i].recover_msg));
		} else {
			eq(st->n_recover, 0);
		}
		eq(st->n_untracked, 0);
		eq(st->input_flushed, (test_cases[i].notified
			&& test_cases[i].disturb));
	}
	player->state.skill_use[SKILL_WILL] = old_will;
	ok;
}

/*
 * Test player_dec_timed() with on/off state and messages for upward change of
 * grade and overall end.
 */
static int test_dec_timed0(void *state) {
	struct test_timed_state *st = (struct test_timed_state*) state;
	struct {
		int16_t in;
		int dec;
		bool notify;
		bool disturb;
		int16_t out;
		bool notified;
		const char *change_msg;
		const char *recover_msg;
	} test_cases[] = {
		/*
		 * No change from zero should never notify or issue a message,
		 * regardless of notify or disturb.
		 */
		{ 0, 0, true, true, 0, false, NULL, NULL },
		{ 0, 0, true, false, 0, false, NULL, NULL },
		{ 0, 0, false, true, 0, false, NULL, NULL },
		{ 0, 0, false, false, 0, false, NULL, NULL },
		/*
		 * Going from zero to a negative value is coerced to no change.
		 * So, no notification or messages.
		 */
		{ 0, 1, true, true, 0, false, NULL, NULL },
		{ 0, 62, false, true, 0, false, NULL, NULL },
		{ 0, 351, true, false, 0, false, NULL, NULL },
		{ 0, 1388, false, false, 0, false, NULL, NULL },
		/*
		 * No change from the current nonzero value should never notify
		 * or issue a message, regardless of notify or disturb.
		 */
		{ 1, 0, true, true, 1, false, NULL, NULL },
		{ 54, 0, true, false, 54, false, NULL, NULL },
		{ 227, 0, false, true, 227, false, NULL, NULL },
		{ 1401, 0, false, false, 1401, false, NULL, NULL },
		{ timed_effects[TMD_FAST].grade->next->max, 0, true, true, timed_effects[TMD_FAST].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_FAST].grade->next->max, 0, true, false, timed_effects[TMD_FAST].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_FAST].grade->next->max, 0, false, true, timed_effects[TMD_FAST].grade->next->max, false, NULL, NULL },
		{ timed_effects[TMD_FAST].grade->next->max, 0, false, false, timed_effects[TMD_FAST].grade->next->max, false, NULL, NULL },
		/*
		 * Going down a grade will only always notify because the
		 * effect lapses; a recover message will be issued.
		 */
		{ 1, 3, true, true, 0, true, NULL, timed_effects[TMD_FAST].on_end },
		{ 90, 90, false, true, 0, true, NULL, timed_effects[TMD_FAST].on_end },
		{ 411, 500, true, false, 0, true, NULL, timed_effects[TMD_FAST].on_end },
		{ 4086, 4086, false, false, 0, true, NULL, timed_effects[TMD_FAST].on_end },
		{ timed_effects[TMD_FAST].grade->next->max, timed_effects[TMD_FAST].grade->next->max, true, true, 0, true, NULL, timed_effects[TMD_FAST].on_end },
		{ timed_effects[TMD_FAST].grade->next->max, timed_effects[TMD_FAST].grade->next->max + 167, false, true, 0, true, NULL, timed_effects[TMD_FAST].on_end },
		{ timed_effects[TMD_FAST].grade->next->max, timed_effects[TMD_FAST].grade->next->max, true, false, 0, true, NULL, timed_effects[TMD_FAST].on_end },
		{ timed_effects[TMD_FAST].grade->next->max, timed_effects[TMD_FAST].grade->next->max + 2143, false, false, 0, true, NULL, timed_effects[TMD_FAST].on_end },
		/*
		 * Decreasing within the same grade will only notify if
		 * requested; no messages will be generated because there
		 * isn't an on_decrease message.
		 */
		{ 2, 1, true, true, 1, true, NULL, NULL },
		{ 92, 38, false, true, 54, false, NULL, NULL },
		{ 705, 700, true, false, 5, true, NULL, NULL },
		{ 4286, 7, false, false, 4279, false, NULL, NULL },
	};
	int i;

	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		bool result;

		reset_event_counters(st, timed_effects[TMD_FAST].msgt);
		player->timed[TMD_FAST] = test_cases[i].in;
		result = player_dec_timed(player, TMD_FAST,
			test_cases[i].dec, test_cases[i].notify,
			test_cases[i].disturb);
		eq(result, test_cases[i].notified);
		eq(player->timed[TMD_FAST], test_cases[i].out);
		if (test_cases[i].change_msg) {
			eq(st->n_tracked, 1);
			require(st->last_tracked_msg
				&& streq(st->last_tracked_msg,
				test_cases[i].change_msg));
		} else {
			eq(st->n_tracked, 0);
		}
		if (test_cases[i].recover_msg) {
			eq(st->n_recover, 1);
			require(st->last_recover_msg
				&& streq(st->last_recover_msg,
				test_cases[i].recover_msg));
		} else {
			eq(st->n_recover, 0);
		}
		eq(st->n_untracked, 0);
		eq(st->input_flushed, (test_cases[i].notified
			&& test_cases[i].disturb));
	}
	ok;
}

/*
 * Test player_clear_timed() with on/off state and messages for upward change
 * of grade and overall end.
 */
static int test_clear_timed0(void *state) {
	struct test_timed_state *st = (struct test_timed_state*) state;

	struct {
		int16_t in;
		bool notify;
		bool disturb;
		bool notified;
		const char *change_msg;
		const char *recover_msg;
	} test_cases[] = {
		/*
		 * No change from zero should never notify or issue a message,
		 * regardless of notify or disturb.
		 */
		{ 0, true, true, false, NULL, NULL },
		{ 0, true, false, false, NULL, NULL },
		{ 0, false, true, false, NULL, NULL },
		{ 0, false, false, false, NULL, NULL },
		/*
		 * Going down a grade will only notify if requested because the
		 * new grade does not have a down message; when notifying, a
		 * recover message will be issued.
		 */
		{ 1, true, true, true, NULL, timed_effects[TMD_FAST].on_end },
		{ 90, false, true, false, NULL, NULL },
		{ 458, true, false, true, NULL, timed_effects[TMD_FAST].on_end },
		{ 8192, false, false, false, NULL, NULL },
		{ timed_effects[TMD_FAST].grade->next->max, true, true, true, NULL, timed_effects[TMD_FAST].on_end },
		{ timed_effects[TMD_FAST].grade->next->max, false, true, false, NULL, NULL },
		{ timed_effects[TMD_FAST].grade->next->max, true, false, true, NULL, timed_effects[TMD_FAST].on_end },
		{ timed_effects[TMD_FAST].grade->next->max, false, false, false, NULL, NULL },
	};
	int i;

	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		bool result;

		reset_event_counters(st, timed_effects[TMD_FAST].msgt);
		player->timed[TMD_FAST] = test_cases[i].in;
		result = player_clear_timed(player, TMD_FAST,
			test_cases[i].notify, test_cases[i].disturb);
		eq(result, test_cases[i].notified);
		eq(player->timed[TMD_FAST], 0);
		if (test_cases[i].change_msg) {
			eq(st->n_tracked, 1);
			require(st->last_tracked_msg
				&& streq(st->last_tracked_msg,
				test_cases[i].change_msg));
		} else {
			eq(st->n_tracked, 0);
		}
		if (test_cases[i].recover_msg) {
			eq(st->n_recover, 1);
			require(st->last_recover_msg
				&& streq(st->last_recover_msg,
				test_cases[i].recover_msg));
		} else {
			eq(st->n_recover, 0);
		}
		eq(st->n_untracked, 0);
		eq(st->input_flushed, (test_cases[i].notified
			&& test_cases[i].disturb));
	}
	ok;
}

const char *suite_name = "player/timed";

struct test tests[] = {
	{ "name2idx0", test_name2idx0 },
	{ "timed_grade_eq0", test_timed_grade_eq0 },
	{ "set_timed0", test_set_timed0 },
	{ "set_timed1", test_set_timed1 },
	{ "set_timed2", test_set_timed2 },
	{ "set_timed3", test_set_timed3 },
	{ "set_timed4", test_set_timed4 },
	{ "inc_check0", test_inc_check0 },
	{ "inc_timed0", test_inc_timed0 },
	{ "inc_timed1", test_inc_timed1 },
	{ "dec_timed0", test_dec_timed0 },
	{ "clear_timed0", test_clear_timed0 },
	{ NULL, NULL }
};
