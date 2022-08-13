/* unit-test-data.h
 * Predefined data for tests
 */

#ifndef UNIT_TEST_DATA
#define UNIT_TEST_DATA

#ifndef TEST_DATA
  #ifdef __GNUC__
    #define TEST_DATA __attribute__ ((unused))
  #else
    #define TEST_DATA 
  #endif
#endif /* TEST_DATA */

#include "angband.h"
#include "init.h"
#include "mon-lore.h"
#include "monster.h"
#include "obj-tval.h"
#include "player.h"
#include "player-calcs.h"
#include "project.h"

/* 21 = TMD_MAX */
static int16_t TEST_DATA test_timed[21] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0
};

static struct object_base TEST_DATA sword_base = {
	.name = "Test Sword",
	.tval = TV_SWORD,
	.next = NULL,
	.break_perc = 50,
};

static struct object_base TEST_DATA light_base = {
	.name = "Test Light~",
	.tval = TV_LIGHT,
	.next = NULL,
	.break_perc = 50,
};

static struct object_base TEST_DATA flask_base = {
	.name = "Test Flask~",
	.tval = TV_FLASK,
	.next = NULL,
	.break_perc = 100,
};

static struct object_base TEST_DATA horn_base = {
	.name = "Test Horn~",
	.tval = TV_HORN,
	.next = NULL,
};

static struct artifact TEST_DATA test_artifact_sword = {
	.name = "Test Artifact",
	.text = "A test artifact.",
	.aidx = 0,
	.next = NULL,
	.tval = TV_SWORD,
	.sval = 6, //Hack - depends on edit file order - Longsword (NRM)
	.att = 1,
	.evn = 2,
	.pd = 3,
	.ps = 5,
	.dd = 2,
	.ds = 5,
	.weight = 16,
	.cost = 40,
};

static struct object_kind TEST_DATA test_longsword = {
	.name = "Test Longsword",
	.text = "A test longsword [0].",
	.base = &sword_base,
	.kidx = 0,
	.tval = TV_SWORD,
	.sval = 8, //Hack - depends on edit file order - Long Sword (NRM)
	.pval = 0,
	.modifiers = { 
		[OBJ_MOD_STR] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_DEX] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_CON] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_GRA] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_MELEE] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_ARCHERY] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_EVASION] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_STEALTH] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_PERCEPTION] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_WILL] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_SMITHING] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_SONG] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_DAMAGE_SIDES] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_TUNNEL] = { 0, 0, 0, 0 }, 
	},
	.att = 0,
	.evn = 1,
	.dd = 2,
	.ds = 5,
	.pd = 0,
	.ps = 0,
	.weight = 30,

	.cost = 20,

	.d_attr = 0,
	.d_char = L'|',

	.level = 0,

	.effect = NULL,

	.gen_mult_prob = 0,
	.flavor = NULL,
};

static struct object_kind TEST_DATA test_torch = {
	.name = "Test Torch",
	.text = "A test torch [1].",
	.base = &light_base,
	.next = NULL,
	.kidx = 2,
	.tval = TV_LIGHT,
	.sval = 1, //Hack - depends on edit file order - Wooden Torch (NRM)
	.pval = 3000,
	.weight = 20,

	.cost = 1,

	.flags = { 0, 0, 8, 0, 4, 2 },
	.kind_flags = { 0 },

	.modifiers = { 
		[OBJ_MOD_STR] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_DEX] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_CON] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_GRA] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_MELEE] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_ARCHERY] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_EVASION] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_STEALTH] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_PERCEPTION] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_WILL] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_SMITHING] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_SONG] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_DAMAGE_SIDES] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_TUNNEL] = { 0, 0, 0, 0 }, 
	},
	.el_info = {
		[ELEM_ACID] = { 0, 0 },
		[ELEM_FIRE] = { 0, 0 },
		[ELEM_COLD] = { 0, 0 },
		[ELEM_POIS] = { 0, 0 },
		[ELEM_DARK] = { 0, 0 },
	},

	.brands = NULL,
	.slays = NULL,

	.d_attr = 7,
	.d_char = L'~',

	.level = 1,
	.alloc = NULL,

	.effect = NULL,
	.effect_msg = NULL,
	.charge = {
		.base = 0,
		.dice = 0,
		.sides = 0,
		.m_bonus = 0,
	},

	.gen_mult_prob = 0,
	.stack_size = {
		.base = 0,
		.dice = 0,
		.sides = 0,
		.m_bonus = 0,
	},
	.flavor = NULL,
};

static struct object_kind TEST_DATA test_lantern = {
	.name = "Test Lantern",
	.text = "A test lantern.",
	.base = &light_base,
	.next = NULL,
 	.kidx = 3,
	.tval = TV_LIGHT,
	.sval = 2, //Hack - depends on edit file order -  Lantern (NRM)
	.pval = 7000,

	.weight = 30,

	.cost = 1,

	.flags = { 0, 0, 16, 0, 0, 0 }, /* OF_TAKES_FUEL */
	.kind_flags = { 0 },

	.modifiers = { 
		[OBJ_MOD_STR] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_DEX] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_CON] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_GRA] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_MELEE] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_ARCHERY] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_EVASION] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_STEALTH] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_PERCEPTION] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_WILL] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_SMITHING] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_SONG] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_DAMAGE_SIDES] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_TUNNEL] = { 0, 0, 0, 0 }, 
	},
	.el_info = {
		[ELEM_ACID] = { 0, 0 },
		[ELEM_FIRE] = { 0, 0 },
		[ELEM_COLD] = { 0, 0 },
		[ELEM_POIS] = { 0, 0 },
		[ELEM_DARK] = { 0, 0 },
	},

	.brands = NULL,
	.slays = NULL,

	.d_attr = 0,
	.d_char = L'~',

	.level = 1,
	.alloc = NULL,

	.effect = NULL,
	.effect_msg = NULL,
	.charge = {
		.base = 0,
		.dice = 0,
		.sides = 0,
		.m_bonus = 0,
	},

	.gen_mult_prob = 0,
	.stack_size = {
		.base = 0,
		.dice = 0,
		.sides = 0,
		.m_bonus = 0,
	},
	.flavor = NULL,
};

static struct object_kind TEST_DATA test_flask = {
	.name = "Test Flask",
	.text = "A test flask.",
	.base = &flask_base,
	.kidx = 1,
	.tval = TV_FLASK,
	.sval = 0,
	.pval = 3000,

	.modifiers = { 
		[OBJ_MOD_STR] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_DEX] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_CON] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_GRA] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_MELEE] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_ARCHERY] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_EVASION] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_STEALTH] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_PERCEPTION] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_WILL] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_SMITHING] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_SONG] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_DAMAGE_SIDES] = { 0, 0, 0, 0 }, 
		[OBJ_MOD_TUNNEL] = { 0, 0, 0, 0 }, 
	},
	.el_info = {
		[ELEM_ACID] = { 0, 0 },
		[ELEM_FIRE] = { 0, 0 },
		[ELEM_COLD] = { 0, 0 },
		[ELEM_POIS] = { 0, 0 },
		[ELEM_DARK] = { 0, 0 },
	},
	.weight = 20,

	.cost = 3,

	.d_attr = 11,
	.d_char = L'!',

	.level = 1,
	.alloc = NULL,

	.effect = NULL,
	.effect_msg = NULL,
	.charge = {
		.base = 0,
		.dice = 0,
		.sides = 0,
		.m_bonus = 0,
	},

	.gen_mult_prob = 0,
	.stack_size = {
		.base = 0,
		.dice = 0,
		.sides = 0,
		.m_bonus = 0,
	},
	.flavor = NULL,
};

static struct player_race TEST_DATA test_race = {
	.name = "TestRace",
	.stat_adj = {
		[STAT_STR] = +2,
		[STAT_DEX] = +1,
		[STAT_CON] = +3,
		[STAT_GRA] = -1,
	},
	.skill_adj = {
		[SKILL_MELEE] = 2,
		[SKILL_ARCHERY] = 0,
		[SKILL_EVASION] = -2,
		[SKILL_STEALTH] = 0,
		[SKILL_PERCEPTION] = 0,
		[SKILL_WILL] = 1,
		[SKILL_SMITHING] = 0,
		[SKILL_SONG] = -2,
	},

	.b_age = 14,
	.m_age = 6,

	.base_hgt = 72,
	.mod_hgt = 6,
	.base_wgt = 150,
	.mod_wgt = 20,


	.history = NULL,
};

static struct player_house TEST_DATA test_house = {
	.name = "House of TestHouse",
	.alt_name = "TestHouse's house",
	.short_name = "TestHouse",
	.stat_adj = {
		[STAT_STR] = +1,
		[STAT_DEX] = 0,
		[STAT_CON] = +1,
		[STAT_GRA] = 0,
	},
	.skill_adj = {
		[SKILL_MELEE] = 0,
		[SKILL_ARCHERY] = 1,
		[SKILL_EVASION] = -2,
		[SKILL_STEALTH] = 1,
		[SKILL_PERCEPTION] = 1,
		[SKILL_WILL] = 1,
		[SKILL_SMITHING] = 2,
		[SKILL_SONG] = 0,
	},
};

static struct player_sex TEST_DATA test_sex = {
	.name = "TestSex",
	.possessive = "their",
	.poetry_name = NULL
};
	
static struct start_item TEST_DATA start_torch = {
	.tval = TV_LIGHT,
	.sval = 1, //Hack - depends on edit file order - Wooden Torch (NRM)
	.min = 3,
	.max = 5,
	.next = NULL,
};

static struct start_item TEST_DATA start_longsword = {
	.tval = TV_SWORD,
	.sval = 8, //Hack - depends on edit file order - Long Sword (NRM)
	.min = 1,
	.max = 1,
	.next = &start_torch,
};

static struct monster_base TEST_DATA test_rb_info = {
	.next = NULL,
	.name = "townsfolk",
	.text = "Townsfolk",
	.flags = "\0\0\0\0\0\0\0\0\0\0",
	.d_char = 116,
	.pain = NULL,
	
};

static struct blow_message TEST_DATA test_blow_message = {
	.act_msg = "hits {target}",
	.next = NULL
};

static struct blow_method TEST_DATA test_blow_method = {
	.name = "HIT",
	.cut = true,
	.stun = true,
	.miss = false,
	.prt = false,
	.msgt = 34,
	.messages = &test_blow_message,
	.num_messages = 1,
	.desc = "hit",
	.next = NULL
};

static struct blow_effect TEST_DATA test_blow_effect_hurt = {
	.name = "HURT",
	.power = 40,
	.eval = 0,
	.desc = "attack",
	.next = NULL
};

static struct blow_effect TEST_DATA test_blow_effect_poison = {
	.name = "POISON",
	.power = 20,
	.eval = 10,
	.desc = "poison",
	.next = NULL
};

static struct blow_effect TEST_DATA test_blow_effect_acid = {
	.name = "ACID",
	.power = 20,
	.eval = 20,
	.desc = "shoot acid",
	.next = NULL
};

static struct blow_effect TEST_DATA test_blow_effect_elec = {
	.name = "ELEC",
	.power = 40,
	.eval = 10,
	.desc = "electrify",
	.next = NULL
};

static struct blow_effect TEST_DATA test_blow_effect_fire = {
	.name = "FIRE",
	.power = 40,
	.eval = 10,
	.desc = "burn",
	.next = NULL
};

static struct blow_effect TEST_DATA test_blow_effect_cold = {
	.name = "COLD",
	.power = 40,
	.eval = 10,
	.desc = "freeze",
	.next = NULL
};

static struct blow_effect TEST_DATA test_blow_effect_blind = {
	.name = "BLIND",
	.power = 0,
	.eval = 20,
	.desc = "blind",
	.next = NULL
};

static struct monster_blow TEST_DATA test_blow[4] = {
	{
		.method = &test_blow_method,
		.effect = &test_blow_effect_hurt,
		.dice = {
			.base = 5,
			.dice = 3,
			.sides = 1,
			.m_bonus = 0,
		},
		.times_seen = 1,
	},
	{
		.method = NULL,
		.effect = NULL,
		.dice = {
			.base = 0,
			.dice = 0,
			.sides = 0,
			.m_bonus = 0,
		},
		.times_seen = 0,
	},
	{
		.method = NULL,
		.effect = NULL,
		.dice = {
			.base = 0,
			.dice = 0,
			.sides = 0,
			.m_bonus = 0,
		},
		.times_seen = 0,
	},
	{
		.method = NULL,
		.effect = NULL,
		.dice = {
			.base = 0,
			.dice = 0,
			.sides = 0,
			.m_bonus = 0,
		},
		.times_seen = 0,
	}
};

static bool TEST_DATA test_blows_known[4] = {
	true,
	false,
	false,
	false,
};

static struct monster_race TEST_DATA test_r_human = {
	.next = NULL,
	.ridx = 0,
	.name = "Human",
	.text = "A random test human",

	.base = &test_rb_info,

	.hdice = 8,
	.hside = 4,
	.evn = 5,
	.pd = 3,
	.ps = 4,
	.sleep = 10,
	.per = 4,
	.stl = 3,
	.wil = 1,
	.song = 0,
	.speed = 2,
	.light = 1,
	.freq_ranged = 0,

	.blow = &test_blow[0],

	.level = 1,
	.rarity = 1,

	.d_attr = 0,
	.d_char = '@',

	.max_num = 100,
	.cur_num = 0,

	.drops = NULL,
};

static monster_lore TEST_DATA test_lore = {
	.ridx = 0,
	.deaths = 0,
	.pkills = 0,
	.psights = 1,
	.tkills = 5,
	.tsights = 10,
	.notice = 1,
	.ignore = 4,
	.drop_item = 0,
	.ranged = 0,
	.mana = 0,
	.spell_power = 0,

	.blows = &test_blow[0],

	.flags = "\0\0\0\0\0\0\0\0\0\0",
	.spell_flags = "\0\0\0",
	.drops = NULL,
	.all_known = false,
	.blow_known = &test_blows_known[0],
	.armour_known = false,
	.drop_known = false,
	.sleep_known = false,
	.ranged_freq_known = false
};

static struct angband_constants TEST_DATA test_z_info = {
	.f_max    = 2,
	.trap_max = 2,
	.k_max    = 2,
	.a_max    = 2,
	.e_max    = 2,
	.r_max    = 2,
	.s_max    = 2,
	.pit_max  = 2,
	.act_max  = 2,
	.level_monster_max = 2,
};

static struct equip_slot TEST_DATA test_slot_light = {
	.type = 5,
	.name = "light",
	.obj = NULL,
};

static struct quest TEST_DATA test_quest = {
	.next = NULL,
	.index = 0,
	.name = "Test",
	.level = 1,
	.race = &test_r_human,
	.cur_num = 0,
	.max_num = 4,
};

static struct player_body TEST_DATA test_player_body = {
	.next    = NULL,
	.name    = "Humanoid",
	.count   = 12,
};

static struct player_upkeep TEST_DATA test_player_upkeep = {
	.playing = 1,
	.autosave = 0,
	.generate_level = 0,
	.energy_use = 0,

	.health_who = NULL,
	.monster_race = NULL,
	.object = NULL,
	.object_kind = NULL,

	.notice = 0,
	.update = 0,
	.redraw = 0,

	.command_wrk = 0,

	.create_stair = 0,

	.running = 0,
	.running_withpathfind = 0,
	.running_firststep = 0,

	.inven = NULL,

	.total_weight = 0,
	.inven_cnt = 0,
	.equip_cnt = 0,
};

static struct player TEST_DATA test_player = {
	.grid = { 1, 1 },
	.race = &test_race,
	.house = &test_house,
	.age = 12,
	.ht = 40,
	.wt = 80,
	.max_depth = 10,
	.depth = 6,
	.new_exp = 10,
	.exp = 80,
	.mhp = 20,
	.chp = 14,
	.msp = 12,
	.csp = 11,
	.stat_base = {
		[STAT_STR] = 1,
		[STAT_DEX] = 2,
		[STAT_CON] = 1,
		[STAT_GRA] = 0,
	},
	.stat_drain = {
		[STAT_STR] = 1,
		[STAT_DEX] = 2,
		[STAT_CON] = 1,
		[STAT_GRA] = 0,
	},
	.skill_base = {
		[SKILL_MELEE] = 2,
		[SKILL_ARCHERY] = 0,
		[SKILL_EVASION] = 2,
		[SKILL_STEALTH] = 0,
		[SKILL_PERCEPTION] = 0,
		[SKILL_WILL] = 2,
		[SKILL_SMITHING] = 0,
		[SKILL_SONG] = 6,
	},
	.timed = test_timed,
	.energy = 100,
	.history = "no history",
	.is_dead = 0,
	.wizard = 0,
	.upkeep = &test_player_upkeep,
	.gear = NULL,
};

static struct chunk TEST_DATA test_cave = {
	.name = "Test",
	.turn = 1,
	.depth = 1,

	.height = 5,
	.width = 5,

	.feat_count = NULL,

	.squares = NULL,

	.monsters = NULL,
	.mon_max = 1,
	.mon_cnt = 0,
	.mon_current = -1,
};

static struct projection TEST_DATA test_projections[4] = {
	{
		.index = 0,
		.name = "acid",
		.type = "element",
		.desc = "acid",
		.player_desc = "acid",
		.blind_desc = "acid",
		.msgt = 0,
		.damaging = true,
		.evade = false,
		.obvious = true,
		.wake = true,
		.color = 2,
		.next = NULL
	},
	{
		.index = 1,
		.name = "electricity",
		.type = "element",
		.desc = "electricity",
		.player_desc = "electricity",
		.blind_desc = "electricity",
		.msgt = 0,
		.damaging = true,
		.evade = false,
		.obvious = true,
		.wake = true,
		.color = 6,
		.next = NULL
	},
	{
		.index = 2,
		.name = "fire",
		.type = "element",
		.desc = "fire",
		.player_desc = "fire",
		.blind_desc = "fire",
		.msgt = 0,
		.damaging = true,
		.evade = false,
		.obvious = true,
		.wake = true,
		.color = 4,
		.next = NULL
	},
	{
		.index = 3,
		.name = "cold",
		.type = "element",
		.desc = "cold",
		.player_desc = "cold",
		.blind_desc = "cold",
		.msgt = 0,
		.damaging = true,
		.evade = false,
		.obvious = true,
		.wake = true,
		.color = 1,
		.next = NULL
	}
};

#endif /* !UNIT_TEST_DATA */
