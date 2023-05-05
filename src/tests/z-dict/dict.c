/* z-dict/dict */

#include "unit-test.h"
#include "z-dict.h"
#include "z-rand.h"
#include "z-util.h"
#include "z-virt.h"


struct dummy_value {
	void *p1, *p2;
};
struct dict_test_state {
	dict_type last_dict;
	char *names[512];
	struct dummy_value *values[512];
	int n;
};


static int key_comparer(const void *a, const void *b)
{
	return strcmp((const char*)a, (const char*)b);
}


static uint32_t key_hasher(const void *key)
{
	return djb2_hash((const char*)key);
}


static void key_freer(void *key)
{
	string_free((char*)key);
}


static void value_freer(void *value)
{
	struct dummy_value *dv = (struct dummy_value*) value;

	mem_free(dv->p1);
	mem_free(dv->p2);
	mem_free(dv);
}


int setup_tests(void **state)
{
	struct dict_test_state *dts = mem_zalloc(sizeof(*dts));

	*state = dts;
	return 0;
}


int teardown_tests(void *state)
{
	struct dict_test_state *dts = (struct dict_test_state*) state;
	int i;

	dict_destroy(dts->last_dict);
	for (i = 0; i < dts->n; ++i) {
		string_free(dts->names[i]);
		value_freer(dts->values[i]);
	}
	mem_free(dts);
	return 0;
}


static int test_empty(void *state)
{
	struct dict_test_state *dts = (struct dict_test_state*) state;

	dts->last_dict = dict_create(key_hasher, key_comparer, key_freer,
		value_freer);
	require(dts->last_dict != NULL);
	require(dict_has(dts->last_dict, "abcdef") == NULL);
	require(dict_has(dts->last_dict, "gheig") == NULL);
	dict_destroy(dts->last_dict);
	dts->last_dict = NULL;
	ok;
}


static int test_one(void *state)
{
	struct dict_test_state *dts = (struct dict_test_state*) state;
	struct dummy_value *v;

	dts->last_dict = dict_create(key_hasher, key_comparer, key_freer,
		value_freer);
	require(dts->last_dict != NULL);
	dts->names[0] = string_make("abcdef");
	dts->values[0] = mem_alloc(sizeof(*dts->values[0]));
	dts->values[0]->p1 = mem_alloc(1);
	dts->values[0]->p2 = mem_alloc(1);
	dts->n = 1;
	require(dict_insert(dts->last_dict, dts->names[0], dts->values[0]));
	dts->n = 0; /* The dictionary now has ownership. */
	v = (struct dummy_value*) dict_has(dts->last_dict, dts->names[0]);
	require(v == dts->values[0] && v->p1 == dts->values[0]->p1
		&& v->p2 == dts->values[0]->p2);
	require(!dict_insert(dts->last_dict, dts->names[0], dts->values[0]));
	require(dict_has(dts->last_dict, "abcdee") == NULL);
	require(dict_has(dts->last_dict, "bbcdef") == NULL);
	require(dict_has(dts->last_dict, "abccef") == NULL);
	dict_destroy(dts->last_dict);
	dts->last_dict = NULL;
	ok;
}


static int test_many(void *state)
{
	struct dict_test_state *dts = (struct dict_test_state*) state;
	char lc[] = "abcdefghijklmnopqrstuvwxyz";
	char template[7] = "aaaaaa";
	int inds[6] = { 0, 1, 2, 3, 4, 5 };
	int i;

	/* Randomly shuffle which part of the template will be modified. */
	for (i = 0; i < (int)N_ELEMENTS(inds); ++i) {
		int j = randint0((int)N_ELEMENTS(inds) - i) + i;
		int k = inds[j];

		inds[j] = inds[i];
		inds[i] = k;
	}
	/* Generate names and values. */
	for (i = 0; i < (int)N_ELEMENTS(dts->names); ++i) {
		int j = i, k;

		for (k = 0; k < (int)N_ELEMENTS(inds); ++k, j /= 26) {
			template[inds[k]] = lc[j % 26];
		}
		dts->names[i] = string_make(template);
		dts->values[i] = mem_alloc(sizeof(*dts->values[i]));
		dts->values[i]->p1 = mem_alloc(1);
		dts->values[i]->p2 = mem_alloc(1);
	}
	dts->n = (int)N_ELEMENTS(dts->names);

	dts->last_dict = dict_create(key_hasher, key_comparer, key_freer,
		value_freer);
	require(dts->last_dict != NULL);
	/* Insert the names and values. */
	for (i = (int)N_ELEMENTS(dts->names) - 1; i >= 0; --i) {
		require(dict_insert(dts->last_dict, dts->names[i],
			dts->values[i]));
		dts->n = i; /* The dictionary now has ownership. */
	}
	/* Verify they are all there. */
	for (i = 0; i < (int)N_ELEMENTS(dts->names); ++i) {
		struct dummy_value *v = (struct dummy_value*)
			dict_has(dts->last_dict, dts->names[i]);

		require(v == dts->values[i] && v->p1 == dts->values[i]->p1
			&& v->p2 == dts->values[i]->p2);
		require(!dict_insert(dts->last_dict, dts->names[i],
			dts->values[i]));
	}
	/* Check for some that are not there. */
	for (i = (int)N_ELEMENTS(dts->names);
			i < (int)N_ELEMENTS(dts->names) + 20; ++i) {
		int j = i, k;

		for (k = 0; k < (int)N_ELEMENTS(inds); ++k, j /= 26) {
			template[inds[k]] = lc[j % 26];
		}
		require(!dict_has(dts->last_dict, template));
	}
	dict_destroy(dts->last_dict);
	dts->last_dict = NULL;
	ok;
}


const char *suite_name = "z-dict/dict";
struct test tests[] = {
	{ "empty", test_empty },
	{ "one", test_one },
	{ "many", test_many },
	{ NULL, NULL },
};
