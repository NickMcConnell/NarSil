/**
 * \file z-dict.h
 * \brief Provide a generic dictionary interface.
 */

#ifndef INCLUDED_Z_DICT_H
#define INCLUDED_Z_DICT_H

#include "h-basic.h"

typedef struct dict_impl *dict_type;

dict_type dict_create(uint32_t (*key_hasher)(const void *key),
	int (*key_comparer)(const void *a, const void *b),
	void (*key_freer)(void *key), void (*value_freer)(void *value));
void dict_destroy(dict_type d);
bool dict_insert(dict_type d, void *key, void *value);
void *dict_has(dict_type d, const void *key);

#endif /* INCLUDED_Z_DICT_H */
