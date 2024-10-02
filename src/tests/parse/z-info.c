/* parse/z-info */
/* Exercise parsing used for constants.txt. */

#include "unit-test.h"
#include "unit-test-data.h"

#include "init.h"


int setup_tests(void **state) {
	*state = constants_parser.init();
	return !*state;
}

int teardown_tests(void *state) {
	struct angband_constants *z = parser_priv(state);
	mem_free(z);
	parser_destroy(state);
	return 0;
}

static int test_negative(void *state) {
	struct parser *p = (struct parser*) state;
	errr r = parser_parse(p, "level-max:monsters:-1");

	eq(r, PARSE_ERROR_INVALID_VALUE);
	r = parser_parse(p, "mon-gen:change:-1");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	r = parser_parse(p, "mon-play:mult-rate:-1");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	r = parser_parse(p, "dun-gen:room-max:-1");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	r = parser_parse(p, "world:dungeon-hgt:-1");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	r = parser_parse(p, "carry-cap:pack-size:-1");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	r = parser_parse(p, "obj-make:great-obj:-1");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	r = parser_parse(p, "player:max-sight:-1");
	eq(r, PARSE_ERROR_INVALID_VALUE);
	ok;
}

static int test_baddirective(void *state) {
	struct parser *p = (struct parser*) state;
	errr r = parser_parse(state, "level-max:D:1");

	eq(r, PARSE_ERROR_UNDEFINED_DIRECTIVE);
	r = parser_parse(p, "mon-gen:xyzzy:5");
	eq(r, PARSE_ERROR_UNDEFINED_DIRECTIVE);
	r = parser_parse(p, "mon-play:xyzzy:10");
	eq(r, PARSE_ERROR_UNDEFINED_DIRECTIVE);
	r = parser_parse(p, "dun-gen:xyzzy:3");
	eq(r, PARSE_ERROR_UNDEFINED_DIRECTIVE);
	r = parser_parse(p, "world:xyzzy:170");
	eq(r, PARSE_ERROR_UNDEFINED_DIRECTIVE);
	r = parser_parse(p, "carry-cap:xyzzy:40");
	eq(r, PARSE_ERROR_UNDEFINED_DIRECTIVE);
	r = parser_parse(p, "obj-make:xyzzy:5000");
	eq(r, PARSE_ERROR_UNDEFINED_DIRECTIVE);
	r = parser_parse(p, "player:xyzzy:300");
	eq(r, PARSE_ERROR_UNDEFINED_DIRECTIVE);
	ok;
}

#define TEST_CONSTANT(l,u,section) \
	static int test_##l(void *s) { \
		struct angband_constants *m = parser_priv(s); \
		char buf[64]; \
		errr r; \
		snprintf(buf, sizeof(buf), "%s:%s:%d", section, u, __LINE__); \
		r = parser_parse(s, buf); \
		eq(m->l, __LINE__); \
		eq(r, 0); \
		ok; \
	}

TEST_CONSTANT(level_monster_max, "monsters", "level-max")

TEST_CONSTANT(alloc_monster_chance, "chance", "mon-gen")
TEST_CONSTANT(monster_group_max, "group-max", "mon-gen")

TEST_CONSTANT(repro_monster_rate, "mult-rate", "mon-play")
TEST_CONSTANT(mana_cost, "mana-cost", "mon-play")
TEST_CONSTANT(mana_max, "mana-max", "mon-play")
TEST_CONSTANT(flee_range, "flee-range", "mon-play")
TEST_CONSTANT(turn_range, "turn-range", "mon-play")
TEST_CONSTANT(hide_range, "hide-range", "mon-play")
TEST_CONSTANT(wander_range, "wander-range", "mon-play")
TEST_CONSTANT(mon_regen_hp_period, "regen-hp-period", "mon-play")
TEST_CONSTANT(mon_regen_sp_period, "regen-sp-period", "mon-play")

TEST_CONSTANT(level_room_max, "room-max", "dun-gen")
TEST_CONSTANT(level_room_min, "room-min", "dun-gen")
TEST_CONSTANT(block_hgt, "block-hgt", "dun-gen")
TEST_CONSTANT(block_wid, "block-wid", "dun-gen")

TEST_CONSTANT(dun_depth, "dun-depth", "world")
TEST_CONSTANT(max_depth, "max-depth", "world")
TEST_CONSTANT(day_length, "day-length", "world")
TEST_CONSTANT(dungeon_hgt, "dungeon-hgt", "world")
TEST_CONSTANT(dungeon_wid, "dungeon-wid", "world")
TEST_CONSTANT(move_energy, "move-energy", "world")
TEST_CONSTANT(flow_max, "flow-max", "world")

TEST_CONSTANT(pack_size, "pack-size", "carry-cap")
TEST_CONSTANT(floor_size, "floor-size", "carry-cap")

TEST_CONSTANT(max_obj_depth, "max-depth", "obj-make")
TEST_CONSTANT(great_obj, "great-obj", "obj-make")
TEST_CONSTANT(great_ego, "great-spec", "obj-make")
TEST_CONSTANT(default_torch, "default-torch", "obj-make")
TEST_CONSTANT(fuel_torch, "fuel-torch", "obj-make")
TEST_CONSTANT(default_lamp, "default-lamp", "obj-make")
TEST_CONSTANT(fuel_lamp, "fuel-lamp", "obj-make")
TEST_CONSTANT(self_arts_max, "self-arts", "obj-make")

TEST_CONSTANT(max_sight, "max-sight", "player")
TEST_CONSTANT(max_range, "max-range", "player")
TEST_CONSTANT(start_exp, "start-exp", "player")
TEST_CONSTANT(ability_cost, "ability-cost", "player")
TEST_CONSTANT(stealth_bonus, "stealth-bonus", "player")
TEST_CONSTANT(player_regen_period, "regen-period", "player")

const char *suite_name = "parse/z-info";
struct test tests[] = {
	{ "negative", test_negative },
	{ "baddirective", test_baddirective },
	{ "monsters_max", test_level_monster_max },
	{ "mon_chance", test_alloc_monster_chance },
	{ "group_max", test_monster_group_max },
	{ "mult_rate", test_repro_monster_rate },
	{ "mana_cost", test_mana_cost },
	{ "mana_max", test_mana_max },
	{ "flee_range", test_flee_range },
	{ "turn_range", test_turn_range },
	{ "hide_range", test_hide_range },
	{ "wander_range", test_wander_range },
	{ "mon_regen_hp_period", test_mon_regen_hp_period },
	{ "mon_regen_sp_period", test_mon_regen_sp_period },
	{ "room_max", test_level_room_max },
	{ "room_min", test_level_room_min },
	{ "block_hgt", test_block_hgt },
	{ "block_wid", test_block_wid },
	{ "dun_depth", test_dun_depth },
	{ "max_depth", test_max_depth },
	{ "day_length", test_day_length },
	{ "dungeon_hgt", test_dungeon_hgt },
	{ "dungeon_wid", test_dungeon_wid },
	{ "move_energy", test_move_energy },
	{ "flow_max", test_flow_max },
	{ "pack_size", test_pack_size },
	{ "floor_size", test_floor_size },
	{ "max_obj_depth", test_max_obj_depth },
	{ "great_obj", test_great_obj },
	{ "great_ego", test_great_ego },
	{ "default_torch", test_default_torch },
	{ "fuel_torch", test_fuel_torch },
	{ "fuel_lamp", test_fuel_lamp },
	{ "default_lamp", test_default_lamp },
	{ "self_arts_max", test_self_arts_max },
	{ "max_sight", test_max_sight },
	{ "max_range", test_max_range },
	{ "start_exp", test_start_exp },
	{ "ability_cost", test_ability_cost },
	{ "stealth_bonus", test_stealth_bonus },
	{ "player_regen_period", test_player_regen_period },
	{ NULL, NULL }
};
