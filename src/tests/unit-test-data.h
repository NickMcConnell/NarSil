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

static char tv_sword_name[16] = "Test Sword";
static struct object_base TEST_DATA sword_base = {
	.name = tv_sword_name,
	.tval = TV_SWORD,
	.next = NULL,
	.break_perc = 50,
};

static char tv_light_name[16] = "Test Light~";
static struct object_base TEST_DATA light_base = {
	.name = tv_light_name,
	.tval = TV_LIGHT,
	.next = NULL,
	.break_perc = 50,
};

static char tv_flask_name[16] = "Test Flask~";
static struct object_base TEST_DATA flask_base = {
	.name = tv_flask_name,
	.tval = TV_FLASK,
	.next = NULL,
	.break_perc = 100,
};

static char tv_horn_name[16] = "Test Horn~";
static struct object_base TEST_DATA horn_base = {
	.name = tv_horn_name,
	.tval = TV_HORN,
	.next = NULL,
};

static char artifact_sword_name[16] = "Test Artifact";
static char artifact_sword_desc[24] = "A test artifact.";
static struct artifact TEST_DATA test_artifact_sword = {
	.name = artifact_sword_name,
	.text = artifact_sword_desc,
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

static char longsword_name[16] = "Test Longsword";
static char longsword_desc[24] = "A test longsword [0].";
static struct object_kind TEST_DATA test_longsword = {
	.name = longsword_name,
	.text = longsword_desc,
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

static char torch_name[16] = "Test Torch";
static char torch_desc[24] = "A test torch [1].";
static struct object_kind TEST_DATA test_torch = {
	.name = torch_name,
	.text = torch_desc,
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

static char lantern_name[16] = "Test Lantern";
static char lantern_desc[24] = "A test lantern.";
static struct object_kind TEST_DATA test_lantern = {
	.name = lantern_name,
	.text = lantern_desc,
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

static char flask_name[16] = "Text Flask";
static char flask_desc[24] = "A text flask.";
static struct object_kind TEST_DATA test_flask = {
	.name = flask_name,
	.text = flask_desc,
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

static char townsfolk_name[16] = "townsfolk";
static char townsfolk_desc[16] = "Townsfolk";
static struct monster_base TEST_DATA test_rb_info = {
	.next = NULL,
	.name = townsfolk_name,
	.text = townsfolk_desc,
	.flags = { '\0', '\0', '\0' },
	.d_char = 116,
	.pain = NULL,
	
};

static char blow_act_msg[16] = "hits {target}";
static struct blow_message TEST_DATA test_blow_message = {
	.act_msg = blow_act_msg,
	.next = NULL
};

static char blow_method_hit_name[16] = "HIT";
static char blow_method_hit_desc[16] = "hit";
static struct blow_method TEST_DATA test_blow_method = {
	.name = blow_method_hit_name,
	.cut = true,
	.stun = true,
	.miss = false,
	.prt = false,
	.msgt = 34,
	.messages = &test_blow_message,
	.num_messages = 1,
	.desc = blow_method_hit_desc,
	.next = NULL
};

static char blow_effect_hurt_name[16] = "HURT";
static char blow_effect_hurt_desc[16] = "attack";
static struct blow_effect TEST_DATA test_blow_effect_hurt = {
	.name = blow_effect_hurt_name,
	.power = 40,
	.eval = 0,
	.desc = blow_effect_hurt_desc,
	.next = NULL
};

static char blow_effect_pois_name[16] = "POISON";
static char blow_effect_pois_desc[16] = "poison";
static struct blow_effect TEST_DATA test_blow_effect_poison = {
	.name = blow_effect_pois_name,
	.power = 20,
	.eval = 10,
	.desc = blow_effect_pois_desc,
	.next = NULL
};

static char blow_effect_acid_name[16] = "ACID";
static char blow_effect_acid_desc[16] = "shoot acid";
static struct blow_effect TEST_DATA test_blow_effect_acid = {
	.name = blow_effect_acid_name,
	.power = 20,
	.eval = 20,
	.desc = blow_effect_acid_desc,
	.next = NULL
};

static char blow_effect_elec_name[16] = "ELEC";
static char blow_effect_elec_desc[16] = "electrify";
static struct blow_effect TEST_DATA test_blow_effect_elec = {
	.name = blow_effect_elec_name,
	.power = 40,
	.eval = 10,
	.desc = blow_effect_elec_desc, 
	.next = NULL
};

static char blow_effect_fire_name[16] = "FIRE";
static char blow_effect_fire_desc[16] = "burn";
static struct blow_effect TEST_DATA test_blow_effect_fire = {
	.name = blow_effect_fire_name,
	.power = 40,
	.eval = 10,
	.desc = blow_effect_fire_desc,
	.next = NULL
};

static char blow_effect_cold_name[16] = "COLD";
static char blow_effect_cold_desc[16] = "freeze";
static struct blow_effect TEST_DATA test_blow_effect_cold = {
	.name = blow_effect_cold_name,
	.power = 40,
	.eval = 10,
	.desc = blow_effect_cold_desc,
	.next = NULL
};

static char blow_effect_blind_name[16] = "BLIND";
static char blow_effect_blind_desc[16] = "blind";
static struct blow_effect TEST_DATA test_blow_effect_blind = {
	.name = blow_effect_blind_name,
	.power = 0,
	.eval = 20,
	.desc = blow_effect_blind_desc,
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

static char human_name[16] = "Human";
static char human_desc[24] = "A random test human.";
static struct monster_race TEST_DATA test_r_human = {
	.next = NULL,
	.ridx = 0,
	.name = human_name,
	.text = human_desc,

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

	.flags = { '\0', '\0', '\0' },
	.spell_flags = { '\0', '\0', '\0' },
	.drops = NULL,
	.all_known = false,
	.blow_known = &test_blows_known[0],
	.armour_known = false,
	.drop_known = false,
	.sleep_known = false,
	.ranged_freq_known = false
};

static struct angband_constants TEST_DATA test_z_info = {
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

static char light_slot_name[16] = "light";
static struct equip_slot TEST_DATA test_slot_light = {
	.type = 5,
	.name = light_slot_name,
	.obj = NULL,
};

static char quest_name[16] = "Test";
static struct quest TEST_DATA test_quest = {
	.next = NULL,
	.index = 0,
	.name = quest_name,
	.level = 1,
	.race = &test_r_human,
	.cur_num = 0,
	.max_num = 4,
};

static char body_name[16] = "Humanoid";
static struct player_body TEST_DATA test_player_body = {
	.next    = NULL,
	.name    = body_name,
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

static struct object TEST_DATA test_player_knowledge = {
	.kind = NULL,
	.image_kind = NULL,
	.ego = NULL,
	.artifact = NULL,
	.prev = NULL,
	.next = NULL,
	.known = NULL,
	.oidx = 0,
	.grid = { 0, 0 },
	.tval = 0,
	.sval = 0,
	.pval = 0,
	.weight = 0,

	.modifiers = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	.el_info = {
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 }
	},
	.brands = NULL,
	.slays = NULL,

	.att = 0,
	.evn = 0,
	.dd = 0,
	.ds = 0,
	.pd = 0,
	.ps = 0,

	.timeout = 0,
	.number = 0,
	.notice = 0,

	.held_m_idx = 0,
	.origin = 0,
	.origin_depth = 0,
	.origin_race = NULL,
	.note = 0,
	.abilities = NULL,
};

static char test_history[24] = "no history";
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
	.history = test_history,
	.is_dead = 0,
	.wizard = 0,
	.upkeep = &test_player_upkeep,
	.gear = NULL,
	.gear_k = NULL,
	.obj_k = &test_player_knowledge,
};

static char cave_name[16] = "Test";
static struct chunk TEST_DATA test_cave = {
	.name = cave_name,
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

static char proj_element[16] = "element";
static char acid_proj_name[16] = "acid";
static char acid_proj_desc[24] = "acid";
static char elec_proj_name[16] = "electricity";
static char elec_proj_desc[24] = "electricity";
static char fire_proj_name[16] = "fire";
static char fire_proj_desc[24] = "fire";
static char cold_proj_name[16] = "cold";
static char cold_proj_desc[24] = "cold";
static struct projection TEST_DATA test_projections[4] = {
	{
		.index = 0,
		.name = acid_proj_name,
		.type = proj_element,
		.desc = acid_proj_desc,
		.player_desc = acid_proj_desc,
		.blind_desc = acid_proj_desc,
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
		.name = elec_proj_name,
		.type = proj_element,
		.desc = elec_proj_desc,
		.player_desc = elec_proj_desc,
		.blind_desc = elec_proj_desc,
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
		.name = fire_proj_name,
		.type = proj_element,
		.desc = fire_proj_desc,
		.player_desc = fire_proj_desc,
		.blind_desc = fire_proj_desc,
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
		.name = cold_proj_name,
		.type = proj_element,
		.desc = cold_proj_desc,
		.player_desc = cold_proj_desc,
		.blind_desc = cold_proj_desc,
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
