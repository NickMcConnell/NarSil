/**
 * \file list-effects.h
 * \brief List of effects
 *
 * name: effect code
 * aim: does the effect require aiming?
 * info: info label for spells
 * args: how many arguments the description takes
 * info flags: flags for object description
 * description: text of description
 * menu_name: format string for menu name; use an empty string if there's no
 * plan to use it from a menu
 */
/* name 							aim		info		args	info flags		description	menu_name */
EFFECT(HEAL_HP,						false,	"heal",		2,		EFINFO_HEAL,	"heals %s hitpoints%s",	"heal self")
EFFECT(DAMAGE,						false,	"hurt",		1,		EFINFO_DICE,	"does %s damage to the player",	"%s damage")
EFFECT(DART,						false,	"hurt",		1,		EFINFO_DICE,	"does %s damage to the player",	"dart")
EFFECT(PIT,							false,	"hurt",		0,		EFINFO_DICE,	"player falls in a pit",	"pitfall")
EFFECT(PROJECT_LOS,					false,	"power",	1,		EFINFO_PROJ,	"%s which are in line of sight",	"%s in line of sight")
EFFECT(PROJECT_LOS_GRIDS,			false,	"power",	1,		EFINFO_PROJ,	"%s which are in line of sight",	"%s in line of sight")
EFFECT(DEADFALL,					false,	"hurt",		0,		EFINFO_DICE,	"makes rocks fall on the player",	"deadfall")
EFFECT(EARTHQUAKE,					false,	NULL,		1,		EFINFO_QUAKE,	"causes an earthquake around you of radius %d",	"cause earthquake")
EFFECT(SPOT,						false,	"dam",		4,		EFINFO_SPOT,	"creates a ball of %s with radius %d, centred on and hitting the player, with full intensity to radius %d, dealing %s damage at the centre",	"engulf with %s")
EFFECT(SPHERE,						false,	"dam",		4,		EFINFO_SPOT,	"creates a ball of %s with radius %d, centred on the player, with full intensity to radius %d, dealing %s damage at the centre",	"project %s")
EFFECT(EXPLOSION,					false,	"dam",		1,		EFINFO_PROJ,	"produces a blast of %s",	"blast %s")
EFFECT(BREATH,						true,	NULL,		3,		EFINFO_BREATH,	"breathes a cone of %s with width %d degrees, dealing %s damage at the source",	"breathe a cone of %s")
EFFECT(BOLT,						true,	"dam",		2,		EFINFO_BOLT,	"casts a bolt of %s dealing %s damage",	"cast a bolt of %s")
EFFECT(BEAM,						true,	"dam",		2,		EFINFO_BOLT,	"casts a beam of %s dealing %s damage",	"cast a beam of %s")
EFFECT(TERRAIN_BEAM,					true,	NULL,
1,		EFINFO_PROJ,	"casts a beam of %s",			"cast a beam of %s")
EFFECT(NOURISH,						false,	NULL,		3,		EFINFO_FOOD,	"%s for %s turns (%s percent)",	"%s %s")
EFFECT(CURE,						false,	NULL,		1,		EFINFO_CURE,	"cures %s",	"cure %s")
EFFECT(TIMED_SET,					false,	NULL,		2,		EFINFO_TIMED,	"administers %s for %s turns",	"administer %s")
EFFECT(TIMED_INC,					false,	"dur",		2,		EFINFO_TIMED,	"extends %s for %s turns",	"extend %s")
EFFECT(TIMED_INC_CHECK,				false,	"dur",		1,		EFINFO_TIMED,	"checks if %s can be extended",	"checks %s extension")
EFFECT(TIMED_INC_NO_RES,			false,	"dur",		2,		EFINFO_TIMED,	"extends %s for %s turns (unresistable)",	"extend %s")
EFFECT(GLYPH,						false,	NULL,		1,		EFINFO_NONE,	"inscribes a glyph beneath you",	"inscribe a glyph")
EFFECT(RESTORE_STAT,				false,	NULL,		1,		EFINFO_STAT,	"restores your %s",	"restore %s")
EFFECT(DRAIN_STAT,					false,	NULL,		1,		EFINFO_STAT,	"reduces your %s",	"drains %s")
EFFECT(RESTORE_MANA,				false,	NULL,		0,		EFINFO_NONE,	"restores some mana",	"restore some mana")
EFFECT(REMOVE_CURSE,				false,	NULL,		1,		EFINFO_DICE,	"attempts power %s removal of a single curse on an object",	"remove curse")
EFFECT(MAP_AREA,					false,	NULL,		0,		EFINFO_NONE,	"maps the current dungeon level",	"map level")
EFFECT(DETECT_TRAPS,				false,	NULL,		0,		EFINFO_NONE,	"detects traps nearby",	"detect traps")
EFFECT(DETECT_DOORS,				false,	NULL,		0,		EFINFO_NONE,	"detects doors nearby",	"detect doors")
EFFECT(DETECT_OBJECTS,				false,	NULL,		0,		EFINFO_NONE,	"detects objects nearby",	"detect objects")
EFFECT(DETECT_MONSTERS,				false,	NULL,		0,		EFINFO_NONE,	"detects monsters on the level",	"detect monsters")
EFFECT(REVEAL_MONSTER,				false,	NULL,		0,		EFINFO_NONE,	"reveals a monster",	"reveal monster")
EFFECT(CLOSE_CHASMS,				false,	NULL,		0,		EFINFO_NONE,	"close nearby chasms",	"close_chasms")
EFFECT(IDENTIFY,					false,	NULL,		0,		EFINFO_NONE,	"identifie a selected item",	"identify")
EFFECT(RECHARGE,					false,	"power",	0,		EFINFO_NONE,	"tries to recharge a wand or staff, destroying the wand or staff on failure",	"recharge")
EFFECT(SUMMON,						false,	NULL,		1,		EFINFO_SUMM,	"summons %s at the current dungeon level",	"summon %s")
EFFECT(TELEPORT_TO,					false,	NULL,		0,		EFINFO_NONE,	"teleports toward a target",	"teleport to target")
EFFECT(DARKEN_LEVEL,				false,	NULL,		0,		EFINFO_NONE,	"completely darkens and forgets the level",	"darken level")
EFFECT(LIGHT_AREA,					false,	NULL,		0,		EFINFO_NONE,	"lights up the surrounding area",	"light area")
EFFECT(DARKEN_AREA,					false,	NULL,		0,		EFINFO_NONE,	"darkens the surrounding area",	"darken area")
EFFECT(SONG_OF_ELBERETH,			false,	NULL,		0,		EFINFO_NONE,	"sings a song of Elbereth",			"song of Elbereth")
EFFECT(SONG_OF_LORIEN,				false,	NULL,		0,		EFINFO_NONE,	"sings a song of Lorien",			"song of Lorien")
EFFECT(SONG_OF_FREEDOM,				false,	NULL,		0,		EFINFO_NONE,	"sings a song of Freedom",			"song of Freedom")
EFFECT(SONG_OF_BINDING,				false,	NULL,		0,		EFINFO_NONE,	"sings a song of Binding",			"song of Binding")
EFFECT(SONG_OF_PIERCING,			false,	NULL,		0,		EFINFO_NONE,	"sings a song of Piercing",			"song of Piercing")
EFFECT(SONG_OF_OATHS,				false,	NULL,		0,		EFINFO_NONE,	"sings a song of Oaths",			"song of Oaths")
EFFECT(AGGRAVATE,					false,	NULL,		0,		EFINFO_NONE,	"makes nearby monsters aggressive",					"make angry")
EFFECT(NOISE,						false,	NULL,		0,		EFINFO_NONE,	"makes a noise that monsters may hear",					"make a noise")
EFFECT(CREATE_TRAPS,				false,	NULL,		0,		EFINFO_NONE,	"create traps on the level",					"create traps")
