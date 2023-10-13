/**
 * \file project.h
 * \brief projection and helpers
 */

#ifndef PROJECT_H
#define PROJECT_H

#include "source.h"

/**
 * Spell types used by project(), and related functions.
 */
enum
{
	#define ELEM(a) PROJ_##a,
	#include "list-elements.h"
	#undef ELEM
	#define PROJ(a) PROJ_##a,
	#include "list-projections.h"
	#undef PROJ
	PROJ_MAX
};

/**
 * Element struct
 */
struct projection {
	int index;
	char *name;
	char *type;
	char *desc;
	char *player_desc;
	char *blind_desc;
	int msgt;
	bool damaging;
	bool evade;
	bool obvious;
	bool wake;
	int color;
	struct projection *next;
};

extern struct projection *projections;

/**
 * Bolt motion (used in prefs.c, project.c)
 */
enum
{
    BOLT_NO_MOTION,
    BOLT_0,
    BOLT_45,
    BOLT_90,
    BOLT_135,
    BOLT_MAX
};

/**
 * Return values for the projectable() function
 */
enum {
	PROJECT_PATH_NO,
	PROJECT_PATH_NOT_CLEAR,
	PROJECT_PATH_CLEAR
};

/**
 *   NONE: No flags
 *   JUMP: Jump directly to the target location without following a path
 *   BEAM: Work as a beam weapon (affect every grid passed through)
 *   THRU: May continue through the target (used for bolts and beams)
 *   STOP: Stop as soon as we hit a monster (used for bolts)
 *   GRID: May affect terrain in the blast area in some way
 *   ITEM: May affect objects in the blast area in some way
 *   KILL: May affect monsters in the blast area in some way
 *   HIDE: Disable visual feedback from projection
 *   ARC: Projection is a sector of circle radiating from the caster
 *   PLAY: May affect the player
 *   INFO: Use believed map rather than truth for player ui
 *   SHORT: Use one quarter of max_range
 *   BOOM: Explode
 *   INVIS: Ignores invisible walls
 *   CHASM: Blocked by chasms
 *   CHCK: Projection notes when it cannot bypass a monster
 *   RANGE_DAM: if range is zero, set the range to be where damage can
 *     still be inflicted
 */
enum {
	PROJECT_NONE  = 0x00000,
	PROJECT_JUMP  = 0x00001,
	PROJECT_BEAM  = 0x00002,
	PROJECT_THRU  = 0x00004,
	PROJECT_STOP  = 0x00008,
	PROJECT_GRID  = 0x00010,
	PROJECT_ITEM  = 0x00020,
	PROJECT_KILL  = 0x00040,
	PROJECT_HIDE  = 0x00080,
	PROJECT_ARC   = 0x00100,
	PROJECT_PLAY  = 0x00200,
	PROJECT_INFO  = 0x00400,
	PROJECT_PASS  = 0x00800,
	PROJECT_BOOM  = 0x01000,
	PROJECT_INVIS = 0x02000,
	PROJECT_CHASM = 0x04000,
	PROJECT_CHCK  = 0x08000,
	PROJECT_WALL  = 0x10000,
	PROJECT_LEAVE = 0x20000,
	PROJECT_RANGE_DAM = 0x40000
};

/* Display attrs and chars */
extern uint8_t proj_to_attr[PROJ_MAX][BOLT_MAX];
extern wchar_t proj_to_char[PROJ_MAX][BOLT_MAX];

int inven_damage(struct player *p, int type, int perc, int resistance);
int adjust_dam(struct player *p, int dd, int ds, int type);

bool project_f(struct source origin, struct loc grid, int dif, int typ);
bool project_o(struct loc grid, int typ, const struct object *protected_obj);
void project_m(struct source origin, int r, struct loc grid, int dam, int ds,
			   int dif, int typ, int flg, bool *did_hit, bool *was_obvious);
bool project_p(struct source, struct loc grid, int dd, int ds, int typ);

int projectable(struct chunk *c, struct loc grid1, struct loc grid2, int flg);
int proj_name_to_idx(const char *name);
const char *proj_idx_to_name(int type);

struct loc origin_get_loc(struct source origin);

bool project(struct source origin, int rad, struct loc finish,
			 int dd, int ds, int dif, int typ, int flg,
			 int degrees_of_arc, bool uniform, const struct object *obj);

#endif /* !PROJECT_H */
