/**
 * \file list-options.h
 * \brief options
 *
 * Currently, if there are more than 21 of any option type, the later ones
 * will be ignored
 * Cheat options need to be followed by corresponding score options
 */

/* name                   description
type     normal */
OP(none,                  "",
SPECIAL, false)
OP(hjkl_movement,         "Move with hjkl etc. (use ^ for underlying keys)",
INTERFACE, false)
OP(use_sound,             "Use sound",
INTERFACE, false)
OP(quick_messages,        "Dismiss '-more-' and 'y/n' prompts with any key",
INTERFACE, true)
OP(angband_keyset,        "Use a keyset more closely based on Angband",
INTERFACE, false)
OP(stop_singing_on_rest,  "Stop singing when you use the rest command",
INTERFACE, true)
OP(forgo_attacking_unwary,"Forgo bonus attacks on non-alert enemies",
INTERFACE, true)
OP(beep,                  "Audible beep (on errors/warnings)",
INTERFACE, false)
OP(highlight_player,      "Highlight the player with the cursor",
INTERFACE, false)
OP(highlight_target,      "Highlight the target with the cursor",
INTERFACE, true)
OP(highlight_unwary,      "Highlight sleeping and unwary creatures",
INTERFACE, true)
OP(solid_walls,           "Show walls as solid blocks",
INTERFACE, true)
OP(hybrid_walls,          "Show walls with shaded background",
INTERFACE, false)
OP(animate_flicker,       "Color: Shimmer multi-colored things",
INTERFACE, false)
OP(center_player,         "Center map continuously",
INTERFACE, false)
OP(run_avoid_center,      "Avoid centering while running",
INTERFACE, false)
OP(auto_more,             "Automatically clear '-more-' prompts",
INTERFACE, false)
OP(mouse_movement,        "Allow mouse clicks to move the player",
INTERFACE, true)
OP(display_hits,          "Display a mark when something gets hit",
INTERFACE, true)
OP(pickup_always,         "Always pickup items",
INTERFACE, false)
OP(pickup_inven,          "Always pickup items matching inventory",
INTERFACE, false)
OP(show_flavors,          "Show flavors in object descriptions",
INTERFACE, false)
OP(cheat_peek,            "Debug: Peek into object creation",
CHEAT, false)
OP(score_peek,            "Score: Peek into object creation",
SCORE, false)
OP(cheat_hear,            "Debug: Peek into monster creation",
CHEAT, false)
OP(score_hear,            "Score: Peek into monster creation",
SCORE, false)
OP(cheat_room,            "Debug: Peek into dungeon creation",
CHEAT, false)
OP(score_room,            "Score: Peek into dungeon creation",
SCORE, false)
OP(cheat_xtra,            "Debug: Peek into something else",
CHEAT, false)
OP(score_xtra,            "Score: Peek into something else",
SCORE, false)
OP(cheat_know,            "Debug: Know complete monster info",
CHEAT, false)
OP(score_know,            "Score: Know complete monster info",
SCORE, false)
OP(cheat_live,            "Debug: Allow player to avoid death",
CHEAT, false)
OP(score_live,            "Score: Allow player to avoid death",
SCORE, false)
OP(cheat_monsters,        "Debug: Continually display all monsters",
CHEAT, false)
OP(score_monsters,        "Score: Continually display all monsters",
SCORE, false)
OP(cheat_noise,           "Debug: Continually display noise levels",
CHEAT, false)
OP(score_noise,           "Score: Continually display noise levels",
SCORE, false)
OP(cheat_scent,           "Debug: Continually display scent levels",
CHEAT, false)
OP(score_scent,           "Score: Continually display scent levels",
SCORE, false)
OP(cheat_light,           "Debug: Continually display light levels",
CHEAT, false)
OP(score_light,           "Score: Continually display light levels",
SCORE, false)
OP(cheat_skill_rolls,     "Debug: Show all skill rolls",
CHEAT, false)
OP(score_skill_rolls,     "Score: Show all skill rolls",
SCORE, false)
OP(cheat_timestop,        "Debug: Don't allow monsters to move",
CHEAT, false)
OP(score_timestop,        "Score: Don't allow monsters to move",
SCORE, false)
OP(birth_discon_stairs,   "Disconnected stairs",
BIRTH, false)
OP(birth_force_descend,   "Force player descent (never make up stairs)",
BIRTH, false)
OP(birth_no_artifacts,    "Restrict creation of artifacts",
BIRTH, false)

