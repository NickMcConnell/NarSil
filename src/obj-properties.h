/**
 * \file obj-properties.h
 * \brief definitions and functions for object flags and modifiers
 *
 * Copyright (c) 2014 Chris Carr, Nick McConnell
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
#ifndef INCLUDED_OBJPROPERTIES_H
#define INCLUDED_OBJPROPERTIES_H

#include "z-file.h"
#include "z-bitflag.h"

/**
 * ------------------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------------------ */

/**
 * The values for the "tval" field of various objects.
 *
 * This value is the primary means by which items are sorted in the
 * player inventory, followed by "sval" and "cost".
 */
enum
{
	#define TV(a, b) TV_##a,
	#include "list-tvals.h"
	#undef TV

	TV_MAX
};

/**
 * The object flags
 */
enum {
	OF_NONE,
	#define OF(a, b) OF_##a,
    #include "list-object-flags.h"
    #undef OF
};

/**
 * The object kind flags
 */
enum {
    #define KF(a, b) KF_##a,
    #include "list-kind-flags.h"
    #undef KF
};

/**
 * The object modifiers
 */
enum {
	#define STAT(a) OBJ_MOD_##a,
    #include "list-stats.h"
    #undef STAT
	#define SKILL(a, b) OBJ_MOD_##a,
	#include "list-skills.h"
	#undef SKILL
    #define OBJ_MOD(a) OBJ_MOD_##a,
    #include "list-object-modifiers.h"
    #undef OBJ_MOD
	OBJ_MOD_MAX
};

/* Where the stats start in modifiers */
#define OBJ_MOD_MIN_STAT OBJ_MOD_STR

/**
 * The object flag types
 */
enum object_flag_type {
	OFT_NONE = 0,	/* placeholder flag */
	OFT_SUST,		/* sustains a stat */
	OFT_PROT,		/* protection from an effect */
	OFT_MISC,		/* a good property, suitable for ego items */
	OFT_BASIC,		/* basic obvious property of the item */
	OFT_MELEE,		/* applicable only to melee weapons */
	OFT_BAD,		/* an undesirable flag */

	OFT_MAX
};

/**
 * How object flags are IDd
 */
enum object_flag_id {
	OFID_NONE = 0,		/* never shown */
	OFID_NORMAL,		/* normal ID on use */
	OFID_TIMED,			/* obvious after time */
	OFID_WIELD			/* obvious on wield */
};

/**
 * The object property types
 */
enum obj_property_type {
	OBJ_PROPERTY_NONE = 0,
	OBJ_PROPERTY_STAT,
	OBJ_PROPERTY_SKILL,
	OBJ_PROPERTY_MOD,
	OBJ_PROPERTY_FLAG,
	OBJ_PROPERTY_SLAY,
	OBJ_PROPERTY_BRAND,
	OBJ_PROPERTY_IGNORE,
	OBJ_PROPERTY_RESIST,
	OBJ_PROPERTY_VULN,
	OBJ_PROPERTY_MAX
};

#define OF_SIZE                	FLAG_SIZE(OF_MAX)

#define of_has(f, flag)        	flag_has_dbg(f, OF_SIZE, flag, #f, #flag)
#define of_next(f, flag)       	flag_next(f, OF_SIZE, flag)
#define of_count(f)             flag_count(f, OF_SIZE)
#define of_is_empty(f)         	flag_is_empty(f, OF_SIZE)
#define of_is_full(f)          	flag_is_full(f, OF_SIZE)
#define of_is_inter(f1, f2)    	flag_is_inter(f1, f2, OF_SIZE)
#define of_is_subset(f1, f2)   	flag_is_subset(f1, f2, OF_SIZE)
#define of_is_equal(f1, f2)    	flag_is_equal(f1, f2, OF_SIZE)
#define of_on(f, flag)         	flag_on_dbg(f, OF_SIZE, flag, #f, #flag)
#define of_off(f, flag)        	flag_off(f, OF_SIZE, flag)
#define of_wipe(f)             	flag_wipe(f, OF_SIZE)
#define of_setall(f)           	flag_setall(f, OF_SIZE)
#define of_negate(f)           	flag_negate(f, OF_SIZE)
#define of_copy(f1, f2)        	flag_copy(f1, f2, OF_SIZE)
#define of_union(f1, f2)       	flag_union(f1, f2, OF_SIZE)
#define of_inter(f1, f2)       	flag_inter(f1, f2, OF_SIZE)
#define of_diff(f1, f2)        	flag_diff(f1, f2, OF_SIZE)

#define KF_SIZE                	FLAG_SIZE(KF_MAX)

#define kf_has(f, flag)        	flag_has_dbg(f, KF_SIZE, flag, #f, #flag)
#define kf_next(f, flag)       	flag_next(f, KF_SIZE, flag)
#define kf_is_empty(f)         	flag_is_empty(f, KF_SIZE)
#define kf_is_full(f)          	flag_is_full(f, KF_SIZE)
#define kf_is_inter(f1, f2)    	flag_is_inter(f1, f2, KF_SIZE)
#define kf_is_subset(f1, f2)   	flag_is_subset(f1, f2, KF_SIZE)
#define kf_is_equal(f1, f2)    	flag_is_equal(f1, f2, KF_SIZE)
#define kf_on(f, flag)         	flag_on_dbg(f, KF_SIZE, flag, #f, #flag)
#define kf_off(f, flag)        	flag_off(f, KF_SIZE, flag)
#define kf_wipe(f)             	flag_wipe(f, KF_SIZE)
#define kf_setall(f)           	flag_setall(f, KF_SIZE)
#define kf_negate(f)           	flag_negate(f, KF_SIZE)
#define kf_copy(f1, f2)        	flag_copy(f1, f2, KF_SIZE)
#define kf_union(f1, f2)       	flag_union(f1, f2, KF_SIZE)
#define kf_inter(f1, f2)       	flag_inter(f1, f2, KF_SIZE)
#define kf_diff(f1, f2)        	flag_diff(f1, f2, KF_SIZE)


/**
 * ------------------------------------------------------------------------
 * Structures
 * ------------------------------------------------------------------------ */

/**
 * The object property structure
 */
struct obj_property {
	struct obj_property *next;
	int type;				/* type of property */
	int subtype;			/* subtype of property */
	int id_type;			/* how the property is identified (flags only?) */
	int index;				/* index of the property for its type */
	int smith_cat;			/* category for smithing */
	int smith_diff;			/* difficulty for smithing */
	int smith_cost_type;	/* cost type for smithing (XP, stats, etc) */
	int smith_cost;			/* cost for smithing */
	bool smith_exclude_base;	/* whether or not the property on the
						base smithed item is excluded
						from the difficulty and cost */
	char *name;				/* property name */
	char *adjective;		/* adjective for property */
	char *neg_adj;			/* adjective for negative of property */
	char *msg;				/* message on noticing property */
	char *slay_msg;			/* message on noticing property for "slays" */
	char *desc;				/* extra text for object info */
};

extern struct obj_property *obj_properties;

/**
 * ------------------------------------------------------------------------
 * Functions
 * ------------------------------------------------------------------------ */
struct obj_property *lookup_obj_property(int type, int index);
void create_obj_flag_mask(bitflag *f, int id, ...);
void insert_name(char *buf, size_t size, char *msg, char *name);
void flag_message(int flag, char *name);
bool flag_slay_message(int flag, char *name, char *message, int len);
void element_message(int elem, char *name, bool vuln);
int sustain_flag(int stat);

#endif /* !INCLUDED_OBJPROPERTIES_H */
