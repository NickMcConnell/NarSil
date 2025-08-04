/**
 * \file z-dict.c
 * \brief Implement a generic dictionary type.
 *
 * Copyright (c) 2022 Eric Branlund
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

#include "z-dict.h"
#include "z-virt.h"


struct dict_list_entry {
	void *key;
	void *value;
	struct dict_list_entry *next;
};
struct dict_impl {
	uint32_t (*key_hasher)(const void *key);
	int (*key_comparer)(const void *a, const void *b);
	void (*key_freer)(void *key);
	void (*value_freer)(void *value);
	struct dict_list_entry* lists[512];
};


/**
 * Recurse through the dictionary depth first.
 *
 * \param d is the dictionary to examine.
 * \param element_visitor is a pointer to a function to call for each key/value
 * pair in the dictionary.  It must take three arguments:  the dictionary,
 * a pointer to the structure holding the key/value pair, and a void*.  The
 * latter is the element_closure argument passed to dict_depth_first_recurse().
 * \param element_closure is passed as is as the third argument to
 * element_visitor.
 */
static void dict_depth_first_recurse(dict_type d,
		void (*element_visitor)(dict_type d, struct dict_list_entry *e, void *closure),
		void *element_closure)
{
	int i;

	for (i = 0; i < (int)N_ELEMENTS(d->lists); ++i) {
		struct dict_list_entry *entry = d->lists[i];

		while (entry) {
			struct dict_list_entry *next_entry = entry->next;

			(*element_visitor)(d, entry, element_closure);
			entry = next_entry;
		}
	}
}


/**
 * Help dict_destroy():  release the resources for one key/value pair.
 */
static void dict_free_element(dict_type d, struct dict_list_entry *e,
		void *closure)
{
	if (d->value_freer) {
		(*d->value_freer)(e->value);
	}
	if (d->key_freer) {
		(*d->key_freer)(e->key);
	}
	mem_free(e);
}


/**
 * Create a new dictionary.
 *
 * \param key_hasher points to a function to compute a 32-bit unsigned hash
 * from a key.  It must not be NULL.  Invoking the function on a key with the
 * same internal details must always give the same result.
 * \param key_comparer points to a function to compare two keys.  It must
 * not be NULL.  That function must return zero if the keys are equal and
 * return a non-zero value if the keys are not equal.
 * \param key_freer points to a function to release the resouces associated
 * with a key.  When a key/value pair is released, the key will always be
 * freed after the value.  It may be NULL.
 * \param value_freer points to a function to release the resources associated
 * with a value.  When a key/value pair is released, the key will always be
 * freed after the value.  It may be NULL.
 * \return the created dictionary.  That should be passed to dict_destroy()
 * when it is no longer needed.
 */
dict_type dict_create(uint32_t (*key_hasher)(const void *key),
		int (*key_comparer)(const void *a, const void *b),
		void (*key_freer)(void *key), void (*value_freer)(void *value))
{
	dict_type d = (struct dict_impl*) mem_zalloc(sizeof(struct dict_impl));

	d->key_hasher = key_hasher;
	d->key_comparer = key_comparer;
	d->key_freer = key_freer;
	d->value_freer = value_freer;
	return d;
}


/**
 * Release the resources for a dictionary created by dict_create().
 *
 * \param d is NULL or a dictionary created by dict_create().
 */
void dict_destroy(dict_type d)
{
	if (d) {
		dict_depth_first_recurse(d, dict_free_element, NULL);
		mem_free(d);
	}
}


/**
 * Insert a key value pair into a dictionary.
 *
 * \param d is the dictionary to modify.
 * \param key is the key.
 * \param value is the value.  This must not be NULL.
 * \return true if the key is not already present in the dictionary and the
 * insertion was successful.  In that case, the dictionary assumes ownership
 * of key and value and will release their resources when necessary.  Otherwise,
 * return false.  In that case, the caller is responsible for releasing
 * whatever resources are associated with key and value.
 */
bool dict_insert(dict_type d, void *key, void *value)
{
	uint32_t hash, ind;
	struct dict_list_entry *entry;

	if (!d || !value) {
		return false;
	}

	hash = (*d->key_hasher)(key);
	ind = hash % (uint32_t)N_ELEMENTS(d->lists);
	/* Determine if the key is already present. */
	entry = d->lists[ind];
	while (entry) {
		if (!(*d->key_comparer)(key, entry->key)) {
			return false;
		}
		entry = entry->next;
	}

	/* Insert the entry. */
	entry = mem_alloc(sizeof(*entry));
	entry->key = key;
	entry->value = value;
	entry->next = d->lists[ind];
	d->lists[ind] = entry;
	return true;
}


/**
 * Get the corresponding value in a dictionary for the given key.
 *
 * \param d is the dictionary to examine.
 * \param key is the key to look for.
 * \return the value corresponding to key.  If key is not present, the
 * return value will be NULL.
 */
void *dict_has(dict_type d, const void *key)
{
	uint32_t hash, ind;
	struct dict_list_entry *entry;

	if (!d) {
		return NULL;
	}

	hash = (*d->key_hasher)(key);
	ind = hash % (uint32_t)N_ELEMENTS(d->lists);
	/* Determine if the key is there. */
	entry = d->lists[ind];
	while (entry) {
		if (!(*d->key_comparer)(key, entry->key)) {
			return entry->value;
		}
		entry = entry->next;
	}
	return NULL;
}
