/**
 * \file save.c
 * \brief Individual saving functions
 *
 * Copyright (c) 1997 Ben Harrison
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
#include "game-world.h"
#include "init.h"
#include "mon-group.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "monster.h"
#include "object.h"
#include "obj-desc.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "option.h"
#include "player-abilities.h"
#include "player.h"
#include "savefile.h"
#include "obj-util.h"
#include "player-history.h"
#include "player-timed.h"
#include "trap.h"
#include "ui-term.h"


/**
 * Write a description of the character
 */
void wr_description(void)
{
	char buf[1024];

	if (player->is_dead)
		strnfmt(buf, sizeof buf, "%s, dead (%s)",
				player->full_name,
				player->died_from);
	else
		strnfmt(buf, sizeof buf, "%s, Exp %ld %s %s, at DL%d",
				player->full_name,
				(long)player->exp,
				player->race->name,
				player->house->name,
				player->depth);

	wr_string(buf);
}


/**
 * Write an "item" record
 */
static void wr_item(const struct object *obj)
{
	size_t i;
	struct ability *ability;

	wr_u16b(0xffff);
	wr_byte(ITEM_VERSION);

	wr_u16b(obj->oidx);

	/* Location */
	wr_byte(obj->grid.y);
	wr_byte(obj->grid.x);

	/* Names of object type and object */
	wr_string(tval_find_name(obj->tval));
	if (obj->sval) {
		char name[1024];
		struct object_kind *kind = lookup_kind(obj->tval, obj->sval);
		obj_desc_name_format(name, sizeof name, 0, kind->name, 0, false);
		wr_string(name);
	} else {
		wr_string("");
	}

	/* Names of hallucinatory object type and object */
	if (obj->image_kind) {
		wr_string(tval_find_name(obj->image_kind->tval));
		if (obj->image_kind->sval) {
			char name[1024];
			struct object_kind *kind = lookup_kind(obj->image_kind->tval,
												   obj->image_kind->sval);
			obj_desc_name_format(name, sizeof name, 0, kind->name, 0, false);
			wr_string(name);
		} else {
			wr_string("");
		}
	} else {
		wr_string("");
	}

	wr_s16b(obj->pval);

	wr_byte(obj->number);
	wr_s16b(obj->weight);

	if (obj->artifact) {
		wr_string(obj->artifact->name);
	} else {
		wr_string("");
	}

	if (obj->ego) {
		wr_string(obj->ego->name);
	} else {
		wr_string("");
	}

	wr_s16b(obj->timeout);

	wr_s16b(obj->att);
	wr_byte(obj->dd);
	wr_byte(obj->ds);
	wr_s16b(obj->evn);
	wr_byte(obj->pd);
	wr_byte(obj->ps);

	wr_byte(obj->origin);
	wr_byte(obj->origin_depth);
	if (obj->origin_race) {
		wr_string(obj->origin_race->name);
	} else {
		wr_string("");
	}
	wr_byte(obj->notice);
	wr_byte(obj->pseudo);
	wr_byte(obj->marked);

	for (i = 0; i < OF_SIZE; i++)
		wr_byte(obj->flags[i]);

	for (i = 0; i < OBJ_MOD_MAX; i++) {
		wr_s16b(obj->modifiers[i]);
	}

	/* Write brands if any */
	if (obj->brands) {
		wr_byte(1);
		for (i = 0; i < z_info->brand_max; i++) {
			wr_byte(obj->brands[i] ? 1 : 0);
		}
	} else {
		wr_byte(0);
	}

	/* Write slays if any */
	if (obj->slays) {
		wr_byte(1);
		for (i = 0; i < z_info->slay_max; i++) {
			wr_byte(obj->slays[i] ? 1 : 0);
		}
	} else {
		wr_byte(0);
	}

	for (i = 0; i < ELEM_MAX; i++) {
		wr_s16b(obj->el_info[i].res_level);
		wr_byte(obj->el_info[i].flags);
	}

	/* Dump the abilities */
	for (ability = obj->abilities; ability; ability = ability->next) {
		wr_string(ability->name);
		wr_byte(ability->skill);
	}
	wr_string("end");

	/* Held by monster index */
	wr_s16b(obj->held_m_idx);
	
	/* Save the inscription (if any) */
	if (obj->note) {
		wr_byte(1);
		wr_string(quark_str(obj->note));
	} else {
		wr_byte(0);
	}
}


/**
 * Write a monster record (including held or mimicked objects)
 */
static void wr_monster(const struct monster *mon)
{
	size_t j;
	struct object *obj = mon->held_obj; 
	struct object *dummy = object_new();

	wr_u16b(mon->midx);
	wr_string(mon->race->name);
	if (mon->image_race) {
		wr_string(mon->image_race->name);
	} else {
		wr_string("none");
	}
	wr_byte(mon->grid.y);
	wr_byte(mon->grid.x);
	wr_s16b(mon->hp);
	wr_s16b(mon->maxhp);
	wr_byte(mon->mana);
	wr_byte(mon->song ? mon->song->index : 0);
	wr_s16b(mon->alertness);
	wr_byte(mon->mspeed);
	wr_byte(mon->energy);
	wr_byte(mon->origin);
	wr_byte(mon->stance);
	wr_s16b(mon->morale);
	wr_s16b(mon->tmp_morale);
	wr_byte(mon->noise);
	wr_byte(mon->encountered);
	wr_byte(MON_TMD_MAX);

	for (j = 0; j < MON_TMD_MAX; j++)
		wr_s16b(mon->m_timed[j]);

	for (j = 0; j < MFLAG_SIZE; j++)
		wr_byte(mon->mflag[j]);

	for (j = 0; j < OF_SIZE; j++)
		wr_s16b(mon->known_pstate.flags[j]);

	for (j = 0; j < ELEM_MAX; j++)
		wr_s16b(mon->known_pstate.el_info[j].res_level);

	for (j = 0; j < MAX_ACTION; j++) {
		wr_byte(mon->previous_action[j]);
	}

	/* Write all held objects, followed by a dummy as a marker */
	while (obj) {
		wr_item(obj);
		obj = obj->next;
	}
	wr_item(dummy);
	object_delete(NULL, &dummy);

	/* Write group info */
	wr_u16b(mon->group_info.index);
	wr_byte(mon->group_info.role);

	wr_byte(mon->target.grid.y);
	wr_byte(mon->target.grid.x);
	wr_byte(mon->skip_this_turn);
	wr_byte(mon->skip_next_turn);
	wr_s16b(mon->consecutive_attacks);
	wr_s16b(mon->turns_stationary);
}

/**
 * Write a trap record
 */
static void wr_trap(struct trap *trap)
{
	size_t i;

	if (trap->t_idx) {
		wr_string(trap_info[trap->t_idx].desc);
	} else {
		wr_string("");
	}
	wr_byte(trap->grid.y);
	wr_byte(trap->grid.x);
	wr_byte(trap->power);

	for (i = 0; i < TRF_SIZE; i++)
		wr_byte(trap->flags[i]);
}

/**
 * Write RNG state
 *
 * There were originally 64 bytes of randomizer saved. Now we only need
 * 32 + 5 bytes saved, so we'll write an extra 27 bytes at the end which won't
 * be used.
 */
void wr_randomizer(void)
{
	int i;

	/* current value for the simple RNG */
	wr_u32b(Rand_value);

	/* state index */
	wr_u32b(state_i);

	/* RNG variables */
	wr_u32b(z0);
	wr_u32b(z1);
	wr_u32b(z2);

	/* RNG state */
	for (i = 0; i < RAND_DEG; i++)
		wr_u32b(STATE[i]);

	/* NULL padding */
	for (i = 0; i < 59 - RAND_DEG; i++)
		wr_u32b(0);
}


/**
 * Write the "options"
 */
void wr_options(void)
{
	int i;

	/* Special Options */
	wr_byte(player->opts.delay_factor);
	wr_byte(player->opts.hitpoint_warn);
	wr_byte(player->opts.lazymove_delay);
	/* Fix for tests - only write if angband_term exists, ie in a real game */
	wr_byte(angband_term[0] ? SIDEBAR_MODE : 0);

	/* Normal options */
	for (i = 0; i < OPT_MAX; i++) {
		const char *name = option_name(i);
		if (name) {
			wr_string(name);
			wr_byte(player->opts.opt[i]);
		}
   }

	/* Sentinel */
	wr_byte(0);
}


void wr_messages(void)
{
	int16_t i;
	uint16_t num;

	num = messages_num();
	if (num > 80) num = 80;
	wr_u16b(num);

	/* Dump the messages (oldest first!) */
	for (i = num - 1; i >= 0; i--) {
		wr_string(message_str(i));
		wr_u16b(message_type(i));
	}
}


void wr_monster_memory(void)
{
	int r_idx;

	wr_byte(MFLAG_SIZE);

	for (r_idx = 0; r_idx < z_info->r_max; r_idx++) {
		struct monster_race *race = &r_info[r_idx];
		struct monster_lore *lore = &l_list[r_idx];

		/* Names and kill counts */
		if (!race->name || ((!lore->pkills) && (!lore->psights))) continue;
		wr_string(race->name);
		wr_u16b(lore->pkills);
		wr_u16b(lore->psights);
	}
	wr_string("No more monsters");
}



void wr_object_memory(void)
{
	int k_idx;

	wr_u16b(z_info->k_max);
	wr_byte(OF_SIZE);
	wr_byte(OBJ_MOD_MAX);
	wr_byte(ELEM_MAX);
	wr_byte(z_info->brand_max);
	wr_byte(z_info->slay_max);

	/* Kind knowledge */
	for (k_idx = 0; k_idx < z_info->k_max; k_idx++) {
		uint8_t tmp8u = 0;
		struct object_kind *kind = &k_info[k_idx];

		if (kind->aware) tmp8u |= 0x01;
		if (kind->tried) tmp8u |= 0x02;
		if (kind_is_ignored_aware(kind)) tmp8u |= 0x04;
		if (kind->everseen) tmp8u |= 0x08;
		if (kind_is_ignored_unaware(kind)) tmp8u |= 0x10;

		wr_byte(tmp8u);
	}
}


void wr_player(void)
{
	int i;
	struct ability *ability;

	wr_string(player->full_name);

	wr_string(player->died_from);

	wr_string(player->history);

	/* Race/Class/Gender/Spells */
	wr_string(player->race->name);
	wr_string(player->house->name);
	wr_string(player->sex->name);
	wr_byte(player->opts.name_suffix);

	wr_s16b(player->game_type);
	wr_s16b(player->age);
	wr_s16b(player->ht);
	wr_s16b(player->wt);
	wr_s16b(player->ht_birth);
	wr_s16b(player->wt_birth);

	/* Dump the stats (base and drained) */
	wr_byte(STAT_MAX);
	for (i = 0; i < STAT_MAX; ++i) wr_s16b(player->stat_base[i]);
	for (i = 0; i < STAT_MAX; ++i) wr_s16b(player->stat_drain[i]);

	/* Dump the skill bases */
	wr_byte(SKILL_MAX);
	for (i = 0; i < SKILL_MAX; ++i) wr_s16b(player->skill_base[i]);

	/* Dump the abilities */
	for (ability = player->abilities; ability; ability = ability->next) {
		wr_string(ability->name);
		wr_byte(ability->skill);
		if (ability->active) {
			wr_byte(1);
		} else {
			wr_byte(0);
		}
	}
	wr_string("end");
	for (ability = player->item_abilities; ability; ability = ability->next) {
		wr_string(ability->name);
		wr_byte(ability->skill);
		if (ability->active) {
			wr_byte(1);
		} else {
			wr_byte(0);
		}
	}
	wr_string("end");

	/* Dump the action list */
	for (i = 0; i < MAX_ACTION; i++) {
		wr_byte(player->previous_action[i]);
	}

	/* Player body */
	wr_string(player->body.name);
	wr_u16b(player->body.count);
	for (i = 0; i < player->body.count; i++) {
		wr_u16b(player->body.slots[i].type);
		wr_string(player->body.slots[i].name);
	}

	wr_s32b(player->new_exp);
	wr_s32b(player->exp);
	wr_s32b(player->encounter_exp);
	wr_s32b(player->kill_exp);
	wr_s32b(player->descent_exp);
	wr_s32b(player->ident_exp);
	wr_s32b(player->turn);

	wr_s16b(player->mhp);
	wr_s16b(player->chp);

	wr_s16b(player->msp);
	wr_s16b(player->csp);

	/* Max Dungeon Level */
	wr_s16b(player->max_depth);
	wr_u16b(player->staircasiness);

	wr_s16b(player->energy);

	/* Total energy used so far */
	wr_u32b(player->total_energy);
	/* # of turns spent resting */
	wr_u32b(player->resting_turn);

	/* Find the number of timed effects */
	wr_byte(TMD_MAX);

	/* Read all the effects, in a loop */
	for (i = 0; i < TMD_MAX; i++)
		wr_s16b(player->timed[i]);

	/* Greater vaults seen */
	wr_u16b(z_info->v_max);
	for (i = 0; i < z_info->v_max; i++) {
		wr_byte(player->vaults[i]);
	}

	/* More info */
	wr_byte(player->unignoring);
	wr_s16b(player->last_attack_m_idx);	
	wr_s16b(player->consecutive_attacks);
	wr_s16b(player->bane_type);
	wr_byte(player->focused);
	wr_byte(player->song[SONG_MAIN] ? player->song[SONG_MAIN]->index : 0);
	wr_byte(player->song[SONG_MINOR] ? player->song[SONG_MINOR]->index : 0);
	wr_s16b(player->song_duration);
	wr_s16b(player->wrath); 
	wr_u16b(player->stairs_taken);
	wr_u16b(player->forge_drought);
	wr_u16b(player->forge_count);
	wr_byte(player->stealth_mode);
	wr_byte(player->self_made_arts);
	wr_byte(player->truce);
	wr_byte(player->morgoth_hits);
	wr_byte(player->crown_hint);
	wr_byte(player->crown_shatter);
	wr_byte(player->cursed);
	wr_byte(player->on_the_run);
	wr_byte(player->morgoth_slain);
	wr_byte(player->escaped);
	wr_u16b(player->noscore);
	wr_s16b(player->smithing_leftover);
	wr_byte(player->unique_forge_made);
	wr_byte(player->unique_forge_seen);
}


void wr_ignore(void)
{
	size_t i;
	uint16_t j, n;

	/* Write number of ignore bytes */
	assert(ignore_size <= 255);
	wr_byte((uint8_t)ignore_size);
	for (i = 0; i < ignore_size; i++)
		wr_byte(ignore_level[i]);

	/* Write ego-item ignore bits */
	wr_u16b(z_info->e_max);
	wr_u16b(ITYPE_SIZE);
	for (i = 0; i < z_info->e_max; i++) {
		bitflag everseen = 0, itypes[ITYPE_SIZE];

		/* Figure out and write the everseen and aware flags */
		if (e_info[i].everseen)
			everseen |= 0x02;
		if (e_info[i].aware)
			everseen |= 0x04;
		wr_byte(everseen);

		/* Figure out and write the ignore flags */
		itype_wipe(itypes);
		for (j = ITYPE_NONE; j < ITYPE_MAX; j++)
			if (ego_is_ignored(i, j))
				itype_on(itypes, j);

		for (j = 0; j < ITYPE_SIZE; j++)
			wr_byte(itypes[j]);
	}

	/* Write the current number of aware object auto-inscriptions */
	n = 0;
	for (i = 0; i < z_info->k_max; i++)
		if (k_info[i].note_aware)
			n++;

	wr_u16b(n);

	/* Write the aware object autoinscriptions array */
	for (i = 0; i < z_info->k_max; i++) {
		if (k_info[i].note_aware) {
			char name[1024];
			wr_string(tval_find_name(k_info[i].tval));
			obj_desc_name_format(name, sizeof name, 0, k_info[i].name, 0,
								 false);
			wr_string(name);
			wr_string(quark_str(k_info[i].note_aware));
		}
	}

	/* Write the current number of unaware object auto-inscriptions */
	n = 0;
	for (i = 0; i < z_info->k_max; i++)
		if (k_info[i].note_unaware)
			n++;

	wr_u16b(n);

	/* Write the unaware object autoinscriptions array */
	for (i = 0; i < z_info->k_max; i++) {
		if (k_info[i].note_unaware) {
			char name[1024];
			wr_string(tval_find_name(k_info[i].tval));
			obj_desc_name_format(name, sizeof name, 0, k_info[i].name, 0,
								 false);
			wr_string(name);
			wr_string(quark_str(k_info[i].note_unaware));
		}
	}

	return;
}


void wr_misc(void)
{
	/* Random artifact seed */
	wr_u32b(seed_randart);

	/* Write the "object seeds" */
	wr_u32b(seed_flavor);

	/* Special stuff */
	wr_u16b(player->noscore);

	/* Write death */
	wr_byte(player->is_dead);

	/* Current turn */
	wr_s32b(turn);
}


void wr_artifacts(void)
{
	int i;
	uint16_t tmp16u;

	/* Hack -- Dump the artifacts */
	tmp16u = z_info->a_max;
	wr_u16b(tmp16u);
	for (i = 0; i < tmp16u; i++) {
		const struct artifact_upkeep *au = &aup_info[i];
		wr_byte(au->created ? 1 : 0);
		wr_byte(au->seen ? 1 : 0);
		wr_byte(au->everseen ? 1 : 0);
		wr_byte(0);
	}
}


static void wr_gear_aux(struct object *gear)
{
	struct object *obj;

	/* Write the inventory */
	for (obj = gear; obj; obj = obj->next) {
		/* Skip non-objects */
		assert(obj->kind);

		/* Write code for equipment or other gear */
		wr_byte(object_slot(player->body, obj));

		/* Dump object */
		wr_item(obj);

	}

	/* Write finished code */
	wr_byte(FINISHED_CODE);
}


void wr_gear(void)
{
	wr_gear_aux(player->gear);
}



/**
 * Write the current dungeon terrain features and info flags
 *
 * Note that the cost and when fields of c->squares[y][x] are not saved
 */
static void wr_dungeon_aux(struct chunk *c)
{
	int y, x;
	size_t i;

	uint8_t tmp8u;

	uint8_t count;
	uint8_t prev_char;

	/* Dungeon specific info follows */
	wr_string(c->name ? c->name : "Blank");
	wr_u16b(c->height);
	wr_u16b(c->width);
	if (c->vault_name) {
		wr_byte(1);
		wr_string(c->vault_name);
	} else {
		wr_byte(0);
	}

	/* Run length encoding of c->squares[y][x].info */
	for (i = 0; i < SQUARE_SIZE; i++) {
		count = 0;
		prev_char = 0;

		/* Dump for each grid */
		for (y = 0; y < c->height; y++) {
			for (x = 0; x < c->width; x++) {
				/* Extract the important c->squares[y][x].info flags */
				tmp8u = square(c, loc(x, y))->info[i];

				/* If the run is broken, or too full, flush it */
				if ((tmp8u != prev_char) || (count == UCHAR_MAX)) {
					wr_byte(count);
					wr_byte(prev_char);
					prev_char = tmp8u;
					count = 1;
				} else /* Continue the run */
					count++;
			}
		}

		/* Flush the data (if any) */
		if (count) {
			wr_byte(count);
			wr_byte(prev_char);
		}
	}

	/* Now the terrain */
	count = 0;
	prev_char = 0;

	/* Dump for each grid */
	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			/* Extract a byte */
			tmp8u = square(c, loc(x, y))->feat;

			/* If the run is broken, or too full, flush it */
			if ((tmp8u != prev_char) || (count == UCHAR_MAX)) {
				wr_byte(count);
				wr_byte(prev_char);
				prev_char = tmp8u;
				count = 1;
			} else /* Continue the run */
				count++;
		}
	}

	/* Flush the data (if any) */
	if (count) {
		wr_byte(count);
		wr_byte(prev_char);
	}
}

/**
 * Write the dungeon floor objects
 */
static void wr_objects_aux(struct chunk *c)
{
	int y, x;
	struct object *dummy;

	if (player->is_dead)
		return;
	
	/* Write the objects */
	wr_u16b(c->obj_max);
	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			struct object *obj = square(c, loc(x, y))->obj;
			while (obj) {
				wr_item(obj);
				obj = obj->next;
			}
		}
	}

	/* Write a dummy record as a marker */
	dummy = mem_zalloc(sizeof(*dummy));
	wr_item(dummy);
	mem_free(dummy);
}

/**
 * Write the monster list
 */
static void wr_monsters_aux(struct chunk *c)//TODO check flow info is covered
{
	int i;

	if (player->is_dead)
		return;

	/* Total monsters */
	wr_u16b(cave_monster_max(c));

	/* Dump the monsters */
	for (i = 1; i < cave_monster_max(c); i++) {
		const struct monster *mon = cave_monster(c, i);

		wr_monster(mon);
	}
}

static void wr_traps_aux(struct chunk *c)
{
    int x, y;
	struct trap *dummy;

    if (player->is_dead)
		return;

    wr_byte(TRF_SIZE);

	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			struct trap *trap = square(c, loc(x, y))->trap;
			while (trap) {
				wr_trap(trap);
				trap = trap->next;
			}
		}
	}

	/* Write a dummy record as a marker */
	dummy = mem_zalloc(sizeof(*dummy));
	wr_trap(dummy);
	mem_free(dummy);
}

void wr_dungeon(void)
{
	/* Dungeon specific info follows */
	wr_u16b(player->depth);
	wr_u16b(daycount);
	wr_u16b(player->grid.y);
	wr_u16b(player->grid.x);
	wr_byte(SQUARE_SIZE);

	if (player->is_dead)
		return;

	/* Write caves */
	wr_dungeon_aux(cave);

	/* Compact the monsters */
	compact_monsters(cave, 0);
}


void wr_objects(void)
{
	wr_objects_aux(cave);
}

void wr_monsters(void)
{
	wr_monsters_aux(cave);
}

void wr_traps(void)
{
	wr_traps_aux(cave);
}

void wr_history(void)
{
	size_t i, j;

	struct history_info *history_list;
	uint32_t length = history_get_list(player, &history_list);

	wr_byte(HIST_SIZE);
	wr_u32b(length);
	for (i = 0; i < length; i++) {
		for (j = 0; j < HIST_SIZE; j++)
			wr_byte(history_list[i].type[j]);
		wr_s32b(history_list[i].turn);
		wr_s16b(history_list[i].dlev);
		if (history_list[i].a_idx) {
			wr_string(a_info[history_list[i].a_idx].name);
		} else {
			wr_string("");
		}
		wr_string(history_list[i].event);
	}
}
