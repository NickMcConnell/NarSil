/**
 * \file generate.h
 * \brief Dungeon generation.
 */


#ifndef GENERATE_H
#define GENERATE_H

#include "monster.h"

#if  __STDC_VERSION__ < 199901L
#define ROOM_LOG  if (OPT(player, cheat_room)) msg
#else
#define ROOM_LOG(...) if (OPT(player, cheat_room)) msg(__VA_ARGS__);
#endif

/**
 * Dungeon allocation places and types, used with alloc_object().
 */
enum
{
	SET_CORR = 0x01, /*!< Hallway */
	SET_ROOM = 0x02, /*!< Room */
	SET_BOTH = 0x03 /*!< Anywhere */
};

enum
{
	TYP_RUBBLE,	/*!< Rubble */
	TYP_OBJECT	/*!< Object */
};

/**
 * Flag for room types
 */
enum {
	ROOMF_NONE,
	#define ROOMF(a, b) ROOMF_##a,
	#include "list-room-flags.h"
	#undef ROOMF
};

#define ROOMF_SIZE FLAG_SIZE(ROOMF_MAX)

#define roomf_has(f, flag) flag_has_dbg(f, ROOMF_SIZE, flag, #f, #flag)
#define roomf_next(f, flag) flag_next(f, ROOMF_SIZE, flag)
#define roomf_count(f) flag_count(f, ROOMF_SIZE)
#define roomf_is_empty(f) flag_is_empty(f, ROOMF_SIZE)
#define roomf_is_full(f) flag_is_full(f, ROOMF_SIZE)
#define roomf_is_inter(f1, f2) flag_is_inter(f1, f2, ROOMF_SIZE)
#define roomf_is_subset(f1, f2) flag_is_subset(f1, f2, ROOMF_SIZE)
#define roomf_is_equal(f1, f2) flag_is_equal(f1, f2, ROOMF_SIZE)
#define roomf_on(f, flag) flag_on_dbg(f, ROOMF_SIZE, flag, #f, #flag)
#define roomf_off(f, flag) flag_off(f, ROOMF_SIZE, flag)
#define roomf_wipe(f) flag_wipe(f, ROOMF_SIZE)
#define roomf_setall(f) flag_setall(f, ROOMF_SIZE)
#define roomf_negate(f) flag_negate(f, ROOMF_SIZE)
#define roomf_copy(f1, f2) flag_copy(f1, f2, ROOMF_SIZE)
#define roomf_union(f1, f2) flag_union(f1, f2, ROOMF_SIZE)
#define roomf_inter(f1, f2) flag_iter(f1, f2, ROOMF_SIZE)
#define roomf_diff(f1, f2) flag_diff(f1, f2, ROOMF_SIZE)


/**
 * Structure to hold the corners of a room
 */
struct rectangle {
	struct loc top_left;
	struct loc bottom_right;
};

/**
 * Structure to hold all "dungeon generation" data
 */
struct dun_data {
    /*!< The profile used to generate the level */
    const struct cave_profile *profile;

    /*!< Array of centers of rooms */
    int cent_n;
    struct loc *cent;

    /*!< Array (cent_n elements) of corners of rooms */
    struct rectangle *corner;

    /*!< Array (cent_n elements) of what dungeon piece each room is in */
    int *piece;

	/*!< Array of connections between rooms */
	bool **connection;

    /*!< Arrays of tunnel grids, marked for the rooms they connect */
    int **tunn1;
    int **tunn2;

    /*!< Whether or not this is a quest level */
    bool quest;
};


struct streamer_profile {
    const char *name;
    int den; /*!< Density of streamers */
    int rng; /*!< Width of streamers */
    int qua; /*!< Number of quartz streamers */
};

/*
 * cave_builder is a function pointer which builds a level.
 */
typedef struct chunk * (*cave_builder) (struct player *p);


struct cave_profile {
    struct cave_profile *next;

    const char *name;
    cave_builder builder;	/*!< Function used to build the level */
    int block_height;		/*!< Default height of dungeon blocks */
    int block_width;		/*!< Default width of dungeon blocks */
    int dun_rooms;			/*!< Maximum number of rooms */
    struct streamer_profile str;	/*!< Used to build mineral streamers*/
    struct room_profile *room_profiles;	/*!< Used to build rooms */
    int n_room_profiles;	/*!< Number of room profiles */
    int alloc;				/*!< Allocation weight for this profile */
};


/**
 * room_builder is a function pointer which builds rooms in the cave given
 * anchor coordinates.
 */
typedef bool (*room_builder) (struct chunk *c, struct loc centre);


/**
 * This tracks information needed to generate the room, including the room's
 * name and the function used to build it.
 */
struct room_profile {
    struct room_profile *next;

    const char *name;
    room_builder builder;	/*!< Function used to build fixed size rooms */
    int hardness;			/*!< Rough level this room can start to appear */
    int level;				/*!< Level on which this room is forced */
    int random;				/*!< Extra chance for this room to appear */
};


/*
 * Information about vault generation
 */
struct vault {
    struct vault *next; 		/*!< Pointer to next vault template */

    char *name;         		/*!< Vault name */
	int16_t index;      		/*!< Vault index */
    char *text;         		/*!< Grid by grid description of vault layout */
    char *typ;					/*!< Vault type */
    bitflag flags[ROOMF_SIZE];	/*!< Vault flags */
    uint8_t hgt;				/*!< Vault height */
    uint8_t wid;				/*!< Vault width */
    uint8_t depth;				/*!< Vault depth */
    uint32_t rarity;				/*!< Vault rarity */
    bool forge;					/*!< Is there a forge in it? */
};



/**
 * Constants for working with random symmetry transforms
 */
#define SYMTR_FLAG_NONE (0)
#define SYMTR_FLAG_NO_ROT (1)
#define SYMTR_FLAG_NO_REF (2)
#define SYMTR_FLAG_FORCE_REF (4)
#define SYMTR_MAX_WEIGHT (32768)

extern struct dun_data *dun;
extern struct vault *vaults;
extern struct room_template *room_templates;

/* generate.c */
void prepare_next_level(struct player *p);
int get_room_builder_count(void);
int get_room_builder_index_from_name(const char *name);
const char *get_room_builder_name_from_index(int i);
int get_level_profile_index_from_name(const char *name);
const char *get_level_profile_name_from_index(int i);

/* gen-cave.c */
struct chunk *cave_gen(struct player *p);
struct chunk *throne_gen(struct player *p);
struct chunk *gates_gen(struct player *p);

/* gen-room.c */
struct vault *random_vault(int depth, const char *typ, bool forge);
void fill_rectangle(struct chunk *c, int y1, int x1, int y2, int x2, int feat,
					int flag);
void generate_mark(struct chunk *c, int y1, int x1, int y2, int x2, int flag);
void draw_rectangle(struct chunk *c, int y1, int x1, int y2, int x2, int feat, 
					int flag, bool overwrite_perm);
void set_marked_granite(struct chunk *c, struct loc grid, int flag);
bool build_vault(struct chunk *c, struct loc centre, struct vault *v,
				 bool flip);

bool build_simple(struct chunk *c, struct loc centre);
bool build_crossed(struct chunk *c, struct loc centre);
bool build_interesting(struct chunk *c, struct loc centre);
bool build_lesser_vault(struct chunk *c, struct loc centre);
bool build_greater_vault(struct chunk *c, struct loc centre);
bool build_throne(struct chunk *c, struct loc centre);
bool build_gates(struct chunk *c, struct loc centre);
bool room_build(struct chunk *c, struct room_profile profile);


/* gen-util.c */
extern uint8_t get_angle_to_grid[41][41];

int *cave_find_init(struct loc top_left, struct loc bottom_right);
void cave_find_reset(int *state);
bool cave_find_get_grid(struct loc *grid, int *state);

bool cave_find_in_range(struct chunk *c, struct loc *grid, struct loc top_left,
	struct loc bottom_right, square_predicate pred);
bool cave_find(struct chunk *c, struct loc *grid, square_predicate pred);
bool find_empty(struct chunk *c, struct loc *grid);
bool find_empty_range(struct chunk *c, struct loc *grid, struct loc top_left,
					  struct loc bottom_right);
bool find_nearby_grid(struct chunk *c, struct loc *grid, struct loc centre,
					  int yd, int xd);
bool new_player_spot(struct chunk *c, struct player *p);
int trap_placement_chance(struct chunk *c, struct loc grid);
void place_traps(struct chunk *c);
void place_item_near_player(struct chunk *c, struct player *p, int tval,
							const char *name);
void place_object(struct chunk *c, struct loc grid, int level, bool good,
	bool great, uint8_t origin, struct drop *drop);
void place_secret_door(struct chunk *c, struct loc grid);
void place_closed_door(struct chunk *c, struct loc grid);
void place_random_door(struct chunk *c, struct loc grid);
void place_forge(struct chunk *c, struct loc grid);
void alloc_stairs(struct chunk *c, int feat, int num);
int alloc_object(struct chunk *c, int set, int typ, int num, int depth,
	uint8_t origin);
struct room_profile lookup_room_profile(const char *name);
void uncreate_artifacts(struct chunk *c);
void uncreate_greater_vaults(struct chunk *c, struct player *p);
void chunk_validate_objects(struct chunk *c);
void dump_level_simple(const char *basefilename, const char *title,
	struct chunk *c);
void dump_level(ang_file *fo, const char *title, struct chunk *c, int **dist);
void dump_level_header(ang_file *fo, const char *title);
void dump_level_body(ang_file *fo, const char *title, struct chunk *c,
	int **dist);
void dump_level_footer(ang_file *fo);


#endif /* !GENERATE_H */
