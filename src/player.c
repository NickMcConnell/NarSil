/**
 * \file player.c
 * \brief Player implementation
 *
 * Copyright (c) 2011 elly+angband@leptoquark.net. See COPYING.
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
#include "effects.h"
#include "init.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "player-abilities.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-timed.h"
#include "randname.h"

/**
 * Pointer to the player struct
 */
struct player *player = NULL;

struct player_body *bodies;
struct player_race *races;
struct player_sex *sexes;
struct player_house *houses;
struct player_ability *player_abilities;

struct player_race *player_id2race(guid id)
{
	struct player_race *r;
	for (r = races; r; r = r->next)
		if (guid_eq(r->ridx, id))
			break;
	return r;
}

struct player_house *player_id2house(guid id)
{
	struct player_house *h;
	for (h = houses; h; h = h->next)
		if (guid_eq(h->hidx, id))
			break;
	return h;
}

struct player_house *player_house_from_count(int idx)
{
	unsigned int min = 100;
	struct player_house *h;
	const struct player_race *race = player->race;
	for (h = houses; h; h = h->next) {
		if ((h->race == race) && (h->hidx < min)) min = h->hidx;
	}
	for (h = houses; h; h = h->next) {
		if ((h->race == race) && (h->hidx == min + idx)) return h;
	}
	return NULL;
}

struct player_sex *player_id2sex(guid id)
{
	struct player_sex *s;
	for (s = sexes; s; s = s->next)
		if (guid_eq(s->sidx, id))
			break;
	return s;
}


static const char *stat_name_list[] = {
	#define STAT(a) #a,
	#include "list-stats.h"
	#undef STAT
	"MAX",
    NULL
};

int stat_name_to_idx(const char *name)
{
    int i;
    for (i = 0; stat_name_list[i]; i++) {
        if (!my_stricmp(name, stat_name_list[i]))
            return i;
    }

    return -1;
}

const char *stat_idx_to_name(int type)
{
    assert(type >= 0);
    assert(type < STAT_MAX);

    return stat_name_list[type];
}

/**
 * Increase a stat by one level
 *
 * Most code will "restore" a stat before calling this function,
 * in particular, stat potions will always restore the stat and
 * then increase the fully restored value.
 */
bool player_stat_inc(struct player *p, int stat)
{
	/* Cannot go above BASE_STAT_MAX */
	if (p->stat_base[stat] < BASE_STAT_MAX) {
		p->stat_base[stat]++;

		/* Recalculate bonuses */
		p->upkeep->update |= (PU_BONUS);

		/* Redisplay the stats later */
		p->upkeep->redraw |= (PR_STATS);

		/* Success */
		return true;
	}

	/* Nothing to gain */
	return false;
}

/**
 * Increase a stat by a number of points
 *
 * Return true only if this actually makes a difference.
 */
bool player_stat_res(struct player *p, int stat, int points)
{
	/* Restore if needed */
	if (p->stat_drain[stat] < 0) {
		p->stat_drain[stat] += points;
		if (p->stat_drain[stat] > 0) p->stat_drain[stat] = 0;

		/* Recalculate bonuses */
		p->upkeep->update |= (PU_BONUS);

		/* Redisplay the stats later */
		p->upkeep->redraw |= (PR_STATS);

		/* Success */
		return true;
	}

	/* Nothing to gain */
	return false;
}

/**
 * Decreases a stat by one level.
 *
 * Note that "permanent" means that the *given* amount is permanent,
 * not that the new value becomes permanent.
 */
void player_stat_dec(struct player *p, int stat)
{
	/* Temporary damage */
	p->stat_drain[stat]--;

	/* Recalculate bonuses */
	p->upkeep->update |= (PU_BONUS);

	/* Redisplay the stats later */
	p->upkeep->redraw |= (PR_STATS);
}

/**
 * Advance experience levels and print experience
 */
static void check_experience(struct player *p)
{
	/* Limits */
	p->exp = MIN(MAX(p->exp, 0), PY_MAX_EXP);
	p->new_exp = MIN(MAX(p->new_exp, 0), PY_MAX_EXP);
	p->new_exp = MIN(p->new_exp, p->exp);

	/* Redraw experience */
	p->upkeep->redraw |= (PR_EXP);

	/* Redraw stuff */
	redraw_stuff(p);
}

/**
 * Gain experience
 */
void player_exp_gain(struct player *p, int32_t amount)
{
	/* Gain some experience */
	p->exp += amount;
	p->new_exp += amount;

	/* Check Experience */
	check_experience(p);
}

/**
 * Lose experience
 */
void player_exp_lose(struct player *p, int32_t amount)
{
	/* Never drop below zero new experience */
	if (amount > p->new_exp) amount = p->new_exp;

	/* Lose some experience */
	p->exp -= amount;
	p->new_exp -= amount;

	/* Check Experience */
	check_experience(p);
}

/**
 * Obtain object flags for the player
 */
void player_flags(struct player *p, bitflag f[OF_SIZE])
{
	/* Add racial flags */
	memcpy(f, p->race->pflags, sizeof(p->race->pflags));
}


/**
 * Combine any flags due to timed effects on the player into those in f.
 */
void player_flags_timed(struct player *p, bitflag f[OF_SIZE])
{
	if (p->timed[TMD_SINVIS]) {
		of_on(f, OF_SEE_INVIS);
	}
}


uint8_t player_hp_attr(struct player *p)
{
	uint8_t attr;
	
	if (p->chp >= p->mhp)
		attr = COLOUR_L_GREEN;
	else if (p->chp > (p->mhp * p->opts.hitpoint_warn) / 10)
		attr = COLOUR_YELLOW;
	else
		attr = COLOUR_RED;
	
	return attr;
}

uint8_t player_sp_attr(struct player *p)
{
	uint8_t attr;
	
	if (p->csp >= p->msp)
		attr = COLOUR_L_GREEN;
	else if (p->csp > (p->msp * p->opts.hitpoint_warn) / 10)
		attr = COLOUR_YELLOW;
	else
		attr = COLOUR_RED;
	
	return attr;
}

bool player_restore_mana(struct player *p, int amt) {
	int old_csp = p->csp;

	p->csp += amt;
	if (p->csp > p->msp) {
		p->csp = p->msp;
	}
	p->upkeep->redraw |= PR_MANA;

	msg("You feel some of your energies returning.");

	return p->csp != old_csp;
}

/**
 * Construct a random player name appropriate for the setting.
 *
 * \param buf is the buffer to contain the name.  Must have space for at
 * least buflen characters.
 * \param buflen is the maximum number of character that can be written to
 * buf.
 * \return the number of characters, excluding the terminating null, written
 * to the buffer
 */
size_t player_random_name(char *buf, size_t buflen)
{
	size_t result = randname_make(RANDNAME_TOLKIEN, 4, 8, buf, buflen,
		name_sections);

	my_strcap(buf);
	return result;
}

/**
 * Return a version of the player's name safe for use in filesystems.
 *
 * XXX This does not belong here.
 */
void player_safe_name(char *safe, size_t safelen, const char *name, bool strip_suffix)
{
	size_t i;
	size_t limit = 0;

	if (name) {
		char *suffix = find_roman_suffix_start(name);

		if (suffix) {
			limit = suffix - name - 1; /* -1 for preceding space */
		} else {
			limit = strlen(name);
		}
	}

	/* Limit to maximum size of safename buffer */
	limit = MIN(limit, safelen);

	for (i = 0; i < limit; i++) {
		char c = name[i];

		/* Convert all non-alphanumeric symbols */
		if (!isalpha((unsigned char)c) && !isdigit((unsigned char)c))
			c = '_';

		/* Build "base_name" */
		safe[i] = c;
	}

	/* Terminate */
	safe[i] = '\0';

	/* Require a "base" name */
	if (!safe[0])
		my_strcpy(safe, "PLAYER", safelen);
}


/**
 * Release resources allocated for fields in the player structure.
 */
void player_cleanup_members(struct player *p)
{
	/* Free the history */
	history_clear(p);

	mem_free(p->timed);
	if (p->upkeep) {
		mem_free(p->upkeep->inven);
		mem_free(p->upkeep);
		p->upkeep = NULL;
	}
	mem_free(p->vaults);

	/* Free the things that are only sometimes initialised */
	if (p->gear) {
		object_pile_free(NULL, p->gear);
	}
	if (p->body.slots) {
		for (int i = 0; i < p->body.count; i++)
			string_free(p->body.slots[i].name);
		mem_free(p->body.slots);
	}
	string_free(p->body.name);
	string_free(p->history);
	release_ability_list(p->abilities);
	release_ability_list(p->item_abilities);
}


/**
 * Initialise player struct
 */
static void init_player(void) {
	/* Create the player array, initialised with 0 */
	player = mem_zalloc(sizeof *player);

	/* Allocate player sub-structs */
	player->upkeep = mem_zalloc(sizeof(struct player_upkeep));
	player->upkeep->inven = mem_zalloc((z_info->pack_size + 1) * sizeof(struct object *));
	player->timed = mem_zalloc(TMD_MAX * sizeof(int16_t));
	player->vaults = mem_zalloc(z_info->v_max * sizeof(int16_t));

	options_init_defaults(&player->opts);
}

/**
 * Free player struct
 */
static void cleanup_player(void) {
	if (!player) return;

	player_cleanup_members(player);

	/* Free the basic player struct */
	mem_free(player);
	player = NULL;
}

struct init_module player_module = {
	.name = "player",
	.init = init_player,
	.cleanup = cleanup_player
};
