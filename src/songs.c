/**
 * \file songs.c
 * \brief Player and monster songs
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
#include "cave.h"
#include "datafile.h"
#include "effects.h"
#include "game-world.h"
#include "init.h"
#include "mon-desc.h"
#include "mon-predicate.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-util.h"
#include "songs.h"

struct song *songs;

/**
 * ------------------------------------------------------------------------
 * Initialize songs
 * ------------------------------------------------------------------------ */
static int song_index = 1;

static enum parser_error parse_song_name(struct parser *p) {
    const char *name = parser_getstr(p, "name");
    struct song *h = parser_priv(p);

    struct song *s = mem_zalloc(sizeof *s);
    s->next = h;
    s->name = string_make(name);
    parser_setpriv(p, s);

	/* Static initialisation means first entry has index 0 */
	s->index = song_index;
	song_index++;
	
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_song_verb(struct parser *p) {
    struct song *s = parser_priv(p);
    assert(s);

    s->verb = string_append(s->verb, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_song_desc(struct parser *p) {
    struct song *s = parser_priv(p);
    assert(s);

    s->desc = string_append(s->desc, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_song_alt_desc(struct parser *p) {
    struct song *s = parser_priv(p);
	struct alt_song_desc *alt = mem_zalloc(sizeof(*alt));
    assert(s);

	alt->next = s->alt_desc;
	s->alt_desc = alt;
    alt->desc = string_make(parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_song_msg(struct parser *p) {
    struct song *s = parser_priv(p);
    assert(s);

    s->msg = string_append(s->msg, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_song_bonus_mult(struct parser *p) {
	struct song *s = parser_priv(p);
	if (!s)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	s->bonus_mult = parser_getint(p, "mult");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_song_bonus_div(struct parser *p) {
	struct song *s = parser_priv(p);
	if (!s)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	s->bonus_div = parser_getint(p, "div");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_song_bonus_min(struct parser *p) {
	struct song *s = parser_priv(p);
	if (!s)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	s->bonus_min = parser_getint(p, "min");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_song_noise(struct parser *p) {
	struct song *s = parser_priv(p);
	if (!s)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	s->noise = parser_getint(p, "noise");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_song_extend(struct parser *p) {
	struct song *s = parser_priv(p);
	if (!s)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	s->extend = parser_getint(p, "extend") ? true : false;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_song_effect(struct parser *p) {
	struct song *s = parser_priv(p);
	struct effect *new_effect = mem_zalloc(sizeof(*new_effect));
	if (!s)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* Songs only have a single effect */
	s->effect = new_effect;

	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static struct parser *init_parse_song(void) {
    struct parser *p = parser_new();
    parser_setpriv(p, NULL);
    parser_reg(p, "name str name", parse_song_name);
	parser_reg(p, "verb str text", parse_song_verb);
	parser_reg(p, "desc str text", parse_song_desc);
	parser_reg(p, "alt-desc str text", parse_song_alt_desc);
	parser_reg(p, "msg str text", parse_song_msg);
	parser_reg(p, "bonus-mult int mult", parse_song_bonus_mult);
	parser_reg(p, "bonus-div int div", parse_song_bonus_div);
	parser_reg(p, "bonus-min int min", parse_song_bonus_min);
	parser_reg(p, "noise int noise", parse_song_noise);
	parser_reg(p, "extend int extend", parse_song_extend);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other",
			   parse_song_effect);
	return p;
}

static errr run_parse_song(struct parser *p) {
    return parse_file_quit_not_found(p, "song");
}

static errr finish_parse_song(struct parser *p) {
	songs = parser_priv(p);
	parser_destroy(p);
	return 0;
}

static void cleanup_song(void)
{
	struct song *s = songs, *next;
	while (s) {
		struct alt_song_desc *alt = s->alt_desc;
		next = s->next;
		while (alt) {
			struct alt_song_desc *alt_tgt = alt;
			alt = alt->next;
			string_free(alt_tgt->desc);
			mem_free(alt_tgt);
		}
		free_effect(s->effect);
		string_free(s->msg);
		string_free(s->desc);
		string_free(s->verb);
		string_free(s->name);
		mem_free(s);
		s = next;
	}
}

struct file_parser song_parser = {
    "song",
    init_parse_song,
    run_parse_song,
    finish_parse_song,
    cleanup_song
};

/**
 * ------------------------------------------------------------------------
 * Player song routines
 * ------------------------------------------------------------------------ */
struct song *song_by_idx(int idx)
{
	struct song *s = songs;
	while (s) {
		if (s->index == idx) return s;
		s = s->next;
	}
	return NULL;
}

struct song *lookup_song(const char *name)
{
	struct song *s = songs;
	while (s) {
		if (streq(s->name, name)) return s;
		s = s->next;
	}
	return NULL;
}

/**
 * Player song bonus, returns 0 if the player is not singing the song
 */
int song_bonus(struct player *p, int pskill, struct song *song)
{
	int skill = MAX(pskill, 0);
	int bonus;

	if (!song) return 0;

	/* Adjust for minor theme and which song */
	if (p->song[SONG_MAIN] != song) {
		skill /= 2;
		if (p->song[SONG_MINOR] != song) {
			return 0;
		}
	}
	bonus = skill * song->bonus_mult;
	bonus /= song->bonus_div;
	bonus = MAX(bonus, song->bonus_min);

	/* Special case */
	if (streq(song->name, "Slaying")) {
		bonus *= p->wrath;
		bonus += 999;
		bonus /= 1000;
	}

	return bonus;
}

void player_change_song(struct player *p, struct song *song, bool exchange)
{
	int song_to_change;

	if (player_active_ability(p, "Woven Themes") && p->song[SONG_MAIN] && song){
		song_to_change = SONG_MINOR;
	} else {
		song_to_change = SONG_MAIN;
	}

	/* Attempting to change to the same song */
	if (p->song[SONG_MAIN] == song) {
		/* This can cancel minor themes... */
		if (!p->song[SONG_MINOR]) {
			song = NULL;
		} else if (song) {
			/* ...but otherwise does nothing */
			msg("You were already singing that.");
			return;
		}
	} else if ((p->song[SONG_MINOR] == song) && (song_to_change == SONG_MINOR)){
		/* Attempting to change minor theme to itself */
		msg("You are already using that minor theme.");
		return;
	}

	/* Recalculate various bonuses */
	p->upkeep->redraw |= (PR_SONG);
	p->upkeep->update |= (PU_BONUS);

	/* Swap the minor and major themes */
	if (exchange) {
		struct song *temp = p->song[SONG_MAIN];
		p->song[SONG_MAIN] = p->song[SONG_MINOR];
		p->song[SONG_MINOR] = temp;

		msg("You change the order of your themes.");

		/* Take time */
		p->upkeep->energy_use = z_info->move_energy;

		/* Store the action type */
		p->previous_action[0] = ACTION_MISC;
		return;
	}

	/* Reset the song duration counter if changing major theme */
	if (song_to_change == SONG_MAIN) {
		p->song_duration = 0;
	}

	/* Deal with ending a song */
	if (!song) {
		if ((song_to_change == SONG_MAIN) && p->song[SONG_MAIN]) {
			msg("You end your song.");
		} else if ((song_to_change == SONG_MINOR) && p->song[SONG_MINOR]){
			msg("You end your minor theme.");
		}
	} else if (song_to_change == SONG_MAIN) {
		/* Start a new main song */
		msg("You %s song %s.", song->verb, song->desc);
	} else if (p->song[SONG_MINOR]) {
		/* Change the minor theme */
		msg("You change you minor theme to one %s.", song->desc);
	} else {
		/* Add a minor theme */
		msg("You add a minor theme %s.", song->desc);
	}

	/* Add a message */
	if (song && song->msg) {
		msg(song->msg);
	}

	/* Actually set the song */
	if (song_to_change == SONG_MAIN) {
		p->song[SONG_MAIN] = song;
	} else if (song) {
		p->song[SONG_MINOR] = song;
	}

	/* Beginning/changing songs takes time */
	if (song) {
		/* Take time */
		p->upkeep->energy_use = z_info->move_energy;
		
		/* Store the action type */
		p->previous_action[0] = ACTION_MISC;
	}
}

bool player_is_singing(struct player *p, struct song *song)
{
	if (p->song[SONG_MAIN] == song) return true;
	if (song && (p->song[SONG_MINOR] == song)) return true;
	return false;
}

int player_song_noise(struct player *p)
{
	struct song *song = p->song[SONG_MAIN];
	if (!song) return 0;
	if (!p->song[SONG_MINOR]) return song->noise;

	/* Average the noise if there are two songs */
	return (song->noise + p->song[SONG_MINOR]->noise) / 2;
}

void player_sing(struct player *p)
{
	int i;
	int cost = 0;
	struct song *smain = p->song[SONG_MAIN];
	struct song *minor = p->song[SONG_MINOR];

	if (!p->song[SONG_MAIN]) return;
	/* Abort song if out of voice, lost the ability to weave themes,
	 * or lost either song ability */
	if ((p->csp < 1) ||
		(p->song[SONG_MINOR] && !player_active_ability(p, "Woven Themes")) ||
		(!player_active_ability(p, format("Song of %s", smain->name))) ||
		(p->song[SONG_MINOR] &&
		 !player_active_ability(p, format("Song of %s", minor->name)))) {
		/* Stop singing */
		player_change_song(p, NULL, false);

		/* Disturb */
		disturb(p, false);
		return;
	} else {
		p->song_duration++;
	}

	for (i = 0; i < SONG_MAX; i++) {
		struct song *song = p->song[i];
		if (!song) continue;

		/* Cost */
		if (!song->extend || ((p->song_duration % 3) == i)) {
			cost++;
		}

		/* Song effects */
		if (song->effect) {
			bool dummy = false;

			effect_do(song->effect, source_player(), NULL, &dummy, true,
					  DIR_NONE, NULL);
		}
	}

	/* Pay costs */
	p->csp -= MIN(cost, p->csp);

	p->upkeep->redraw |= (PR_MANA);
}

/**
 * ------------------------------------------------------------------------
 * Monster songs
 * ------------------------------------------------------------------------ */
/**
 * Print messages and calculate song skill for singing monsters
 */
int monster_sing(struct monster *mon, struct song *song)
{
    char m_name[80];
    char *description;
	struct song *silence = lookup_song("Silence");
	int song_skill = mon->race->song;
    int dist = flow_dist(cave->player_noise, mon->grid);
    
    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), mon, MDESC_SHOW);

    /* Messages for beginning a new song */
    if ((mon->song != song) && monster_is_visible(mon)) {
        msg("%s begins a song of %s.", m_name, song->desc);

        /* Disturb if message printed */
        disturb(player, true);
    } else {
        /* Messages for continuing a song */
		int pick = randint0(8);
		struct alt_song_desc *alt_desc = song->alt_desc;
		description = alt_desc->desc;
		while (pick) {
			alt_desc = alt_desc->next;
			if (!alt_desc) {
				description = song->desc;
				break;
			}
			description = alt_desc->desc;
			pick--;
		}

		if (monster_is_visible(mon)) {
			msg("%s sings of %s.", m_name, description);
			disturb(player, true);
		} else if (dist <= 20) {
			msg("You hear a song of %s.", description);
			disturb(player, true);
		} else if (dist <= 20) {
			msg("You hear singing in the distance.");
			disturb(player, true);
		}
	}

	/* Remember the monster is now singing this song */
	mon->song = song;

    /* If the player is singing the song of silence, penalise the monster */
	if (player_is_singing(player, silence)) {
		song_skill -= song_bonus(player, player->state.skill_use[SKILL_SONG],
								 silence) / 2;
	}

	return song_skill;
}

