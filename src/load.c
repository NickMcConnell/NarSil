/**
 * \file load.c
 * \brief Individual loading functions
 *
 * Copyright (c) 1997 Ben Harrison, and others
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
#include "effects.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-group.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-init.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "savefile.h"
#include "songs.h"
#include "trap.h"
#include "ui-term.h"

/**
 * Dungeon constants
 */
static uint8_t square_size = 0;

/**
 * Player constants
 */
static uint8_t hist_size = 0;

/**
 * Object constants
 */
static uint8_t obj_mod_max = 0;
static uint8_t of_size = 0;
static uint8_t elem_max = 0;
static uint8_t brand_max;
static uint8_t slay_max;

/**
 * Monster constants
 */
static uint8_t mflag_size = 0;

/**
 * Trap constants
 */
static uint8_t trf_size = 0;

/**
 * Shorthand function pointer for rd_item version
 */
typedef struct object *(*rd_item_t)(void);

/**
 * Read an object.
 */
static struct object *rd_item(void)
{
	struct object *obj = object_new();

	uint8_t tmp8u;
	uint16_t tmp16u;
	size_t i;
	char buf[128];
	uint8_t ver = 1;

	rd_u16b(&tmp16u);
	rd_byte(&ver);
	if (tmp16u != 0xffff)
		return NULL;

	rd_u16b(&obj->oidx);

	/* Location */
	rd_byte(&tmp8u);
	obj->grid.y = tmp8u;
	rd_byte(&tmp8u);
	obj->grid.x = tmp8u;

	/* Type/Subtype */
	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		obj->tval = tval_find_idx(buf);
	}
	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		obj->sval = lookup_sval(obj->tval, buf);
	}

	/* Image Type/Subtype */
	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		int tv, sv;
		tv = tval_find_idx(buf);
		rd_string(buf, sizeof(buf));
		if (buf[0]) {
			sv = lookup_sval(tv, buf);
			obj->image_kind = lookup_kind(tv, sv);
		}
	}

	rd_s16b(&obj->pval);

	rd_byte(&obj->number);
	rd_s16b(&obj->weight);

	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		obj->artifact = lookup_artifact_name(buf);
		if (!obj->artifact) {
			note(format("Couldn't find artifact %s!", buf));
			return NULL;
		}
	}
	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		obj->ego = lookup_ego_item(buf, obj->tval, obj->sval);
		if (!obj->ego) {
			note(format("Couldn't find ego item %s!", buf));
			return NULL;
		}
	}

	rd_s16b(&obj->timeout);

	rd_s16b(&obj->att);
	rd_byte(&obj->dd);
	rd_byte(&obj->ds);
	rd_s16b(&obj->evn);
	rd_byte(&obj->pd);
	rd_byte(&obj->ps);

	rd_byte(&obj->origin);
	rd_byte(&obj->origin_depth);
	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		obj->origin_race = lookup_monster(buf);
	}
	rd_byte(&obj->notice);
	rd_byte(&obj->pseudo);
	rd_byte(&tmp8u);
	obj->marked = tmp8u ? true : false;;

	for (i = 0; i < of_size; i++)
		rd_byte(&obj->flags[i]);

	for (i = 0; i < obj_mod_max; i++) {
		rd_s16b(&obj->modifiers[i]);
	}

	/* Read brands */
	rd_byte(&tmp8u);
	if (tmp8u) {
		obj->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
		for (i = 0; i < brand_max; i++) {
			rd_byte(&tmp8u);
			obj->brands[i] = tmp8u ? true : false;
		}
	}

	/* Read slays */
	rd_byte(&tmp8u);
	if (tmp8u) {
		obj->slays = mem_zalloc(z_info->slay_max * sizeof(bool));
		for (i = 0; i < slay_max; i++) {
			rd_byte(&tmp8u);
			obj->slays[i] = tmp8u ? true : false;
		}
	}

	for (i = 0; i < elem_max; i++) {
		rd_s16b(&obj->el_info[i].res_level);
		rd_byte(&obj->el_info[i].flags);
	}

	/* Read the abilities */
	while (true) {
		struct ability *ability;
		rd_string(buf, sizeof(buf));
		if (streq(buf, "end")) break;
		rd_byte(&tmp8u);
		ability = lookup_ability(tmp8u, buf);
		if (ability == NULL)  {
			note(format("Ability not found (%s).", buf));
			return NULL;
		}
		add_ability(&obj->abilities, ability);
	}

	/* Monster holding object */
	rd_s16b(&obj->held_m_idx);

	/* Read the inscription */
	rd_byte(&tmp8u);
	if (tmp8u) {
		rd_string(buf, sizeof(buf));
		if (buf[0]) obj->note = quark_add(buf);
	}

	/* Lookup item kind */
	obj->kind = lookup_kind(obj->tval, obj->sval);

	/* Check we have a kind */
	if ((!obj->tval && !obj->sval) || !obj->kind) {
		object_delete(NULL, &obj);
		return NULL;
	}

	/* Success */
	return obj;
}


/**
 * Read a monster
 */
static bool rd_monster(struct chunk *c, struct monster *mon)
{
	uint8_t tmp8u;
	uint16_t tmp16u;
	char race_name[80];
	size_t j;

	/* Read the monster race */
	rd_u16b(&tmp16u);
	mon->midx = tmp16u;
	rd_string(race_name, sizeof(race_name));
	mon->race = lookup_monster(race_name);
	if (!mon->race) {
		note(format("Monster race %s no longer exists!", race_name));
		return false;
	}
	rd_string(race_name, sizeof(race_name));
	if (streq(race_name, "none")) {
		mon->image_race = NULL;
	} else {
		mon->image_race = lookup_monster(race_name);
	}

	/* Read the other information */
	rd_byte(&tmp8u);
	mon->grid.y = tmp8u;
	rd_byte(&tmp8u);
	mon->grid.x = tmp8u;
	rd_s16b(&mon->hp);
	rd_s16b(&mon->maxhp);
	rd_byte(&mon->mana);
	rd_byte(&tmp8u);
	mon->song = song_by_idx(tmp8u);
	rd_s16b(&mon->alertness);
	rd_byte(&mon->mspeed);
	rd_byte(&mon->energy);
	rd_byte(&mon->origin);
	rd_byte(&mon->stance);
	rd_s16b(&mon->morale);
	rd_s16b(&mon->tmp_morale);
	rd_byte(&mon->noise);
	rd_byte(&mon->encountered);
	rd_byte(&tmp8u);

	for (j = 0; j < tmp8u; j++)
		rd_s16b(&mon->m_timed[j]);

	/* Read and extract the flag */
	for (j = 0; j < mflag_size; j++)
		rd_byte(&mon->mflag[j]);

	for (j = 0; j < of_size; j++)
		rd_s16b(&mon->known_pstate.flags[j]);

	for (j = 0; j < elem_max; j++)
		rd_s16b(&mon->known_pstate.el_info[j].res_level);

	for (j = 0; j < MAX_ACTION; j++)
		rd_byte(&mon->previous_action[j]);

	/* Read all the held objects (order is unimportant) */
	while (true) {
		struct object *obj = rd_item();
		if (!obj)
			break;

		pile_insert(&mon->held_obj, obj);
		assert(obj->oidx);
		assert(c->objects[obj->oidx] == NULL);
		c->objects[obj->oidx] = obj;
	}

	/* Read group info */
	rd_u16b(&tmp16u);
	mon->group_info.index = tmp16u;
	rd_byte(&tmp8u);
	mon->group_info.role = tmp8u;

	rd_byte(&tmp8u);
	mon->target.grid.y = tmp8u;
	rd_byte(&tmp8u);
	mon->target.grid.x = tmp8u;
	rd_byte(&mon->skip_this_turn);
	rd_byte(&mon->skip_next_turn);
	rd_s16b(&mon->consecutive_attacks);
	rd_s16b(&mon->turns_stationary);

	return true;
}


/**
 * Read a trap record
 */
static void rd_trap(struct trap *trap)
{
	int i;
	uint8_t tmp8u;
	char buf[80];

	rd_string(buf, sizeof(buf));
	if (buf[0]) {
		trap->kind = lookup_trap(buf);
		trap->t_idx = trap->kind->tidx;
	}
	rd_byte(&tmp8u);
	trap->grid.y = tmp8u;
	rd_byte(&tmp8u);
	trap->grid.x = tmp8u;
	rd_byte(&trap->power);

	for (i = 0; i < trf_size; i++)
		rd_byte(&trap->flags[i]);
}

/**
 * Read RNG state
 *
 * There were originally 64 bytes of randomizer saved. Now we only need
 * 32 + 5 bytes saved, so we'll read an extra 27 bytes at the end which won't
 * be used.
 */
int rd_randomizer(void)
{
	int i;
	uint32_t noop;

	/* current value for the simple RNG */
	rd_u32b(&Rand_value);

	/* state index */
	rd_u32b(&state_i);

	/* for safety, make sure state_i < RAND_DEG */
	state_i = state_i % RAND_DEG;
    
	/* RNG variables */
	rd_u32b(&z0);
	rd_u32b(&z1);
	rd_u32b(&z2);
    
	/* RNG state */
	for (i = 0; i < RAND_DEG; i++)
		rd_u32b(&STATE[i]);

	/* NULL padding */
	for (i = 0; i < 59 - RAND_DEG; i++)
		rd_u32b(&noop);

	Rand_quick = false;

	return 0;
}


/**
 * Read options.
 */
int rd_options(void)
{
	uint8_t b;

	/*** Special info */

	/* Read "delay_factor" */
	rd_byte(&b);
	player->opts.delay_factor = b;

	/* Read "hitpoint_warn" */
	rd_byte(&b);
	player->opts.hitpoint_warn = b;

	/* Read lazy movement delay */
	rd_byte(&b);
	player->opts.lazymove_delay = b;

	/* Read sidebar mode (if it's an actual game) */
	if (angband_term[0]) {
		rd_byte(&b);
		if (b >= SIDEBAR_MAX) b = SIDEBAR_LEFT;
		SIDEBAR_MODE = b;
	} else {
		strip_bytes(1);
	}


	/* Read options */
	while (1) {
		uint8_t value;
		char name[40];
		rd_string(name, sizeof name);

		if (!name[0])
			break;

		rd_byte(&value);
		option_set(name, !!value);
	}

	return 0;
}

/**
 * Read the saved messages
 */
int rd_messages(void)
{
	int i;
	char buf[128];
	uint16_t tmp16u;

	int16_t num;

	/* Total */
	rd_s16b(&num);

	/* Read the messages */
	for (i = 0; i < num; i++) {
		/* Read the message */
		rd_string(buf, sizeof(buf));

		/* Read the message type */
		rd_u16b(&tmp16u);

		/* Save the message */
		message_add(buf, tmp16u);
	}

	return 0;
}

/**
 * Read monster memory.
 */
int rd_monster_memory(void)
{
	uint16_t nkill, nsight;
	char buf[128];
	int i;

	/* Monster temporary flags */
	rd_byte(&mflag_size);

	/* Incompatible save files */
	if (mflag_size > MFLAG_SIZE) {
	        note(format("Too many (%u) monster temporary flags!", mflag_size));
		return (-1);
	}

	/* Reset maximum numbers per level */
	for (i = 1; z_info && i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];
		race->max_num = 100;
		if (rf_has(race->flags, RF_UNIQUE))
			race->max_num = 1;
	}

	rd_string(buf, sizeof(buf));
	while (!streq(buf, "No more monsters")) {
		struct monster_race *race = lookup_monster(buf);

		/* Get the kill and sight counts, skip if monster invalid */
		rd_u16b(&nkill);
		rd_u16b(&nsight);
		if (!race) continue;

		/* Store the kill count, ensure dead uniques stay dead */
		l_list[race->ridx].pkills = nkill;
		if (rf_has(race->flags, RF_UNIQUE) && nkill)
			race->max_num = 0;

		/* Store the sight count */
		l_list[race->ridx].psights = nsight;

		/* Look for the next monster */
		rd_string(buf, sizeof(buf));
	}

	return 0;
}


int rd_object_memory(void)
{
	size_t i;
	uint16_t tmp16u;

	/* Object Memory */
	rd_u16b(&tmp16u);
	if (tmp16u > z_info->k_max) {
		note(format("Too many (%u) object kinds!", tmp16u));
		return (-1);
	}

	/* Object flags */
	rd_byte(&of_size);
	if (of_size > OF_SIZE) {
	        note(format("Too many (%u) object flags!", of_size));
		return (-1);
	}

	/* Object modifiers */
	rd_byte(&obj_mod_max);
	if (obj_mod_max > OBJ_MOD_MAX) {
	        note(format("Too many (%u) object modifiers allowed!",
						obj_mod_max));
		return (-1);
	}

	/* Elements */
	rd_byte(&elem_max);
	if (elem_max > ELEM_MAX) {
	        note(format("Too many (%u) elements allowed!", elem_max));
		return (-1);
	}

	/* Brands */
	rd_byte(&brand_max);
	if (brand_max > z_info->brand_max) {
	        note(format("Too many (%u) brands allowed!", brand_max));
		return (-1);
	}

	/* Slays */
	rd_byte(&slay_max);
	if (slay_max > z_info->slay_max) {
	        note(format("Too many (%u) slays allowed!", slay_max));
		return (-1);
	}

	/* Read the kind knowledge */
	for (i = 0; i < tmp16u; i++) {
		uint8_t tmp8u;
		struct object_kind *kind = &k_info[i];

		rd_byte(&tmp8u);

		kind->aware = (tmp8u & 0x01) ? true : false;
		kind->tried = (tmp8u & 0x02) ? true : false;
		kind->everseen = (tmp8u & 0x08) ? true : false;

		if (tmp8u & 0x04) kind_ignore_when_aware(kind);
		if (tmp8u & 0x10) kind_ignore_when_unaware(kind);
	}

	return 0;
}



/**
 * Read the player information
 */
int rd_player(void)
{
	int i;
	uint8_t tmp8u, num;
	uint8_t stat_max = 0;
	uint8_t skill_max = 0;
	uint16_t vault_max = 0;
	char buf[80];
	struct player_sex *s;
	struct player_race *r;
	struct player_house *h;

	rd_string(player->full_name, sizeof(player->full_name));
	rd_string(player->died_from, 80);
	player->history = mem_zalloc(250);
	rd_string(player->history, 250);

	/* Player race */
	rd_string(buf, sizeof(buf));
	for (r = races; r; r = r->next) {
		if (streq(r->name, buf)) {
			player->race = r;
			break;
		}
	}

	/* Verify player race */
	if (!player->race) {
		note(format("Invalid player race (%s).", buf));
		return -1;
	}

	/* Player house */
	rd_string(buf, sizeof(buf));
	for (h = houses; h; h = h->next) {
		if (streq(h->name, buf)) {
			player->house = h;
			break;
		}
	}

	if (!player->house) {
		note(format("Invalid player house (%s).", buf));
		return -1;
	}

	/* Player sex */
	rd_string(buf, sizeof(buf));
	for (s = sexes; s; s = s->next) {
		if (streq(s->name, buf)) {
			player->sex = s;
			break;
		}
	}

	if (!player->sex) {
		note(format("Invalid player sex (%s).", buf));
		return -1;
	}

	/* Numeric name suffix */
	rd_byte(&player->opts.name_suffix);

	/* Age/Height/Weight */
	rd_s16b(&player->game_type);
	rd_s16b(&player->age);
	rd_s16b(&player->ht);
	rd_s16b(&player->wt);
	rd_s16b(&player->ht_birth);
	rd_s16b(&player->wt_birth);

	/* Read the stat info */
	rd_byte(&stat_max);
	if (stat_max > STAT_MAX) {
		note(format("Too many stats (%d).", stat_max));
		return -1;
	}

	for (i = 0; i < stat_max; i++) rd_s16b(&player->stat_base[i]);
	for (i = 0; i < stat_max; i++) rd_s16b(&player->stat_drain[i]);

	/* Read the skill info */
	rd_byte(&skill_max);
	if (skill_max > SKILL_MAX) {
		note(format("Too many skills (%d).", skill_max));
		return -1;
	}

	for (i = 0; i < skill_max; i++) rd_s16b(&player->skill_base[i]);

	/* Read the abilities */
	while (true) {
		struct ability *ability;
		rd_string(buf, sizeof(buf));
		if (streq(buf, "end")) break;
		rd_byte(&tmp8u);
		ability = lookup_ability(tmp8u, buf);
		if (ability == NULL)  {
			note(format("Ability not found (%s).", buf));
			return -1;
		}
		add_ability(&player->abilities, ability);
		rd_byte(&tmp8u);
		if (tmp8u) {
			struct ability *instance = player->abilities;
			instance = locate_ability(player->abilities, ability);
			instance->active = true;
		}
	}
	while (true) {
		struct ability *ability;
		rd_string(buf, sizeof(buf));
		if (streq(buf, "end")) break;
		rd_byte(&tmp8u);
		ability = lookup_ability(tmp8u, buf);
		if (ability == NULL)  {
			note(format("Ability not found (%s).", buf));
			return -1;
		}
		add_ability(&player->item_abilities, ability);
		rd_byte(&tmp8u);
		if (tmp8u) {
			struct ability *instance = player->item_abilities;
			instance = locate_ability(player->item_abilities, ability);
			instance->active = true;
		}
	}

	/* Read the action list */
	for (i = 0; i < MAX_ACTION; i++) {
		rd_byte(&tmp8u);
		player->previous_action[i] = tmp8u;
	}

	/* Player body */
	rd_string(buf, sizeof(buf));
	player->body.name = string_make(buf);
	rd_u16b(&player->body.count);
	if (player->body.count > z_info->equip_slots_max) {
		note(format("Too many (%u) body parts!", player->body.count));
		return (-1);
	}

	player->body.slots = mem_zalloc(player->body.count *
									sizeof(struct equip_slot));
	for (i = 0; i < player->body.count; i++) {
		rd_u16b(&player->body.slots[i].type);
		rd_string(buf, sizeof(buf));
		player->body.slots[i].name = string_make(buf);
	}

	rd_s32b(&player->new_exp);
	rd_s32b(&player->exp);
	rd_s32b(&player->encounter_exp);
	rd_s32b(&player->kill_exp);
	rd_s32b(&player->descent_exp);
	rd_s32b(&player->ident_exp);
	rd_s32b(&player->turn);

	rd_s16b(&player->mhp);
	rd_s16b(&player->chp);

	rd_s16b(&player->msp);
	rd_s16b(&player->csp);

	rd_s16b(&player->max_depth);
	rd_u16b(&player->staircasiness);

	/* Hack -- Repair maximum dungeon level */
	if (player->max_depth < 0) player->max_depth = 1;

	/* Hack -- Reset cause of death */
	if (player->chp >= 0)
		my_strcpy(player->died_from, "(alive and well)",
				  sizeof(player->died_from));

	rd_s16b(&player->energy);

	/* Total energy used so far */
	rd_u32b(&player->total_energy);
	/* # of turns spent resting */
	rd_u32b(&player->resting_turn);

	/* Find the number of timed effects */
	rd_byte(&num);

	if (num <= TMD_MAX) {
		/* Read all the effects */
		for (i = 0; i < num; i++)
			rd_s16b(&player->timed[i]);

		/* Initialize any entries not read */
		if (num < TMD_MAX)
			memset(player->timed + num, 0, (TMD_MAX - num) * sizeof(int16_t));
	} else {
		/* Probably in trouble anyway */
		for (i = 0; i < TMD_MAX; i++)
			rd_s16b(&player->timed[i]);

		/* Discard unused entries */
		strip_bytes(2 * (num - TMD_MAX));
		note("Discarded unsupported timed effects");
	}

	/* Greater vaults seen */
	rd_u16b(&vault_max);
	if (vault_max > z_info->v_max) {
		note(format("Too many (%u) vaults!", vault_max));
		return (-1);
	}
	for (i = 0; i < vault_max; i++) {
		rd_byte(&tmp8u);
		player->vaults[i] = tmp8u ? true : false;
	}


	/* More info */
	rd_byte(&player->unignoring);
	rd_s16b(&player->last_attack_m_idx);	
	rd_s16b(&player->consecutive_attacks);
	rd_s16b(&player->bane_type);
	rd_byte(&tmp8u);
	player->focused = tmp8u ? true : false;
	rd_byte(&tmp8u);
	player->song[SONG_MAIN] = song_by_idx(tmp8u);
	rd_byte(&tmp8u);
	player->song[SONG_MINOR] = song_by_idx(tmp8u);
	rd_s16b(&player->song_duration);
	rd_s16b(&player->wrath);
	rd_u16b(&player->stairs_taken);
	rd_u16b(&player->forge_drought);
	rd_u16b(&player->forge_count);
	rd_byte(&tmp8u);
	player->stealth_mode = tmp8u;
	rd_byte(&player->self_made_arts);
	rd_byte(&tmp8u);
	player->truce = tmp8u ? true : false;
	rd_byte(&player->morgoth_hits);
	rd_byte(&tmp8u);
	player->crown_hint = tmp8u ? true : false;
	rd_byte(&tmp8u);
	player->crown_shatter = tmp8u ? true : false;
	rd_byte(&tmp8u);
	player->cursed = tmp8u ? true : false;
	rd_byte(&tmp8u);
	player->on_the_run = tmp8u ? true : false;
	rd_byte(&tmp8u);
	player->morgoth_slain = tmp8u ? true : false;
	rd_byte(&tmp8u);
	player->escaped = tmp8u ? true : false;
	rd_u16b(&player->noscore);
	rd_s16b(&player->smithing_leftover);
	rd_byte(&tmp8u);
	player->unique_forge_made = tmp8u ? true : false;
	rd_byte(&tmp8u);
	player->unique_forge_seen = tmp8u ? true : false;

	return 0;
}


/**
 * Read ignore and autoinscription submenu for all known objects
 */
int rd_ignore(void)
{
	size_t i, j;
	uint8_t tmp8u = 24;
	uint16_t file_e_max;
	uint16_t itype_size;
	uint16_t inscriptions;

	/* Read how many ignore bytes we have */
	rd_byte(&tmp8u);

	/* Check against current number */
	if (tmp8u != ignore_size) {
		strip_bytes(tmp8u);
	} else {
		for (i = 0; i < ignore_size; i++)
			rd_byte(&ignore_level[i]);
	}

	/* Read the number of saved ego-item */
	rd_u16b(&file_e_max);
	rd_u16b(&itype_size);
	if (itype_size > ITYPE_SIZE) {
		note(format("Too many (%u) ignore bytes!", itype_size));
		return (-1);
	}

	for (i = 0; i < file_e_max; i++) {
		if (i < z_info->e_max) {
			bitflag flags, itypes[ITYPE_SIZE];
			
			/* Read and extract the everseen and aware flags */
			rd_byte(&flags);
			e_info[i].everseen = (flags & 0x02) ? true : false;
			e_info[i].aware = (flags & 0x04) ? true : false;

			/* Read and extract the ignore flags */
			for (j = 0; j < itype_size; j++)
				rd_byte(&itypes[j]);

			/* If number of ignore types has changed, don't set anything */
			if (itype_size == ITYPE_SIZE) {
				for (j = ITYPE_NONE; j < ITYPE_MAX; j++)
					if (itype_has(itypes, j))
						ego_ignore_toggle(i, j);
			}
		}
	}

	/* Read the current number of aware object auto-inscriptions */
	rd_u16b(&inscriptions);

	/* Read the aware object autoinscriptions array */
	for (i = 0; i < inscriptions; i++) {
		char tmp[80];
		uint8_t tval, sval;
		struct object_kind *k;

		rd_string(tmp, sizeof(tmp));
		tval = tval_find_idx(tmp);
		rd_string(tmp, sizeof(tmp));
		sval = lookup_sval(tval, tmp);
		k = lookup_kind(tval, sval);
		if (!k)
			quit_fmt("lookup_kind(%d, %d) failed", tval, sval);
		rd_string(tmp, sizeof(tmp));
		k->note_aware = quark_add(tmp);
	}

	/* Read the current number of unaware object auto-inscriptions */
	rd_u16b(&inscriptions);

	/* Read the unaware object autoinscriptions array */
	for (i = 0; i < inscriptions; i++) {
		char tmp[80];
		uint8_t tval, sval;
		struct object_kind *k;

		rd_string(tmp, sizeof(tmp));
		tval = tval_find_idx(tmp);
		rd_string(tmp, sizeof(tmp));
		sval = lookup_sval(tval, tmp);
		k = lookup_kind(tval, sval);
		if (!k)
			quit_fmt("lookup_kind(%d, %d) failed", tval, sval);
		rd_string(tmp, sizeof(tmp));
		k->note_unaware = quark_add(tmp);
	}

	return 0;
}


int rd_misc(void)
{
	uint8_t tmp8u;
	
	/* Read the randart seed */
	rd_u32b(&seed_randart);

	/* Read the flavors seed */
	rd_u32b(&seed_flavor);
	flavor_init();

	/* Special stuff */
	rd_u16b(&player->noscore);

	/* Read "death" */
	rd_byte(&tmp8u);
	player->is_dead = tmp8u;

	/* Current turn */
	rd_s32b(&turn);

	/* Handle smithed artifact file parsing */
	if (player->self_made_arts > 0) {
		activate_randart_file();
		run_parser(&randart_parser);
		deactivate_randart_file();
	}

	return 0;
}

int rd_artifacts(void)
{
	int i;
	uint16_t tmp16u;
	const struct artifact *crown = lookup_artifact_name("of Morgoth");

	/* Load the Artifacts */
	rd_u16b(&tmp16u);
	if (tmp16u > z_info->a_max) {
		/* Saved number may be greater for dead characters, skip the check */
		if (!player->is_dead) {
			note(format("Too many (%u) artifacts!", tmp16u));
			return (-1);
		}
	}

	/* Read the artifact flags */
	for (i = 0; i < z_info->a_max; i++) {
		uint8_t tmp8u;

		rd_byte(&tmp8u);
		aup_info[i].created = tmp8u ? true : false;
		rd_byte(&tmp8u);
		aup_info[i].seen = tmp8u ? true : false;
		rd_byte(&tmp8u);
		aup_info[i].everseen = tmp8u ? true : false;
		rd_byte(&tmp8u);
	}

	/* Change Morgoth's stats if his crown has been knocked off */
	if (is_artifact_created(crown)) {
		struct monster_race *race = lookup_monster("Morgoth, Lord of Darkness");
		race->pd -= 1;
		race->light = 0;
		race->wil += 5;
		race->per += 5;
	}

	return 0;
}



/**
 * Read the player gear
 */
static int rd_gear_aux(rd_item_t rd_item_version, struct object **gear)
{
	uint8_t code;
	struct object *last_gear_obj = NULL;

	/* Get the first item code */
	rd_byte(&code);

	/* Read until done */
	while (code != FINISHED_CODE) {
		struct object *obj = (*rd_item_version)();

		/* Read the item */
		if (!obj) {
			note("Error reading item");
			return (-1);
		}

		/* Append the object */
		obj->prev = last_gear_obj;
		if (last_gear_obj)
			last_gear_obj->next = obj;
		else
			*gear = obj;
		last_gear_obj = obj;

		/* If it's equipment, wield it */
		if (code < player->body.count) {
			player->body.slots[code].obj = obj;
			player->upkeep->equip_cnt++;
		}

		/* Get the next item code */
		rd_byte(&code);
	}

	/* Success */
	return (0);
}

/**
 * Read the player gear - wrapper functions
 */
int rd_gear(void)
{
	struct object *obj;

	/* Get gear */
	if (rd_gear_aux(rd_item, &player->gear))
		return -1;

	/* Add weight */
	for (obj = player->gear; obj; obj = obj->next) {
		player->upkeep->total_weight += (obj->number * obj->weight);
	}

	calc_inventory(player);

	return 0;
}


/**
 * Read the dungeon
 *
 * The monsters/objects must be loaded in the same order
 * that they were stored, since the actual indexes matter.
 *
 * Note that the size of the dungeon is now the currrent dimensions of the
 * cave global variable.
 *
 * Note that dungeon objects, including objects held by monsters, are
 * placed directly into the dungeon, using "object_copy()", which will
 * copy "iy", "ix", and "held_m_idx", leaving "next_o_idx" blank for
 * objects held by monsters, since it is not saved in the savefile.
 *
 * After loading the monsters, the objects being held by monsters are
 * linked directly into those monsters.
 */
static int rd_dungeon_aux(struct chunk **c)
{
	struct chunk *c1;
	int i, n, y, x;

	uint16_t height, width;

	uint8_t count;
	uint8_t tmp8u;
	char name[100];

	/* Header info */
	rd_string(name, sizeof(name));
	rd_u16b(&height);
	rd_u16b(&width);

	/* We need a cave struct */
	c1 = cave_new(height, width);
	c1->name = string_make(name);

	rd_byte(&tmp8u);
	if (tmp8u) {
		rd_string(name, sizeof(name));
		c1->vault_name = string_make(name);
	}

    /* Run length decoding of cave->squares[y][x].info */
	for (n = 0; n < square_size; n++) {
		/* Load the dungeon data */
		for (x = y = 0; y < c1->height; ) {
			/* Grab RLE info */
			rd_byte(&count);
			rd_byte(&tmp8u);

			/* Apply the RLE info */
			for (i = count; i > 0; i--) {
				/* Extract "info" */
				c1->squares[y][x].info[n] = tmp8u;

				/* Advance/Wrap */
				if (++x >= c1->width) {
					/* Wrap */
					x = 0;

					/* Advance/Wrap */
					if (++y >= c1->height) break;
				}
			}
		}
	}

	/* Run length decoding of dungeon data */
	for (x = y = 0; y < c1->height; ) {
		/* Grab RLE info */
		rd_byte(&count);
		rd_byte(&tmp8u);

		/* Apply the RLE info */
		for (i = count; i > 0; i--) {
			/* Extract "feat" */
			square_set_feat(c1, loc(x, y), tmp8u);

			/* Advance/Wrap */
			if (++x >= c1->width) {
				/* Wrap */
				x = 0;

				/* Advance/Wrap */
				if (++y >= c1->height) break;
			}
		}
	}

	/* Assign */
	*c = c1;

	return 0;
}

/**
 * Read the floor object list
 */
static int rd_objects_aux(rd_item_t rd_item_version, struct chunk *c)
{
	int i;

	/* Only if the player's alive */
	if (player->is_dead)
		return 0;

	/* Make the object list */
	rd_u16b(&c->obj_max);
	c->objects = mem_realloc(c->objects,
							 (c->obj_max + 1) * sizeof(struct object*));
	for (i = 0; i <= c->obj_max; i++)
		c->objects[i] = NULL;

	/* Read the dungeon items until one isn't returned */
	while (true) {
		struct object *obj = (*rd_item_version)();
		if (!obj)
			break;
		if (square_in_bounds_fully(c, obj->grid)) {
			pile_insert_end(&c->squares[obj->grid.y][obj->grid.x].obj, obj);
		}
		assert(obj->oidx);
		assert(c->objects[obj->oidx] == NULL);
		c->objects[obj->oidx] = obj;
	}

	return 0;
}

/**
 * Read monsters
 */
static int rd_monsters_aux(struct chunk *c)
{
	int i;
	uint16_t limit;

	/* Only if the player's alive */
	if (player->is_dead)
		return 0;

	/* Read the monster count */
	rd_u16b(&limit);
	if (limit > z_info->level_monster_max) {
		note(format("Too many (%d) monster entries!", limit));
		return (-1);
	}

	/* Read the monsters */
	for (i = 1; i < limit; i++) {
		struct monster *mon;
		struct monster monster_body;

		/* Get local monster */
		mon = &monster_body;
		memset(mon, 0, sizeof(*mon));

		/* Read the monster */
		if (!rd_monster(c, mon)) {
			note(format("Cannot read monster %d", i));
			return (-1);
		}

		/* Place monster in dungeon */
		if (place_monster(c, mon->grid, mon, mon->origin) != i) {
			note(format("Cannot place monster %d", i));
			return (-1);
		}

		/* Initialize flow */
		mon = cave_monster(c, mon->midx);
		flow_new(c, &mon->flow);
	}

	return 0;
}

static int rd_traps_aux(struct chunk *c)
{
	struct loc grid;
	struct trap *trap;

	/* Only if the player's alive */
	if (player->is_dead)
		return 0;

	rd_byte(&trf_size);

	/* Read traps until one has no location */
	while (true) {
		trap = mem_zalloc(sizeof(*trap));
		rd_trap(trap);
		grid = trap->grid;
		if (loc_is_zero(grid))
			break;
		else {
			/* Put the trap at the front of the grid trap list */
			trap->next = square_trap(c, grid);
			square_set_trap(c, grid, trap);
		}
	}

	mem_free(trap);
	return 0;
}

int rd_dungeon(void)
{
	uint16_t depth;
	uint16_t py, px;

	/* Header info */
	rd_u16b(&depth);
	rd_u16b(&daycount);
	rd_u16b(&py);
	rd_u16b(&px);
	rd_byte(&square_size);

	/* Only if the player's alive */
	if (player->is_dead)
		return 0;

	/* Ignore illegal dungeons */
	if (depth > z_info->dun_depth) {
		note(format("Ignoring illegal dungeon depth (%d)", depth));
		return (0);
	}

	if (rd_dungeon_aux(&cave))
		return 1;

	/* Ignore illegal dungeons */
	if ((px >= cave->width) || (py >= cave->height)) {
		note(format("Ignoring illegal player location (%d,%d).", py, px));
		return (1);
	}

	/* Load player depth */
	player->depth = depth;
	cave->depth = depth;

	/* Place player in dungeon */
	player_place(cave, player, loc(px, py));

	/* The dungeon is ready */
	character_dungeon = true;

	return 0;
}


/**
 * Read the objects - wrapper functions
 */
int rd_objects(void)
{
	if (rd_objects_aux(rd_item, cave))
		return -1;

	return 0;
}

/**
 * Read the monster list - wrapper functions
 */
int rd_monsters(void)
{
	/* Only if the player's alive */
	if (player->is_dead)
		return 0;

	if (rd_monsters_aux(cave))
		return -1;

	return 0;
}

/**
 * Read the traps - wrapper functions
 */
int rd_traps(void)
{
	if (rd_traps_aux(cave))
		return -1;
	return 0;
}

int rd_history(void)
{
	uint32_t tmp32u;
	size_t i, j;
	
	history_clear(player);

	/* History type flags */
	rd_byte(&hist_size);
	if (hist_size > HIST_SIZE) {
	        note(format("Too many (%u) history types!", hist_size));
		return (-1);
	}

	rd_u32b(&tmp32u);
	for (i = 0; i < tmp32u; i++) {
		int32_t turnno;
		int16_t dlev;
		bitflag type[HIST_SIZE];
		const struct artifact *art = NULL;
		int aidx = 0;
		char name[80];
		char text[80];

		for (j = 0; j < hist_size; j++)		
			rd_byte(&type[j]);
		rd_s32b(&turnno);
		rd_s16b(&dlev);
		rd_string(name, sizeof(name));
		if (name[0]) {
			art = lookup_artifact_name(name);
			if (art) {
				aidx = art->aidx;
			}
		}
		rd_string(text, sizeof(text));
		if (name[0] && !art) {
			note(format("Couldn't find artifact %s!", name));
			continue;
		}

		history_add_full(player, type, aidx, dlev, turnno, text);
	}

	return 0;
}

/**
 * For blocks that don't need loading anymore.
 */
int rd_null(void) {
	return 0;
}
