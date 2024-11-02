/**
 * \file cave-fire.c
 * \brief Line-of-fire calculations
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
#include "cave.h"
#include "init.h"
#include "project.h"

/**
 * This whole file is just the code from cave.c in original Sil, lifted and
 * modified as needed (ie the bits that influence field-of-view are ripped out)
 * to provide the correct line-of-fire calculations.
 * 
 * At some point it should be translated into actual readable code.
 */

/**
 * Convert a "location" (Y,X) into a "grid" (G)
 */
#define GRID(Y,X) \
	(256 * (Y) + (X))

/**
 * Convert a "grid" (G) into a "location" (Y)
 */
#define GRID_Y(G) \
	((int)((G) / 256U))

/**
 * Convert a "grid" (G) into a "location" (X)
 */
#define GRID_X(G) \
	((int)((G) % 256U))

/**
 * Maximum number of grids in a single octant
 */
#define VINFO_MAX_GRIDS 161


/**
 * Maximum number of slopes in a single octant
 */
#define VINFO_MAX_SLOPES 126


/**
 * Mask of bits used in a single octant
 */
#define VINFO_BITS_3 0x3FFFFFFF
#define VINFO_BITS_2 0xFFFFFFFF
#define VINFO_BITS_1 0xFFFFFFFF
#define VINFO_BITS_0 0xFFFFFFFF


/**
 * The 'vinfo_type' structure
 */
struct vinfo_type
{
	int16_t grid[8];

	/* LOS slopes (up to 128) */
	uint32_t bits_3;
	uint32_t bits_2;
	uint32_t bits_1;
	uint32_t bits_0;

	/* Index of the first LOF slope */
	uint8_t slope_fire_index1;

	/* Index of the (possible) second LOF slope */
	uint8_t slope_fire_index2;

	struct vinfo_type *next_0;
	struct vinfo_type *next_1;

	uint8_t y;
	uint8_t x;
	uint8_t d;
	uint8_t r;
};



/**
 * The array of "vinfo" objects, initialized by "vinfo_init()"
 */
static struct vinfo_type vinfo[VINFO_MAX_GRIDS];

/**
 * Slope scale factor
 */
#define SCALE 100000L


/**
 * The actual slopes (for reference)
 */

/* Bit :     Slope   Grids */
/* --- :     -----   ----- */
/*   0 :      2439      21 */
/*   1 :      2564      21 */
/*   2 :      2702      21 */
/*   3 :      2857      21 */
/*   4 :      3030      21 */
/*   5 :      3225      21 */
/*   6 :      3448      21 */
/*   7 :      3703      21 */
/*   8 :      4000      21 */
/*   9 :      4347      21 */
/*  10 :      4761      21 */
/*  11 :      5263      21 */
/*  12 :      5882      21 */
/*  13 :      6666      21 */
/*  14 :      7317      22 */
/*  15 :      7692      20 */
/*  16 :      8108      21 */
/*  17 :      8571      21 */
/*  18 :      9090      20 */
/*  19 :      9677      21 */
/*  20 :     10344      21 */
/*  21 :     11111      20 */
/*  22 :     12000      21 */
/*  23 :     12820      22 */
/*  24 :     13043      22 */
/*  25 :     13513      22 */
/*  26 :     14285      20 */
/*  27 :     15151      22 */
/*  28 :     15789      22 */
/*  29 :     16129      22 */
/*  30 :     17241      22 */
/*  31 :     17647      22 */
/*  32 :     17948      23 */
/*  33 :     18518      22 */
/*  34 :     18918      22 */
/*  35 :     20000      19 */
/*  36 :     21212      22 */
/*  37 :     21739      22 */
/*  38 :     22580      22 */
/*  39 :     23076      22 */
/*  40 :     23809      22 */
/*  41 :     24137      22 */
/*  42 :     24324      23 */
/*  43 :     25714      23 */
/*  44 :     25925      23 */
/*  45 :     26315      23 */
/*  46 :     27272      22 */
/*  47 :     28000      23 */
/*  48 :     29032      23 */
/*  49 :     29411      23 */
/*  50 :     29729      24 */
/*  51 :     30434      23 */
/*  52 :     31034      23 */
/*  53 :     31428      23 */
/*  54 :     33333      18 */
/*  55 :     35483      23 */
/*  56 :     36000      23 */
/*  57 :     36842      23 */
/*  58 :     37142      24 */
/*  59 :     37931      24 */
/*  60 :     38461      24 */
/*  61 :     39130      24 */
/*  62 :     39393      24 */
/*  63 :     40740      24 */
/*  64 :     41176      24 */
/*  65 :     41935      24 */
/*  66 :     42857      23 */
/*  67 :     44000      24 */
/*  68 :     44827      24 */
/*  69 :     45454      23 */
/*  70 :     46666      24 */
/*  71 :     47368      24 */
/*  72 :     47826      24 */
/*  73 :     48148      24 */
/*  74 :     48387      24 */
/*  75 :     51515      25 */
/*  76 :     51724      25 */
/*  77 :     52000      25 */
/*  78 :     52380      25 */
/*  79 :     52941      25 */
/*  80 :     53846      25 */
/*  81 :     54838      25 */
/*  82 :     55555      24 */
/*  83 :     56521      25 */
/*  84 :     57575      26 */
/*  85 :     57894      25 */
/*  86 :     58620      25 */
/*  87 :     60000      23 */
/*  88 :     61290      25 */
/*  89 :     61904      25 */
/*  90 :     62962      25 */
/*  91 :     63636      25 */
/*  92 :     64705      25 */
/*  93 :     65217      25 */
/*  94 :     65517      25 */
/*  95 :     67741      26 */
/*  96 :     68000      26 */
/*  97 :     68421      26 */
/*  98 :     69230      26 */
/*  99 :     70370      26 */
/* 100 :     71428      25 */
/* 101 :     72413      26 */
/* 102 :     73333      26 */
/* 103 :     73913      26 */
/* 104 :     74193      27 */
/* 105 :     76000      26 */
/* 106 :     76470      26 */
/* 107 :     77777      25 */
/* 108 :     78947      26 */
/* 109 :     79310      26 */
/* 110 :     80952      26 */
/* 111 :     81818      26 */
/* 112 :     82608      26 */
/* 113 :     84000      26 */
/* 114 :     84615      26 */
/* 115 :     85185      26 */
/* 116 :     86206      27 */
/* 117 :     86666      27 */
/* 118 :     88235      27 */
/* 119 :     89473      27 */
/* 120 :     90476      27 */
/* 121 :     91304      27 */
/* 122 :     92000      27 */
/* 123 :     92592      27 */
/* 124 :     93103      28 */
/* 125 :    100000      13 */



/**
 * Forward declare
 */
typedef struct vinfo_hack vinfo_hack;

/**
 * Hard-coded version of z_info->max_sight etc for now - NRM
 */
#define SIGHT_MAX 20

/**
 * Hard-coded max fire grids - NRM
 */
#define FIRE_MAX 1536

/**
 * Versions of view array - NRM
 */
static uint16_t fire_g[FIRE_MAX];
static int fire_n = 0;

/**
 * Version of SQUARE_FIRE flags in old cave_info array - NRM
 */
static bool	fire_info[256 * 55];

/**
 * Temporary data used by "vinfo_init()"
 *
 *	- Number of line of sight slopes
 *
 *	- Slope values
 *
 *	- Slope range for each grid
 */
struct vinfo_hack
{
	int num_slopes;

	long slopes[VINFO_MAX_SLOPES];

	long slopes_min[SIGHT_MAX + 1][SIGHT_MAX + 1];
	long slopes_max[SIGHT_MAX + 1][SIGHT_MAX + 1];
};

/*
 * Current "comp" function for ang_sort()
 */
bool (*ang_sort_comp)(const void *u, const void *v, int a, int b);


/*
 * Current "swap" function for ang_sort()
 */
void (*ang_sort_swap)(void *u, void *v, int a, int b);


/**
 * Sorting hook -- comp function -- array of long's (see below)
 *
 * We use "u" to point to an array of long integers.
 */
static bool ang_sort_comp_hook_longs(const void *u, const void *v, int a, int b)
{
	long *x = (long*)(u);

	/* Unused parameter */
	(void)v;

	return (x[a] <= x[b]);
}


/**
 * Sorting hook -- comp function -- array of long's (see below)
 *
 * We use "u" to point to an array of long integers.
 */
static void ang_sort_swap_hook_longs(void *u, void *v, int a, int b)
{
	long *x = (long*)(u);

	long temp;

	/* Unused parameter */
	(void)v;

	/* Swap */
	temp = x[a];
	x[a] = x[b];
	x[b] = temp;
}

/*
 * Angband sorting algorithm -- quick sort in place
 *
 * Note that the details of the data we are sorting is hidden,
 * and we rely on the "ang_sort_comp()" and "ang_sort_swap()"
 * function hooks to interact with the data, which is given as
 * two pointers, and which may have any user-defined form.
 */
static void ang_sort_aux(void *u, void *v, int p, int q)
{
	int z, a, b;

	/* Done sort */
	if (p >= q) return;

	/* Pivot */
	z = p;

	/* Begin */
	a = p;
	b = q;

	/* Partition */
	while (true)
	{
		/* Slide i2 */
		while (!(*ang_sort_comp)(u, v, b, z)) b--;

		/* Slide i1 */
		while (!(*ang_sort_comp)(u, v, z, a)) a++;

		/* Done partition */
		if (a >= b) break;

		/* Swap */
		(*ang_sort_swap)(u, v, a, b);

		/* Advance */
		a++, b--;
	}

	/* Recurse left side */
	ang_sort_aux(u, v, p, b);

	/* Recurse right side */
	ang_sort_aux(u, v, b+1, q);
}


/*
 * Angband sorting algorithm -- quick sort in place
 *
 * Note that the details of the data we are sorting is hidden,
 * and we rely on the "ang_sort_comp()" and "ang_sort_swap()"
 * function hooks to interact with the data, which is given as
 * two pointers, and which may have any user-defined form.
 */
static void ang_sort(void *u, void *v, int n)
{
	/* Sort the array */
	ang_sort_aux(u, v, 0, n-1);
}

/**
 * Save a slope
 */
static void vinfo_init_aux(struct vinfo_hack *hack, int y, int x, long m)
{
	int i;

	/* Handle "legal" slopes */
	if ((m > 0) && (m <= SCALE)) {
		/* Look for that slope */
		for (i = 0; i < hack->num_slopes; i++) {
			if (hack->slopes[i] == m) break;
		}

		/* New slope */
		if (i == hack->num_slopes) {
			/* Paranoia */
			if (hack->num_slopes >= VINFO_MAX_SLOPES) {
				quit_fmt("Too many LOS slopes (%d)!", VINFO_MAX_SLOPES);
			}

			/* Save the slope, increment count */
			hack->slopes[hack->num_slopes++] = m;
		}
	}

	/* Track slope range */
	if (hack->slopes_min[y][x] > m) hack->slopes_min[y][x] = m;
	if (hack->slopes_max[y][x] < m) hack->slopes_max[y][x] = m;

}

/**
 * Initialize the "vinfo" array
 *
 * Full Octagon (radius 20), Grids=1149
 *
 * Quadrant (south east), Grids=308, Slopes=251
 *
 * Octant (east then south), Grids=161, Slopes=126
 *
 * This function assumes that VINFO_MAX_GRIDS and VINFO_MAX_SLOPES
 * have the correct values, which can be derived by setting them to
 * a number which is too high, running this function, and using the
 * error messages to obtain the correct values.
 */
errr vinfo_init(void)
{
	int i, g;
	int y, x;

	long m;

	struct vinfo_hack *hack;

	int num_grids = 0;

	int queue_head = 0;
	int queue_tail = 0;
	struct vinfo_type *queue[VINFO_MAX_GRIDS * 2];


	/* Make hack */
	hack = mem_zalloc(sizeof(*hack));

	/* Analyze grids */
	for (y = 0; y <= SIGHT_MAX; ++y) {
		for (x = y; x <= SIGHT_MAX; ++x) {
			/* Skip grids which are out of sight range */
			if (distance(loc(0, 0), loc(x, y)) > SIGHT_MAX) continue;

			/* Default slope range */
			hack->slopes_min[y][x] = 999999999;
			hack->slopes_max[y][x] = 0;

			/* Paranoia */
			if (num_grids >= VINFO_MAX_GRIDS) {
				quit_fmt("Too many grids (%d >= %d)!",
					num_grids, VINFO_MAX_GRIDS);
			}

			/* Count grids */
			num_grids++;

			/* Slope to the top right corner */
			m = SCALE * (1000L * y - 500) / (1000L * x + 500);

			/* Handle "legal" slopes */
			vinfo_init_aux(hack, y, x, m);

			/* Slope to top left corner */
			m = SCALE * (1000L * y - 500) / (1000L * x - 500);

			/* Handle "legal" slopes */
			vinfo_init_aux(hack, y, x, m);

			/* Slope to bottom right corner */
			m = SCALE * (1000L * y + 500) / (1000L * x + 500);

			/* Handle "legal" slopes */
			vinfo_init_aux(hack, y, x, m);

			/* Slope to bottom left corner */
			m = SCALE * (1000L * y + 500) / (1000L * x - 500);

			/* Handle "legal" slopes */
			vinfo_init_aux(hack, y, x, m);
		}
	}

	/* Enforce maximal efficiency (grids) */
	if (num_grids < VINFO_MAX_GRIDS) {
		quit_fmt("Too few grids (%d < %d)!",
			num_grids, VINFO_MAX_GRIDS);
	}

	/* Enforce maximal efficiency (line of sight slopes) */
	if (hack->num_slopes < VINFO_MAX_SLOPES) {
		quit_fmt("Too few LOS slopes (%d < %d)!",
			hack->num_slopes, VINFO_MAX_SLOPES);
	}


	/* Sort slopes numerically */
	ang_sort_comp = ang_sort_comp_hook_longs;

	/* Sort slopes numerically */
	ang_sort_swap = ang_sort_swap_hook_longs;

	/* Sort the (unique) LOS slopes */
	ang_sort(hack->slopes, NULL, hack->num_slopes);


	/* Enqueue player grid */
	queue[queue_tail++] = &vinfo[0];

	/* Process queue */
	while (queue_head < queue_tail) {
		int e;

		/* Index */
		e = queue_head++;

		/* Main Grid */
		g = vinfo[e].grid[0];

		/* Location */
		y = GRID_Y(g);
		x = GRID_X(g);


		/* Compute grid offsets */
		vinfo[e].grid[0] = GRID(+y,+x);
		vinfo[e].grid[1] = GRID(+x,+y);
		vinfo[e].grid[2] = GRID(+x,-y);
		vinfo[e].grid[3] = GRID(+y,-x);
		vinfo[e].grid[4] = GRID(-y,-x);
		vinfo[e].grid[5] = GRID(-x,-y);
		vinfo[e].grid[6] = GRID(-x,+y);
		vinfo[e].grid[7] = GRID(-y,+x);


		/* Skip player grid */
		if (e > 0) {
			long slope_fire;
			long slope_min = 0;
			long slope_max = 999999L;

			uint8_t tmp0 = 0;
			uint8_t tmp1 = 0;
			uint8_t tmp2 = 0;

			/* Determine LOF slope for this grid */
			if (x == 0) slope_fire = SCALE;
			else slope_fire = SCALE * (1000L * y) / (1000L * x);

			/* Analyze LOS slopes */
			for (i = 0; i < hack->num_slopes; ++i) {
				m = hack->slopes[i];

				/* Memorize intersecting slopes */
				if ((hack->slopes_min[y][x] < m) &&
				    (hack->slopes_max[y][x] > m))
				{
					/* Add it to the LOS slope set */
					switch (i / 32)
					{
						case 3: vinfo[e].bits_3 |= (1L << (i % 32)); break;
						case 2: vinfo[e].bits_2 |= (1L << (i % 32)); break;
						case 1: vinfo[e].bits_1 |= (1L << (i % 32)); break;
						case 0: vinfo[e].bits_0 |= (1L << (i % 32)); break;
					}

					/* Check for exact match with the LOF slope */
					if (m == slope_fire) {
						tmp0 = i;
					} else if ((m < slope_fire) && (m > slope_min)) {
						/* Store index of nearest LOS slope < than LOF slope */
						tmp1 = i;
						slope_min = m;
					} else if ((m > slope_fire) && (m < slope_max)) {
						/* Store index of nearest LOS slope > than LOF slope */
						tmp2 = i;
						slope_max = m;
					}
				}
			}

			/* There is a perfect match with one of the LOS slopes */
			if (tmp0) {
				/* Save the (unique) slope */
				vinfo[e].slope_fire_index1 = tmp0;

				/* Mark the other empty */
				vinfo[e].slope_fire_index2 = 0;
			}

			/* The LOF slope lies between two LOS slopes */
			else
			{
				/* Save the first slope */
				vinfo[e].slope_fire_index1 = tmp1;

				/* Save the second slope */
				vinfo[e].slope_fire_index2 = tmp2;
			}
		}

		/* Default */
		vinfo[e].next_0 = &vinfo[0];

		/* Grid next child */
		if (distance(loc(0, 0), loc(x+1, y)) <= SIGHT_MAX) {
			g = GRID(y,x+1);

			if (queue[queue_tail-1]->grid[0] != g) {
				vinfo[queue_tail].grid[0] = g;
				queue[queue_tail] = &vinfo[queue_tail];
				queue_tail++;
			}

			vinfo[e].next_0 = &vinfo[queue_tail - 1];
		}


		/* Default */
		vinfo[e].next_1 = &vinfo[0];

		/* Grid diag child */
		if (distance(loc(0, 0), loc(x+1, y+1)) <= SIGHT_MAX) {
			g = GRID(y+1,x+1);

			if (queue[queue_tail-1]->grid[0] != g) {
				vinfo[queue_tail].grid[0] = g;
				queue[queue_tail] = &vinfo[queue_tail];
				queue_tail++;
			}

			vinfo[e].next_1 = &vinfo[queue_tail - 1];
		}


		/* Hack -- main diagonal has special children */
		if (y == x) vinfo[e].next_0 = vinfo[e].next_1;


		/* Grid coordinates, approximate distance  */
		vinfo[e].y = y;
		vinfo[e].x = x;
		vinfo[e].d = ((y > x) ? (y + x/2) : (x + y/2));
		vinfo[e].r = ((!y) ? x : (!x) ? y : (y == x) ? y : 0);
	}

	/* Verify maximal bits XXX XXX XXX */
	if (((vinfo[1].bits_3 | vinfo[2].bits_3) != VINFO_BITS_3) ||
	    ((vinfo[1].bits_2 | vinfo[2].bits_2) != VINFO_BITS_2) ||
	    ((vinfo[1].bits_1 | vinfo[2].bits_1) != VINFO_BITS_1) ||
	    ((vinfo[1].bits_0 | vinfo[2].bits_0) != VINFO_BITS_0))
	{
		quit("Incorrect bit masks!");
	}

	/* Kill hack */
	mem_free(hack);

	/* Success */
	return (0);
}

/**
 * Change a struct loc to an int GRID
 */
static int loc_to_grid(struct loc grid)
{
	return GRID(grid.y, grid.x);
}

/**
 * Change an int GRID to a struct loc
 */
static struct loc grid_to_loc(int grid)
{
	return loc(GRID_X(grid), GRID_Y(grid));
}

/**
 * Forget the fire_g grids, redrawing as needed
 */
void forget_fire(struct chunk *c)
{
	int i;

	/* None to forget */
	if (!fire_n) return;

	/* Clear them all */
	for (i = 0; i < fire_n; i++) {
		int g;
		struct loc grid;

		/* Grid */
		g = fire_g[i];

		/* Location */
		grid = grid_to_loc(g);

		/* Clear SQUARE_FIRE flags */
		sqinfo_off(square(c, grid)->info, SQUARE_FIRE);
		fire_info[g] = false;
	}

	/* None left */
	fire_n = 0;
}

/**
 * Calculate the complete field of fire using a cut-down version of the old
 * Sil field of view algorithm (and a modifieed version of this comment).
 *
 * Note the following idiom, which is used in the function below.
 * This idiom processes each "octant" of the field of fire, in a
 * clockwise manner, starting with the east strip, south side,
 * and for each octant, allows a simple calculation to set "g"
 * equal to the proper grids, relative to "pg", in the octant.
 *
 *   for (o2 = 0; o2 < 8; o2++)
 *   ...
 *         g = pg + p->grid[o2];
 *   ...
 *
 *
 * Normally, fire along the major axes is more likely than fire
 * along the diagonal axes, so we check the bits corresponding to
 * the lines of sight near the major axes first.
 *
 * This function is now responsible for maintaining the "SQUARE_FIRE"
 * flags as well as the "SQUARE_FIRE" flags.
 *
 * Basically, this function divides the "octagon of fire" into octants of
 * grids (where grids on the main axes and diagonal axes are "shared" by
 * two octants), and processes each octant one at a time, processing each
 * octant one grid at a time, processing only those grids which "might" be
 * fireable, and setting the "SQUARE_FIRE" flag for each grid for which there
 * is an (unobstructed) line of fire from the center of the player grid to
 * any internal point in the grid (and collecting these "SQUARE_FIRE" grids
 * into the "fire_g" array).
 *
 * This function relies on a theorem (suggested and proven by Mat Hostetter)
 * which states that in each octant of a field of fire, a given grid will
 * be "intersected" by one or more unobstructed "lines of fire" from the
 * center of the player grid if and only if it is "intersected" by at least
 * one such unobstructed "line of fire" which passes directly through some
 * corner of some grid in the octant which is not shared by any other octant.
 * The proof is based on the fact that there are at least three significant
 * lines of fire involving any non-shared grid in any octant, one which
 * intersects the grid and passes though the corner of the grid closest to
 * the player, and two which "brush" the grid, passing through the "outer"
 * corners of the grid, and that any line of fire which intersects a grid
 * without passing through the corner of a grid in the octant can be "slid"
 * slowly towards the corner of the grid closest to the player, until it
 * either reaches it or until it brushes the corner of another grid which
 * is closer to the player, and in either case, the existence of a suitable
 * line of fire is thus demonstrated.
 *
 * It turns out that in each octant of the radius 20 "octagon of fire",
 * there are 161 grids (with 128 not shared by any other octant), and there
 * are exactly 126 distinct "lines of fire" passing from the center of the
 * player grid through any corner of any non-shared grid in the octant.  To
 * determine if a grid is "fireable" by the player, therefore, you need to
 * simply show that one of these 126 lines of fire intersects the grid but
 * does not intersect any wall grid closer to the player.  So we simply use
 * a bit vector with 126 bits to represent the set of interesting lines of
 * fire which have not yet been obstructed by wall grids, and then we scan
 * all the grids in the octant, moving outwards from the player grid.  For
 * each grid, if any of the lines of fire which intersect that grid have not
 * yet been obstructed, then the grid is fireable.  Furthermore, if the grid
 * is a wall grid, then all of the lines of fire which intersect the grid
 * should be marked as obstructed for future reference.  Also, we only need
 * to check those grids for whom at least one of the "parents" was a fireable
 * non-wall grid, where the parents include the two grids touching the grid
 * but closer to the player grid (one adjacent, and one diagonal).  For the
 * bit vector, we simply use 4 32-bit integers.  All of the static values
 * which are needed by this function are stored in the large "vinfo" array
 * (above), which is machine generated by another program.  XXX XXX XXX
 *
 * Hack -- The queue must be able to hold more than VINFO_MAX_GRIDS grids
 * because the grids at the edge of the field of fire use "grid zero" as
 * their children, and the queue must be able to hold several of these
 * special grids.  Because the actual number of required grids is bizarre,
 * we simply allocate twice as many as we would normally need.  XXX XXX XXX
 */
void update_fire(struct chunk *c, struct player *p)
{
	int i, g, o2;
	struct loc grid;
	bool in_pit = square_ispit(c, p->grid) && !p->upkeep->leaping;

	/*** Step 0 -- Begin ***/

	/* Wipe */
	forget_fire(c);

	/*** Step 1 -- player grid ***/

	/* Player grid */
	g = loc_to_grid(p->grid);

	/* Assume fireable */
	fire_info[g] = true;
	sqinfo_on(square(c, p->grid)->info, SQUARE_FIRE);

	/* Save in array */
	fire_g[fire_n++] = g;

	/*** Step 2 -- octants ***/

	/* Scan each octant */
	for (o2 = 0; o2 < 8; o2++) {
		struct vinfo_type *point;

		/* Last added */
		struct vinfo_type *last = &vinfo[0];

		/* Grid queue */
		int queue_head = 0;
		int queue_tail = 0;
		struct vinfo_type *queue[VINFO_MAX_GRIDS*2];

		/* Slope bit vector */
		uint32_t bits0 = VINFO_BITS_0;
		uint32_t bits1 = VINFO_BITS_1;
		uint32_t bits2 = VINFO_BITS_2;
		uint32_t bits3 = VINFO_BITS_3;

		/* Reset queue */
		queue_head = queue_tail = 0;

		/* Initial grids */
		queue[queue_tail++] = &vinfo[1];
		queue[queue_tail++] = &vinfo[2];

		/* Process queue */
		while (queue_head < queue_tail) {
			/* Assume no line of fire */
			bool line_fire = false;

			/* Dequeue next grid */
			point = queue[queue_head++];

			/* Check bits */
			if ((bits0 & (point->bits_0)) ||
			    (bits1 & (point->bits_1)) ||
			    (bits2 & (point->bits_2)) ||
			    (bits3 & (point->bits_3))) {
				bool new = false;

				/* Extract grid value XXX XXX XXX */
				g = loc_to_grid(p->grid) + point->grid[o2];
				grid = grid_to_loc(g);
				new = fire_info[g];

				/* Check bounds */
				if (!square_in_bounds_fully(c, grid)) continue;

				/* If the player is in a pit, skip non-adjacent grids */
				if (in_pit && (distance(grid, p->grid) > 1)) {
					continue;
				}

				/* Check for first possible line of fire */
				i = point->slope_fire_index1;

				/* Check line(s) of fire */
				while (true) {
					switch (i / 32) {
						case 3: {
							if (bits3 & (1L << (i % 32))) line_fire = true;
							break;
						}
						case 2: {
							if (bits2 & (1L << (i % 32))) line_fire = true;
							break;
						}
						case 1: {
							if (bits1 & (1L << (i % 32))) line_fire = true;
							break;
						}
						case 0: {
							if (bits0 & (1L << (i % 32))) line_fire = true;
							break;
						}
					}

					/* Check second LOF slope if necessary */
					if ((!point->slope_fire_index2) || (line_fire) ||
					    (i == point->slope_fire_index2)) {
						break;
					}

					/* Check second possible line of fire */
					i = point->slope_fire_index2;
				}

				/* Record line of fire */
				if (line_fire) {
					fire_info[g] = true;
					sqinfo_on(square(c, grid)->info, SQUARE_FIRE);

					/* Newly fireable grid */
					if (new) {
						/* Save in array */
						fire_g[fire_n++] = g;
					}

					/* Handle wall or non-wall */
					if (square_iswall(c, grid)) {
						/* Clear bits */
						bits0 &= ~(point->bits_0);
						bits1 &= ~(point->bits_1);
						bits2 &= ~(point->bits_2);
						bits3 &= ~(point->bits_3);
					} else {
						/* Enqueue child */
						if (last != point->next_0) {
							queue[queue_tail++] = last = point->next_0;
						}

						/* Enqueue child */
						if (last != point->next_1) {
							queue[queue_tail++] = last = point->next_1;
						}
					}
				}
			}
		}
	}
}

/**
 * Determine the path taken by a projection.  -BEN-, -LM-
 *
 * The projection will always start one grid from the grid (y1,x1), and will
 * travel towards the grid (y2,x2), touching one grid per unit of distance
 * along the major axis, and stopping when it satisfies certain conditions
 * or has travelled the maximum legal distance of "range".  Projections
 * cannot extend further than MAX_SIGHT (at least at present).
 *
 * A projection only considers those grids which contain the line(s) of fire
 * from the start to the end point.  Along any step of the projection path,
 * either one or two grids may be valid options for the next step.  When a
 * projection has a choice of grids, it chooses that which offers the least
 * resistance.  Given a choice of clear grids, projections prefer to move
 * orthogonally.
 *
 * Also, projections to or from the character must stay within the pre-
 * calculated field of fire ("cave_info & (CAVE_FIRE)").  This is a hack.
 * XXX XXX
 *
 * The path grids are saved into the grid array pointed to by "gp", and
 * there should be room for at least "range" grids in "gp".  Note that
 * due to the way in which distance is calculated, this function normally
 * uses fewer than "range" grids for the projection path, so the result
 * of this function should never be compared directly to "range".  Note
 * that the initial grid (y1,x1) is never saved into the grid array, not
 * even if the initial grid is also the final grid.  XXX XXX XXX
 *
 * We modify y2 and x2 if they are too far away, or (for PROJECT_PASS only)
 * if the projection threatens to leave the dungeon.
 *
 * The "flg" flags can be used to modify the behavior of this function:
 *    PROJECT_STOP:  projection stops when it cannot bypass a monster.
 *    PROJECT_CHCK:  projection notes when it cannot bypass a monster.
 *    PROJECT_THRU:  projection extends past destination grid
 *    PROJECT_PASS:  projection passes through walls
 *    PROJECT_INVIS: projection passes through invisible walls (ie unknown ones)
 *
 * This function returns the number of grids (if any) in the path.  This
 * may be zero if no grids are legal except for the starting one.
 */
int project_path(struct chunk *c, struct loc *gp, int range, struct loc grid1,
				 struct loc *grid2, int flg)
{
	int i, j, k;
	int dy, dx;
	int num, dist, octant;
	int n_grids = 0;
	bool line_fire;
	bool full_stop = false;

	struct loc grid_a, grid_b;
	struct loc grid = loc(0, 0), old_grid = loc(0, 0);

	/* Start with all lines of sight unobstructed */
	uint32_t bits0 = VINFO_BITS_0;
	uint32_t bits1 = VINFO_BITS_1;
	uint32_t bits2 = VINFO_BITS_2;
	uint32_t bits3 = VINFO_BITS_3;

	int slope_fire1 = -1, slope_fire2 = 0;

	/* Projections are either vertical or horizontal */
	bool vertical = false;

	/* Require projections to be strictly LOF when possible  XXX XXX */
	//bool require_strict_lof = false;

	/* Count of grids in LOF, storage of LOF grids */
	struct loc tmp_grids[80];

	/* Count of grids in projection path */
	int step;

	/* Remember whether and how a grid is blocked */
	int blockage[2];

	/* Assume no monsters in way */
	bool monster_in_way = false;

	/* Initial grid */
	int16_t g0 = loc_to_grid(grid1);
	int16_t g;

	/* Pointer to vinfo data */
	struct vinfo_type *point;

	/* Handle projections of zero length */
	if ((range <= 0) || loc_eq(grid1, *grid2)) return 0;

	/* Note that the character is the source or target of the projection */
	//if (loc_eq(grid1, player->grid) || loc_eq(*grid2, player->grid)) {
		/* Require strict LOF */
	//	require_strict_lof = true;
	//}

	/* Get position change (signed) */
	dy = (*grid2).y - grid1.y;
	dx = (*grid2).x - grid1.x;

	/* Get distance from start to finish */
	dist = distance(grid1, *grid2);

	/* Must stay within the field of sight XXX XXX */
	if ((dist > z_info->max_sight) && !(flg & PROJECT_LEAVE)) {
		/* Always watch your (+/-) when doing rounded integer math. */
		int round_y = (dy < 0 ? -(dist / 2) : (dist / 2));
		int round_x = (dx < 0 ? -(dist / 2) : (dist / 2));

		/* Rescale the endpoint */
		dy = ((dy * (z_info->max_sight - 1)) + round_y) / dist;
		dx = ((dx * (z_info->max_sight - 1)) + round_x) / dist;
		*grid2 = loc_sum(grid1, loc(dx, dy));
	}

	/* Get the correct octant, establish vertical or horizontal major axis */
	if (dy < 0) {
		/* Up and to the left */
		if (dx < 0) {
			/* More upwards than to the left - octant 4 */
			if (ABS(dy) > ABS(dx)) {
				octant = 5;
				vertical = true;
			} else {
				octant = 4;
			}
		} else {
			if (ABS(dy) > ABS(dx)) {
				octant = 6;
				vertical = true;
			} else {
				octant = 7;
			}
		}
	} else {
		if (dx < 0) {
			if (ABS(dy) > ABS(dx)) {
				octant = 2;
				vertical = true;
			} else {
				octant = 3;
			}
		} else {
			if (ABS(dy) > ABS(dx)) {
				octant = 1;
				vertical = true;
			} else {
				octant = 0;
			}
		}
	}

	/* Scan the octant, find the grid corresponding to the end point */
	for (j = 1; j < VINFO_MAX_GRIDS; j++) {
		int16_t vy, vx;

		/* Point to this vinfo record */
		point = &vinfo[j];

		/* Extract grid value */
		g = g0 + point->grid[octant];

		/* Get axis coordinates */
		vy = GRID_Y(g);
		vx = GRID_X(g);

		/* Allow for negative values XXX XXX XXX */
		if (vy > 256 * 127) {
			vy = vy - (256 * 256);
		}
		if (vx > grid1.x + 127) {
			vy++;
			vx = vx - 256;
		}

		/* Require that grid be correct */
		if ((vy != (*grid2).y) || (vx != (*grid2).x)) continue;

		/* Store lines of fire */
		slope_fire1 = point->slope_fire_index1;
		slope_fire2 = point->slope_fire_index2;

		break;
	}

	/* Note failure XXX XXX */
	if (slope_fire1 == -1) return (0);

	/* Scan the octant, collect all grids having the correct line of fire */
	for (j = 1; j < VINFO_MAX_GRIDS; j++) {
		line_fire = false;

		/* Point to this vinfo record */
		point = &vinfo[j];

		/* See if any lines of sight pass through this grid */
		if (!((bits0 & (point->bits_0)) ||
			  (bits1 & (point->bits_1)) ||
			  (bits2 & (point->bits_2)) ||
			  (bits3 & (point->bits_3)))) {
			continue;
		}

		/*
		 * Extract grid value.  Use pointer shifting to get the
		 * correct grid offset for this octant.
		 */
		g = g0 + point->grid[octant];
		grid = grid_to_loc(g);

		/* Must be legal (this is important) */
		if (!square_in_bounds_fully(c, grid)) continue;

		/* Check for first possible line of fire */
		i = slope_fire1;

		/* Check line(s) of fire */
		while (true) {
			switch (i / 32) {
				case 3: {
					if (bits3 & (1L << (i % 32))) {
						if (point->bits_3 & (1L << (i % 32))) line_fire = true;
					}
					break;
				}
				case 2: {
					if (bits2 & (1L << (i % 32))) {
						if (point->bits_2 & (1L << (i % 32))) line_fire = true;
					}
					break;
				}
				case 1: {
					if (bits1 & (1L << (i % 32))) {
						if (point->bits_1 & (1L << (i % 32))) line_fire = true;
					}
					break;
				}
				case 0: {
					if (bits0 & (1L << (i % 32))) {
						if (point->bits_0 & (1L << (i % 32))) line_fire = true;
					}
					break;
				}
			}

			/* We're done if no second LOF exists, or when we've checked it */
			if ((!slope_fire2) || (i == slope_fire2)) break;

			/* Check second possible line of fire */
			i = slope_fire2;
		}

		/* This grid contains at least one of the lines of fire */
		if (line_fire) {
			/* Do not accept breaks in the series of grids  XXX XXX */
			if (n_grids && distance(grid, old_grid) > 1) {
				break;
			}

			/* Optionally, require strict line of fire */
			//TODO see if this conditional is actually needed; I believe it's
			//purpose is covered by the checks in projectable() - NRM
			//if ((!require_strict_lof) || square_isfire(c, grid) ||
			//   ((flg & (PROJECT_INVIS)) && !square_ismark(c, grid))) {
			/* Store grid value */
			tmp_grids[n_grids++] = grid_to_loc(g);
				//}

			/* Remember previous coordinates */
			old_grid = grid;
		}

		/*
		 * Handle wall (unless ignored).  Walls can be in a projection path,
		 * but the path cannot pass through them.
		 */
		if (!(flg & (PROJECT_PASS)) && square_iswall(c, grid)) {
			if (!(flg & (PROJECT_INVIS)) || square_ismark(c, grid)) {
				/* Clear any lines of sight passing through this grid */
				bits0 &= ~(point->bits_0);
				bits1 &= ~(point->bits_1);
				bits2 &= ~(point->bits_2);
				bits3 &= ~(point->bits_3);
			}
		}
 	}

	/* Scan the grids along the line(s) of fire */
	for (step = 0, j = 0; j < n_grids;) {
		/* Get the coordinates of this grid */
		grid_a = tmp_grids[j];

		/* Get the coordinates of the next grid, if legal */
		if (j < n_grids - 1) {
			grid_b = tmp_grids[j + 1];
		} else {
			grid_b = loc(-1, -1);
		}

		/*
		 * We always have at least one legal grid, and may have two.  Allow
		 * the second grid if its position differs only along the minor axis.
		 */
		if (vertical ? grid_a.y == grid_b.y : grid_a.x == grid_b.x) {
			num = 2;
		} else {
			num = 1;
		}

		/* Scan one or both grids */
		for (i = 0; i < num; i++) {
			blockage[i] = 0;

			/* Get the coordinates of this grid */
			grid = i == 0 ? grid_a : grid_b;

			/* Determine perpendicular distance */
			k = (vertical ? ABS(grid.x - grid1.x) : ABS(grid.y - grid1.y));

			/* Hack -- Check maximum range */
			if ((i == num - 1) && (step + (k >> 1)) >= range - 1) {
				/* End of projection */
				full_stop = true;
			}

			/* Sometimes stop at destination grid */
			if (!(flg & (PROJECT_THRU))) {
				if (loc_eq(grid, *grid2)) {
					/* End of projection */
					full_stop = true;
				}
			}

			/* Usually stop at wall grids */
			if (!(flg & (PROJECT_PASS)) &&
				(!(flg & (PROJECT_INVIS)) || square_ismark(c, grid))) {
				if (!square_isprojectable(c, grid)) {
					blockage[i] = 2;
				}
			} else if (!square_in_bounds_fully(c, grid)) {
				/* If we don't stop at wall grids, explicitly check legality */
				full_stop = true;
				blockage[i] = 3;
			}

			/* Try to avoid monsters/players between the endpoints */
			if ((square_monster(c, grid) || square_isplayer(c, grid)) &&
				(blockage[i] < 2)) {
				/* Hack: ignore monsters on the designated square on request */
				if (!loc_eq(c->project_path_ignore, grid)) {
					if (flg & (PROJECT_STOP)) {
						blockage[i] = 2;
					} else if (flg & (PROJECT_CHCK)) {
						blockage[i] = 1;
					}
				}
			}
		}

		/* Pick the first grid if possible, the second if necessary */
		if ((num == 1) || (blockage[0] <= blockage[1])) {
			/* Store the first grid, advance */
			if (blockage[0] < 3) gp[step++] = tmp_grids[j];

			/* Blockage of 2 or greater means the projection ends */
			if (blockage[0] >= 2) break;

			/* Blockage of 1 means a monster bars the path */
			if (blockage[0] == 1) {
				/* Endpoints are always acceptable */
				if (!loc_eq(grid, *grid2)) monster_in_way = true;
			}

			/* Handle end of projection */
			if (full_stop) break;
		} else {
			/* Store the second grid, advance */
			if (blockage[1] < 3) gp[step++] = tmp_grids[j + 1];

			/* Blockage of 2 or greater means the projection ends */
			if (blockage[1] >= 2) break;

			/* Blockage of 1 means a monster bars the path */
			if (blockage[1] == 1) {
				/* Endpoints are always acceptable */
				if (!loc_eq(grid, *grid2)) monster_in_way = true;
			}

			/* Handle end of projection */
			if (full_stop) break;
		}

		/* Advance to the next unexamined LOF grid */
		j += num;
	}

	/* Accept last grid as the new endpoint if allowed */
	if (!(flg & PROJECT_LEAVE))
		*grid2 = gp[step - 1];

	/* Return count of grids in projection path */
	if (monster_in_way) return -step;

	return step;
}

