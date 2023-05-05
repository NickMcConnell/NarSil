/**
 * \file list-tutorial-sym.h
 * \brief Symbols for tutorial section layouts
 *
 * Changing the second arguments will affect the parsing and interpretation of
 * the layout lines for tutorial sections in tutorial.txt.  Changing the
 * other arguments will have to match up with code changes in tutorial.c (and
 * tutorial.txt if adding or removing symbol types).  Any changes will require
 * recompiling tutorial.c and tutorial-init.c.
 *
 * The first argument is appended to SECTION_SYM_ to get the enumeration member.
 * The second argument is single code point UTF-8 string to use for a predefined
 * symbol.  Customizable symbols use NULL for it.
 * The third argument is the FEAT_ constant to use for a symbol that uses the
 * tutorial_section_place_feature() function.  Others use FEAT_NONE for it.
 * The fourth argument is the function name to call when setting up a grid
 * in the tutorial chunk.
 */

/* Placeholder for no value or error condition */
TSYM(DUMMY, NULL, FEAT_NONE, NULL)

/* Predefined symbols */
TSYM(START, "0", FEAT_NONE, tutorial_section_place_note)
TSYM(FLOOR, ".", FEAT_FLOOR, tutorial_section_place_feature)
TSYM(GRANITE0, "#", FEAT_GRANITE, tutorial_section_place_feature)
TSYM(GRANITE1, " ", FEAT_GRANITE, tutorial_section_place_feature)
TSYM(PERMROCK, "@", FEAT_PERM, tutorial_section_place_feature)
TSYM(IMPASS_RUBBLE, ":", FEAT_RUBBLE, tutorial_section_place_feature)
TSYM(CLOSED_DOOR, "+", FEAT_CLOSED, tutorial_section_place_feature)
TSYM(OPEN_DOOR, ",", FEAT_OPEN, tutorial_section_place_feature)
TSYM(SECRET_DOOR, "s", FEAT_SECRET, tutorial_section_place_feature)
TSYM(TRAP_RANDOM, "^", FEAT_NONE, tutorial_section_place_trap)

/* Customizable symbols */
TSYM(NOTE, NULL, FEAT_NONE, tutorial_section_place_note)
TSYM(TRIGGER, NULL, FEAT_NONE, tutorial_section_place_trigger)
TSYM(GATE, NULL, FEAT_NONE, tutorial_section_place_gate)
TSYM(FORGE, NULL, FEAT_NONE, tutorial_section_place_forge)
TSYM(ITEM, NULL, FEAT_NONE, tutorial_section_place_object)
TSYM(MONSTER, NULL, FEAT_NONE, tutorial_section_place_monster)
TSYM(TRAP, NULL, FEAT_NONE, tutorial_section_place_custom_trap)
TSYM(DOOR, NULL, FEAT_NONE, tutorial_section_place_custom_door)
