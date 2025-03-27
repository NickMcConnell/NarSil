/**
 * \file list-player-flags.h
 * \brief player race and class flags
 *
 * Adjusting these flags does not break savefiles. Flags below start from 1
 * on line 14, so a flag's sequence number is its line number minus 13.
 *
 * Fields:
 * symbol - the flag name
 * additional details in player_property.txt
 */

PF(NONE, "")
PF(BLADE_PROFICIENCY, "Blade proficiency")
PF(AXE_PROFICIENCY,   "Axe proficiency")
