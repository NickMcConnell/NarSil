/* command/lookup
 *
 * Tests for command lookup
 *
 * Created by: myshkin
 *             1 May 2011
 */

#include "unit-test.h"
#include "obj-properties.h"
#include "object.h"
#include "cmds.h"
#include "ui-keymap.h"
#include "ui-event.h"
#include "ui-game.h"
#include "ui-input.h"
#include "z-virt.h"

int setup_tests(void **state) {
	cmd_init();
	*state = 0;
	return 0;
}

int teardown_tests(void *state) {
	mem_free(state);
	return 0;
}

static int test_cmd_lookup_orig(void *state) {
	require(cmd_lookup('Z', KEYMAP_MODE_ORIG) == CMD_NULL);
	require(cmd_lookup('{', KEYMAP_MODE_ORIG) == CMD_INSCRIBE);
	require(cmd_lookup('a', KEYMAP_MODE_ORIG) == CMD_USE_STAFF);
	require(cmd_lookup('u', KEYMAP_MODE_ORIG) == CMD_USE);
	require(cmd_lookup('g', KEYMAP_MODE_ORIG) == CMD_PICKUP);
	require(cmd_lookup('r', KEYMAP_MODE_ORIG) == CMD_TAKEOFF);
	require(cmd_lookup('/', KEYMAP_MODE_ORIG) == CMD_ALTER);
	
	ok;
}

static int test_cmd_lookup_rogue(void *state) {
	require(cmd_lookup('{', KEYMAP_MODE_ROGUE) == CMD_INSCRIBE);
	require(cmd_lookup('a', KEYMAP_MODE_ROGUE) == CMD_USE_STAFF);
	require(cmd_lookup(KTRL('U'), KEYMAP_MODE_ROGUE) == CMD_USE);
	require(cmd_lookup('g', KEYMAP_MODE_ROGUE) == CMD_PICKUP);
	require(cmd_lookup('r', KEYMAP_MODE_ROGUE) == CMD_TAKEOFF);
	require(cmd_lookup('/', KEYMAP_MODE_ROGUE) == CMD_ALTER);
	
	ok;
}

static int test_cmd_lookup_angband(void *state) {
	require(cmd_lookup('Z', KEYMAP_MODE_ANGBAND) == CMD_NULL);
	require(cmd_lookup('{', KEYMAP_MODE_ANGBAND) == CMD_INSCRIBE);
	require(cmd_lookup('u', KEYMAP_MODE_ANGBAND) == CMD_USE_STAFF);
	require(cmd_lookup('U', KEYMAP_MODE_ANGBAND) == CMD_USE);
	require(cmd_lookup('g', KEYMAP_MODE_ANGBAND) == CMD_PICKUP);
	require(cmd_lookup('t', KEYMAP_MODE_ANGBAND) == CMD_TAKEOFF);
	require(cmd_lookup('+', KEYMAP_MODE_ANGBAND) == CMD_ALTER);
	
	ok;
}

static int test_cmd_lookup_angrogue(void *state) {
	require(cmd_lookup('{', KEYMAP_MODE_ANGROGUE) == CMD_INSCRIBE);
	require(cmd_lookup('u', KEYMAP_MODE_ANGROGUE) == CMD_USE_STAFF);
	require(cmd_lookup(KTRL('U'), KEYMAP_MODE_ANGROGUE) == CMD_USE);
	require(cmd_lookup('g', KEYMAP_MODE_ANGROGUE) == CMD_PICKUP);
	require(cmd_lookup('t', KEYMAP_MODE_ANGROGUE) == CMD_TAKEOFF);
	require(cmd_lookup('+', KEYMAP_MODE_ANGROGUE) == CMD_ALTER);
	
	ok;
}

const char *suite_name = "command/lookup";
struct test tests[] = {
	{ "cmd_lookup_orig",  test_cmd_lookup_orig },
	{ "cmd_lookup_rogue", test_cmd_lookup_rogue },
	{ "cmd_lookup_angband",  test_cmd_lookup_angband },
	{ "cmd_lookup_angrogue", test_cmd_lookup_angrogue },
	{ NULL, NULL }
};
