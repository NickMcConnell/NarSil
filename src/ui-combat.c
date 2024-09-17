/**
 * \file ui-combat.c
 * \brief Printing of combat roll information
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
#include "combat.h"
#include "game-event.h"
#include "monster.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player.h"
#include "project.h"
#include "source.h"
#include "trap.h"
#include "ui-combat.h"
#include "ui-term.h"

static int combat_number = 0;
static int combat_number_old = 0;
static int turns_since_combat = 0;
struct combat_roll combat_rolls[2][MAX_COMBAT_ROLLS] = { { { 0 }, { 0 } } };

/**
 * Start a new combat round
 */
void new_combat_round(game_event_type type, game_event_data *data, void *user)
{
	int i;

	if (combat_number != 0) combat_number_old = combat_number;
	combat_number = 0;
	turns_since_combat++;

	if (turns_since_combat == 1) {
		/* Copy previous round's rolls into old round's rolls */
		for (i = 0; i < MAX_COMBAT_ROLLS; i++) {
			memcpy(&combat_rolls[1][i], &combat_rolls[0][i],
				   sizeof(struct combat_roll));
		}
	} else if (turns_since_combat == 11) {
		/* Reset old round's rolls */
		combat_number_old = 0;
		for (i = 0; i < MAX_COMBAT_ROLLS; i++) {
			combat_rolls[1][i].att_type = COMBAT_ROLL_NONE;
		}
	}

	/* Reset new round's rolls */
	for (i = 0; i < MAX_COMBAT_ROLLS; i++) {
		combat_rolls[0][i].att_type = COMBAT_ROLL_NONE;
	}

	/* Redraw */
	player->upkeep->redraw |= (PR_COMBAT);
}

/**
 * Update combat roll table part 1 (the attack rolls)
 *
 * If melee is false, there is no roll made -- eg breath attack
 */
void update_combat_rolls_attack(game_event_type type, game_event_data *data,
								void *user)
{
	struct monster *mon;
	struct monster_race *race1 = NULL;
	struct monster_race *race2 = NULL;
	struct source attacker = data->combat_attack.attacker;
	struct source defender = data->combat_attack.defender;
	bool vis = data->combat_attack.vis;
	int att = data->combat_attack.att;
	int att_roll = data->combat_attack.att_roll;
	int evn = data->combat_attack.evn;
	int evn_roll = data->combat_attack.evn_roll;
	bool melee = data->combat_attack.melee;

	switch (attacker.what) {
		case SRC_PLAYER: race1 = &r_info[0]; break;
		case SRC_MONSTER: {
			mon = cave_monster(cave, attacker.which.monster);
			if (player->timed[TMD_IMAGE]) {
				race1 = mon->image_race;
			} else {
				race1 = mon->race;
			}
			break;
		}
		case SRC_TRAP: break;
		default: break;
	}

	switch (defender.what) {
		case SRC_PLAYER: race2 = &r_info[0]; break;
		case SRC_MONSTER: {
			mon = cave_monster(cave, defender.which.monster);
			if (player->timed[TMD_IMAGE]) {
				race2 = mon->image_race;
			} else {
				race2 = mon->race;
			}
			break;
		}
		default: break;
	}

	if (combat_number < MAX_COMBAT_ROLLS) {
		combat_rolls[0][combat_number].att_type =
			melee ? COMBAT_ROLL_ROLL : COMBAT_ROLL_AUTO;
		
		if (attacker.what == SRC_GRID) {
			combat_rolls[0][combat_number].attacker_char =
				square_feat(cave, player->grid)->d_char;
			combat_rolls[0][combat_number].attacker_attr =
				square_feat(cave, player->grid)->d_attr;
		} else if (attacker.what == SRC_TRAP) {
			combat_rolls[0][combat_number].attacker_char =
				attacker.which.trap->kind->d_char;
			combat_rolls[0][combat_number].attacker_attr =
				attacker.which.trap->kind->d_attr;
		} else if ((vis && (attacker.what == SRC_MONSTER))
				|| (attacker.what == SRC_PLAYER)) {
			combat_rolls[0][combat_number].attacker_char = race1->d_char;

			if (player->timed[TMD_RAGE] && (attacker.what != SRC_PLAYER)) {
				combat_rolls[0][combat_number].attacker_attr = COLOUR_RED;
			} else {
				combat_rolls[0][combat_number].attacker_attr = race1->d_attr;
			}
		} else {
			combat_rolls[0][combat_number].attacker_char = '?';
			combat_rolls[0][combat_number].attacker_attr = COLOUR_SLATE;
		}

		if ((defender.what == SRC_NONE) && melee) {
			/* Hack for Iron Crown */
			combat_rolls[0][combat_number].defender_char = ']';
			combat_rolls[0][combat_number].defender_attr = COLOUR_L_DARK;
		} else if ((vis && (defender.what == SRC_MONSTER))
				|| (defender.what == SRC_PLAYER)) {
			combat_rolls[0][combat_number].defender_char = race2->d_char;
			
			if (player->timed[TMD_RAGE] && (defender.what != SRC_PLAYER)) {
				combat_rolls[0][combat_number].defender_attr = COLOUR_RED;
			} else {
				combat_rolls[0][combat_number].defender_attr = race2->d_attr;
			}
		} else {
			combat_rolls[0][combat_number].defender_char = '?';
			combat_rolls[0][combat_number].defender_attr = COLOUR_SLATE;
		}

		if (melee) {
			combat_rolls[0][combat_number].att = att;
			combat_rolls[0][combat_number].att_roll = att_roll;
			combat_rolls[0][combat_number].evn = evn;
			combat_rolls[0][combat_number].evn_roll = evn_roll;
		}

		combat_number++;
		turns_since_combat = 0;
	}

	/* Redraw */
	player->upkeep->redraw |= (PR_COMBAT);
}


/**
 * Update combat roll table part 2 (the damage rolls)
 */
void update_combat_rolls_damage(game_event_type type, game_event_data *data,
								void *user)
{
	int dd = data->combat_damage.dd;
	int ds = data->combat_damage.ds;
	int dam = data->combat_damage.dam;
	int pd = data->combat_damage.pd;
	int ps = data->combat_damage.ps;
	int prot = data->combat_damage.prot;
	int prt_percent = data->combat_damage.prt_percent;
	int dam_type = data->combat_damage.dam_type;
	bool melee = data->combat_damage.melee;

	if (combat_number - 1 < MAX_COMBAT_ROLLS) {
		combat_rolls[0][combat_number - 1].dam_type = dam_type;
		combat_rolls[0][combat_number - 1].dd = dd;
		combat_rolls[0][combat_number - 1].ds = ds;
		combat_rolls[0][combat_number - 1].dam = dam;
		combat_rolls[0][combat_number - 1].pd = pd;
		combat_rolls[0][combat_number - 1].ps = ps;
		combat_rolls[0][combat_number - 1].prot = prot;
		combat_rolls[0][combat_number - 1].prt_percent = prt_percent;
		combat_rolls[0][combat_number - 1].melee = melee;
		
		/* deal with protection for the player */
		/* this hackishly uses the pd and ps to store the min and max prot for
		 * the player */
		if (pd == -1) {
			/* use the protection values for pure elemental types if there was
			 * no attack roll */
			if (combat_rolls[0][combat_number - 1].att_type
				== COMBAT_ROLL_AUTO) {
				combat_rolls[0][combat_number - 1].pd =
					protection_roll(player, dam_type, melee, MINIMISE);
				combat_rolls[0][combat_number - 1].ps =
					protection_roll(player, dam_type, melee, MAXIMISE);
			} else {
				/* otherwise use the normal protection values  */
				combat_rolls[0][combat_number - 1].pd =
					protection_roll(player, PROJ_HURT, melee, MINIMISE);
				combat_rolls[0][combat_number - 1].ps =
					protection_roll(player, PROJ_HURT, melee, MAXIMISE);
			}
		}
	}

	/* Redraw */
	player->upkeep->redraw |= (PR_COMBAT);
}

/**
 * Display combat rolls in a window
 */
void display_combat_rolls(game_event_type type, game_event_data *data, void *user)
{
	/* all the update_combat_rolls*() stuff */
	int i;
	int line = 0;
	char buf[80];

	int net_att = 0;   /* a default value (required) */
	int net_dam;

	int a_att;
	int a_evn;
	int a_hit;
	int a_dam_roll;
	int a_prot_roll;
	int a_net_dam;

	int round;
	int combat_num_for_round = combat_number;

	int total_player_attacks = 0;
	int player_attacks = 0;
	int monster_attacks = 0;

	int line_jump = 0;

	int res = 1;   /* a default value to soothe compilation warnings */

	/* Clear the window */
	for (i = 0; i < Term->hgt; i++) {
		/* Erase the line */
		Term_erase(0, i, 255);
	}

	for (round = 0; round < 2; round++) {
		/* initialise some things */
		if (round == 1) {
			combat_num_for_round = combat_number_old;
			line_jump = player_attacks + monster_attacks + 2;
			if (player_attacks > 0) line_jump++;
			if (monster_attacks > 0) line_jump++;
			if (combat_number + combat_number_old > 0) {
				Term_putstr(0, line_jump - 1, 80, COLOUR_L_DARK, "_______________________________________________________________________________");
			}
		}
		total_player_attacks = 0;
		player_attacks = 0;
		monster_attacks = 0;

		for (i = 0; i < combat_num_for_round; i++) {
			if ((combat_rolls[round][i].attacker_char == r_info[0].d_char) &&
				(combat_rolls[round][i].attacker_attr == r_info[0].d_attr)) {
				total_player_attacks++;
			}
		}

		for (i = 0; i < combat_num_for_round; i++) {
			/* default values: */
			a_net_dam = COLOUR_L_RED;
			res = 1;

			/* determine the appropriate resistance if the player was attacked*/
			if ((combat_rolls[round][i].defender_char == r_info[0].d_char) &&
				(combat_rolls[round][i].defender_attr == r_info[0].d_attr)) {
				int dam_type = combat_rolls[round][i].dam_type;
				if (dam_type && (dam_type < ELEM_MAX)) {
					res = player->state.el_info[dam_type].res_level;
				}
			}

			if ((combat_rolls[round][i].attacker_char == r_info[0].d_char) &&
				(combat_rolls[round][i].attacker_attr == r_info[0].d_attr)) {
				player_attacks++;

				a_att = COLOUR_L_BLUE;
				a_evn = COLOUR_WHITE;
				a_hit = COLOUR_L_RED;
				a_dam_roll = COLOUR_L_BLUE;
				if (combat_rolls[round][i].prt_percent >= 100) {
					a_prot_roll = COLOUR_WHITE;
				} else if (combat_rolls[round][i].prt_percent >= 1) {
					a_prot_roll = COLOUR_SLATE;
				} else {
					a_prot_roll = COLOUR_DARK;
				}

				line = player_attacks + line_jump;
			} else {
				monster_attacks++;

				a_att = COLOUR_WHITE;
				a_evn = COLOUR_L_BLUE;
				a_hit = COLOUR_L_RED;
				a_dam_roll = COLOUR_WHITE;
				if (combat_rolls[round][i].prt_percent >= 100)
					a_prot_roll = COLOUR_L_BLUE;
				else if (combat_rolls[round][i].prt_percent >= 1)
					a_prot_roll = COLOUR_BLUE;
				else
					a_prot_roll = COLOUR_DARK;

				line = 1 + total_player_attacks + monster_attacks + line_jump;
				if (total_player_attacks == 0) line--;
			}



			/* Display the entry itself */
			Term_putstr(0, line, 1, COLOUR_WHITE, " ");
			Term_addch(combat_rolls[round][i].attacker_attr,
					   combat_rolls[round][i].attacker_char);


			/* First display the attack side of the roll.
			 * Don't print attack info if there isn't any (i.e. if it is
			 * a breath or other elemental attack) */
			if (combat_rolls[round][i].att_type == COMBAT_ROLL_ROLL) {
				if (combat_rolls[round][i].att < 10) {
					strnfmt(buf, sizeof (buf), "  (%+d)",
							combat_rolls[round][i].att);
				} else {
					strnfmt(buf, sizeof (buf), " (%+d)",
							combat_rolls[round][i].att);
				}
				Term_addstr(-1, a_att, buf);

				strnfmt(buf, sizeof (buf), "%4d",
						combat_rolls[round][i].att +
						combat_rolls[round][i].att_roll);
				Term_addstr(-1, a_att, buf);

				net_att = combat_rolls[round][i].att_roll +
					combat_rolls[round][i].att -
					combat_rolls[round][i].evn_roll -
					combat_rolls[round][i].evn;
				if (net_att > 0) {
					strnfmt(buf, sizeof (buf), "%4d", net_att);
					Term_addstr(-1, a_hit, buf);
				} else {
					Term_addstr(-1, COLOUR_SLATE, "   -");
				}

				strnfmt(buf, sizeof (buf), "%4d",
						combat_rolls[round][i].evn +
						combat_rolls[round][i].evn_roll);
				Term_addstr(-1, a_evn, buf);

				if (combat_rolls[round][i].evn < 10) {
					strnfmt(buf, sizeof (buf), "   [%+d]",
							combat_rolls[round][i].evn);
				} else {
					strnfmt(buf, sizeof (buf), "  [%+d]",
							combat_rolls[round][i].evn);
				}
				Term_addstr(-1, a_evn, buf);

				/* add the defender char */
				Term_addch(COLOUR_WHITE, ' ');
				Term_addch(combat_rolls[round][i].defender_attr,
						   combat_rolls[round][i].defender_char);
			} else if (combat_rolls[round][i].att_type == COMBAT_ROLL_AUTO) {
				Term_addstr(-1, COLOUR_L_DARK, "                         ");

				/* add the defender char */
				Term_addch(COLOUR_WHITE, ' ');
				Term_addch(combat_rolls[round][i].defender_attr,
						   combat_rolls[round][i].defender_char);
			}

			/* Now display the damage side of the roll */
			if ((net_att > 0) ||
				(combat_rolls[round][i].att_type == COMBAT_ROLL_AUTO)) {
				Term_addstr(-1, COLOUR_L_DARK, "  ->");

				if (combat_rolls[round][i].ds < 10) {
					strnfmt(buf, sizeof (buf), "   (%dd%d)",
							combat_rolls[round][i].dd,
							combat_rolls[round][i].ds);
				} else {
					strnfmt(buf, sizeof (buf), "  (%dd%d)",
							combat_rolls[round][i].dd,
							combat_rolls[round][i].ds);
				}
				Term_addstr(-1, a_dam_roll, buf);

				strnfmt(buf, sizeof (buf), "%4d", combat_rolls[round][i].dam);
				Term_addstr(-1, a_dam_roll, buf);

				if (combat_rolls[round][i].att_type == COMBAT_ROLL_ROLL) {
					net_dam = combat_rolls[round][i].dam -
						combat_rolls[round][i].prot;

					if (net_dam > 0) {
						strnfmt(buf, sizeof (buf), "%4d", net_dam);
						Term_addstr(-1, a_net_dam, buf);
					} else {
						Term_addstr(-1, COLOUR_SLATE, "   -");
					}

					strnfmt(buf, sizeof (buf), "%4d",
							combat_rolls[round][i].prot);
					Term_addstr(-1, a_prot_roll, buf);

					/* if monster is being hit, show protection dice */
					if ((combat_rolls[round][i].defender_char !=
						 r_info[0].d_char) ||
						(combat_rolls[round][i].defender_attr !=
						 r_info[0].d_attr)) {
						if ((combat_rolls[round][i].ps < 1) ||
							(combat_rolls[round][i].pd < 1)) {
							my_strcpy(buf, "        ", sizeof (buf));
							Term_addstr(-1, a_prot_roll, buf);
						} else if (combat_rolls[round][i].ps < 10) {
							strnfmt(buf, sizeof (buf), "   [%dd%d]",
									combat_rolls[round][i].pd,
									combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						} else {
							strnfmt(buf, sizeof (buf), "  [%dd%d]",
									combat_rolls[round][i].pd,
									combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						}
						if ((combat_rolls[round][i].prt_percent > 0) &&
							(combat_rolls[round][i].prt_percent < 100)) {
							strnfmt(buf, sizeof (buf), " (%d%%)",
									combat_rolls[round][i].prt_percent);
							Term_addstr(-1, a_prot_roll, buf);
						}
					} else {
						/* if player is being hit, show protection *range* */
						strnfmt(buf, sizeof (buf), "  [%d-%d]",
								(combat_rolls[round][i].pd *
								 combat_rolls[round][i].prt_percent) / 100,
								(combat_rolls[round][i].ps *
								 combat_rolls[round][i].prt_percent) / 100);
						Term_addstr(-1, a_prot_roll, buf);
					}

				} else if (combat_rolls[round][i].att_type == COMBAT_ROLL_AUTO){
					/* display attacks that don't use hit rolls */
					/* shield etc protection and resistance */
					if (combat_rolls[round][i].melee) {
 						net_dam = combat_rolls[round][i].dam -
							combat_rolls[round][i].prot;
					} else if (res > 0) {
						net_dam = (combat_rolls[round][i].dam / res) -
							combat_rolls[round][i].prot;
					} else {
						net_dam = (combat_rolls[round][i].dam * (-res)) -
							combat_rolls[round][i].prot;
					}

					if (net_dam > 0) {
						strnfmt(buf, sizeof (buf), "%4d", net_dam);
						Term_addstr(-1, a_net_dam, buf);
					} else {
						Term_addstr(-1, COLOUR_SLATE, "   -");
					}

					strnfmt(buf, sizeof (buf), "%4d",
							combat_rolls[round][i].prot);
					Term_addstr(-1, a_prot_roll, buf);

					/* if monster is being hit, show protection dice */
					if ((combat_rolls[round][i].defender_char !=
						 r_info[0].d_char) ||
						(combat_rolls[round][i].defender_attr !=
						 r_info[0].d_attr)) {
						if ((combat_rolls[round][i].ps < 1) ||
							(combat_rolls[round][i].pd < 1)) {
							my_strcpy(buf, "        ", sizeof (buf));
							Term_addstr(-1, a_prot_roll, buf);
						} else if (combat_rolls[round][i].ps < 10) {
							strnfmt(buf, sizeof (buf), "   [%dd%d]",
									combat_rolls[round][i].pd,
									combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						} else {
							strnfmt(buf, sizeof (buf), "  [%dd%d]",
									combat_rolls[round][i].pd,
									combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						}
						if ((combat_rolls[round][i].prt_percent > 0) &&
							(combat_rolls[round][i].prt_percent < 100)) {
							strnfmt(buf, sizeof (buf), " (%d%%)",
									combat_rolls[round][i].prt_percent);
							Term_addstr(-1, a_prot_roll, buf);
						}
					} else {
						/* if a player is being hit, show protection range etc*/
						if (!(combat_rolls[round][i].melee)) {
							if (res > 1) {
								strnfmt(buf, sizeof (buf), "  1/%d then", res);
								Term_addstr(-1, COLOUR_L_BLUE, buf);
							} else if (res < 0) {
								strnfmt(buf, sizeof (buf), "  x%d then", -res);
								Term_addstr(-1, COLOUR_L_BLUE, buf);
							}
						}

						if (combat_rolls[round][i].ps < 10) {
							strnfmt(buf, sizeof (buf), "  [%d-%d]",
									combat_rolls[round][i].pd,
									combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						} else {
							strnfmt(buf, sizeof (buf), " [%d-%d]",
									combat_rolls[round][i].pd,
									combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						}
					}
				}
			}
		}
	}
}
