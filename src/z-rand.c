/**
 * \file z-rand.c
 * \brief A Random Number Generator for Angband
 *
 * Copyright (c) 1997 Ben Harrison, Randy Hutson
 * 
 * See below for copyright on the WELL random number generator.
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
#include "z-rand.h"
#ifdef _WIN32
#include <windows.h> /* GetCurrentProcessId() */
#endif

/**
 * This file provides a pseudo-random number generator.
 *
 * This code provides both a "quick" random number generator (4 bytes of
 * state), and a "complex" random number generator (128 + 4 bytes of state).
 *
 * The complex RNG (used for most game entropy) is provided by the WELL102a
 * algorithm, used with permission. See below for copyright information
 * about the WELL implementation.
 *
 * To use of the "simple" RNG, activate it via "Rand_quick = true" and
 * "Rand_value = seed". After that it will be automatically used instead of
 * the "complex" RNG. When you are done, you can de-activate it via
 * "Rand_quick = false". You can also choose a new seed.
 */

/* begin WELL RNG
 * *************************************************************************
 * Copyright:  Francois Panneton and Pierre L'Ecuyer, University of Montreal
 *             Makoto Matsumoto, Hiroshima University                       
 * *************************************************************************
 * Code was modified slightly by Erik Osheim to work on unsigned integers.  
 */
#define M1 3
#define M2 24
#define M3 10

#define MAT0POS(t, v) (v ^ (v >> t))
#define MAT0NEG(t, v) (v ^ (v << (-(t))))
#define Identity(v) (v)

uint32_t state_i = 0;
uint32_t STATE[RAND_DEG] = {0, 0, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0};
uint32_t z0, z1, z2;

#define V0    STATE[state_i]
#define VM1   STATE[(state_i + M1) & 0x0000001fU]
#define VM2   STATE[(state_i + M2) & 0x0000001fU]
#define VM3   STATE[(state_i + M3) & 0x0000001fU]
#define VRm1  STATE[(state_i + 31) & 0x0000001fU]
#define newV0 STATE[(state_i + 31) & 0x0000001fU]
#define newV1 STATE[state_i]

static uint32_t WELLRNG1024a (void){
	z0      = VRm1;
	z1      = Identity(V0) ^ MAT0POS (8, VM1);
	z2      = MAT0NEG (-19, VM2) ^ MAT0NEG(-14,VM3);
	newV1   = z1 ^ z2; 
	newV0   = MAT0NEG (-11,z0) ^ MAT0NEG(-7,z1) ^ MAT0NEG(-13,z2);
	state_i = (state_i + 31) & 0x0000001fU;
	return STATE[state_i];
}
/* end WELL RNG */

/**
 * Simple RNG, implemented with a linear congruent algorithm.
 */
#define LCRNG(X) ((X) * 1103515245 + 12345)


/**
 * Whether to use the simple RNG or not.
 */
bool Rand_quick = true;

/**
 * The current "seed" of the simple RNG.
 */
uint32_t Rand_value;

static bool rand_fixed = false;
static uint32_t rand_fixval = 0;

/**
 * Initialize the complex RNG using a new seed.
 */
void Rand_state_init(uint32_t seed)
{
	int i, j;

	/* Seed the table */
	STATE[0] = seed;

	/* Propagate the seed */
	for (i = 1; i < RAND_DEG; i++)
		STATE[i] = LCRNG(STATE[i - 1]);

	/* Cycle the table ten times per degree */
	for (i = 0; i < RAND_DEG * 10; i++) {
		/* Acquire the next index */
		j = (state_i + 1) % RAND_DEG;

		/* Update the table, extract an entry */
		STATE[j] += STATE[state_i];

		/* Advance the index */
		state_i = j;
	}
}

/**
 * Initialise the RNG
 */
void Rand_init(void)
{
	/* Init RNG */
	if (Rand_quick) {
		uint32_t seed;

		/* Basic seed */
		seed = (uint32_t)(time(NULL));

#ifdef UNIX
		/* Mutate the seed on Unix machines */
		seed = ((seed >> 3) * (getpid() << 1));
#elif defined(_WIN32)
		/* Or on Windows */
		seed = ((seed >> 3) * (GetCurrentProcessId() << 1));
#endif

		/* Use the complex RNG */
		Rand_quick = false;

		/* Seed the "complex" RNG */
		Rand_state_init(seed);
	}
}


/**
 * Extract a "random" number from 0 to m - 1, via division.
 *
 * This method selects "random" 28-bit numbers, and then uses division to drop
 * those numbers into "m" different partitions, plus a small non-partition to
 * reduce bias, taking as the final value the first "good" partition that a
 * number falls into.
 *
 * This method has no bias, and is much less affected by patterns in the "low"
 * bits of the underlying RNG's. However, it is potentially non-terminating.
 */
uint32_t Rand_div(uint32_t m)
{
	uint32_t n, r = 0;

	/* Division by zero will result if m is larger than 0x10000000 */
	assert(m <= 0x10000000);

	/* Hack -- simple case */
	if (m <= 1) return (0);

	if (rand_fixed)
		return (rand_fixval * 1000 * (m - 1)) / (100 * 1000);

	/* Partition size */
	n = (0x10000000 / m);

	if (Rand_quick) {
		/* Use a simple RNG */
		/* Wait for it */
		while (1) {
			/* Cycle the generator */
			r = (Rand_value = LCRNG(Rand_value));

			/* Mutate a 28-bit "random" number */
			r = ((r >> 4) & 0x0FFFFFFF) / n;

			/* Done */
			if (r < m) break;
		}
	} else {
		/* Use a complex RNG */
		while (1) {
			/* Get the next pseudorandom number */
			r = WELLRNG1024a();

			/* Mutate a 28-bit "random" number */
			r = ((r >> 4) & 0x0FFFFFFF) / n;

			/* Done */
			if (r < m) break;
		}
	}

	/* Use the value */
	return (r);
}


/**
 * The number of entries in the "Rand_normal_table"
 */
#define RANDNOR_NUM	256

/**
 * The standard deviation of the "Rand_normal_table"
 */
#define RANDNOR_STD	64

/**
 * The normal distribution table for the "Rand_normal()" function (below)
 */
static int16_t Rand_normal_table[RANDNOR_NUM] = {
	206,   613,   1022,  1430,  1838,  2245,  2652,  3058,
	3463,  3867,  4271,  4673,  5075,  5475,  5874,  6271,
	6667,  7061,  7454,  7845,  8234,  8621,  9006,  9389,
	9770,  10148, 10524, 10898, 11269,	11638,	12004,	12367,
	12727, 13085, 13440, 13792, 14140,	14486,	14828,	15168,
	15504, 15836, 16166, 16492, 16814,	17133,	17449,	17761,
	18069, 18374, 18675, 18972, 19266,	19556,	19842,	20124,
	20403, 20678, 20949, 21216, 21479,	21738,	21994,	22245,

	22493, 22737, 22977, 23213, 23446,	23674,	23899,	24120,
	24336, 24550, 24759, 24965, 25166,	25365,	25559,	25750,
	25937, 26120, 26300, 26476, 26649,	26818,	26983,	27146,
	27304, 27460, 27612, 27760, 27906,	28048,	28187,	28323,
	28455, 28585, 28711, 28835, 28955,	29073,	29188,	29299,
	29409, 29515, 29619, 29720, 29818,	29914,	30007,	30098,
	30186, 30272, 30356, 30437, 30516,	30593,	30668,	30740,
	30810, 30879, 30945, 31010, 31072,	31133,	31192,	31249,

	31304, 31358, 31410, 31460, 31509,	31556,	31601,	31646,
	31688, 31730, 31770, 31808, 31846,	31882,	31917,	31950,
	31983, 32014, 32044, 32074, 32102,	32129,	32155,	32180,
	32205, 32228, 32251, 32273, 32294,	32314,	32333,	32352,
	32370, 32387, 32404, 32420, 32435,	32450,	32464,	32477,
	32490, 32503, 32515, 32526, 32537,	32548,	32558,	32568,
	32577, 32586, 32595, 32603, 32611,	32618,	32625,	32632,
	32639, 32645, 32651, 32657, 32662,	32667,	32672,	32677,

	32682, 32686, 32690, 32694, 32698,	32702,	32705,	32708,
	32711, 32714, 32717, 32720, 32722,	32725,	32727,	32729,
	32731, 32733, 32735, 32737, 32739,	32740,	32742,	32743,
	32745, 32746, 32747, 32748, 32749,	32750,	32751,	32752,
	32753, 32754, 32755, 32756, 32757,	32757,	32758,	32758,
	32759, 32760, 32760, 32761, 32761,	32761,	32762,	32762,
	32763, 32763, 32763, 32764, 32764,	32764,	32764,	32765,
	32765, 32765, 32765, 32766, 32766, 32766, 32766, 32767,
};


/**
 * Generate a random integer number of NORMAL distribution
 *
 * The table above is used to generate a psuedo-normal distribution, in a
 * manner which is much faster than calling a transcendental function to
 * calculate a true normal distribution.
 *
 * Basically, entry 64 * N in the table above represents the number of times
 * out of 32767 that a random variable with normal distribution will fall
 * within N standard deviations of the mean.  That is, about 68 percent of the
 * time for N=1 and 95 percent of the time for N=2.
 *
 * The table above contains a "faked" final entry which allows us to pretend
 * that all values in a normal distribution are strictly less than four
 * standard deviations away from the mean.  This results in "conservative"
 * distribution of approximately 1/32768 values.
 *
 * Note that the binary search takes up to 16 quick iterations.
 */
int16_t Rand_normal(int mean, int stand)
{
	int16_t tmp, offset;

	int16_t low = 0;
	int16_t high = RANDNOR_NUM;

	/* Paranoia */
	if (stand < 1) return (mean);

	/* Roll for probability */
	tmp = (int16_t)randint0(32768);

	/* Binary Search */
	while (low < high) {
		int mid = (low + high) >> 1;

		/* Move right if forced */
		if (Rand_normal_table[mid] < tmp) {
			low = mid + 1;
		} else {
			high = mid;
		}
	}

	/* Convert the index into an offset */
	offset = (int16_t)((long)stand * (long)low / RANDNOR_STD);

	/* One half should be negative */
	if (one_in_(2)) return (mean - offset);

	/* One half should be positive */
	return (mean + offset);
}


/**
 * Choose an integer from a distribution where we know the mean and approximate
 * upper and lower bounds.
 *
 * We divide the imagined distribution into two halves, above and below the
 * mean, and then treat the bounds as if they are the given number of
 * standard deviations from the mean in the appropriate direction.  Note that
 * `stand_u` and `stand_l` are 10 times the number of standart deviations we
 * are asking for.
 * The function chooses an integer from a normal distribution, and then scales
 * it to fit the target distribution.
 */
int Rand_sample(int mean, int upper, int lower, int stand_u, int stand_l)
{
	int pick = Rand_normal(0, 1000);

	/* Scale to fit */
	if (pick > 0) {
		/* Positive pick, scale up */
		pick *= (upper - mean);
		pick /= (100 * stand_u);
	} else if (pick < 0) {
		/* Negative pick, scale down */
		pick *= (mean - lower);
		pick /= (100 * stand_l);
	}

	return mean + pick;
}

/**
 * Generates damage for "2d6" style dice rolls
 */
int damroll(int num, int sides)
{
	int i;
	int sum = 0;

	if (sides <= 0) return 0;

	for (i = 0; i < num; i++)
		sum += randint1(sides);
	return sum;
}



/**
 * Calculation helper function for damroll
 */
int damcalc(int num, int sides, aspect dam_aspect)
{
	switch (dam_aspect) {
		case MAXIMISE:
		case EXTREMIFY: return num * sides;
		case RANDOMISE: return damroll(num, sides);
		case MINIMISE: return num;
		case AVERAGE: return num * (sides + 1) / 2;
	}

	assert(0 && "Should never reach here");
	return 0;
}


/**
 * Generates a random signed long integer X where `A` <= X <= `B`.
 * The integer X falls along a uniform distribution.
 *
 * Note that "rand_range(0, N-1)" == "randint0(N)".
 */
int rand_range(int A, int B)
{
	if (A == B) return A;
	assert(A < B);

	return A + (int32_t)Rand_div(1 + B - A);
}


/**
 * Help determine an "enchantment bonus" for an object.
 */
int16_t m_bonus(int divisor, int level)
{
	if (!divisor) return 0;
	return (level + randint1(10)) / divisor;
}


/**
 * Calculation helper function for m_bonus
 */
int16_t m_bonus_calc(int divisor, int level, aspect bonus_aspect)
{
	if (!divisor) return 0;
	switch (bonus_aspect) {
		case EXTREMIFY:
		case MAXIMISE:  return (level + 10) / divisor;
		case RANDOMISE: return m_bonus(divisor, level);
		case MINIMISE:  return 0;
		case AVERAGE:   return (2 * level + 11) / (2 * divisor);
	}

	assert(0 && "Should never reach here");
	return 0;
}


/**
 * Calculation helper function for random_value structs
 */
int randcalc(random_value v, int level, aspect rand_aspect)
{
	if (rand_aspect == EXTREMIFY) {
		int min = randcalc(v, level, MINIMISE);
		int max = randcalc(v, level, MAXIMISE);
		return abs(min) > abs(max) ? min : max;

	} else {
		int dmg   = damcalc(v.dice, v.sides, rand_aspect);
		int bonus = m_bonus_calc(v.m_bonus, level, rand_aspect);
		return v.base + dmg + bonus;
	}
}


/**
 * Test to see if a value is within a random_value's range
 */
bool randcalc_valid(random_value v, int test)
{
	if (test < randcalc(v, 0, MINIMISE))
		return false;
	else if (test > randcalc(v, 0, MAXIMISE))
		return false;
	else
		return true;
}

/**
 * Test to see if a random_value actually varies
 */
bool randcalc_varies(random_value v)
{
	return randcalc(v, 0, MINIMISE) != randcalc(v, 0, MAXIMISE);
}

/**
 * Roll on a random chance and check for success.
 *
 * \param c The random_chance to roll on
 */
bool random_chance_check(random_chance c)
{
	/* Calculated so that high rolls pass the check */
	return randint0(c.denominator) >= c.denominator - c.numerator;
}

/**
 * Scales a random chance to use the denominator provided in the scale argument
 * and returns the appropriate numerator. For example, a chance of 7 / 13 (53.8%)
 * with scale 100 would be 53. For extra integer precision, a scale of 1000 would
 * yield 538.
 *
 * \param c The random_chance to scale
 * \param scale The scale by which the ratio is multiplied
 */
int random_chance_scaled(random_chance c, int scale)
{
	return scale * c.numerator / c.denominator;
}

/**
 * Cause the output from Rand_div() to be fixed rather than random.
 *
 * \param val Is the percent of the maximum value that Rand_div() will
 * return.  val should be between 0 and 100, inclusive.
 */
void rand_fix(uint32_t val)
{
	rand_fixed = true;
	rand_fixval = val;
}

/**
 * Another simple RNG that does not use any of the above state
 * (so can be used without disturbing the game's RNG state)
 */
uint32_t Rand_simple(uint32_t m)
{
	static uint32_t seed;
	uint32_t v = (uint32_t)time(NULL);

#ifdef UNIX
	seed = LCRNG(seed % m) + ((v << 16) ^ v ^ getpid());
#elif defined(_WIN32)
	seed = LCRNG(seed % m) + ((v << 16) ^ v ^ GetCurrentProcessId());
#else
	seed = LCRNG(seed % m) + ((v << 16) ^ v);
#endif
	return (seed % m);
}
