/**
 * \file ui-tutorial.c
 * \brief Implement starting the tutorial and tutorial hooks into the UI layer.
 *
 * Copyright (c) 2022 Eric Branlund
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

#include "cmd-core.h"
#include "game-world.h"
#include "grafmode.h"
#include "monster.h"
#include "player-abilities.h"
#include "player-skills.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "tutorial.h"
#include "tutorial-init.h"
#include "ui-event.h"
#include "ui-input.h"
#include "ui-object.h"
#include "ui-output.h"
#include "ui-prefs.h"
#include "ui-tutorial.h"
#include "z-util.h"


static void textui_tutorial_textblock_show(textblock *tb, const char *header)
{
	region orig_area = { COL_MAP, ROW_MAP, 60, 18 };

	(void) textui_textblock_show(tb, orig_area, header);
}


static void textui_tutorial_textblock_append_command_phrase(textblock *tb,
		const char *command_name, bool capital, bool gerund)
{
	int i = 0;

	while (1) {
		size_t j;

		if (!cmds_all[i].list) {
			break;
		}
		j = 0;
		while (1) {
			if (j >= cmds_all[i].len) {
				break;
			}
			if (streq(cmds_all[i].list[j].desc, command_name)) {
				int mode = OPT(player, hjkl_movement) ?
					KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;
				keycode_t code = cmds_all[i].list[j].key[mode];

				/*
				 * If a keymap doesn't specify a key, use the
				 * default keymap.
				 */
				if (!code && mode != KEYMAP_MODE_ORIG) {
					code = cmds_all[i].list[j].key[KEYMAP_MODE_ORIG];
				}
				if (code) {
					const char *desc =
						keycode_find_desc(code);

					textblock_append(tb, "%s%s ",
						(capital) ?  "Press" : "press",
						(gerund) ? "ing" : "");
					if (desc) {
						textblock_append(tb, "'%s'",
							desc);
					} else if (KTRL(code) == code) {
						textblock_append(tb, "'ctrl-%c'",
							UN_KTRL(code));
					} else {
						textblock_append(tb, "'%c'",
							code);
					}
				}
				return;
			}
			++j;
		}
		++i;
	}
}


static void textui_tutorial_textblock_append_direction_phrase(textblock *tb,
		int dirnum, bool capital, bool gerund)
{
	int mode = OPT(player, hjkl_movement) ?
		KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;
	char keys[2][9] = {
		{ '1', '2', '3', '4', '5', '6', '7', '8', '9' },
		{ 'b', 'j', 'n', 'h', '.', 'l', 'y', 'k', 'u' }
	};

	if (dirnum > 0 && dirnum < 10) {
		textblock_append(tb, "%s%s '%c'", (capital) ? "Press" : "press",
			(gerund) ? "ing" : "", keys[mode][dirnum - 1]);
	}
}


static void textui_tutorial_textblock_append_direction_rose(textblock *tb)
{
	if (OPT(player, hjkl_movement)) {
		textblock_append(tb,
			"\nUse the keyboard to move (or stay still) as follows:\n\n"
			"y (northeast)  k (north)   u (northeast)\n"
			"             \\    |      /\n"
			"   h (west)  -    .      -   l (east)\n"
			"             /    |      \\\n"
			"b (southeast)  j (south)   n (southeast)\n"
			"\nIn most environments, the numeric keypad or "
			"pointing and clicking with the mouse may also be "
			"used.\n");
	} else {
		textblock_append(tb,
			"\nUse the keyboard to move (or stay still) as follows:\n\n"
			"7 (northeast)  8 (north)   9 (northeast)\n"
			"             \\    |      /\n"
			"   4 (west)  -    5      -   6 (east)\n"
			"             /    |      \\\n"
			"1 (southeast)  2 (south)   3 (southeast)\n"
			"\nIn most environments, the numeric keypad or "
			"pointing and clicking with the mouse may also be "
			"used.  If you don't have a numeric keypad, you may "
			"want to enable the rogue-like keys (press =, select "
			"a for the interface options, then turn on the first "
			"option there) for a more convenient set of movement "
			"controls.\n");
	}
}


static void textui_tutorial_textblock_append_feature_symbol(textblock *tb,
		int feat)
{
	int attr = feat_x_attr[LIGHTING_LIT][feat];

	if (use_graphics == GRAPHICS_NONE && feat_is_wall(feat)) {
		if (OPT(player, hybrid_walls)) {
			attr += (MULT_BG * BG_DARK);
		} else if (OPT(player, solid_walls)) {
			attr += (MULT_BG * BG_SAME);
		}
	}
	textblock_append(tb, "('");
	textblock_append_pict(tb, attr, feat_x_char[LIGHTING_LIT][feat]);
	textblock_append(tb, "')");
}


static void textui_tutorial_textblock_append_monster_symbol(textblock *tb,
		const struct monster_race *race)
{
	textblock_append(tb, "('");
	textblock_append_pict(tb, monster_x_attr[race->ridx],
		monster_x_char[race->ridx]);
	textblock_append(tb, "')");
}


static void textui_tutorial_textblock_append_object_symbol(textblock *tb,
		const struct object_kind *kind)
{
	textblock_append(tb, "('");
	textblock_append_pict(tb, object_kind_attr(kind),
		object_kind_char(kind));
	textblock_append(tb, "')");
}


/**
 * Load the tutorial definitions and start the tutorial.
 */
void start_tutorial(void)
{
	char name[PLAYER_NAME_LEN];

	tutorial_parse_data();

	if (tutorial_parsed_data.default_archetype) {
		struct tutorial_dict_val_type *a =
			tutorial_parsed_data.default_archetype;
		const struct player_race *rpick = NULL;
		int i, exp;

		character_generated = false;
		cmdq_push(CMD_BIRTH_INIT);
		cmdq_push(CMD_BIRTH_RESET);
		if (!races) {
			quit_fmt("No races specified prior to loading tutorial.");
		}
		cmdq_push(CMD_CHOOSE_RACE);
		if (a->v.archetype.race_name) {
			if (streq(a->v.archetype.race_name, "*")) {
				/* Choose one at random. */
				const struct player_race *rc = races;
				int nr = 0;

				while (rc) {
					++nr;
					if (one_in_(nr)) {
						rpick = rc;
					}
					rc = rc->next;
				}
			} else {
				rpick = races;
				while (1) {
					if (!rpick) {
						quit_fmt("Unknown race, %s, "
							"specified for "
							"tutorial archetype.",
							a->v.archetype.race_name);
						break;
					}
					if (streq(rpick->name, a->v.archetype.race_name)) {
						break;
					}
					rpick = rpick->next;
				}
			}
		} else {
			/* Use the first race. */
			rpick = races;
		}
		cmd_set_arg_choice(cmdq_peek(), "choice", rpick->ridx);
		cmdq_push(CMD_CHOOSE_HOUSE);
		if (a->v.archetype.house_name) {
			const struct player_house *hpick = NULL;
			unsigned int hmin = UINT_MAX;

			if (streq(a->v.archetype.house_name, "*")) {
				/* Choose one at random. */
				const struct player_house *hc = houses;
				int nh = 0;

				while (hc) {
					if (hc->race == rpick) {
						++nh;
						if (hmin > hc->hidx) {
							hmin = hc->hidx;
						}
						if (one_in_(nh)) {
							hpick = hc;
						}
					}
					hc = hc->next;
				}
				if (!hpick) {
					quit_fmt("No houses available for the "
						"race, %s, specified in the "
						"tutorial.", rpick->name);
				}
			} else {
				const struct player_house *hc = houses;

				while (hc) {
					if (hc->race == rpick
							&& hmin > hc->hidx) {
						hmin = hc->hidx;
					}
					if (streq(hc->short_name, a->v.archetype.house_name)) {
						if (hc->race != rpick) {
							quit_fmt("House, %s, "
								"selected for "
								"tutorial does "
								"not match the "
								"selected race, "
								"%s.",
								a->v.archetype.house_name,
								rpick->name);
						}
						hpick = hc;
					}
					hc = hc->next;
				}
				if (!hpick) {
					quit_fmt("Unknown house, %s, "
						"specified for the tutorial.",
						a->v.archetype.house_name);
				}
			}
			cmd_set_arg_choice(cmdq_peek(), "choice",
				hpick->hidx - hmin);
		} else {
			cmd_set_arg_choice(cmdq_peek(), "choice", 0);
		}
		if (!sexes) {
			quit_fmt("No sexes specified prior to loading tutorial.");
		}
		cmdq_push(CMD_CHOOSE_SEX);
		if (a->v.archetype.sex_name) {
			const struct player_sex *spick = NULL;

			if (streq(a->v.archetype.sex_name, "*")) {
				/* Choose one at random. */
				const struct player_sex *sc = sexes;
				int ns = 0;

				while (sc) {
					++ns;
					if (one_in_(ns)) {
						spick = sc;
					}
					sc = sc->next;
				}
			} else {
				spick = sexes;
				while (1) {
					if (!spick) {
						quit_fmt("Unknown sex, %s, "
							"specified for "
							"tutorial archetype.",
							a->v.archetype.sex_name);
						break;
					}
					if (streq(spick->name, a->v.archetype.sex_name)) {
						break;
					}
					spick = spick->next;
				}
			}
			cmd_set_arg_choice(cmdq_peek(), "choice", spick->sidx);
		} else {
			cmd_set_arg_choice(cmdq_peek(), "choice", sexes->sidx);
		}
		cmdq_push(CMD_NAME_CHOICE);
		if (a->v.archetype.character_name
				&& !streq(a->v.archetype.character_name, "*")) {
			my_strcpy(name, a->v.archetype.character_name,
				sizeof(name));
		} else {
			(void) player_random_name(name, sizeof(name));
		}
		cmd_set_arg_string(cmdq_peek(), "name", name);
		if (a->v.archetype.history
				&& !streq(a->v.archetype.history, "*")) {
			char history[240];

			/*
			 * Limit the size to what the user interface can
			 * display.
			 */
			my_strcpy(history, a->v.archetype.history,
				sizeof(history));
			cmdq_push(CMD_HISTORY_CHOICE);
			cmd_set_arg_string(cmdq_peek(), "history", history);
		}
		cmdq_push(CMD_ACCEPT_CHARACTER);
		cmdq_execute(CTX_BIRTH);

		/* Adjust the starting stats. */
		for (i = 0; i < STAT_MAX; ++i) {
			if (a->v.archetype.stat_adj[i] >= 0) {
				if (player->stat_base[i] <= BASE_STAT_MAX
						- a->v.archetype.stat_adj[i]) {
					player->stat_base[i] +=
						a->v.archetype.stat_adj[i];
				} else {
					player->stat_base[i] = BASE_STAT_MAX;
				}
			} else {
				if (player->stat_base[i] >= BASE_STAT_MIN
						- a->v.archetype.stat_adj[i]) {
					player->stat_base[i] +=
						a->v.archetype.stat_adj[i];
				} else {
					player->stat_base[i] = BASE_STAT_MIN;
				}
			}
		}

		/*
		 * Adjust the starting skills.  In first pass, get the amount
		 * of additional experience needed and cap, if necessary the
		 * adjustment from the data file.
		 */
		exp = 0;
		for (i = 0; i < SKILL_MAX; ++i) {
			/*
			 * The experience cost calculations assumed here
			 * duplicate logic in player-abilities.c's skill_cost().
			 */
			int max_exp, cskill, inc_lo, inc_hi;

			if (a->v.archetype.skill_adj[i] == 0) {
				continue;
			}
			assert(a->v.archetype.skill_adj[i] > 0);
			/*
			 * Avoid the experience total overflowing PY_MAX_EXP.
			 * The experience to get from a skill level of n to
			 * n + m is 100 * (((n + m) * (n + m + 1)) / 2) -
			 * 100 * ((n * (n + 1)) / 2) = 50 * m * (m + 2 * n + 1).
			 */
			max_exp = PY_MAX_EXP - player->exp;
			assert(max_exp >= 0);
			cskill = player->skill_base[i];
			inc_lo = 0;
			if (cskill > max_exp / 100 - 1) {
				inc_hi = 1;
			} else {
				inc_hi = max_exp / ((cskill > 0) ?
						100 * cskill : 50);
			}
			while (1) {
				int inc_try, exp_try;

				/*
				 * Done if have determined the maximum increment
				 * or if it won't affect the value from the
				 * data file.
				 */
				if (inc_lo == inc_hi - 1
						|| inc_lo >= a->v.archetype.skill_adj[i]) {
					break;
				}
				inc_try = (inc_lo + inc_hi) / 2;
				if (inc_try > max_exp / 50) {
					inc_hi = inc_try;
				} else if (inc_try + cskill + cskill >=
						max_exp / (50 * inc_try)) {
					inc_hi = inc_try;
				} else {
					exp_try = 50 * inc_try * (inc_try
						+ cskill + cskill + 1);
					if (exp_try <= max_exp) {
						inc_lo = inc_try;
					} else {
						inc_hi = inc_try;
					}
				}
			}
			if (a->v.archetype.skill_adj[i] > inc_lo) {
				a->v.archetype.skill_adj[i] = inc_lo;
			}
			exp += 50 * a->v.archetype.skill_adj[i]
				* (a->v.archetype.skill_adj[i] + cskill
				+ cskill + 1);
			if (player->new_exp < exp) {
				/*
				 * Give the necessary experience for free.
				 */
				player_exp_gain(player, exp - player->new_exp);
			}
		}
		/* Now apply the adjustments. */
		init_skills(false, false);
		for (i = 0; i < SKILL_MAX; ++i) {
			int inc = a->v.archetype.skill_adj[i];

			if (inc == 0) {
				continue;
			}
			assert(a->v.archetype.skill_adj[i] > 0);
			while (inc) {
				cmdq_push(CMD_BUY_SKILL);
				cmd_set_arg_choice(cmdq_peek(), "choice", i);
				cmdq_execute(CTX_GAME);
				--inc;
			}
		}
		finalise_skills();

		/* Add the additional abilities. */
		for (i = 0; i < a->v.archetype.ability_count; ++i) {
			struct ability *anew;

			if (!player_has_prereq_abilities(player,
					a->v.archetype.added_abilities[i])) {
				msg("Missing prerequisites for ability, %s, "
					"specified for the tutorial archetype.",
					a->v.archetype.added_abilities[i]->name);
				continue;
			}

			exp = player_ability_cost(player,
				a->v.archetype.added_abilities[i]);
			if (player->new_exp < exp) {
				/*
				 * Give the necessary experience for free.
				 */
				player_exp_gain(player, exp - player->new_exp);
			}
			/*
			 * Mimic player_gain_ability() but omit checks that
			 * are not necessary.
			 */
			player->new_exp -= exp;
			add_ability(&player->abilities,
				a->v.archetype.added_abilities[i]);
			anew = locate_ability(player->abilities,
				a->v.archetype.added_abilities[i]);
			assert(anew);
			anew->active = true;
		}

		/* Adjust the pool of unspent experience if necesary. */
		if (a->v.archetype.unspent_experience >= 0) {
			if (player->new_exp
					< a->v.archetype.unspent_experience) {
				player_exp_gain(player,
					a->v.archetype.unspent_experience
					- player->new_exp);
			} else if (player->new_exp >
					a->v.archetype.unspent_experience) {
				/*
				 * If it needs to decrease; simply coerce it to
				 * the requested value.  Do not bother to keep
				 * other tabulations (player->exp, for
				 * instance) consistent with that.
				 */
				player->new_exp =
					a->v.archetype.unspent_experience;
			}
		}

		if (a->v.archetype.purge_kit) {
			/*
			 * Do it in two passes.  The first takes care of pack
			 * and quiver.  Then there's slots available to take
			 * off and delete equipment without overflowing the
			 * pack.
			 */
			struct object *curr = player->gear;
			int pass = 0;

			while (1) {
				struct object *next;
				int slot;
				bool none_left;

				if (curr == NULL) {
					if (pass == 0) {
						curr = player->gear;
						if (curr == NULL) {
							break;
						}
						++pass;
					} else {
						break;
					}
				}

				next = curr->next;
				slot = equipped_item_slot(player->body, curr);
				if (slot != player->body.count) {
					if (pass == 0) {
						curr = next;
						continue;
					}
					/*
					 * inven_takeoff() always generates
					 * messages; do it ourselves without
					 * those.
					 */
					player->body.slots[slot].obj = NULL;
					--player->upkeep->equip_cnt;
				}

				none_left = false;
				curr = gear_object_for_use(player, curr,
					curr->number, false, &none_left);
				assert(none_left);
				/*
				 * Not checking if it's an artifact, that may
				 * be an issue if the birth process allows
				 * artifacts in the starting kit.
				 */
				object_free(curr->known);
				object_free(curr);
				curr = next;
			}
		}

		if (a->v.archetype.kit_count > 0) {
			/* Add to the starting kit. */
			assert(a->v.archetype.kit);
			for (i = 0; i < a->v.archetype.kit_count; ++i) {
				const struct tutorial_kit_item *kit =
					a->v.archetype.kit + i;
				struct object *obj;

				if (!kit->equipped && pack_is_full()) {
					continue;
				}
				obj = (kit->item.is_artifact) ?
					tutorial_create_artifact(kit->item.v.art) :
					tutorial_create_object(&kit->item);
				if (!obj) {
					continue;
				}
				obj->origin = ORIGIN_BIRTH;
				obj->known = object_new();
				object_set_base_known(player, obj);
				object_flavor_aware(player, obj);
				if (kit->equipped) {
					/*
					 * Wield what can be wielded; put the
					 * rest in the pack if possible.
					 */
					int slot = wield_slot(obj);
					struct object *eobj = (slot == -1) ?
						NULL : slot_object(player, slot);
					if (slot != -1 && eobj == NULL) {
						if (obj->number > 1) {
							eobj = object_split(
								obj, 1);
						} else {
							eobj = obj;
						}
						player->body.slots[slot].obj =
							eobj;
						object_learn_on_wield(player, eobj);
						player->upkeep->total_weight +=
							eobj->weight;
						++player->upkeep->equip_cnt;
						if (obj == eobj) {
							continue;
						}
					}

					if (pack_is_full()) {
						if (obj->artifact) {
							mark_artifact_created(
								obj->artifact,
								false);
						}
						object_free(obj->known);
						object_free(obj);
						continue;
					}
				}
				inven_carry(player, obj, true, false);
			}
			update_player_object_knowledge(player);
		}
	} else {
		/* Use the first race and class with a random name. */
		bool result;

		(void) player_random_name(name, sizeof(name));
		result = player_make_simple(NULL, NULL, NULL, name);
		if (!result) {
			assert(false);
		}
	}

	/* Mark player as in the tutorial; disable autosaving. */
	player->game_type = -1;
	player->upkeep->autosave = false;

	/* Install event handlers so notes and triggers work. */
	event_add_handler(EVENT_ENTER_WORLD, tutorial_handle_enter_world, NULL);
	event_add_handler(EVENT_LEAVE_WORLD, tutorial_handle_leave_world, NULL);

	/* Set up UI hooks for the tutorial. */
	tutorial_textblock_show_hook = textui_tutorial_textblock_show;
	tutorial_textblock_append_command_phrase_hook =
		textui_tutorial_textblock_append_command_phrase;
	tutorial_textblock_append_direction_phrase_hook =
		textui_tutorial_textblock_append_direction_phrase;
	tutorial_textblock_append_direction_rose_hook =
		textui_tutorial_textblock_append_direction_rose;
	tutorial_textblock_append_feature_symbol_hook =
		textui_tutorial_textblock_append_feature_symbol;
	tutorial_textblock_append_monster_symbol_hook =
		textui_tutorial_textblock_append_monster_symbol;
	tutorial_textblock_append_object_symbol_hook =
		textui_tutorial_textblock_append_object_symbol;

	/* Tell the UI, we've started.  Mimics start_game(). */
	event_signal(EVENT_LEAVE_INIT);
	event_signal(EVENT_ENTER_GAME);
	event_signal(EVENT_ENTER_WORLD);

	/* Enter the default tutorial section. */
	tutorial_prepare_section(NULL, player);
	on_new_level();
}
