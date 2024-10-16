/* object/slays.c */
/* Exercise functions from obj-slays.{h,c}. */

#include "unit-test.h"
#include "unit-test-data.h"
#include "test-utils.h"
#include "init.h"
#include "mon-spell.h"
#include "obj-slays.h"
#include "player-birth.h"
#include "player-timed.h"
#include "z-color.h"
#include "z-util.h"
#include "z-virt.h"

struct slays_test_state {
	bool *slays;
	bool *brands;
};

int setup_tests(void **state)
{
	struct slays_test_state *ts;

	set_file_paths();
	init_angband();
#ifdef UNIX
	/* Necessary for creating the randart file. */
	create_needed_dirs();
#endif
	ts = mem_alloc(sizeof(*ts));
	ts->slays = mem_zalloc(z_info->slay_max * sizeof(*ts->slays));
	ts->brands = mem_zalloc(z_info->brand_max * sizeof(*ts->brands));
	/* Set up the player. */
	if (!player_make_simple(NULL, NULL, NULL, "Tester")) {
		mem_free(ts->brands);
		mem_free(ts->slays);
		mem_free(ts);
		cleanup_angband();
		return 1;
	}
	/*
	 * Needed for scare_onlooking_friends().  For now, set mon_max
	 * to zero so scare_onlooking_friends() does nothing, but at some
	 * point, invest the effort to test that the morale changes are
	 * correctly applied as a side effect of slay_bonus().
	 */
	cave = mem_zalloc(sizeof(*cave));
	*state = ts;
	return 0;
}

int teardown_tests(void *state)
{
	struct slays_test_state *ts = state;

	mem_free(ts->brands);
	mem_free(ts->slays);
	mem_free(ts);
	cleanup_angband();
	return 0;
}

static void fill_in_monster_base(struct monster_base *base)
{
	static char name[20] = "blob";
	static char text[20] = "blob";

	base->next = NULL;
	base->name = name;
	base->text = text;
	rf_wipe(base->flags);
	base->d_char = L'b';
	base->pain = NULL;
}

static void fill_in_monster_race(struct monster_race *race,
		struct monster_base *base)
{
	static char name[20] = "white blob";
	static char text[20] = "white blob";

	race->next = NULL;
	race->ridx = 1;
	race->name = name;
	race->text = text;
	race->plural = NULL;
	race->base = base;
	race->hdice = 8,
	race->hside = 4,
	race->evn = 5,
	race->pd = 3,
	race->ps = 4,
	race->sleep = 10,
	race->per = 4,
	race->stl = 3,
	race->wil = 1,
	race->song = 0,
	race->speed = 2,
	race->light = 1,
	race->freq_ranged = 0,
	race->light = 0;
	race->spell_power = 0;
	rf_wipe(race->flags);
	rsf_wipe(race->spell_flags);
	race->blow = &test_blow[0];
	race->level = 1;
	race->rarity = 1;
	race->d_attr = COLOUR_WHITE;
	race->d_char = base->d_char;
	race->max_num = 100;
	race->cur_num = 0;
	race->drops = NULL;
}

static void fill_in_monster(struct monster *mon, struct monster_race *race)
{
	mon->race = race;
	mon->image_race = NULL;
	mon->midx = 1;
	mon->grid = loc(1, 1);
	mon->hp = (race->hdice * (race->hside + 1)) / 2;
	mon->maxhp = mon->hp;
	memset(mon->m_timed, 0, MON_TMD_MAX * sizeof(mon->m_timed[0]));
	mon->mspeed = race->speed;
	mon->energy = 0;
	mon->cdis = 100;
	rf_wipe(mon->mflag);
	mon->held_obj = NULL;
	mon->attr = race->d_attr;
	memset(&mon->known_pstate, 0, sizeof(mon->known_pstate));
	mon->target.grid = loc(0, 0);
	mon->target.midx = 0;
	memset(&mon->group_info, 0, sizeof(mon->group_info));
	mon->flow.grids = NULL;
	mon->min_range = 0;
	mon->best_range = 0;
}

static void fill_in_object_base(struct object_base *base)
{
	static char name[20] = "weapon";

	base->name = name;
	base->tval = 1;
	base->next = NULL;
	base->attr = COLOUR_WHITE;
	of_wipe(base->flags);
	kf_wipe(base->kind_flags);
	memset(base->el_info, 0, ELEM_MAX * sizeof(base->el_info[0]));
	base->smith_attack_valid = false;
	base->smith_attack_artistry = 0;
	base->smith_attack_artefact = 0;
	memset(base->smith_flags, 0, OF_SIZE * sizeof(base->smith_flags[0]));
	memset(base->smith_el_info, 0, ELEM_MAX * sizeof(base->smith_el_info[0]));
	memset(base->smith_modifiers, 0, OBJ_MOD_MAX * sizeof(base->smith_modifiers[0]));
	base->break_perc = 0;
	base->max_stack = 40;
	base->num_svals = 1;
}

static void fill_in_object_kind(struct object_kind *kind,
		struct object_base *base)
{
	static char name[20] = "weapon";
	static char text[20] = "weapon";

	kind->name = name;
	kind->text = text;
	kind->base = base;
	kind->next = NULL;
	kind->kidx = 1;
	kind->tval = base->tval;
	kind->sval = 1;
	kind->pval = 0;
	kind->special1.base = 0;
	kind->special1.dice = 0;
	kind->special1.sides = 0;
	kind->special1.m_bonus = 0;
	kind->special2 = 0;
	kind->att = 0,
	kind->evn = 1,
	kind->dd = 1,
	kind->ds = 4,
	kind->pd = 0,
	kind->ps = 0,
	kind->weight = 10;
	kind->cost = 0;
	of_wipe(kind->flags);
	kf_wipe(kind->kind_flags);
	memset(kind->modifiers, 0, OBJ_MOD_MAX * sizeof(kind->modifiers[0]));
	memset(kind->el_info, 0, OBJ_MOD_MAX * sizeof(kind->el_info[0]));
	kind->brands = NULL;
	kind->slays = NULL;
	kind->d_attr = COLOUR_WHITE;
	kind->d_char = L'/';
	kind->alloc = NULL;
	kind->level = 1;
	kind->effect = NULL;
	kind->effect_msg = NULL;
	kind->charge.base = 0;
	kind->charge.dice = 0;
	kind->charge.sides = 0;
	kind->charge.m_bonus = 0;
	kind->gen_mult_prob = 0;
	kind->stack_size.base = 1;
	kind->stack_size.dice = 0;
	kind->stack_size.sides = 0;
	kind->stack_size.m_bonus = 0;
	kind->flavor = NULL;
	kind->note_aware = 0;
	kind->note_unaware = 0;
	kind->aware = true;
	kind->tried = true;
	kind->ignore = false;
	kind->everseen = true;
}

static void fill_in_object(struct object *obj, struct object_kind *kind)
{
	obj->kind = kind;
	obj->ego = NULL;
	obj->artifact = NULL;
	obj->prev = NULL;
	obj->next = NULL;
	obj->oidx = 1;
	obj->grid = loc(1, 1);
	obj->tval = kind->tval;
	obj->sval = kind->sval;
	obj->pval = kind->pval;
	obj->weight = kind->weight;
	obj->att = kind->att,
	obj->evn = kind->evn,
	obj->dd = kind->dd;
	obj->ds = kind->ds;
	obj->pd = kind->pd;
	obj->ps = kind->ps;
	of_wipe(obj->flags);
	memset(obj->modifiers, 0, OBJ_MOD_MAX * sizeof(obj->modifiers[0]));
	memset(obj->el_info, 0, ELEM_MAX * sizeof(obj->el_info[0]));
	obj->brands = kind->brands;
	obj->slays = kind->slays;
	obj->timeout = 0;
	obj->number = 1;
	obj->notice = 0;
	obj->held_m_idx = 0;
	obj->origin = ORIGIN_DROP_WIZARD;
	obj->origin_depth = 1;
	obj->origin_race = NULL;
	obj->note = 0;
}

static int test_same_monsters_slain(void *state)
{
	int i1, i2;

	for (i1 = 1; i1 < z_info->slay_max; ++i1) {
		for (i2 = i1; i2 < z_info->slay_max; ++i2) {
			bool s1 = same_monsters_slain(i1, i2);
			bool s2 = same_monsters_slain(i2, i1);

			eq(s1, s2);
			if (i1 == i2) {
				require(s1);
			} else if (s1) {
				require(slays[i1].race_flag
					== slays[i2].race_flag);
			} else {
				require(slays[i1].race_flag
					!= slays[i2].race_flag);
			}
		}
	}
	ok;
}

static int test_slay_bonus(void *state)
{
	struct slays_test_state *ts = state;
	struct object_base weapon_base;
	struct object_kind weapon_kind;
	struct object weapon;
	struct monster_base dummy_base;
	struct monster_race dummy_race;
	struct monster dummy;
	int bonus, b, s, i1;

	fill_in_object_base(&weapon_base);
	fill_in_object_kind(&weapon_kind, &weapon_base);
	fill_in_object(&weapon, &weapon_kind);
	fill_in_monster_base(&dummy_base);
	fill_in_monster_race(&dummy_race, &dummy_base);
	fill_in_monster(&dummy, &dummy_race);

	/* Has no slays or brands that would be effective. */
	b = 0;
	s = 0;
	bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
	require(bonus == 0 && b == 0 && s == 0);
	b = 0;
	s = 0;
	bonus = slay_bonus(player, NULL, &dummy, &s, &b);
	require(bonus == 0 && b == 0 && s == 0);

	/*
	 * Has no slays or brands that would be effective; check that preset
	 * value of b or s is preserved.
	 */
	i1 = rand_range(1, z_info->brand_max - 1);
	b = i1;
	s = 0;
	bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
	require(bonus == 0 && b == i1 && s == 0);
	b = i1;
	s = 0;
	bonus = slay_bonus(player, NULL, &dummy, &s, &b);
	require(bonus == 0 && b == i1 && s == 0);
	i1 = rand_range(1, z_info->slay_max - 1);
	b = 0;
	s = i1;
	bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
	require(bonus == 0 && b == 0 && s == i1);
	b = 0;
	s = i1;
	bonus = slay_bonus(player, NULL, &dummy, &s, &b);
	require(bonus == 0 && b == 0 && s == i1);

	memset(ts->slays, 0, z_info->slay_max * sizeof(*ts->slays));
	memset(ts->brands, 0, z_info->brand_max * sizeof(*ts->brands));

	/* Test with one brand on the weapon. */
	for (i1 = 1; i1 < z_info->brand_max; ++i1) {
		bool *old_brands = weapon.brands;

		weapon.brands = ts->brands;
		weapon.brands[i1] = true;

		b = 0;
		s = 0;
		bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
		require(bonus == brands[i1].dice && b == i1 && s == 0);

		if (brands[i1].resist_flag) {
			rf_on(dummy.race->flags, brands[i1].resist_flag);
			b = 0;
			s = 0;
			bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
			require(bonus == 0 && b == 0 && s == 0);
			rf_off(dummy.race->flags, brands[i1].resist_flag);
		}

		if (brands[i1].vuln_flag) {
			rf_on(dummy.race->flags, brands[i1].vuln_flag);
			b = 0;
			s = 0;
			bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
			require(bonus == brands[i1].dice + brands[i1].vuln_dice
				&& b == i1 && s == 0);

			if (brands[i1].resist_flag) {
				rf_on(dummy.race->flags, brands[i1].resist_flag);
				b = 0;
				s = 0;
				bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
				require(bonus == 0 && b == 0 && s == 0);
				rf_off(dummy.race->flags, brands[i1].resist_flag);
			}

			rf_off(dummy.race->flags, brands[i1].vuln_flag);
		}

		weapon.brands[i1] = false;
		weapon.brands = old_brands;
	}

	/* Test with one slay on the weapon. */
	for (i1 = 1; i1 < z_info->slay_max; ++i1) {
		bool *old_slays;

		if (!slays[i1].race_flag) continue;

		old_slays = weapon.slays;
		weapon.slays = ts->slays;
		weapon.slays[i1] = true;

		b = 0;
		s = 0;
		bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
		require(bonus == 0 && b == 0 && s == 0);

		rf_on(dummy_race.flags, slays[i1].race_flag);
		b = 0;
		s = 0;
		bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
		require(bonus == slays[i1].dice && b == 0 && s == i1);
		rf_off(dummy_race.flags, slays[i1].race_flag);

		weapon.slays[i1] = false;
		weapon.slays = old_slays;
	}

	/*
	 * Test with a combination of two (both brands, one slay and one brand,
	 * or both slays).
	 */
	for (i1 = 1; i1 < z_info->brand_max; ++i1) {
		int i2;

		for (i2 = i1 + 1; i2 < z_info->brand_max; ++i2) {
			bool *old_brands;

			if (!brands[i1].resist_flag && brands[i1].resist_flag
					== brands[i2].resist_flag) {
				continue;
			}

			old_brands = weapon.brands;
			weapon.brands = ts->brands;
			weapon.brands[i1] = true;
			weapon.brands[i2] = true;

			/* Susceptible to both */
			b = 0;
			s = 0;
			bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
			require(bonus == brands[i1].dice + brands[i2].dice
				&& b == i2 && s == 0);

			/* Only susceptible to the second */
			if (brands[i1].resist_flag) {
				rf_on(dummy.race->flags, brands[i1].resist_flag);
				b = 0;
				s = 0;
				bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
				require(bonus == brands[i2].dice && b == i2 && s == 0);
				rf_off(dummy.race->flags, brands[i1].resist_flag);
			}

			/* Only susceptible to the first */
			if (brands[i2].resist_flag) {
				rf_on(dummy.race->flags, brands[i2].resist_flag);
				b = 0;
				s = 0;
				bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
				require(bonus == brands[i1].dice && b == i1 && s == 0);
				rf_off(dummy.race->flags, brands[i2].resist_flag);
			}

			if (brands[i1].vuln_flag) {
				/* Especially vulnerable to the first */
				rf_on(dummy.race->flags, brands[i1].vuln_flag);
				b = 0;
				s = 0;
				bonus = slay_bonus(player, &weapon, &dummy,
					&s, &b);
				require(bonus == brands[i1].dice
					+ brands[i1].vuln_dice
					+ brands[i2].dice
					&& b == i2 && s == 0);
				rf_off(dummy.race->flags, brands[i1].vuln_flag);
			}

			if (brands[i2].vuln_flag) {
				/* Especially vulnerable to the second */
				rf_on(dummy.race->flags, brands[i2].vuln_flag);
				bonus = slay_bonus(player, &weapon, &dummy,
					&s, &b);
				require(bonus == brands[i1].dice
					+ brands[i2].dice
					+ brands[i2].vuln_dice
					&& b == i2 && s == 0);
				rf_off(dummy.race->flags, brands[i2].vuln_flag);
			}

			weapon.brands[i1] = false;
			weapon.brands[i2] = false;
			weapon.brands = old_brands;
		}

		for (i2 = 1; i2 < z_info->slay_max; ++i2) {
			bool *old_brands;
			bool *old_slays;

			if (!slays[i2].race_flag) continue;

			old_brands = weapon.brands;
			weapon.brands = ts->brands;
			weapon.brands[i1] = true;
			old_slays = weapon.slays;
			weapon.slays = ts->slays;
			weapon.slays[i2] = true;

			/* Susceptible to both */
			rf_on(dummy_race.flags, slays[i2].race_flag);
			b = 0;
			s = 0;
			bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
			require(bonus == brands[i1].dice + slays[i2].dice
				&& b == i1 && s == i2);
			rf_off(dummy_race.flags, slays[i2].race_flag);

			/*
			 * Susceptible to both; especially vulnerable to the
			 * brand
			 */
			if (brands[i1].vuln_flag) {
				rf_on(dummy_race.flags, brands[i1].vuln_flag);
				rf_on(dummy_race.flags, slays[i2].race_flag);
				b = 0;
				s = 0;
				bonus = slay_bonus(player, &weapon, &dummy,
					&s, &b);
				require(bonus == brands[i1].dice
					+ brands[i1].vuln_dice + slays[i2].dice
					&& b == i1 && s == i2);
				rf_off(dummy_race.flags, brands[i1].vuln_flag);
				rf_off(dummy_race.flags, slays[i2].race_flag);
			}

			/* Only susceptible to the brand */
			b = 0;
			s = 0;
			bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
			require(bonus == brands[i1].dice && b == i1 && s == 0);

			/* Only susceptible to the slay */
			if (brands[i1].resist_flag) {
				rf_on(dummy_race.flags, brands[i1].resist_flag);
				rf_on(dummy_race.flags, slays[i2].race_flag);
				b = 0;
				s = 0;
				bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
				require(bonus == slays[i1].dice && b == 0 && s == i2);
				rf_off(dummy_race.flags, brands[i1].resist_flag);
				rf_off(dummy_race.flags, slays[i2].race_flag);
			}

			weapon.brands[i1] = false;
			weapon.brands = old_brands;
			weapon.slays[i2] = false;
			weapon.slays = old_slays;
		}
	}

	for (i1 = 1; i1 < z_info->slay_max; ++i1) {
		int i2;

		if (!slays[i1].race_flag) continue;

		for (i2 = i1 + 1; i2 < z_info->slay_max; ++i2) {
			bool *old_slays;

			if (!slays[i2].race_flag) continue;
			if (slays[i1].race_flag == slays[i2].race_flag) continue;

			old_slays = weapon.slays;
			weapon.slays = ts->slays;
			weapon.slays[i1] = true;
			weapon.slays[i2] = true;

			/* Susceptible to both */
			rf_on(dummy_race.flags, slays[i1].race_flag);
			rf_on(dummy_race.flags, slays[i2].race_flag);
			b = 0;
			s = 0;
			bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
			require(bonus == slays[i1].dice + slays[i2].dice
				&& b == 0 && s == i2);
			rf_off(dummy_race.flags, slays[i1].race_flag);
			rf_off(dummy_race.flags, slays[i2].race_flag);

			/* Only susceptible to the first */
			rf_on(dummy_race.flags, slays[i1].race_flag);
			b = 0;
			s = 0;
			bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
			require(bonus == slays[i1].dice && b == 0 && s == i1);
			rf_off(dummy_race.flags, slays[i1].race_flag);

			/* Only susceptible to the second */
			rf_on(dummy_race.flags, slays[i2].race_flag);
			bonus = slay_bonus(player, &weapon, &dummy, &s, &b);
			require(bonus == slays[i2].dice && b == 0 && s == i2);
			rf_off(dummy_race.flags, slays[i2].race_flag);

			weapon.slays[i1] = false;
			weapon.slays[i2] = false;
			weapon.slays = old_slays;
		}
	}

	ok;
}

static int test_react_to_slay(void *state)
{
	struct monster_base dummy_base;
	struct monster_race dummy_race;
	struct monster dummy;
	int i1;

	fill_in_monster_base(&dummy_base);
	fill_in_monster_race(&dummy_race, &dummy_base);
	fill_in_monster(&dummy, &dummy_race);

	for (i1 = 1; i1 < z_info->slay_max; ++i1) {
		if (!slays[i1].race_flag) continue;

		rf_on(dummy_race.flags, slays[i1].race_flag);
		/* Monster is vulnerable to this slay. */
		eq(react_to_slay(&slays[i1], &dummy), true);
		rf_off(dummy_race.flags, slays[i1].race_flag);
	}

	ok;
}

const char *suite_name = "object/slays";
struct test tests[] = {
	{ "same_monsters_slain", test_same_monsters_slain },
	{ "slay_bonus", test_slay_bonus },
	{ "react_to_slay", test_react_to_slay },
	{ NULL, NULL }
};
