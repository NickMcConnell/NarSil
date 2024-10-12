/* parse/mspell */
/* Exercise parsing used for monster_spell.txt. */

#include "unit-test.h"
#include "datafile.h"
#include "effects.h"
#include "init.h"
#include "message.h"
#include "monster.h"
#include "mon-init.h"
#include "parser.h"
#include "player-timed.h"
#include "project.h"
#include "z-color.h"
#include "z-dice.h"
#include "z-virt.h"

int setup_tests(void **state) {
	*state = mon_spell_parser.init();
	/* Needed for max_range. */
	z_info = mem_zalloc(sizeof(*z_info));
	z_info->max_range = 30;
	return !*state;
}

int teardown_tests(void *state) {
	struct parser *p = (struct parser *) state;

	mon_spell_parser.finish(p);
	mon_spell_parser.cleanup();
	mem_free(z_info);
	return 0;
}

/*
 * Check that supplying any of the other directives before specifying the name
 * works as expected.
 */
static int test_missing_record0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	null(s);
	r = parser_parse(p, "msgt:TELEPORT");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "mana:10");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "best-range:4");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "max-range:10");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "desire:2");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "disturb:1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "use-past-range:100");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "effect:DAMAGE");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "dice:3+1d35");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "expr:D:SPELL_POWER:/ 8 + 1");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "effect-xtra:NOISE");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "dice-xtra:-10");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "expr-xtra:D:SPELL_POWER:* 2");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "power-cutoff:15");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "lore:cough up a hairball");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-vis:{name} cackles.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-invis:Something cackles.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-smart-vis:{name} shouts for help.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-smart-invis:You hear a shout for help.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-silence-vis:{name} lets out a muffled shriek.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-silence-invis:You hear a muffled shriek.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-silence-smart-vis:{name} lets out a muffled shout for help.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-silence-smart-invis:You hear a muffled shout for help.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-save:Something brushes your check, but you seem unharmed.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);
	r = parser_parse(p, "message-no-save:Your memories fade away.");
	eq(r, PARSE_ERROR_MISSING_RECORD_HEADER);

	ok;
}

static int test_name_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:XYZZY");

	eq(r, PARSE_ERROR_INVALID_SPELL_NAME);
	ok;
}

static int test_name0(void *state) {
	struct parser *p = (struct parser*) state;
	enum parser_error r = parser_parse(p, "name:BOULDER");
	const struct monster_spell *s;

	eq(r, PARSE_ERROR_NONE);
	s = parser_priv(p);
	eq(s->index, RSF_BOULDER);
	eq(s->msgt, 0);
	eq(s->mana, 0);
	eq(s->best_range, 0);
	eq(s->max_range, z_info->max_range);
	eq(s->desire, 0);
	eq(s->use_past_range, 0);
	eq(s->disturb_stealth, false);
	null(s->effect);
	null(s->effect_xtra);
	notnull(s->level);
	eq(s->level->power, 0);
	null(s->level->lore_desc);
	null(s->level->message);
	null(s->level->blind_message);
	null(s->level->silence_message);
	null(s->level->blind_silence_message);
	null(s->level->smart_message);
	null(s->level->smart_blind_message);
	null(s->level->smart_silence_message);
	null(s->level->smart_blind_silence_message);
	null(s->level->save_message);
	null(s->level->no_save_message);
	ok;
}

static int test_msgt0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);
	r = parser_parse(p, "msgt:TELEPORT");
	eq(r, PARSE_ERROR_NONE);
	eq(s->msgt, MSG_TELEPORT);
	ok;
}

static int test_msgt_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);
	r = parser_parse(p, "msgt:XYZZY");
	eq(r, PARSE_ERROR_INVALID_MESSAGE);
	ok;
}

static int test_mana0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);
	r = parser_parse(p, "mana:10");
	eq(r, PARSE_ERROR_NONE);
	eq(s->mana, 10);
	ok;
}

static int test_best_range0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);
	r = parser_parse(p, "best-range:2");
	eq(r, PARSE_ERROR_NONE);
	eq(s->best_range, 2);
	ok;
}

static int test_max_range0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);
	r = parser_parse(p, "max-range:16");
	eq(r, PARSE_ERROR_NONE);
	eq(s->max_range, 16);
	ok;
}

static int test_desire0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);
	r = parser_parse(p, "desire:2");
	eq(r, PARSE_ERROR_NONE);
	eq(s->desire, 2);
	ok;
}

static int test_disturb0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);
	r = parser_parse(p, "disturb:1");
	eq(r, PARSE_ERROR_NONE);
	eq(s->disturb_stealth, true);
	r = parser_parse(p, "disturb:0");
	eq(r, PARSE_ERROR_NONE);
	eq(s->disturb_stealth, false);
	ok;
}

static int test_use_past_range0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);
	r = parser_parse(p, "use-past-range:100");
	eq(r, PARSE_ERROR_NONE);
	eq(s->use_past_range, 100);
	ok;
}

/*
 * Check that placing "dice:" and "expr:" directives before any "effect:"
 * directives for a spell works as expected:  do nothing and return
 * PARSE_ERROR_NONE.
 */
static int test_misplaced_effect_deps0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	require(s && !s->effect);
	r = parser_parse(p, "dice:5+1d4");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "expr:D:SPELL_POWER:* 8 + 20");
	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_effect0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;
	const struct effect *e;

	notnull(s);

	/* Check effect where just the type matters. */
	r = parser_parse(p, "effect:DAMAGE");
	eq(r, PARSE_ERROR_NONE);
	e = s->effect;
	notnull(e);
	while (e->next) {
		e = e->next;
	}
	eq(e->index, EF_DAMAGE);
	eq(e->subtype, 0);
	eq(e->radius, 0);
	eq(e->other, 0);
	null(e->dice);
	null(e->msg);

	/* Check effect where there's a type and subtype. */
	r = parser_parse(p, "effect:TIMED_INC:CONFUSED");
	eq(r, PARSE_ERROR_NONE);
	e = e->next;
	notnull(e);
	eq(e->index, EF_TIMED_INC);
	eq(e->subtype, TMD_CONFUSED);
	eq(e->radius, 0);
	eq(e->other, 0);
	null(e->dice);
	null(e->msg);

	/* Check effect with a type, subtype, and radius. */
	r = parser_parse(p, "effect:SPHERE:ACID:2");
	eq(r, PARSE_ERROR_NONE);
	e = e->next;
	notnull(e);
	eq(e->index, EF_SPHERE);
	eq(e->subtype, PROJ_ACID);
	eq(e->radius, 2);
	eq(e->other, 0);
	null(e->dice);
	null(e->msg);

	/* Check effect with a type, subtype, radius, and other parameter. */
	r = parser_parse(p, "effect:BREATH:FIRE:10:30");
	eq(r, PARSE_ERROR_NONE);
	e = e->next;
	notnull(e);
	eq(e->index, EF_BREATH);
	eq(e->subtype, PROJ_FIRE);
	eq(e->radius, 10);
	eq(e->other, 30);
	null(e->dice);
	null(e->msg);

	ok;
}

static int test_effect_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);

	/* Check bad effect name. */
	r = parser_parse(p, "effect:XYZZY");
	eq(r, PARSE_ERROR_INVALID_EFFECT);

	/* Check bad effect subtype. */
	r = parser_parse(p, "effect:CURE:XYZZY");
	eq(r, PARSE_ERROR_INVALID_VALUE);

	ok;
}

static int test_dice0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct { const char *s; int base, ndice, nsides; } test_cases[] = {
		{ "dice:-1", -1, 0, 0 },
		{ "dice:8", 8, 0, 0 },
		{ "dice:d10", 0, 1, 10 },
		{ "dice:-1+d5", -1, 1, 5 },
		{ "dice:3+2d7", 3, 2, 7 },
	};
	int i;

	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		enum parser_error r = parser_parse(p, "effect:DAMAGE");
		struct effect *e;

		eq(r, PARSE_ERROR_NONE);
		e = s->effect;
		notnull(e);
		while (e->next) {
			e = e->next;
		}
		r = parser_parse(p, test_cases[i].s);
		eq(r, PARSE_ERROR_NONE);
		notnull(e->dice);
		require(dice_test_values(e->dice, test_cases[i].base,
			test_cases[i].ndice, test_cases[i].nsides, 0));
	}
	ok;
}

static int test_dice_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct { const char *s; } test_cases[] = {
		{ "dice:5+d8+d4" },
	};
	int i;

	notnull(s);
	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		enum parser_error r = parser_parse(p, "effect:DAMAGE");
		struct effect *e;

		eq(r, PARSE_ERROR_NONE);
		e = s->effect;
		notnull(e);
		while (e->next) {
			e = e->next;
		}
		r = parser_parse(p, test_cases[i].s);
		eq(r, PARSE_ERROR_INVALID_DICE);
	}
	ok;
}

static int test_expr0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct { const char *s; } test_cases[] = {
		{ "expr:B:MAX_SIGHT: " },
		{ "expr:D:SPELL_POWER:/ 10 + 1" },
		{ "expr:S:SPELL_POWER:* 2 + 3" },
	};
	enum parser_error r;
	struct effect *e;
	int i;

	notnull(s);
	r = parser_parse(p, "effect:DAMAGE");
	eq(r, PARSE_ERROR_NONE);
	e = s->effect;
	notnull(e);
	while (e->next) {
		e = e->next;
	}
	r = parser_parse(p, "dice:$B+$Dd$S");
	eq(r, PARSE_ERROR_NONE);
	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		r = parser_parse(p, test_cases[i].s);
		eq(r, PARSE_ERROR_NONE);
	}
	ok;
}

static int test_expr_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);
	r = parser_parse(p, "effect:DAMAGE");
	eq(r, PARSE_ERROR_NONE);
	/*
	 * Using expr before the effect has dice specified currently does
	 * nothing.
	 */
	r = parser_parse(p, "expr:MAX_SIGHT:B:+ 1");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "dice:$B+$Dd$S");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "expr:C:SPELL_POWER:* 3 / 2");
	eq(r, PARSE_ERROR_UNBOUND_EXPRESSION);
	r = parser_parse(p, "expr:B:MAX_SIGHT:- 40000");
	eq(r, PARSE_ERROR_BAD_EXPRESSION_STRING);
	r = parser_parse(p, "expr:D:SPELL_POWER:/ 0");
	eq(r, PARSE_ERROR_BAD_EXPRESSION_STRING);
	r = parser_parse(p, "expr:S:MAX_SIGHT:% 2");
	eq(r, PARSE_ERROR_BAD_EXPRESSION_STRING);
	ok;
}

/*
 * Check that placing "dice-xtra:" and "expr-xtra:" directives before any
 * "effect-xtra:" directives for a spell works as expected:  do nothing and
 * return PARSE_ERROR_NONE.
 */
static int test_misplaced_effect_xtra_deps0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	require(s && !s->effect);
	r = parser_parse(p, "dice-xtra:-10");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "expr-xtra:D:SPELL_POWER:* 2 - 5");
	eq(r, PARSE_ERROR_NONE);
	ok;
}

static int test_effect_xtra0(void *state) {
	struct parser *p = (struct parser*) state;
	const struct monster_spell *s = parser_priv(p);
	enum parser_error r;
	const struct effect *e;

	notnull(s);

	/* Check effect where just the type matters. */
	r = parser_parse(p, "effect-xtra:NOISE");
	eq(r, PARSE_ERROR_NONE);
	e = s->effect_xtra;
	notnull(e);
	while (e->next) {
		e = e->next;
	}
	eq(e->index, EF_NOISE);
	eq(e->subtype, 0);
	eq(e->radius, 0);
	eq(e->other, 0);
	null(e->dice);
	null(e->msg);

	/* Check effect where there's a type and subtype. */
	r = parser_parse(p, "effect-xtra:CURE:CONFUSED");
	eq(r, PARSE_ERROR_NONE);
	e = e->next;
	notnull(e);
	eq(e->index, EF_CURE);
	eq(e->subtype, TMD_CONFUSED);
	eq(e->radius, 0);
	eq(e->other, 0);
	null(e->dice);
	null(e->msg);

	/* Check effect with a type, subtype, and radius. */
	r = parser_parse(p, "effect-xtra:SPOT:DARK:3");
	eq(r, PARSE_ERROR_NONE);
	e = e->next;
	notnull(e);
	eq(e->index, EF_SPOT);
	eq(e->subtype, PROJ_DARK);
	eq(e->radius, 3);
	eq(e->other, 0);
	null(e->dice);
	null(e->msg);

	/* Check effect with a type, subtype, radius, and other parameter. */
	r = parser_parse(p, "effect-xtra:BREATH:COLD:15:20");
	eq(r, PARSE_ERROR_NONE);
	e = e->next;
	notnull(e);
	eq(e->index, EF_BREATH);
	eq(e->subtype, PROJ_COLD);
	eq(e->radius, 15);
	eq(e->other, 20);
	null(e->dice);
	null(e->msg);

	ok;
}

static int test_effect_xtra_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);

	/* Check bad effect name. */
	r = parser_parse(p, "effect-xtra:XYZZY");
	eq(r, PARSE_ERROR_INVALID_EFFECT);

	/* Check bad effect subtype. */
	r = parser_parse(p, "effect-xtra:BEAM:XYZZY");
	eq(r, PARSE_ERROR_INVALID_VALUE);

	ok;
}

static int test_dice_xtra0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct { const char *s; int base, ndice, nsides, mbonus; }
			test_cases[] = {
		{ "dice-xtra:-1", -3, 0, 0, 0 },
		{ "dice-xtra:12", 12, 0, 0, 0 },
		{ "dice-xtra:d8", 0, 1, 8, 0 },
		{ "dice-xtra:-1+d5", -1, 1, 5, 0 },
		{ "dice-xtra:3+2d7", 3, 2, 7, 0 },
		{ "dice-xtra:-1+m10", -1, 0, 0, 10 },
	};
	int i;

	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		enum parser_error r = parser_parse(p, "effect-xtra:DAMAGE");
		struct effect *e;

		eq(r, PARSE_ERROR_NONE);
		e = s->effect_xtra;
		notnull(e);
		while (e->next) {
			e = e->next;
		}
		r = parser_parse(p, test_cases[i].s);
		eq(r, PARSE_ERROR_NONE);
		notnull(e->dice);
		require(dice_test_values(e->dice, test_cases[i].base,
			test_cases[i].ndice, test_cases[i].nsides, 0));
	}
	ok;
}

static int test_dice_xtra_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct { const char *s; } test_cases[] = {
		{ "dice-xtra:d7+5" },
	};
	int i;

	notnull(s);
	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		enum parser_error r = parser_parse(p, "effect-xtra:DAMAGE");
		struct effect *e;

		eq(r, PARSE_ERROR_NONE);
		e = s->effect;
		notnull(e);
		while (e->next) {
			e = e->next;
		}
		r = parser_parse(p, test_cases[i].s);
		eq(r, PARSE_ERROR_INVALID_DICE);
	}
	ok;
}

static int test_expr_xtra0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct { const char *s; } test_cases[] = {
		{ "expr-xtra:B:MAX_SIGHT: " },
		{ "expr-xtra:D:SPELL_POWER:/ 5 - 3" },
		{ "expr-xtra:S:SPELL_POWER:* 2 + 4" },
	};
	enum parser_error r;
	struct effect *e;
	int i;

	notnull(s);
	r = parser_parse(p, "effect-xtra:DAMAGE");
	eq(r, PARSE_ERROR_NONE);
	e = s->effect_xtra;
	notnull(e);
	while (e->next) {
		e = e->next;
	}
	r = parser_parse(p, "dice-xtra:$B+$Dd$S");
	eq(r, PARSE_ERROR_NONE);
	for (i = 0; i < (int) N_ELEMENTS(test_cases); ++i) {
		r = parser_parse(p, test_cases[i].s);
		eq(r, PARSE_ERROR_NONE);
	}
	ok;
}

static int test_expr_xtra_bad0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	enum parser_error r;

	notnull(s);
	r = parser_parse(p, "effect-xtra:DAMAGE");
	eq(r, PARSE_ERROR_NONE);
	/*
	 * Using expr before the effect has dice specified currently does
	 * nothing.
	 */
	r = parser_parse(p, "expr-xtra:MAX_SIGHT:B:+ 1");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "dice-xtra:$B+$Dd$S");
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(p, "expr-xtra:C:SPELL_POWER:* 3 / 2");
	eq(r, PARSE_ERROR_UNBOUND_EXPRESSION);
	r = parser_parse(p, "expr-xtra:B:MAX_SIGHT:- 40000");
	eq(r, PARSE_ERROR_BAD_EXPRESSION_STRING);
	r = parser_parse(p, "expr-xtra:D:SPELL_POWER:/ 0");
	eq(r, PARSE_ERROR_BAD_EXPRESSION_STRING);
	r = parser_parse(p, "expr-xtra:S:MAX_SIGHT:% 2");
	eq(r, PARSE_ERROR_BAD_EXPRESSION_STRING);
	ok;
}

static int test_cutoff0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	r = parser_parse(p, "power-cutoff:10");
	eq(r, PARSE_ERROR_NONE);
	l = s->level;
	notnull(s);
	while (l->next) {
		l = l->next;
	}
	eq(l->power, 10);
	null(l->lore_desc);
	null(l->message);
	null(l->blind_message);
	null(l->silence_message);
	null(l->blind_silence_message);
	null(l->smart_message);
	null(l->smart_blind_message);
	null(l->smart_silence_message);
	null(l->smart_blind_silence_message);
	null(l->save_message);
	null(l->no_save_message);
	r = parser_parse(p, "power-cutoff:1000");
	eq(r, PARSE_ERROR_NONE);
	l = s->level;
	notnull(s);
	while (l->next) {
		l = l->next;
	}
	eq(l->power, 1000);
	null(l->lore_desc);
	null(l->message);
	null(l->blind_message);
	null(l->silence_message);
	null(l->blind_silence_message);
	null(l->smart_message);
	null(l->smart_blind_message);
	null(l->smart_silence_message);
	null(l->smart_blind_silence_message);
	null(l->save_message);
	null(l->no_save_message);
	ok;
}

static int test_lore0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	l = s->level;
	notnull(l);
	while (l->next) {
		l = l->next;
	}
	r = parser_parse(p, "lore:clean windows");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->lore_desc);
	require(streq(l->lore_desc, "clean windows"));
	r = parser_parse(p, "lore: expertly");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->lore_desc);
	require(streq(l->lore_desc, "clean windows expertly"));
	ok;
}

static int test_message_vis0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	l = s->level;
	notnull(l);
	while (l->next) {
		l = l->next;
	}
	r = parser_parse(p, "message-vis:{name} cackles");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->message);
	require(streq(l->message, "{name} cackles"));
	r = parser_parse(p, "message-vis: evilly.");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->message);
	require(streq(l->message, "{name} cackles evilly."));
	ok;
}

static int test_message_invis0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	l = s->level;
	notnull(l);
	while (l->next) {
		l = l->next;
	}
	r = parser_parse(p, "message-invis:Something cackles");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->blind_message);
	require(streq(l->blind_message, "Something cackles"));
	r = parser_parse(p, "message-invis: evilly.");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->blind_message);
	require(streq(l->blind_message, "Something cackles evilly."));
	ok;
}

static int test_message_smart_vis0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	l = s->level;
	notnull(l);
	while (l->next) {
		l = l->next;
	}
	r = parser_parse(p, "message-smart-vis:{name} shouts");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->smart_message);
	require(streq(l->smart_message, "{name} shouts"));
	r = parser_parse(p, "message-smart-vis: for help.");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->smart_message);
	require(streq(l->smart_message, "{name} shouts for help."));
	ok;
}

static int test_message_smart_invis0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	l = s->level;
	notnull(l);
	while (l->next) {
		l = l->next;
	}
	r = parser_parse(p, "message-smart-invis:You hear a shout");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->smart_blind_message);
	require(streq(l->smart_blind_message, "You hear a shout"));
	r = parser_parse(p, "message-smart-invis: for help.");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->smart_blind_message);
	require(streq(l->smart_blind_message, "You hear a shout for help."));
	ok;
}

static int test_message_silence_vis0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	l = s->level;
	notnull(l);
	while (l->next) {
		l = l->next;
	}
	r = parser_parse(p, "message-silence-vis:{name} lets out");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->silence_message);
	require(streq(l->silence_message, "{name} lets out"));
	r = parser_parse(p, "message-silence-vis: a muffled shriek.");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->silence_message);
	require(streq(l->silence_message, "{name} lets out a muffled shriek."));
	ok;
}

static int test_message_silence_invis0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	l = s->level;
	notnull(l);
	while (l->next) {
		l = l->next;
	}
	r = parser_parse(p, "message-silence-invis:You hear");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->blind_silence_message);
	require(streq(l->blind_silence_message, "You hear"));
	r = parser_parse(p, "message-silence-invis: a muffled shriek.");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->blind_silence_message);
	require(streq(l->blind_silence_message, "You hear a muffled shriek."));
	ok;
}

static int test_message_silence_smart_vis0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	l = s->level;
	notnull(l);
	while (l->next) {
		l = l->next;
	}
	r = parser_parse(p, "message-silence-smart-vis:{name} lets out");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->smart_silence_message);
	require(streq(l->smart_silence_message, "{name} lets out"));
	r = parser_parse(p,
		"message-silence-smart-vis: a muffled shout for help.");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->smart_silence_message);
	require(streq(l->smart_silence_message,
		"{name} lets out a muffled shout for help."));
	ok;
}

static int test_message_silence_smart_invis0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	l = s->level;
	notnull(l);
	while (l->next) {
		l = l->next;
	}
	r = parser_parse(p,
		"message-silence-smart-invis:You hear a muffled shout");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->smart_blind_silence_message);
	require(streq(l->smart_blind_silence_message,
		"You hear a muffled shout"));
	r = parser_parse(p, "message-silence-smart-invis: for help.");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->smart_blind_silence_message);
	require(streq(l->smart_blind_silence_message,
		"You hear a muffled shout for help."));
	ok;
}

static int test_message_save0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	l = s->level;
	notnull(l);
	while (l->next) {
		l = l->next;
	}
	r = parser_parse(p, "message-save:You duck");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->save_message);
	require(streq(l->save_message, "You duck"));
	r = parser_parse(p, "message-save: and are shaken but unharmed.");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->save_message);
	require(streq(l->save_message,
		"You duck and are shaken but unharmed."));
	ok;
}

static int test_message_no_save0(void *state) {
	struct parser *p = (struct parser*) state;
	struct monster_spell *s = parser_priv(p);
	struct monster_spell_level *l;
	enum parser_error r;

	notnull(s);
	l = s->level;
	notnull(l);
	while (l->next) {
		l = l->next;
	}
	r = parser_parse(p, "message-no-save:Your memories fade");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->no_save_message);
	require(streq(l->no_save_message, "Your memories fade"));
	r = parser_parse(p, "message-no-save: away.");
	eq(r, PARSE_ERROR_NONE);
	notnull(l->no_save_message);
	require(streq(l->no_save_message, "Your memories fade away."));
	ok;
}

const char *suite_name = "parse/mspell";
/*
 * test_missing_record0() has to be first.  test_name0() has to be before any
 * of the other tests except for test_name_bad0() and test_missing_record0().
 * test_name_bad0() should be before test_name0() and after
 * test_missing_record0() or after any of the other test that depend on
 * test_name0() (all except test_missing_record0()).
 * test_misplaced_effect_deps0() has to be before test_effect0(),
 * test_effect_bad0(), test_dice0(), test_dice_bad0(), test_expr0(), and
 * test_expr_bad0().
 * test_misplaced_effect_xtra_deps0() has to be before test_effect_xtra0(),
 * test_effect_xtra_bad0(), test_dice_xtra0(), test_dice_xtra_bad0(),
 * test_expr_xtra0(), and test_expr_xtra_bad0().
 */
struct test tests[] = {
	{ "missing_record0", test_missing_record0 },
	{ "name_bad0", test_name_bad0 },
	{ "name0", test_name0 },
	{ "msgt0", test_msgt0 },
	{ "msgt_bad0", test_msgt_bad0 },
	{ "mana0", test_mana0 },
	{ "best_range0", test_best_range0 },
	{ "max_range0", test_max_range0 },
	{ "desire0", test_desire0 },
	{ "disturb0", test_disturb0 },
	{ "use_past_range0", test_use_past_range0 },
	{ "misplaced_effect_deps0", test_misplaced_effect_deps0 },
	{ "effect0", test_effect0 },
	{ "effect_bad0", test_effect_bad0 },
	{ "dice0", test_dice0 },
	{ "dice_bad0", test_dice_bad0 },
	{ "expr0", test_expr0 },
	{ "expr_bad0", test_expr_bad0 },
	{ "misplaced_effect_xtra_deps0", test_misplaced_effect_xtra_deps0 },
	{ "effect_xtra0", test_effect_xtra0 },
	{ "effect_xtra_bad0", test_effect_xtra_bad0 },
	{ "dice_xtra_0", test_dice_xtra0 },
	{ "dice_xtra_bad0", test_dice_xtra_bad0 },
	{ "expr_xtra0", test_expr_xtra0 },
	{ "expr_xtra_bad0", test_expr_xtra_bad0 },
	{ "cutoff0", test_cutoff0 },
	{ "lore0", test_lore0 },
	{ "message_vis0", test_message_vis0 },
	{ "message_invis0", test_message_invis0 },
	{ "message_smart_vis0", test_message_smart_vis0 },
	{ "message_smart_invis0", test_message_smart_invis0 },
	{ "message_silence_vis0", test_message_silence_vis0 },
	{ "message_silence_invis0", test_message_silence_invis0 },
	{ "message_silence_smart_vis0", test_message_silence_smart_vis0 },
	{ "message_silence_smart_invis0", test_message_silence_smart_invis0 },
	{ "message_save0", test_message_save0 },
	{ "message_no_save0", test_message_no_save0 },
	{ NULL, NULL }
};
