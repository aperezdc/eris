/*
 * eris-typecache.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef ERIS_TYPECACHE_H
#define ERIS_TYPECACHE_H

#include "eris-typing.h"
#include <stdbool.h>
#include <stdint.h>


typedef struct _ErisTypeCacheEntry* ErisTypeCache;
typedef bool (*ErisTypeCacheIter) (ErisTypeCache*,
                                   const ErisTypeInfo*,
                                   void *userdata);

extern void eris_type_cache_init (ErisTypeCache *cache);
extern void eris_type_cache_free (ErisTypeCache *cache);

extern void eris_type_cache_add (ErisTypeCache      *cache,
                                 uint32_t            offset,
                                 const ErisTypeInfo *typeinfo);

extern const ErisTypeInfo* eris_type_cache_lookup (ErisTypeCache *cache,
                                                   uint32_t       offset);

extern void eris_type_cache_foreach (ErisTypeCache    *cache,
                                     ErisTypeCacheIter callback,
                                     void             *userdata);

#endif /* !ERIS_TYPECACHE_H */
