/* object/util */

#include "unit-test.h"
#include "unit-test-data.h"

#include "object.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-util.h"

int setup_tests(void **state) {
	player = &test_player;
	player->body = test_player_body;
	player->body.slots = &test_slot_light;
	z_info = mem_zalloc(sizeof(struct angband_constants));
	z_info->fuel_torch = 5000;
	z_info->default_torch = 2000;
	z_info->fuel_lamp = 15000;
	z_info->default_lamp = 7500;
	z_info->k_max = 1;
	k_info = mem_zalloc(z_info->k_max * sizeof(*k_info));

	quarks_init();
    return 0;
}

int teardown_tests(void *state) {
	quarks_free();
	mem_free(k_info);
	mem_free(z_info);
	return 0;
}

/* Regression test for #1661 */
static int test_obj_can_refuel(void *state) {
    struct object obj_torch, obj_lantern, obj_flask;

    /* Torches can be refueled */
    object_prep(&obj_torch, &test_torch, 1, AVERAGE);
	player->gear = &obj_torch;
    player->body.slots->obj = &obj_torch; 

    /* By torches */
    eq(obj_can_refuel(&obj_torch), true);

    /* Not by flasks of oil */
    object_prep(&obj_flask, &test_flask, 1, AVERAGE);
    eq(obj_can_refuel(&obj_flask), false);

    /* Or lanterns */    
    object_prep(&obj_lantern, &test_lantern, 1, AVERAGE);
    eq(obj_can_refuel(&obj_lantern), false);

    /* Lanterns can be refueled */    
	player->gear = &obj_lantern;
    player->body.slots->obj = &obj_lantern; 

    /* Not by torches */
    eq(obj_can_refuel(&obj_torch), false);

    /* Lanterns can be refueled by other lanterns */
    eq(obj_can_refuel(&obj_lantern), true);

    /* ...but not by empty lanterns */
    obj_lantern.timeout = 0;
    eq(obj_can_refuel(&obj_lantern), false);

    /* Lanterns can be refueled by flasks of oil */
    eq(obj_can_refuel(&obj_flask), true);
    ok;
}

/* Test basic functionality of check_for_inscrip_with_int(). */
static int test_basic_check_for_inscrip_with_int(void *state) {
    struct object obj;
    int dummy = 8974;
    int inarg;

    /* No inscription should return zero and leave inarg unchanged. */
    memset(&obj, 0, sizeof(obj));
    inarg = dummy;
    eq(check_for_inscrip_with_int(&obj, "=g", &inarg), 0);
    eq(inarg, dummy);

    /* An inscription not containing the search string should return zero and
     * leave inarg unchanged. */
    obj.note = quark_add("@m1@b1@G1");
    inarg = dummy;
    eq(check_for_inscrip_with_int(&obj, "=g", &inarg), 0);
    eq(inarg, dummy);

    /* An inscription containing the search string but not followed by an
     * integer should return zero and leave inarg unchanged. */
    obj.note = quark_add("=g@m1@b1@G1");
    inarg = dummy;
    eq(check_for_inscrip_with_int(&obj, "=g", &inarg), 0);
    eq(inarg, dummy);

    /* An inscription containing one instance of the search string followed
     * by a nonnegative integer should return one and set inarg to the value
     * of the integer. */
    obj.note = quark_add("=g5@m1@b1@G1");
    inarg = dummy;
    eq(check_for_inscrip_with_int(&obj, "=g", &inarg), 1);
    eq(inarg, 5);

    /* An inscription containing two instances of the search string followed
     * by a nonnegative integer should return two and set inarg to the value
     * of the integer following the first instance. */
    obj.note = quark_add("@m1@b1=g8@G1=g5");
    inarg = dummy;
    eq(check_for_inscrip_with_int(&obj, "=g", &inarg), 2);
    eq(inarg, 8);

    ok;
}

const char *suite_name = "object/util";
struct test tests[] = {
    { "obj_can_refuel", test_obj_can_refuel },
    { "basic_check_for_inscrip_with_uint", test_basic_check_for_inscrip_with_int },
    { NULL, NULL }
};
