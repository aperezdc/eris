/*
 * eol-typecache.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef EOL_TYPECACHE_H
#define EOL_TYPECACHE_H

#include "eol-typing.h"
#include <stdbool.h>
#include <stdint.h>


typedef struct _EolTypeCacheEntry* EolTypeCache;
typedef bool (*EolTypeCacheIter)  (EolTypeCache*,
                                   const EolTypeInfo*,
                                   void *userdata);

extern void eol_type_cache_init (EolTypeCache *cache);
extern void eol_type_cache_free (EolTypeCache *cache);

extern void eol_type_cache_add (EolTypeCache      *cache,
                                uint32_t           offset,
                                const EolTypeInfo *typeinfo);

extern const EolTypeInfo* eol_type_cache_lookup (EolTypeCache *cache,
                                                 uint32_t      offset);

extern void eol_type_cache_foreach (EolTypeCache    *cache,
                                    EolTypeCacheIter callback,
                                    void             *userdata);

#endif /* !EOL_TYPECACHE_H */
