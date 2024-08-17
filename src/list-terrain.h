/**
 * \file list-terrain.h
 * \brief List the terrain (feature) types that can appear
 *
 * These are how the code and data files refer to terrain.  Any changes will
 * break savefiles.  Note that the terrain code is stored as an unsigned 8-bit
 * integer so there can be at most 256 types of terrain.  Flags below start
 * from zero on line 13, so a terrain's sequence number is its line number
 * minus 13.
 */

/* symbol */
FEAT(NONE) /* nothing/unknown */
FEAT(FLOOR) /* open floor */
FEAT(CLOSED) /* closed door */
FEAT(OPEN) /* open door */
FEAT(BROKEN) /* broken door */
FEAT(LESS) /* up staircase */
FEAT(MORE) /* down staircase */
FEAT(LESS_SHAFT) /* up shaft */
FEAT(MORE_SHAFT) /* down shaft */
FEAT(CHASM) /* chasm */
FEAT(SECRET) /* secret door */
FEAT(RUBBLE) /* impassable rubble */
FEAT(QUARTZ) /* quartz vein wall */
FEAT(GRANITE) /* granite wall */
FEAT(PERM) /* permanent wall */
FEAT(FORGE)
FEAT(FORGE_GOOD)
FEAT(FORGE_UNIQUE)
FEAT(PIT)
FEAT(SPIKED_PIT)
