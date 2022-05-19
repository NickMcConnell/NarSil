/* monster/attack */

#include "unit-test.h"
#include "unit-test-data.h"

#include "mon-attack.h"
#include "mon-lore.h"
#include "monster.h"
#include "option.h"
#include "player-timed.h"
#include "ui-input.h"

int setup_tests(void **state) {
	struct monster_race *r = &test_r_human;
	struct monster *m = mem_zalloc(sizeof *m);
	int y, x;
	textui_input_init();
	z_info = mem_zalloc(sizeof(struct angband_constants));
	z_info->mon_blows_max = 2;
	projections = test_projections;
	l_list = &test_lore;
	m->race = r;
	m->midx = 1;
	r_info = r;
	*state = m;

	cave = &test_cave;
	cave->squares = mem_zalloc(cave->height * sizeof(struct square*));
	for (y = 0; y < cave->height; y++) {
		cave->squares[y] = mem_zalloc(cave->width * sizeof(struct square));
		for (x = 0; x < cave->width; x++) {
			cave->squares[y][x].info = mem_zalloc(SQUARE_SIZE * sizeof(bitflag));
		}
	}
	cave->monsters = mem_zalloc(2 *sizeof(struct monster));
	cave->mon_max = 1;

	rand_fix(100);
	return 0;
}

int teardown_tests(void *state) {
	struct monster *m = state;
	int y, x;
	mem_free(cave->monsters);
	for (y = 0; y < cave->height; y++) {
		for (x = 0; x < cave->width; x++) {
			mem_free(cave->squares[y][x].info);
		}
		mem_free(cave->squares[y]);
	}
	mem_free(cave->squares);
	mem_free(m);
	mem_free(z_info);
	return 0;
}

static int mdam(struct monster *m)
{
	return m->race->blow[0].dice.dice;
}

static int take1(struct player *p, struct monster *m, struct blow_method *blow,
				 struct blow_effect *eff)
{
	int old, new;
	cave = &test_cave;
	m->race->blow[0].effect = eff;
	m->race->blow[0].method = blow;
	p->chp = p->mhp;
	old = p->chp;
	make_attack_normal(m, p);
	new = p->chp;
	p->chp = p->mhp;
	return old - new;
}

static int test_blows(void *state) {
	struct monster *m = state;
	struct player *p = &test_player;
	int delta;

	p->upkeep = &test_player_upkeep;

	delta = take1(p, m, &test_blow_method, &test_blow_effect_hurt);
	eq(delta, mdam(m));

	ok;
}

static int test_effects(void *state) {
	struct monster *m = state;
	struct player *p = &test_player;
	int delta;

	options_init_defaults(&p->opts);
	p->upkeep = &test_player_upkeep;

	delta = take1(p, m, &test_blow_method, &test_blow_effect_acid);
	require(delta > 0);
	delta = take1(p, m, &test_blow_method, &test_blow_effect_fire);
	require(delta > 0);
	delta = take1(p, m, &test_blow_method, &test_blow_effect_cold);
	require(delta > 0);

	ok;
}

const char *suite_name = "monster/attack";
struct test tests[] = {
	{ "blows", test_blows },
	{ "effects", test_effects },
	{ NULL, NULL },
};
