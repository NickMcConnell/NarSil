/**
 * \file ui-smith.c
 * \brief Text-based user interface for object smithing
 *
 * Copyright (c) 1987 - 2022 Angband contributors
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
#include "cmds.h"
#include "cmd-core.h"
#include "game-event.h"
#include "game-input.h"
#include "init.h"
#include "obj-desc.h"
#include "obj-info.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-smith.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-abilities.h"
#include "player-calcs.h"
#include "trap.h"
#include "ui-input.h"
#include "ui-menu.h"
#include "ui-options.h"
#include "ui-smith.h"


/**
 * ------------------------------------------------------------------------
 * The various "menu" bits of the smithing process - item type, item sub-type,
 * special type, artefact flags, artefact abilities, artefact, number
 * modification, melting.
 * ------------------------------------------------------------------------ */
/**
 * The smithed object
 */
static struct object smith_obj_body;
static struct object *smith_obj = &smith_obj_body;
static struct object smith_obj_known_body;
static struct object *smith_obj_known = &smith_obj_known_body;

/**
 * Backup of the smithed object.
 *
 * For most purposes we can and do use a local backup, but we need one external
 * to the include/exclude pval functions as including a 0 pval destroys
 * information about modifiers.
 */
static struct object smith_obj_body_backup;
static struct object *smith_obj_backup = &smith_obj_body_backup;

/**
 * The smithed artifact
 */
static struct artifact smith_art_body;
static struct artifact *smith_art = &smith_art_body;
static char smith_art_name[50];

/**
 * Current smithing cost
 */
static struct smithing_cost current_cost;

/**
 * Smithing menu data struct
 */
struct smithing_menu_data {
	struct object *obj;
};

static struct menu *smithing_menu;

static bool no_forge;
static bool exhausted;
static bool create_smithed_item;
static bool numbers_changed;

/**
 * Locations of the menus, etc. on the screen
 */
#define	COL_SMT1		 2
#define COL_SMT2		16
#define COL_SMT3		34
#define COL_SMT4		62

#define	ROW_SMT1		 2
#define ROW_SMT2		 8
#define ROW_SMT3		10


/**
 * Current special bonus value.
 * This is stored here, and only included in the necessary places in objects
 * when we're doing calculations that need it.
 */
static int pval = 0;
static bool pval_included = false;

static void include_pval(struct object *obj)
{
	int i;
	if (!pval_included) {
		object_wipe(smith_obj_backup);
		object_copy(smith_obj_backup, smith_obj);
	}
	if (pval_valid(obj)) {
		obj->pval = pval;
		for (i = 0; i < OBJ_MOD_MAX; i++) {
			if (ABS(obj->modifiers[i]) <= 1) obj->modifiers[i] = pval * ABS(obj->modifiers[i]);
			obj->known->modifiers[i] = obj->modifiers[i];
		}
	}
	pval_included = true;
}

static void exclude_pval(struct object *obj)
{
	if (pval_included) {
		object_wipe(smith_obj);
		object_copy(smith_obj, smith_obj_backup);
	}
	pval_included = false;
}

static void wipe_smithing_objects(void)
{
	object_wipe(smith_obj);
	object_wipe(smith_obj_backup);
	mem_free(smith_art->slays);
	mem_free(smith_art->brands);
	release_ability_list(smith_art->abilities);
	memset(smith_art, 0, sizeof(*smith_art));
}

static void reset_smithing_objects(struct object_kind *kind)
{
	object_wipe(smith_obj);
	object_wipe(smith_obj_backup);
	mem_free(smith_art->slays);
	mem_free(smith_art->brands);
	release_ability_list(smith_art->abilities);
	memset(smith_art, 0, sizeof(*smith_art));
	create_base_object(kind, smith_obj);
	object_copy(smith_obj_known, smith_obj);
	smith_obj->known = smith_obj_known;
	object_copy(smith_obj_backup, smith_obj);
	pval = pval_valid(smith_obj) ? smith_obj->kind->pval : 0;
}

/**
 * Show smithing object data
 */
static void show_smith_obj(void)
{
	int effective_skill = player->state.skill_use[SKILL_SMITHING] +
		square_forge_bonus(cave, player->grid);
		//TODO allow for masterpiece, probably by adding effective_skill to data
	int dif;
	uint8_t attr;
	bool affordable = true;
	int costs = 0;
	char o_desc[80];
	int mode = ODESC_FULL | ODESC_CAPITAL | ODESC_SPOIL;
	textblock *tb;
	region bottom = { COL_SMT2, MAX_SMITHING_TVALS + 3, 0, 0};
	region right = { COL_SMT4, ROW_SMT1, 0, 0};
	const struct object_kind *kind = smith_obj->kind;

	/* Abort if there is no object to display */
	if (!kind) return;

	/* Evaluate difficulty */
	include_pval(smith_obj);
	dif = object_difficulty(smith_obj, &current_cost);
	attr = effective_skill < dif ? COLOUR_SLATE : COLOUR_L_DARK;
	exclude_pval(smith_obj);

	/* Redirect output to the screen */
	text_out_hook = text_out_to_screen;
	text_out_wrap = 80;

	/* Object difficulty */
	text_out_indent = COL_SMT4;

	region_erase(&bottom);
	region_erase(&right);
	Term_gotoxy(COL_SMT4, ROW_SMT1);
	text_out_c(attr, "Difficulty:\n\n");
	if (current_cost.drain) {
		attr = COLOUR_BLUE;
	}
	text_out_c(attr, "%3d", dif);
	text_out_c(COLOUR_L_DARK, "  (max %d)", effective_skill);

	/* Object costs */
	Term_gotoxy(COL_SMT4, ROW_SMT2);
	if (affordable)	{
		attr = COLOUR_SLATE;
	} else {
		attr = COLOUR_L_DARK;
	}
	text_out_c(attr, "Cost:");

	Term_gotoxy(COL_SMT4 + 2, ROW_SMT3);
	if (current_cost.weaponsmith) {
		text_out_c(COLOUR_RED, "Weaponsmith\n");
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.armoursmith) {
		text_out_c(COLOUR_RED, "Armoursmith\n");
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.jeweller) {
		text_out_c(COLOUR_RED, "Jeweller\n");
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.enchantment) {
		text_out_c(COLOUR_RED, "Enchantment\n");
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.artistry) {
		text_out_c(COLOUR_RED, "Artistry\n");
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.artifice) {
		text_out_c(COLOUR_RED, "Artifice\n");
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.uses > 0) {
		if (square_forge_uses(cave, player->grid) >= current_cost.uses) {
			attr = COLOUR_SLATE;
		} else {
			affordable = false;
			attr = COLOUR_L_DARK;
		}
		if (current_cost.uses == 1) {
			text_out_c(attr, "1 Use");
		} else {
			text_out_c(attr, "%d Uses", current_cost.uses);
		}
		text_out_c(COLOUR_L_DARK, " (of %d)",
				   square_forge_uses(cave, player->grid));
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.drain > 0) {
		if (current_cost.drain <= player->skill_base[SKILL_SMITHING]) {
			attr = COLOUR_BLUE;
		} else {
			attr = COLOUR_L_DARK;
			affordable = false;
		}													
		text_out_c(attr, "%d Smithing", current_cost.drain);
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.mithril > 0) {
		if (current_cost.mithril <= mithril_carried(player)) {
			attr = COLOUR_SLATE;
		} else {
			attr = COLOUR_L_DARK;
			affordable = false;
		}													
		text_out_c(attr, "%d.%d lb Mithril", current_cost.mithril / 10,
				   current_cost.mithril % 10);
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.stat[STAT_STR] > 0) {
		if (player->stat_base[STAT_STR] + player->stat_drain[STAT_STR]
			- current_cost.stat[STAT_STR] >= -5) {
			attr = COLOUR_SLATE;
		} else {
			attr = COLOUR_L_DARK;
			affordable = false;
		}													
		text_out_c(attr, "%d Str", current_cost.stat[STAT_STR]);
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.stat[STAT_DEX] > 0) {
		if (player->stat_base[STAT_DEX] + player->stat_drain[STAT_DEX]
			- current_cost.stat[STAT_DEX] >= -5) {
			attr = COLOUR_SLATE;
		} else {
			attr = COLOUR_L_DARK;
			affordable = false;
		}													
		text_out_c(attr, "%d Dex", current_cost.stat[STAT_DEX]);
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.stat[STAT_CON] > 0) {
		if (player->stat_base[STAT_CON] + player->stat_drain[STAT_CON]
			- current_cost.stat[STAT_CON] >= -5) {
			attr = COLOUR_SLATE;
		} else {
			attr = COLOUR_L_DARK;
			affordable = false;
		}													
		text_out_c(attr, "%d Con", current_cost.stat[STAT_CON]);
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.stat[STAT_GRA] > 0) {
		if (player->stat_base[STAT_GRA] + player->stat_drain[STAT_GRA]
			- current_cost.stat[STAT_GRA] >= -5) {
			attr = COLOUR_SLATE;
		} else {
			attr = COLOUR_L_DARK;
			affordable = false;
		}													
		text_out_c(attr, "%d Gra", current_cost.stat[STAT_GRA]);
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	if (current_cost.exp > 0) {
		if (player->new_exp >= current_cost.exp) {
			attr = COLOUR_SLATE;
		} else {
			attr = COLOUR_L_DARK;
			affordable = false;
		}													
		text_out_c(attr, "%d Exp", current_cost.exp);
		costs++;
		Term_gotoxy(COL_SMT4 + 2, ROW_SMT3 + costs);
	}
	attr = COLOUR_SLATE;
	text_out_c(attr, "%d Turns", MAX(10, dif * 10));

	/* Object description */
	clear_from(MAX_SMITHING_TVALS + 3);
	if (player->smithing_leftover) {
		Term_gotoxy(COL_SMT1, MAX_SMITHING_TVALS + 3);
		text_out_c(COLOUR_L_BLUE, "In progress:");
		Term_gotoxy(COL_SMT1 - 1, MAX_SMITHING_TVALS + 5);
		text_out_c(COLOUR_BLUE, "%3d turns left", player->smithing_leftover);
	}

	if (smith_obj->number > 1) {
		mode |= ODESC_PREFIX;
	}
	include_pval(smith_obj);
	object_desc(o_desc, sizeof(o_desc), smith_obj, mode, player);
	my_strcat(o_desc, format("   %d.%d lb",
							 smith_obj->weight * smith_obj->number / 10,
							 (smith_obj->weight * smith_obj->number) % 10),
			  sizeof(o_desc));
	tb = object_info(smith_obj, OINFO_SMITH);
	exclude_pval(smith_obj);
	textui_textblock_place(tb, bottom, o_desc);
	textblock_free(tb);

	/* Reset indent/wrap */
	text_out_indent = 0;
	text_out_wrap = 0;
}

/**
 * Show smithing object data
 */
static void smith_obj_browser(int oid, void *data, const region *loc)
{
	show_smith_obj();
}

/**
 * ------------------------------------------------------------------------
 * Base items menu
 * ------------------------------------------------------------------------ */
static struct object_kind *smithing_svals[20];

static int get_smithing_svals(int tval)
{
	int i, count = 0;
	for (i = 0; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];
		if (kind->tval != tval) continue;
		if (kf_has(kind->kind_flags, KF_INSTA_ART)) continue;
		if (of_has(kind->flags, OF_NO_SMITHING)) continue;
		smithing_svals[count++] = kind;
	}
	return count;
}

/**
 * Display the subtypes.
 */
static void sval_display(struct menu *menu, int oid, bool cursor, int row,
							   int col, int width)
{
	char name[40];
	struct object_kind **choice = (struct object_kind **) menu->menu_data;
	uint8_t attr = (cursor ? COLOUR_L_BLUE : COLOUR_WHITE);
	struct object object_body, object_known_body;
	struct object *obj = &object_body, *known_obj = &object_known_body;
	struct smithing_cost local_cost;
	struct smithing_cost *cost = &local_cost;
	if (cursor) {
		obj = smith_obj;
		cost = &current_cost;
	}
	create_base_object(choice[oid], obj);
	object_copy(known_obj, obj);
	obj->known = known_obj;
	if (cursor) {
		object_wipe(smith_obj_backup);
		object_copy(smith_obj_backup, smith_obj);
	}
	include_pval(obj);
	(void)object_difficulty(obj, cost);
	attr = smith_affordable(obj, cost) ? COLOUR_WHITE : COLOUR_SLATE;
	if (cursor) {
		show_smith_obj();
	}
	exclude_pval(obj);
	object_kind_name(name, sizeof(name), choice[oid], true);
	c_put_str(attr, name, row, col);
}


/**
 * Handle keypresses.
 */
static bool sval_action(struct menu *m, const ui_event *event, int oid)
{
	return (event->type == EVT_SELECT) ? false : true;
}

/**
 * Display an entry in the menu.
 */
static void tval_display(struct menu *menu, int oid, bool cursor, int row,
							int col, int width)
{
	const char *name = smithing_tvals[oid].desc;
	uint8_t attr = COLOUR_RED;

	if (((smithing_tvals[oid].category == SMITH_TYPE_WEAPON) &&
		 player_active_ability(player, "Weaponsmith")) ||
		((smithing_tvals[oid].category == SMITH_TYPE_JEWELRY) &&
		 player_active_ability(player, "Jeweller")) ||
		((smithing_tvals[oid].category == SMITH_TYPE_ARMOUR) &&
		 player_active_ability(player, "Armoursmith"))) {
		attr = COLOUR_WHITE;
	}

	c_put_str(attr, name, row, col);
}


/**
 * Handle keypresses.
 */
static bool tval_action(struct menu *m, const ui_event *event, int oid)
{
	struct menu menu;
	menu_iter menu_f = { NULL, NULL, sval_display, sval_action, NULL };
	region area = { COL_SMT3, ROW_SMT1, COL_SMT4 - COL_SMT3, MAX_SMITHING_TVALS };
	ui_event evt;
	int count = 0;
	bool selected = false;

	/* Save */
	screen_save();

	/* Work out how many options we have */
	count = get_smithing_svals(smithing_tvals[oid].tval);

	/* Run menu */
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.selections = lower_case;
	menu.flags = MN_CASELESS_TAGS;
	menu.browse_hook = smith_obj_browser;
	menu_setpriv(&menu, count, smithing_svals);
	menu_layout(&menu, &area);

	evt = menu_select(&menu, 0, true);

	/* Set the new value appropriately */
	if (evt.type == EVT_SELECT) {
		smith_obj->kind = smithing_svals[menu.cursor];
		pval = pval_valid(smith_obj) ? smith_obj->kind->pval : 0;
		selected = true;
	}
	menu_refresh(smithing_menu, false);

	/* Load and finish */
	screen_load();
	return !selected;
}

/**
 * Display tval menu.
 */
static void tval_menu(const char *name, int row)
{
	struct menu menu;
	menu_iter menu_f = { NULL, NULL, tval_display, tval_action, NULL };
	ui_event evt = EVENT_EMPTY;
	region area = { COL_SMT2, ROW_SMT1, COL_SMT3 - COL_SMT2, MAX_SMITHING_TVALS };
	region big = { COL_SMT2, ROW_SMT1, 0, 0 };

	/* Reset everything */
	wipe_smithing_objects();
	pval = 0;

	/* Set up the menu */
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.selections = lower_case;
	menu.flags = MN_CASELESS_TAGS;
	menu.browse_hook = smith_obj_browser;
	menu_setpriv(&menu, MAX_SMITHING_TVALS, (void *)smithing_tvals);
	region_erase(&big);
	numbers_changed = false;
	menu_layout(&menu, &area);

	/* Select an entry */
	while (evt.type != EVT_ESCAPE) {
		evt = menu_select(&menu, 0, false);
		if ((evt.type == EVT_SELECT) && smith_obj->kind)
			break;
	}

	return;
}

/**
 * ------------------------------------------------------------------------
 * Special items menu
 * ------------------------------------------------------------------------ */
struct ego_item **smithing_specials;
bool *affordable_specials;

static int get_smithing_specials(struct object_kind *kind)
{
	int i, count = 0, pval_old = pval;
	struct object dummy_body, dummy_body_known;
	struct smithing_cost dummy_cost;

	if (!kind) return 0;
	for (i = 0; i < z_info->e_max; i++) {
		struct ego_item *ego = &e_info[i];
		struct poss_item *poss;
		for (poss = ego->poss_items; poss; poss = poss->next) {
			if (kind->kidx == poss->kidx) break;
		}
		if (!poss) continue;
		smithing_specials[count] = ego;
		object_copy(&dummy_body, smith_obj);
		create_special(&dummy_body, ego);
		object_copy(&dummy_body_known, &dummy_body);
		dummy_body.known = &dummy_body_known;
		pval = pval_valid(&dummy_body) ? dummy_body.pval : 0;
		include_pval(&dummy_body);
		(void)object_difficulty(&dummy_body, &dummy_cost);
		affordable_specials[count] = smith_affordable(&dummy_body,
			&dummy_cost);
		exclude_pval(&dummy_body);
		object_wipe(&dummy_body_known);
		object_wipe(&dummy_body);
		++count;
	}
	pval = pval_old;
	return count;
}

/**
 * Display an entry in the menu.
 */
static void special_display(struct menu *menu, int oid, bool cursor, int row,
							int col, int width)
{
	struct ego_item **choice = (struct ego_item **) menu->menu_data;
	uint8_t attr = affordable_specials[oid] ? COLOUR_WHITE : COLOUR_SLATE;
	if (cursor) {
		struct object known_body;
		struct object *known_obj = &known_body;
		create_special(smith_obj, choice[oid]);
		object_copy(known_obj, smith_obj);
		smith_obj->known = known_obj;
		pval = pval_valid(smith_obj) ? smith_obj->pval : 0;
		include_pval(smith_obj);
		show_smith_obj();
		exclude_pval(smith_obj);
	}
	c_put_str(attr, strip_ego_name(choice[oid]->name), row, col);
}

/**
 * Handle keypresses.
 */
static bool special_action(struct menu *m, const ui_event *event, int oid)
{
	return (event->type == EVT_SELECT) ? false : true;
}


/**
 * Display special item menu.
 */
static void special_menu(const char *name, int row)
{
	struct object_kind *kind = smith_obj->kind;
	struct menu menu;
	menu_iter menu_f = { NULL, NULL, special_display, special_action, NULL };
	int count;
	region area = { COL_SMT2, ROW_SMT1, COL_SMT4 - COL_SMT2, MAX_SMITHING_TVALS };
	ui_event evt;

	/* Remove any artefact info */
	if (smith_obj->artifact) reset_smithing_objects(kind);

	/* Set up the menu */
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	smithing_specials = mem_zalloc(z_info->e_max * sizeof(smithing_specials));
	affordable_specials = mem_zalloc(z_info->e_max * sizeof(affordable_specials));
	count = get_smithing_specials(smith_obj->kind);
	if (!count) {
		mem_free(affordable_specials);
		mem_free(smithing_specials);
		return;
	}
	menu.selections = lower_case;
	menu.flags = MN_CASELESS_TAGS;
	menu_setpriv(&menu, count, smithing_specials);
	menu_layout(&menu, &area);

	/* Select an entry */
	evt = menu_select(&menu, 0, false);

	/* Set the new value appropriately */
	if (evt.type == EVT_SELECT)
		smith_obj->ego = smithing_specials[menu.cursor];

	menu_refresh(&menu, false);
	mem_free(affordable_specials);
	mem_free(smithing_specials);

	return;
}

/**
 * ------------------------------------------------------------------------
 * Artefact menu
 * ------------------------------------------------------------------------ */
#define MAX_LEN_ART_NAME 30

static const char *smithing_art_cats[] =
{
	"Stat bonuses",
	"Sustains",
	"Skill bonuses",
	"Melee powers",
	"Slays",
	"Resistances",
	"Curses",
	"Misc",
	#define SKILL(a, b) b,
	#include "list-skills.h"
	#undef SKILL
	"Name Artefact"
};
static int *smithing_art_cat_counts = NULL;

struct property_info {
	struct obj_property *prop;
	bool negative;
};

static struct property_info *smith_art_properties;
static struct ability **smith_art_abilities;
static bool negative;

static int get_smith_properties(enum smithing_category cat)
{
	int i, count = 0;
	for (i = 0; i < z_info->property_max; i++) {
		struct obj_property *prop = &obj_properties[i];
		if (prop->smith_cat != (int) cat) continue;
		if (!applicable_property(prop, smith_obj)) continue;
		negative = false;
		smith_art_properties[count].prop = prop;
		smith_art_properties[count++].negative = false;
		if (cat == SMITH_CAT_STAT) {
			smith_art_properties[count].prop = prop;
			smith_art_properties[count++].negative = true;
		}
	}
	return count;
}


static int get_smith_art_abilities(int skill)
{
	struct ability *a;
	int count = 0;
	for (a = abilities; a; a = a->next) {
		struct poss_item *poss = a->poss_items;
		if (a->skill != skill) continue;
		while (poss) {
			if (poss->kidx == smith_obj->kind->kidx) break;
			poss = poss->next;
		}
		if (!poss) continue;
		smith_art_abilities[count++] = a;
	}
	return count;
}

/**
 * Allows the player to choose a new name for an artefact.
 */
static void rename_artefact(void)
{
	char tmp[20];
	char o_desc[30];
	bool name_selected = false;

	/* Clear the name */
	tmp[0] = '\0';

	/* Use old name as a default */
	my_strcpy(tmp, smith_art->name, sizeof(tmp));
	my_strcpy(smith_art_name, "", sizeof(smith_art_name));

	/* Determine object name */
	object_desc(o_desc, sizeof(o_desc), smith_obj, ODESC_BASE, player);

	/* Display shortened object name */
	Term_putstr(COL_SMT2, MAX_SMITHING_TVALS + 3, -1, COLOUR_L_WHITE, o_desc);

	/* Prompt for a new name */
	Term_gotoxy(COL_SMT2 + strlen(o_desc), MAX_SMITHING_TVALS + 3);

	while (!name_selected) {
		if (askfor_aux(tmp, sizeof(tmp), NULL)) {
			my_strcpy(smith_art->name, tmp, MAX_LEN_ART_NAME);
			player->upkeep->redraw |= (PR_MISC);
		} else {
			strnfmt(smith_art_name, sizeof(smith_art_name), "of %s",
				player->full_name);
			return;
		}

		if (tmp[0] != '\0')	{
			name_selected = true;
		} else {
			strnfmt(smith_art_name, sizeof(smith_art_name), "of %s",
				player->full_name);
		}
	}
}

/**
 * Display an entry in the menu.
 */
static void skill_display(struct menu *menu, int oid, bool cursor, int row,
						 int col, int width)
{
	struct ability **choice = menu->menu_data;
	bool chosen = !!locate_ability(smith_obj->abilities, choice[oid]);
	uint8_t attr = chosen ? COLOUR_BLUE : COLOUR_SLATE;
	struct object backup;
	if (!applicable_ability(choice[oid], smith_obj)) {
		attr = COLOUR_L_DARK;
	} else {
		include_pval(smith_obj);
		object_copy(&backup, smith_obj);
		if (!chosen) {
			add_ability(&smith_obj->abilities, choice[oid]);
			if (smith_affordable(smith_obj, &current_cost) && cursor) {
				attr = COLOUR_BLUE;
			}
		}
		if (!chosen) {
			remove_ability(&smith_obj->abilities, choice[oid]);
		}
		object_wipe(smith_obj);
		object_copy(smith_obj, &backup);
		(void) smith_affordable(smith_obj, &current_cost);
		exclude_pval(smith_obj);
	}
	c_put_str(attr, choice[oid]->name, row, col);	
}

/**
 * Handle keypresses.
 */
static bool skill_action(struct menu *m, const ui_event *event, int oid)
{
	struct ability **choice = m->menu_data;
	if (event->type == EVT_SELECT) {
		if (!applicable_ability(choice[oid], smith_obj)) return false;
		if (!locate_ability(smith_obj->abilities, choice[oid])) {
			add_ability(&smith_obj->abilities, choice[oid]);
		} else {
			remove_ability(&smith_obj->abilities, choice[oid]);
		}
		return true;
	}
	return false;
}

/**
 * Display an entry in the menu.
 */
static void prop_display(struct menu *menu, int oid, bool cursor, int row,
						 int col, int width)
{
	struct property_info *choice = menu->menu_data;
	bool chosen = object_has_property(choice[oid].prop, smith_obj, false);
	uint8_t attr;
	char *name;

	if (choice[oid].prop->smith_cat == SMITH_CAT_STAT) {
		if (choice[oid].negative) {
			chosen = object_has_property(choice[oid].prop, smith_obj, true);
			name = format("%s penalty", choice[oid].prop->name);
		} else {
			name = format("%s bonus", choice[oid].prop->name);
		}
	} else {
		name = choice[oid].prop->name;
	}
	attr = chosen ? COLOUR_BLUE : COLOUR_SLATE;
	c_put_str(attr, name, row, col);
}

/**
 * Handle keypresses.
 */
static bool prop_action(struct menu *m, const ui_event *event, int oid)
{
	struct property_info *choice = m->menu_data;
	if (event->type == EVT_SELECT) {
		if (!object_has_property(choice[oid].prop, smith_obj,
								 choice[oid].negative)) {
			add_object_property(choice[oid].prop, smith_obj,
								  choice[oid].negative);
		} else {
			remove_object_property(choice[oid].prop, smith_obj);
		}
		return true;
	}
	return false;
}

/**
 * Display an entry in the menu.
 */
static void artefact_display(struct menu *menu, int oid, bool cursor, int row,
							int col, int width)
{
	char **choice = menu->menu_data;
	uint8_t attr;

	assert(oid >= 0 && oid < SMITH_CAT_MAX + SKILL_MAX + 1
		&& smithing_art_cat_counts);
	attr = smithing_art_cat_counts[oid] > 0 ?
		(cursor ? COLOUR_L_BLUE : COLOUR_WHITE) : COLOUR_L_DARK;
	if (cursor) {
		struct object known_body;
		struct object *known_obj = &known_body;
		object_copy(known_obj, smith_obj);
		smith_obj->known = known_obj;
		include_pval(smith_obj);
		show_smith_obj();
		exclude_pval(smith_obj);
	}
	c_put_str(attr, choice[oid], row, col);
}

/**
 * Display artefact menu.
 */
static bool artefact_action(struct menu *m, const ui_event *event, int oid)
{
	struct menu menu;
	region area = { COL_SMT3, ROW_SMT1, COL_SMT4 - COL_SMT3, MAX_SMITHING_TVALS };
	int count;

	if (event->type == EVT_SELECT) {
		/* Get the different data types and run the appropriate menu */
		if (oid < SMITH_CAT_MAX) {
			menu_iter menu_f = { NULL, NULL, prop_display, prop_action, NULL };
			menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
			smith_art_properties = mem_zalloc(z_info->property_max
				* sizeof(struct property_info));
			count = get_smith_properties(oid);
			if (!count) {
				mem_free(smith_art_properties);
				smith_art_properties = NULL;
				return true;
			}
			menu.selections = lower_case;
			menu.flags = MN_CASELESS_TAGS;
			menu_setpriv(&menu, count, smith_art_properties);
			menu_layout(&menu, &area);
			menu_select(&menu, 0, true);
			mem_free(smith_art_properties);
			smith_art_properties = NULL;
		} else if (oid < SMITH_CAT_MAX + SKILL_MAX) {
			menu_iter menu_f = { NULL, NULL, skill_display, skill_action, NULL};
			menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
			smith_art_abilities =
				mem_zalloc(100 * sizeof(struct ability*));
			count = get_smith_art_abilities(oid - SMITH_CAT_MAX);
			if (!count) {
				mem_free(smith_art_abilities);
				smith_art_abilities = NULL;
				return true;
			}
			menu.flags = MN_CASELESS_TAGS;
			menu.selections = lower_case;
			menu_setpriv(&menu, count, smith_art_abilities);
			menu_layout(&menu, &area);
			menu_select(&menu, 0, true);
			mem_free(smith_art_abilities);
			smith_art_abilities = NULL;
		} else {
			rename_artefact();
		}
	}

	return true;
}

/**
 * Display artefact menu.
 */
static void artefact_menu(const char *name, int row)
{
	struct object_kind *kind;
	struct menu menu;
	menu_iter menu_f = { NULL, NULL, artefact_display, artefact_action, NULL };
	region area = { COL_SMT2, ROW_SMT1, COL_SMT4 - COL_SMT2, MAX_SMITHING_TVALS };
	int i;

	if (!smith_obj->kind) return;
	/*
	 * Some types of objects use a special base for all smithed artefacts.
	 * All others use the base item already selected.
	 */
	kind = lookup_selfmade_kind(smith_obj->kind->tval);
	if (!kind) {
		kind = smith_obj->kind;
	}

	/* Mark as an artefact, remove any special item info */
	if (smith_obj->ego || kind != smith_obj->kind) {
		reset_smithing_objects(kind);
	}
	smith_obj->artifact = smith_art;

	strnfmt(smith_art_name, sizeof(smith_art_name), "of %s",
		player->full_name);
	smith_art->name = smith_art_name;

	/*
	 * So the category entries can be colored appropriately, remember
	 * what categories have applicable entries for this type of object.
	 */
	smithing_art_cat_counts = mem_alloc((SMITH_CAT_MAX + SKILL_MAX + 1)
		* sizeof(smithing_art_cat_counts));
	for (i = 0; i < SMITH_CAT_MAX; ++i) {
		smith_art_properties = mem_zalloc(z_info->property_max
			* sizeof(*smith_art_properties));
		smithing_art_cat_counts[i] = get_smith_properties(i);
		mem_free(smith_art_properties);
		smith_art_properties = NULL;
	}
	for (i = SMITH_CAT_MAX; i < SMITH_CAT_MAX + SKILL_MAX; ++i) {
		smith_art_abilities =
			mem_zalloc(100 * sizeof(*smith_art_abilities));
		smithing_art_cat_counts[i] =
			get_smith_art_abilities(i - SMITH_CAT_MAX);
		mem_free(smith_art_abilities);
		smith_art_abilities = NULL;
	}
	/* Renaming is always possible. */
	smithing_art_cat_counts[SMITH_CAT_MAX + SKILL_MAX] = 1;

	/* Set up the menu */
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.selections = lower_case;
	menu.flags = MN_CASELESS_TAGS;
	menu.browse_hook = smith_obj_browser;
	menu_setpriv(&menu, SMITH_CAT_MAX + SKILL_MAX + 1, smithing_art_cats);
	menu_layout(&menu, &area);

	/* Select an entry */
	menu_select(&menu, 0, false);

	mem_free(smithing_art_cat_counts);
	smithing_art_cat_counts = NULL;
}

/**
 * ------------------------------------------------------------------------
 * Mithril menu
 * ------------------------------------------------------------------------ */
/**
 * Display an entry in the menu.
 */
static void melt_display(struct menu *menu, int oid, bool cursor, int row,
							int col, int width)
{
	struct object **choice = menu->menu_data;
	char o_name[80];
	uint8_t attr = (cursor ? COLOUR_L_BLUE : COLOUR_WHITE);
	object_desc(o_name, sizeof(o_name), choice[oid], ODESC_PREFIX | ODESC_FULL,
				player);
	c_put_str(attr, o_name, row, col);
}

/**
 * Handle keypresses.
 */
static bool melt_action(struct menu *m, const ui_event *event, int oid)
{
	struct object **choice = m->menu_data;
	if (event->type == EVT_SELECT) {
		melt_mithril_item(player, choice[oid]);
	}
	return false;
}

/**
 * Display mithril melting menu.
 */
static void melt_menu(const char *name, int row)
{
	struct menu menu;
	menu_iter menu_f = { NULL, NULL, melt_display, melt_action, NULL };
	int count = 0;
	region area = { COL_SMT2, ROW_SMT1, 0, 0 };
	struct object *obj;
	struct object **melt_menu_info = NULL;

	/* Fill the melt menu */
	for (obj = player->gear; obj; obj = obj->next) {
		if (object_is_mithril(obj)) {
			if (count) {
				melt_menu_info = mem_realloc(melt_menu_info,
											 count * sizeof(struct object*));
			} else {
				melt_menu_info = mem_zalloc(sizeof(struct object*));
			}
			melt_menu_info[count++] = obj;
		}
	}

	/* Set up the menu */
	if (!count) return;
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.selections = lower_case;
	menu.flags = MN_CASELESS_TAGS;
	menu.browse_hook = smith_obj_browser;
	menu_setpriv(&menu, count, melt_menu_info);
	menu_layout(&menu, &area);

	/* Select an entry */
	menu_select(&menu, 0, false);

	return;
}

/**
 * ------------------------------------------------------------------------
 * Numbers menu
 * ------------------------------------------------------------------------ */
struct numbers_menu_entry {
	enum smithing_numbers_mod_index index;
	const char *name;
};

struct numbers_menu_entry numbers_menu_info[SMITH_NUM_MAX] =
{
	{ SMITH_NUM_INC_ATT,	"increase attack bonus" },
	{ SMITH_NUM_DEC_ATT,	"decrease attack bonus" },
	{ SMITH_NUM_INC_DS,		"increase damage sides" },
	{ SMITH_NUM_DEC_DS,		"decrease damage sides" },
	{ SMITH_NUM_INC_EVN,	"increase evasion bonus" },
	{ SMITH_NUM_DEC_EVN,	"decrease evasion bonus" },
	{ SMITH_NUM_INC_PS,		"increase protection sides" },
	{ SMITH_NUM_DEC_PS,		"decrease protection sides" },
	{ SMITH_NUM_INC_PVAL,	"increase special bonus" },
	{ SMITH_NUM_DEC_PVAL,	"decrease special bonus" },
	{ SMITH_NUM_INC_WGT,	"increase weight" },
	{ SMITH_NUM_DEC_WGT,	"decrease weight" }
};

/**
 * Validity and affordability for numbers changes
 * Note that numbers_can_afford[] and numbers_need_artistry[] should only be
 * used when numbers_valid[] is true
 */
bool numbers_valid[SMITH_NUM_MAX];
bool numbers_can_afford[SMITH_NUM_MAX] = { false };
bool numbers_needs_artistry[SMITH_NUM_MAX] = { false };

/**
 * Set validity status for each of the numbers changes
 */
static void numbers_set_validity(void)
{
	struct object backup;
	int old_pval = pval;
	int i;

	/* Attack */
	numbers_valid[SMITH_NUM_INC_ATT] = att_valid(smith_obj) &&
		(smith_obj->att < att_max(smith_obj, true));
	numbers_needs_artistry[SMITH_NUM_INC_ATT] = att_valid(smith_obj) &&
		!(smith_obj->att < att_max(smith_obj, false));
	numbers_valid[SMITH_NUM_DEC_ATT] = att_valid(smith_obj) &&
		(smith_obj->att > att_min(smith_obj));

	/* Damage sides */
	numbers_valid[SMITH_NUM_INC_DS] = ds_valid(smith_obj) &&
		(smith_obj->ds < ds_max(smith_obj, true));
	numbers_needs_artistry[SMITH_NUM_INC_DS] = ds_valid(smith_obj) &&
		!(smith_obj->ds < ds_max(smith_obj, false));
	numbers_valid[SMITH_NUM_DEC_DS] = ds_valid(smith_obj) &&
		(smith_obj->ds > ds_min(smith_obj));

	/* Evasion */
	numbers_valid[SMITH_NUM_INC_EVN] = evn_valid(smith_obj) &&
		(smith_obj->evn < evn_max(smith_obj, true));
	numbers_needs_artistry[SMITH_NUM_INC_EVN] = evn_valid(smith_obj) &&
		!(smith_obj->evn < evn_max(smith_obj, false));
	numbers_valid[SMITH_NUM_DEC_EVN] = evn_valid(smith_obj) &&
		(smith_obj->evn > evn_min(smith_obj));

	/* Protection sides */
	numbers_valid[SMITH_NUM_INC_PS] = ps_valid(smith_obj) &&
		(smith_obj->ps < ps_max(smith_obj, true));
	numbers_needs_artistry[SMITH_NUM_INC_PS] = ps_valid(smith_obj) &&
		!(smith_obj->ps < ps_max(smith_obj, false));
	numbers_valid[SMITH_NUM_DEC_PS] = ps_valid(smith_obj) &&
		(smith_obj->ps > ps_min(smith_obj));

	/* Special bonus */
	numbers_valid[SMITH_NUM_INC_PVAL] = pval_valid(smith_obj) &&
		(pval < pval_max(smith_obj));
	numbers_valid[SMITH_NUM_DEC_PVAL] = pval_valid(smith_obj) &&
		(pval > pval_min(smith_obj));

	/* Weight */
	numbers_valid[SMITH_NUM_INC_WGT] = wgt_valid(smith_obj) &&
		(smith_obj->weight < wgt_max(smith_obj));
	numbers_valid[SMITH_NUM_DEC_WGT] = wgt_valid(smith_obj) &&
		(smith_obj->weight > wgt_min(smith_obj));

	/* Affordability */
	for (i = 0; i < SMITH_NUM_MAX; i++) {
		if (numbers_valid[i]) {
			/* Back up the object */
			object_copy(&backup, smith_obj);

			/* See if we can afford the change */
			modify_numbers(smith_obj, i, &pval);
			include_pval(smith_obj);
			numbers_can_afford[i] = smith_affordable(smith_obj, &current_cost);

			/* Restore the object */
			pval = old_pval;
			exclude_pval(smith_obj);
			object_wipe(smith_obj);
			object_copy(smith_obj, &backup);
		}
	}
}

/**
 * Display an entry in the menu.
 */
static void numbers_display(struct menu *menu, int oid, bool cursor, int row,
							int col, int width)
{
	struct numbers_menu_entry *choice = menu->menu_data;
	uint8_t attr = numbers_valid[oid] ? COLOUR_SLATE : COLOUR_L_DARK;
	if (numbers_valid[oid] && numbers_can_afford[oid]) attr = COLOUR_WHITE;
	if (numbers_valid[oid] && numbers_needs_artistry[oid]) attr = COLOUR_RED;
	show_smith_obj();
	c_put_str(attr, choice[oid].name, row, col);
}


/**
 * Handle keypresses.
 */
static bool numbers_action(struct menu *m, const ui_event *event, int oid)
{
	if (event->type == EVT_SELECT) {
		if (numbers_valid[oid]) {
			modify_numbers(smith_obj, oid, &pval);
			numbers_changed = true;
			/* Update whan can be changed. */
			numbers_set_validity();
			menu_refresh(m, false);
		}
	}
	return false;
}

/**
 * Display numbers menu.
 */
static void numbers_menu(const char *name, int row)
{
	struct menu menu;
	menu_iter menu_f = { NULL, NULL, numbers_display, numbers_action, NULL };
	ui_event evt = EVENT_EMPTY;
	region area = { COL_SMT2, ROW_SMT1, COL_SMT3 - COL_SMT2, MAX_SMITHING_TVALS };
	region old = { COL_SMT2, ROW_SMT1, COL_SMT4 - COL_SMT2,
					MAX_SMITHING_TVALS  };

	if (!smith_obj->kind) return;

	/* Set validity status */
	numbers_set_validity();

	/* Set up the menu */
	region_erase(&old);
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.selections = lower_case;
	menu.flags = MN_CASELESS_TAGS;
	menu_setpriv(&menu, SMITH_NUM_MAX, numbers_menu_info);
	menu_layout(&menu, &area);

	/* Select an entry */
	while (evt.type != EVT_ESCAPE) {
		evt = menu_select(&menu, 0, false);
	}

	return;
}

static void accept_item(const char *name, int row)
{
	include_pval(smith_obj);
	if (!smith_affordable(smith_obj, &current_cost) ||
		!square_isforge(cave, player->grid) ||
		!square_forge_uses(cave, player->grid)) {
		exclude_pval(smith_obj);
		return;
	}
	exclude_pval(smith_obj);
	if (current_cost.drain > 0) {
		char buf[80];

		strnfmt(buf, sizeof(buf), "This will drain your smithing "
			"skill by %d points. Proceed? ", current_cost.drain);
		if (!get_check(buf)) return;
	}
	create_smithed_item = true;

	/* Add the details to the artefact type if applicable */
	include_pval(smith_obj);
	if (smith_obj->artifact) add_artefact_details(smith_art, smith_obj);
	exclude_pval(smith_obj);
}

/**
 * ------------------------------------------------------------------------
 * Main smithing menu functions
 * ------------------------------------------------------------------------ */
static menu_action smithing_actions[] = 
{
	{ 0, 'a', "Base Item", tval_menu },
	{ 0, 'b', "Enchant", special_menu },
	{ 0, 'c', "Artifice", artefact_menu },
	{ 0, 'd', "Numbers", numbers_menu },
	{ 0, 'e', "Melt", melt_menu },
	{ 0, 'f', "Accept", accept_item },
};

/**
 * Show smithing object data
 */
static void smithing_menu_browser(int oid, void *data, const region *loc)
{
	uint8_t attr = COLOUR_SLATE;
	const char *desc[] = { "Start with a new base item.               ",
						   "                                          ",
						   "Choose a special enchantment to add to the",
						   "base item. (not compatible with Artifice) ",
						   "Design your own artefact.                 ",
						   "(not compatible with Enchant)             ",
						   "Change the item's key numbers.            ",
						   "                                          ",
						   "Choose a mithril item to melt down.       ",
						   "                                          ",
						   "Create the item you have designed.        ",
						   "(to cancel it instead, just press Escape) "
	};
	const char *extra[] = { "(Enchantment cannot be changed after     ",
							"using the Numbers menu)                  ",
							"This forge has no resources, so you cannot",
							"create items. To exit, press Escape.     ",
							"You are not at a forge and thus cannot   ",
							"create items. To exit, press Escape.     "
	};
	region area = { COL_SMT2, ROW_SMT1, COL_SMT4 - COL_SMT2,
					MAX_SMITHING_TVALS + 2 };

	/* Redirect output to the screen */
	text_out_hook = text_out_to_screen;
	text_out_wrap = COL_SMT4;

	/* Object difficulty */
	text_out_indent = COL_SMT2;

	region_erase(&area);
	Term_gotoxy(COL_SMT2, ROW_SMT1);
	if (no_forge && (oid == 5)) {
		text_out_c(attr, "%s", extra[4]);
	} else if (exhausted && (oid == 5)) {
		text_out_c(attr, "%s", extra[2]);
	} else if (numbers_changed && (oid == 1)) {
		text_out_c(attr, "%s", extra[0]);
	} else {
		text_out_c(attr, "%s", desc[oid * 2]);
	}
	Term_gotoxy(COL_SMT2, ROW_SMT1 + 1);
	if (no_forge && (oid == 5)) {
		text_out_c(attr, "%s", extra[5]);
	} else if (exhausted && (oid == 5)) {
		text_out_c(attr, "%s", extra[3]);
	} else if (numbers_changed && (oid == 1)) {
		text_out_c(attr, "%s", extra[1]);
	} else {
		text_out_c(attr, "%s", desc[oid * 2 + 1]);
	}
	if (smith_obj->kind) {
		show_smith_obj();
	}
}

static void check_smithing_menu_row_colors(void)
{
	size_t i;

	/* Recognise which actions are valid, and which need a new ability */
	for (i = 0; i < N_ELEMENTS(smithing_actions); i++) {
		if (i == 0) {
			if (player_active_ability(player, "Weaponsmith") ||
				player_active_ability(player, "Armoursmith") ||
				player_active_ability(player, "Jeweller")) {
				smithing_actions[i].flags = 0;
			} else {
				smithing_actions[i].flags = MN_ACT_MAYBE;
			}
		}
		if (i == 1) {
			if (!smith_obj->kind || smith_obj->artifact || numbers_changed ||
				tval_is_jewelry(smith_obj) || tval_is_horn(smith_obj) ||
				strstr(smith_obj->kind->name, "Shovel")) {
				smithing_actions[i].flags = MN_ACT_GRAYED;
			} else if (player_active_ability(player, "Enchantment")) {
				smithing_actions[i].flags = 0;
			} else {
				smithing_actions[i].flags = MN_ACT_MAYBE;
			}
		}
		if (i == 2) {
			if (!smith_obj->kind || smith_obj->ego || tval_is_horn(smith_obj) ||
				(player->self_made_arts >= z_info->self_arts_max)) {
				smithing_actions[i].flags = MN_ACT_GRAYED;
			} else if (player_active_ability(player, "Artifice")) {
				smithing_actions[i].flags = 0;
			} else {
				smithing_actions[i].flags = MN_ACT_MAYBE;
			}
		}
		if (i == 3) {
			if (!smith_obj->kind) {
				smithing_actions[i].flags = MN_ACT_GRAYED;
			} else {
				smithing_actions[i].flags = 0;
			}
		}
		if (i == 4) {
			if (!mithril_items_carried(player) ||
					!square_isforge(cave, player->grid) ||
					!square_forge_uses(cave, player->grid)) {
				smithing_actions[i].flags = MN_ACT_GRAYED;
			} else {
				smithing_actions[i].flags = 0;
			}
		}
		if (i == 5) {
			include_pval(smith_obj);
			if (!smith_obj->kind ||
				!smith_affordable(smith_obj, &current_cost) ||
				!square_isforge(cave, player->grid) ||
				!square_forge_uses(cave, player->grid)) {
				smithing_actions[i].flags = MN_ACT_GRAYED;
			} else {
				smithing_actions[i].flags = 0;
			}
			exclude_pval(smith_obj);
		}
	}
}

/**
 * Display the smithing main menu.
 */
struct object *textui_smith_object(struct smithing_cost *cost)
{
	region area = {COL_SMT1, ROW_SMT1, COL_SMT2 - COL_SMT1,
				   ROW_SMT2 - ROW_SMT1};
	/* Deal with previous interruptions */
	if (player->smithing_leftover > 0) {
		/* Add the cost */
		*cost = current_cost;

		/* Return the smithing item */
		return smith_obj;
	} else {
		/* Otherwise wipe the smithing item and artefact */
		wipe_smithing_objects();
	}

	screen_save();
	clear_from(0);

	/* Main smithing menu */
	create_smithed_item = false;
	smithing_menu = menu_new_action(smithing_actions,
									N_ELEMENTS(smithing_actions));

	/* Prepare some menu details */
	check_smithing_menu_row_colors();
	if (!square_isforge(cave, player->grid)) {
		no_forge = true;
		exhausted = false;
		prt("Exploration mode:  Smithing requires a forge.", 0, 0);
	} else if (!square_forge_uses(cave, player->grid)) {
		no_forge = false;
		exhausted = true;
		prt("Exploration mode:  Smithing requires a forge with resources left.", 0, 0);
	} else {
		no_forge = false;
		exhausted = false;
	}

	smithing_menu->flags = MN_CASELESS_TAGS;
	smithing_menu->browse_hook = smithing_menu_browser;
	menu_layout(smithing_menu, &area);
	while (!create_smithed_item) {
		ui_event evt = EVENT_EMPTY;
		check_smithing_menu_row_colors();

		evt = menu_select(smithing_menu, EVT_KBRD, false);
		if (evt.type == EVT_ESCAPE) {
			/* Wipe the smithing object and artefact */
			wipe_smithing_objects();
			create_smithed_item = false;

			break;
		}
	}

	menu_free(smithing_menu);
	screen_load();

	include_pval(smith_obj);
	*cost = current_cost;
	return create_smithed_item ? smith_obj : NULL;
}

