/**
 * \file player-birth.c
 * \brief Character creation
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cmd-core.h"
#include "cmds.h"
#include "game-event.h"
#include "game-world.h"
#include "init.h"
#include "mon-lore.h"
#include "monster.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-init.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-properties.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-skills.h"
#include "player-timed.h"
#include "player-util.h"
#include "savefile.h"

/**
 * Overview
 * ========
 * This file contains the game-mechanical part of the birth process.
 * To follow the code, start at player_birth towards the bottom of
 * the file - that is the only external entry point to the functions
 * defined here.
 *
 * Player (in the Angband sense of character) birth is modelled as a
 * a series of commands from the UI to the game to manipulate the
 * character and corresponding events to inform the UI of the outcomes
 * of these changes.
 *
 * The current aim of this section is that after any birth command
 * is carried out, the character should be left in a playable state.
 * In particular, this means that if a savefile is supplied, the
 * character will be set up according to the "quickstart" rules until
 * another race or house is chosen, or until the stats are reset by
 * the UI.
 *
 * Once the UI signals that the player is happy with the character, the
 * game does housekeeping to ensure the character is ready to start the
 * game (clearing the history log, making sure options are set, etc)
 * before returning control to the game proper.
 */


/* These functions are defined at the end of the file */
static int roman_to_int(const char *roman);
static int int_to_roman(int n, char *roman, size_t bufsize);


/**
 * Forward declare
 */
typedef struct birther birther;

/**
 * A structure to hold "rolled" information, and any
 * other useful state for the birth process.
 */
struct birther
{
	const struct player_race *race;
	const struct player_house *house;
	const struct player_sex *sex;

	int16_t age;
	int16_t wt;
	int16_t ht;

	int16_t stat[STAT_MAX];

	char *history;
	char name[PLAYER_NAME_LEN];
};


/**
 * ------------------------------------------------------------------------
 * All of these should be in some kind of 'birth state' struct somewhere else
 * ------------------------------------------------------------------------ */


static int stats[STAT_MAX];
static int points_spent[STAT_MAX];
static int points_inc[STAT_MAX];
static int points_left;

static bool quickstart_allowed;

/**
 * The last character displayed, to allow the user to flick between two.
 * We rely on prev.age being zero to determine whether there is a stored
 * character or not, so initialise it here.
 */
static birther prev;

/**
 * If quickstart is allowed, we store the old character in this,
 * to allow for it to be reloaded if we step back that far in the
 * birth process.
 */
static birther quickstart_prev;




/**
 * Save the current birth data into the supplied 'player'.
 */
static void save_birth_data(birther *tosave)
{
	int i;

	/* Save the data */
	tosave->race = player->race;
	tosave->house = player->house;
	tosave->sex = player->sex;
	tosave->age = player->age;
	tosave->wt = player->wt_birth;
	tosave->ht = player->ht_birth;

	/* Save the stats */
	for (i = 0; i < STAT_MAX; i++)
		tosave->stat[i] = player->stat_base[i] -
			(player->race->stat_adj[i] + player->house->stat_adj[i]);

	if (tosave->history) {
		string_free(tosave->history);
	}
	tosave->history = player->history;
	player->history = NULL;
	my_strcpy(tosave->name, player->full_name, sizeof(tosave->name));
}


/**
 * Load stored player data from 'player' as the current birth data,
 * optionally placing the current data in 'prev_player' (if 'prev_player'
 * is non-NULL).
 *
 * It is perfectly legal to specify the same "birther" for both 'player'
 * and 'prev_player'.
 */
static void load_birth_data(birther *saved, birther *prev_player)
{
	int i;

     /* The initialisation is just paranoia - structure assignment is
        (perhaps) not strictly defined to work with uninitialised parts
        of structures. */
	birther temp;
	memset(&temp, 0, sizeof(birther));

	/* Save the current data if we'll need it later */
	if (prev_player)
		save_birth_data(&temp);

	/* Load previous data */
	player->race     = saved->race;
	player->house    = saved->house;
	player->sex      = saved->sex;
	player->age      = saved->age;
	player->wt       = player->wt_birth = saved->wt;
	player->ht       = player->ht_birth = saved->ht;

	/* Load previous stats */
	for (i = 0; i < STAT_MAX; i++) {
		player->stat_base[i] = saved->stat[i];
	}

	if (player->history) {
		string_free(player->history);
	}
	player->history = string_make(saved->history);
	my_strcpy(player->full_name, saved->name, sizeof(player->full_name));

	/* Save the current data if the caller is interested in it. */
	if (prev_player) {
		if (prev_player->history) {
			string_free(prev_player->history);
		}
		*prev_player = temp;
	}
}


static void get_bonuses(struct player *p)
{
	/* Calculate the bonuses and hitpoints */
	p->upkeep->update |= (PU_BONUS | PU_HP);

	/* Update stuff */
	update_stuff(p);

	/* Fully healed */
	p->chp = p->mhp;

	/* Fully rested */
	calc_voice(p, true);
	p->csp = p->msp;
}


/**
 * Get the racial history, and social class, using the "history charts".
 */
char *get_history(struct history_chart *chart, struct player *p)
{
	struct history_entry *entry;
	char *res = NULL;

	while (chart) {
		int roll = randint1(100);
		for (entry = chart->entries; entry; entry = entry->next)
			if (roll <= entry->roll)
				break;
		assert(entry);

		res = string_append(res, entry->text);
		/* Hack for Noldor houses */
		if (strstr(entry->text, "house of") && streq(p->race->name, "Noldor")) {
			res = string_append(res, " ");
			res = string_append(res, p->house->short_name);
			res = string_append(res, ".");
		}
		chart = entry->succ;
	}

	return res;
}


/**
 * Computes character's age, height, and weight
 */
void get_ahw(struct player *p)
{
	/* Calculate the age */
	p->age = p->race->b_age + randint1(p->race->m_age);

	/* Calculate the height/weight */
	p->ht = p->ht_birth = Rand_normal(p->race->base_hgt, p->race->mod_hgt);
	p->wt = p->wt_birth = Rand_normal(p->race->base_wgt, p->race->mod_wgt);
}




/**
 * Creates the player's body
 */
static void player_embody(struct player *p)
{
	char buf[80];
	int i;

	assert(p->race);

	memcpy(&p->body, &bodies[p->race->body], sizeof(p->body));
	my_strcpy(buf, bodies[p->race->body].name, sizeof(buf));
	p->body.name = string_make(buf);
	p->body.slots = mem_zalloc(p->body.count * sizeof(struct equip_slot));
	for (i = 0; i < p->body.count; i++) {
		p->body.slots[i].type = bodies[p->race->body].slots[i].type;
		my_strcpy(buf, bodies[p->race->body].slots[i].name, sizeof(buf));
		p->body.slots[i].name = string_make(buf);
	}
}

void player_init(struct player *p)
{
	int i;
	struct player_options opts_save = p->opts;

	player_cleanup_members(p);

	/* Wipe the player */
	memset(p, 0, sizeof(struct player));

	/* Start with no artifacts made yet */
	for (i = 0; z_info && i < z_info->a_max; i++) {
		mark_artifact_created(&a_info[i], false);
		mark_artifact_seen(&a_info[i], false);
	}

	for (i = 1; z_info && i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];
		kind->tried = false;
		kind->aware = false;
	}

	for (i = 1; z_info && i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];
		struct monster_lore *lore = get_lore(race);
		race->cur_num = 0;
		race->max_num = 100;
		if (rf_has(race->flags, RF_UNIQUE))
			race->max_num = 1;
		lore->pkills = 0;
		lore->psights = 0;
	}

	p->upkeep = mem_zalloc(sizeof(struct player_upkeep));
	p->upkeep->inven = mem_zalloc((z_info->pack_size + 1) *
								  sizeof(struct object *));
	p->timed = mem_zalloc(TMD_MAX * sizeof(int16_t));
	p->vaults = mem_zalloc(z_info->v_max * sizeof(int16_t));
	p->obj_k = mem_zalloc(sizeof(struct object));
	p->obj_k->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
	p->obj_k->slays = mem_zalloc(z_info->slay_max * sizeof(bool));

	/* Options should persist */
	p->opts = opts_save;

	/* First turn. */
	turn = 1;

	/* Default to the first race/house/sex in the edit file */
	p->race = races;
	p->house = houses;
	p->sex = sexes;
}

/**
 * Try to wield everything wieldable in the inventory.
 */
void wield_all(struct player *p)
{
	struct object *obj, *new_pile = NULL, *new_known_pile = NULL;
	int slot;

	/* Scan through the slots */
	for (obj = p->gear; obj; obj = obj->next) {
		struct object *obj_temp;

		/* Skip non-objects */
		assert(obj);

		/* Make sure we can wield it */
		slot = wield_slot(obj);
		if (slot < 0 || slot >= p->body.count)
			continue;

		obj_temp = slot_object(p, slot);
		if (obj_temp)
			continue;

		/* Split if necessary */
		if ((obj->number > 1) && !tval_is_ammo(obj)) {
			/* All but one go to the new object */
			struct object *new = object_split(obj, obj->number - 1);

			/* Add to the pile of new objects to carry */
			pile_insert(&new_pile, new);
			pile_insert(&new_known_pile, new->known);
		}

		/* Wear the new stuff */
		p->body.slots[slot].obj = obj;
		object_learn_on_wield(p, obj);

		/* Increment the equip counter by hand */
		p->upkeep->equip_cnt++;
	}

	/* Now add the unwielded split objects to the gear */
	if (new_pile) {
		pile_insert_end(&p->gear, new_pile);
		pile_insert_end(&p->gear_k, new_known_pile);
	}
	return;
}


/**
 * Initialize the global player as if the full birth process happened.
 * \param nrace Is the name of the race to use.  It may be NULL to use *races.
 * \param nhouse Is the name of the house to use.  It may be NULL to use
 * *houses.
 * \param nsex Is the name of the sex to use.  It may be NULL to use *sexes.
 * \param nplayer Is the name to use for the player.  It may be NULL.
 * \return The return value will be true if the full birth process will be
 * successful.  It will be false if the process failed.  One reason for that
 * would be that the requested race or house could not be found.
 * Requires a prior call to init_angband().  Intended for use by test cases
 * or stub front ends that need a fully initialized player.
 */
bool player_make_simple(const char *nrace, const char *nhouse, const char *nsex,
	const char* nplayer)
{
	int ir = 0, ih = 0, is = 0;

	if (nrace) {
		const struct player_race *rc = races;
		int nr = 0;

		while (1) {
			if (!rc) return false;
			if (streq(rc->name, nrace)) break;
			rc = rc->next;
			++ir;
			++nr;
		}
		while (rc) {
			rc = rc->next;
			++nr;
		}
		ir = nr - ir  - 1;
	}

	if (nhouse) {
		const struct player_house *hc = houses;
		int nh = 0;

		while (1) {
			if (!hc) return false;
			if (streq(hc->name, nhouse)) break;
			hc = hc->next;
			++ih;
			++nh;
		}
		while (hc) {
			hc = hc->next;
			++nh;
		}
		ih = nh - ih - 1;
	}

	if (nsex) {
		const struct player_sex *sc = sexes;
		int ns = 0;

		while (1) {
			if (!sc) return false;
			if (streq(sc->name, nsex)) break;
			sc = sc->next;
			++is;
			++ns;
		}
		while (sc) {
			sc = sc->next;
			++ns;
		}
		is = ns - is - 1;
	}

	cmdq_push(CMD_BIRTH_INIT);
	cmdq_push(CMD_BIRTH_RESET);
	cmdq_push(CMD_CHOOSE_RACE);
	cmd_set_arg_choice(cmdq_peek(), "choice", ir);
	cmdq_push(CMD_CHOOSE_HOUSE);
	cmd_set_arg_choice(cmdq_peek(), "choice", ih);
	cmdq_push(CMD_CHOOSE_SEX);
	cmd_set_arg_choice(cmdq_peek(), "choice", is);
	cmdq_push(CMD_NAME_CHOICE);
	cmd_set_arg_string(cmdq_peek(), "name",
		(nplayer == NULL) ? "Simple" : nplayer);
	cmdq_push(CMD_ACCEPT_CHARACTER);
	cmdq_execute(CTX_BIRTH);

	return true;
}


/**
 * Init players with some belongings
 *
 * Having an item identifies it and makes the player "aware" of its purpose.
 */
static void player_outfit(struct player *p)
{
	int i;
	const struct start_item *si;
	struct object *obj, *known_obj;

	/* Currently carrying nothing */
	p->upkeep->total_weight = 0;

	/* Give the player obvious object knowledge */
	p->obj_k->dd = 1;
	p->obj_k->ds = 1;
	p->obj_k->pd = 1;
	p->obj_k->ps = 1;
	p->obj_k->att = 1;
	p->obj_k->evn = 1;
	for (i = 1; i < OF_MAX; i++) {
		struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_FLAG, i);
		if (prop->subtype == OFT_BASIC) of_on(p->obj_k->flags, i);
	}

	/* Give the player starting equipment */
	for (si = p->race->start_items; si; si = si->next) {
		int num = rand_range(si->min, si->max);
		struct object_kind *kind = lookup_kind(si->tval, si->sval);
		assert(kind);

		/* Prepare a new item */
		obj = object_new();
		object_prep(obj, kind, 0, MINIMISE);
		obj->number = num;
		obj->origin = ORIGIN_BIRTH;

		known_obj = object_new();
		obj->known = known_obj;
		object_set_base_known(p, obj);
		object_flavor_aware(p, obj);
		obj->known->pval = obj->pval;
		obj->known->notice |= OBJ_NOTICE_ASSESSED;

		/* Carry the item */
		inven_carry(p, obj, true, false);
		kind->everseen = true;
	}

	/* Now try wielding everything */
	wield_all(p);
}


/**
 * Cost of each "point" of a stat.
 */
static const int birth_stat_costs[11] = { -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6};

#define MAX_COST 13 

static void recalculate_stats(int *stats_local_local)
{
	int i;

	/* Variable stat maxes */
	for (i = 0; i < STAT_MAX; i++) {
		player->stat_base[i] = stats_local_local[i];
	}

	/* Update bonuses, hp, etc. */
	get_bonuses(player);

	/* Tell the UI about all this stuff that's changed. */
	event_signal(EVENT_HP);
	event_signal(EVENT_STATS);
}

static void reset_stats(int stats_local[STAT_MAX],
						int points_spent_local[STAT_MAX],
						int points_inc_local[STAT_MAX],
						int *points_left_local, bool update_display)
{
	int i;

	/* Calculate and signal initial stats and points totals. */
	*points_left_local = MAX_COST;

	for (i = 0; i < STAT_MAX; i++) {
		/* Initial stats are set to the race/house values and costs are zero */
		stats_local[i] = 0;
		points_spent_local[i] = 0;
		points_inc_local[i] = birth_stat_costs[stats_local[i] + 4 + 1];
	}

	/* Use the new "birth stat" values to work out the "other"
	   stat values (i.e. after modifiers) and tell the UI things have 
	   changed. */
	if (update_display) {
		recalculate_stats(stats_local);
		event_signal_birthpoints(points_spent_local, points_inc_local,
								 *points_left_local);
	}
}

static bool buy_stat(int choice, int stats_local[STAT_MAX],
					 int points_spent_local[STAT_MAX],
					 int points_inc_local[STAT_MAX],
					 int *points_left_local, bool update_display)
{
	/* Must be a valid stat to be adjusted */
	if (!(choice >= STAT_MAX || choice < 0)) {
		/* Get the cost of buying the extra point (beyond what
		   it has already cost to get this far). */
		int stat_cost = birth_stat_costs[stats_local[choice] + 4 + 1];

		assert(stat_cost == points_inc_local[choice]);
		if (stat_cost <= *points_left_local) {
			stats_local[choice]++;
			points_spent_local[choice] += stat_cost;
			points_inc_local[choice] =
				birth_stat_costs[stats_local[choice] + 4 + 1];
			*points_left_local -= stat_cost;

			if (update_display) {
				/* Tell the UI the new points situation. */
				event_signal_birthpoints(points_spent_local,
					points_inc_local, *points_left_local);

				/* Recalculate everything that's changed because
				   the stat has changed, and inform the UI. */
				recalculate_stats(stats_local);
			}

			return true;
		}
	}

	/* Didn't adjust stat. */
	return false;
}


static bool sell_stat(int choice, int stats_local[STAT_MAX],
					  int points_spent_local[STAT_MAX],
					  int points_inc_local[STAT_MAX],
					  int *points_left_local, bool update_display)
{
	/* Must be a valid stat, and we can't "sell" stats below 0. */
	if (!(choice >= STAT_MAX || choice < 0) && (stats_local[choice] > 0)) {
		int stat_cost = birth_stat_costs[stats_local[choice] + 4];

		stats_local[choice]--;
		points_spent_local[choice] -= stat_cost;
		points_inc_local[choice] =
			birth_stat_costs[stats_local[choice] + 4 + 1];
		*points_left_local += stat_cost;

		if (update_display) {
			/* Tell the UI the new points situation. */
			event_signal_birthpoints(points_spent_local,
				points_inc_local, *points_left_local);

			/* Recalculate everything that's changed because
			   the stat has changed, and inform the UI. */
			recalculate_stats(stats_local);
		}

		return true;
	}

	/* Didn't adjust stat. */
	return false;
}


/**
 * Add race and house stat points to what we've chosen.
 */
static void finalise_stats(struct player *p)
{
	int i;
	for (i = 0; i < STAT_MAX; i++) {
		p->stat_base[i] += p->race->stat_adj[i] + p->house->stat_adj[i];
	}
}

/**
 * This fleshes out a full player based on the choices currently made,
 * and so is called whenever things like race or house are chosen.
 */
void player_generate(struct player *p, const struct player_race *r,
					 const struct player_house *h, const struct player_sex *s,
					 bool old_history)
{
	if (!h)
		h = p->house;
	if (!r)
		r = p->race;
	if (!s)
		s = p->sex;

	p->house = h;
	p->race = r;
	p->sex = s;
	if (!p->house) {
		p->house = player_house_from_count(0);
	}

	/* Initial experience */
	p->exp = p->new_exp = z_info->start_exp;

	/* Initial hitpoints etc */
	get_bonuses(p);

	/* Roll for age/height/weight */
	get_ahw(p);

	/* Always start with a well fed player */
	p->timed[TMD_FOOD] = PY_FOOD_FULL - 1;

	if (!old_history) {
		if (p->history) {
			string_free(p->history);
		}
		p->history = get_history(p->race->history, p);
	}
}


/**
 * Reset everything back to how it would be on loading the game.
 */
static void do_birth_reset(bool use_quickstart, birther *quickstart_prev_local)
{
	/* If there's quickstart data, we use it to set default
	   character choices. */
	if (use_quickstart && quickstart_prev_local)
		load_birth_data(quickstart_prev_local, NULL);

	player_generate(player, NULL, NULL, NULL,
					use_quickstart && quickstart_prev_local);

	player->depth = 1;

	/* Update stats with bonuses, etc. */
	get_bonuses(player);
}

void do_cmd_birth_init(struct command *cmd)
{
	char *buf;

	/* The dungeon is not ready */
	character_dungeon = false;

	/*
	 * If there's a quickstart character, store it for later use.
	 * If not, default to whatever the first of the choices is.
	 */
	if (player->ht_birth) {
		int i, total_stat_cost = 0;
		bool stats_ok = true;

		/* Handle incrementing name suffix */
		buf = find_roman_suffix_start(player->full_name);
		if (buf) {
			/* Try to increment the roman suffix */
			int success = int_to_roman(
				roman_to_int(buf) + 1,
				buf,
				sizeof(player->full_name) - (buf - (char *)&player->full_name));

			if (!success) {
				msg("Sorry, could not deal with suffix");
			}
		}

		/* Sanity check stats */
		for (i = 0; i < STAT_MAX; i++) {
			int stat = player->stat_base[i];

			/* This stat is too expensive, must be debug altered */
			if (stat > 6) {
				stats_ok = false;
				break;
			}

			/* Check if the total cost is too much */
			while (stat) {
				total_stat_cost += birth_stat_costs[4 + 1 + stat];
				if (total_stat_cost > MAX_COST) {
					stats_ok = false;
					break;
				}
			}
			if (!stats_ok) break;
		}

		if (stats_ok) {
			save_birth_data(&quickstart_prev);
			quickstart_allowed = true;
		}
	} else {
		player_generate(player, player_id2race(0), player_house_from_count(0),
						player_id2sex(0), false);
		quickstart_allowed = false;
	}

	/* We're ready to start the birth process */
	event_signal_flag(EVENT_ENTER_BIRTH, quickstart_allowed);
}

void do_cmd_birth_reset(struct command *cmd)
{
	player_init(player);
	reset_stats(stats, points_spent, points_inc, &points_left, false);
	init_skills(true, false);
	do_birth_reset(quickstart_allowed, &quickstart_prev);
}

void do_cmd_choose_race(struct command *cmd)
{
	int choice;
	cmd_get_arg_choice(cmd, "choice", &choice);
	player_generate(player, player_id2race(choice), NULL, NULL, false);

	init_skills(true, true);
}

void do_cmd_choose_house(struct command *cmd)
{
	int choice;
	cmd_get_arg_choice(cmd, "choice", &choice);
	player_generate(player, NULL, player_house_from_count(choice), NULL, false);

	init_skills(true, true);
}

void do_cmd_choose_sex(struct command *cmd)
{
	int choice;
	cmd_get_arg_choice(cmd, "choice", &choice);
	player_generate(player, NULL, NULL, player_id2sex(choice), false);

	init_skills(true, true);
}

void do_cmd_buy_stat(struct command *cmd)
{
	/* .choice is the stat to sell */
	int choice;
	cmd_get_arg_choice(cmd, "choice", &choice);
	buy_stat(choice, stats, points_spent, points_inc, &points_left, true);
}

void do_cmd_sell_stat(struct command *cmd)
{
	/* .choice is the stat to sell */
	int choice;
	cmd_get_arg_choice(cmd, "choice", &choice);
	sell_stat(choice, stats, points_spent, points_inc, &points_left, true);
}

void do_cmd_reset_stats(struct command *cmd)
{
	reset_stats(stats, points_spent, points_inc, &points_left, true);
}

void do_cmd_refresh_stats(struct command *cmd)
{
	event_signal_birthpoints(points_spent, points_inc, points_left);
}

void do_cmd_choose_name(struct command *cmd)
{
	const char *str;
	cmd_get_arg_string(cmd, "name", &str);

	/* Set player name */
	my_strcpy(player->full_name, str, sizeof(player->full_name));
}

void do_cmd_choose_history(struct command *cmd)
{
	const char *str;

	/* Forget the old history */
	if (player->history)
		string_free(player->history);

	/* Get the new history */
	cmd_get_arg_string(cmd, "history", &str);
	player->history = string_make(str);
}

void do_cmd_accept_character(struct command *cmd)
{
	options_init_cheat();

	ignore_birth_init();

	/* Clear old messages, add new starting message */
	history_clear(player);
	history_add(player, "Began the quest to recover a Silmaril.", HIST_PLAYER_BIRTH);

	/* Note player birth in the message recall */
	message_add(" ", MSG_GENERIC);
	message_add("  ", MSG_GENERIC);
	message_add("====================", MSG_GENERIC);
	message_add("  ", MSG_GENERIC);
	message_add(" ", MSG_GENERIC);

	/* Embody */
	player_embody(player);

	/* Record final starting stats and skills */
	finalise_stats(player);
	finalise_skills();

	/* Hack - player knows the tunneling rune. */
	player->obj_k->modifiers[OBJ_MOD_TUNNEL] = 1;

	/* This is actually just a label for the file of self-made artefacts */
	seed_randart = randint0(0x10000000);

	/* Seed for flavors */
	seed_flavor = randint0(0x10000000);
	flavor_init();

	/* Outfit the player, if they can sell the stuff */
	player_outfit(player);

	/* Stop the player being quite so dead */
	player->is_dead = false;

	/* Character is now "complete" */
	character_generated = true;
	player->upkeep->playing = true;

	/* Disable repeat command, so we don't try to be born again */
	cmd_disable_repeat();

	/* No longer need the cached history. */
	string_free(prev.history);
	prev.history = NULL;
	string_free(quickstart_prev.history);
	quickstart_prev.history = NULL;

	/* Now we're really done.. */
	event_signal(EVENT_LEAVE_BIRTH);
}



/**
 * ------------------------------------------------------------------------
 * Roman numeral functions, for dynastic successions
 * ------------------------------------------------------------------------ */


/**
 * Find the start of a possible Roman numerals suffix by going back from the
 * end of the string to a space, then checking that all the remaining chars
 * are valid Roman numerals.
 * 
 * Return the start position, or NULL if there isn't a valid suffix. 
 */
char *find_roman_suffix_start(const char *buf)
{
	const char *start = strrchr(buf, ' ');
	const char *p;
	
	if (start) {
		start++;
		p = start;
		while (*p) {
			if (*p != 'I' && *p != 'V' && *p != 'X' && *p != 'L' &&
			    *p != 'C' && *p != 'D' && *p != 'M') {
				start = NULL;
				break;
			}
			++p;			    
		}
	}
	return (char *)start;
}

/**
 * Converts an arabic numeral (int) to a roman numeral (char *).
 *
 * An arabic numeral is accepted in parameter `n`, and the corresponding
 * upper-case roman numeral is placed in the parameter `roman`.  The
 * length of the buffer must be passed in the `bufsize` parameter.  When
 * there is insufficient room in the buffer, or a roman numeral does not
 * exist (e.g. non-positive integers) a value of 0 is returned and the
 * `roman` buffer will be the empty string.  On success, a value of 1 is
 * returned and the zero-terminated roman numeral is placed in the
 * parameter `roman`.
 */
static int int_to_roman(int n, char *roman, size_t bufsize)
{
	/* Roman symbols */
	char roman_symbol_labels[13][3] =
		{"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX",
		 "V", "IV", "I"};
	int  roman_symbol_values[13] =
		{1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};

	/* Clear the roman numeral buffer */
	roman[0] = '\0';

	/* Roman numerals have no zero or negative numbers */
	if (n < 1)
		return 0;

	/* Build the roman numeral in the buffer */
	while (n > 0) {
		int i = 0;

		/* Find the largest possible roman symbol */
		while (n < roman_symbol_values[i])
			i++;

		/* No room in buffer, so abort */
		if (strlen(roman) + strlen(roman_symbol_labels[i]) + 1
			> bufsize)
			break;

		/* Add the roman symbol to the buffer */
		my_strcat(roman, roman_symbol_labels[i], bufsize);

		/* Decrease the value of the arabic numeral */
		n -= roman_symbol_values[i];
	}

	/* Ran out of space and aborted */
	if (n > 0) {
		/* Clean up and return */
		roman[0] = '\0';

		return 0;
	}

	return 1;
}


/**
 * Converts a roman numeral (char *) to an arabic numeral (int).
 *
 * The null-terminated roman numeral is accepted in the `roman`
 * parameter and the corresponding integer arabic numeral is returned.
 * Only upper-case values are considered. When the `roman` parameter
 * is empty or does not resemble a roman numeral, a value of -1 is
 * returned.
 *
 * XXX This function will parse certain non-sense strings as roman
 *     numerals, such as IVXCCCVIII
 */
static int roman_to_int(const char *roman)
{
	size_t i;
	int n = 0;
	char *p;

	char roman_token_chr1[] = "MDCLXVI";
	const char *roman_token_chr2[] = {0, 0, "DM", 0, "LC", 0, "VX"};

	int roman_token_vals[7][3] = {{1000},
	                              {500},
	                              {100, 400, 900},
	                              {50},
	                              {10, 40, 90},
	                              {5},
	                              {1, 4, 9}};

	if (strlen(roman) == 0)
		return -1;

	/* Check each character for a roman token, and look ahead to the
	   character after this one to check for subtraction */
	for (i = 0; i < strlen(roman); i++) {
		char c1, c2;
		int c1i, c2i;

		/* Get the first and second chars of the next roman token */
		c1 = roman[i];
		c2 = roman[i + 1];

		/* Find the index for the first character */
		p = strchr(roman_token_chr1, c1);
		if (p)
			c1i = p - roman_token_chr1;
		else
			return -1;

		/* Find the index for the second character */
		c2i = 0;
		if (roman_token_chr2[c1i] && c2) {
			p = strchr(roman_token_chr2[c1i], c2);
			if (p) {
				c2i = (p - roman_token_chr2[c1i]) + 1;
				/* Two-digit token, so skip a char on the next pass */
				i++;
			}
		}

		/* Increase the arabic numeral */
		n += roman_token_vals[c1i][c2i];
	}

	return n;
}
