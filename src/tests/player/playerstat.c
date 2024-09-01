/* player/playerstat */

#include "unit-test.h"
#include "unit-test-data.h"

#include "player-birth.h"
#include "player.h"

int setup_tests(void **state) {
	struct player *p = mem_zalloc(sizeof *p);
	z_info = mem_zalloc(sizeof(struct angband_constants));
	z_info->pack_size = 23;
	player_init(p);
	*state = p;
	return 0;
}

int teardown_tests(void *state) {
	struct player *p = state;
	mem_free(z_info);
	mem_free(p->upkeep->inven);
	mem_free(p->upkeep);
	mem_free(p->timed);
	mem_free(p->obj_k->brands);
	mem_free(p->obj_k->slays);
	mem_free(p->obj_k);
	mem_free(state);
	return 0;
}

static int test_stat_inc(void *state) {
	struct player *p = state;
	int v;

	p->stat_base[STAT_STR] = 1;
	v = player_stat_inc(p, STAT_STR);
	require(v);
	p->stat_base[STAT_STR] = 5;
	player_stat_inc(p, STAT_STR);
	eq(p->stat_base[STAT_STR], 6);
	player_stat_inc(p, STAT_STR);
	eq(p->stat_base[STAT_STR], 7);
	player_stat_inc(p, STAT_STR);
	eq(p->stat_base[STAT_STR], 8);
	player_stat_inc(p, STAT_STR);
	require(p->stat_base[STAT_STR] > 8);
	ok;
}

static int test_stat_dec(void *state) {
	struct player *p = state;

	p->stat_base[STAT_STR] = 3;
	p->stat_drain[STAT_STR] = 0;
	player_stat_dec(p, STAT_STR);
	p->stat_base[STAT_STR] = 5;
	p->stat_drain[STAT_STR] = 0;
	player_stat_dec(p, STAT_STR);
	eq(p->stat_drain[STAT_STR], -1);
	player_stat_dec(p, STAT_STR);
	eq(p->stat_base[STAT_STR], 5);
	eq(p->stat_drain[STAT_STR], -2);
	ok;
}

const char *suite_name = "player/playerstat";
struct test tests[] = {
	{ "stat-inc", test_stat_inc },
	{ "stat-dec", test_stat_dec },
	{ NULL, NULL }
};
