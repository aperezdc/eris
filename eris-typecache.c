/*
 * eris-typecache.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eris-typecache.h"
#include "eris-util.h"
#include "uthash.h"


typedef struct _ErisTypeCacheEntry ErisTypeCacheEntry;

struct _ErisTypeCacheEntry {
    uint32_t            offset;
    const ErisTypeInfo *typeinfo;
    UT_hash_handle      hh;
};


void
eris_type_cache_init (ErisTypeCache *cache)
{
    CHECK_NOT_NULL (cache);
    *cache = NULL;
}


void
eris_type_cache_free (ErisTypeCache *cache)
{
    CHECK_NOT_NULL (cache);

    ErisTypeCacheEntry *entry, *tmp;
    HASH_ITER (hh, *cache, entry, tmp) {
        /* XXX: This leaks the ErisTypeInfo. */
        HASH_DEL (*cache, entry);
        free (entry);
    }
}



const ErisTypeInfo*
eris_type_cache_lookup (ErisTypeCache *cache,
                        uint32_t       offset)
{
    CHECK_NOT_NULL (cache);
    CHECK_NOT_ZERO (offset);

    ErisTypeCacheEntry *entry;
    HASH_FIND_INT (*cache, &offset, entry);
    return entry ? entry->typeinfo : NULL;
}


void
eris_type_cache_add (ErisTypeCache      *cache,
                     uint32_t            offset,
                     const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (cache);
    CHECK_NOT_ZERO (offset);
    CHECK_NOT_NULL (typeinfo);

    ErisTypeCacheEntry *entry = malloc (sizeof (ErisTypeCacheEntry));
    entry->offset   = offset;
    entry->typeinfo = typeinfo;

    HASH_ADD_INT (*cache, offset, entry);
}


void
eris_type_cache_foreach (ErisTypeCache    *cache,
                         ErisTypeCacheIter callback,
                         void             *userdata)
{
    CHECK_NOT_NULL (cache);
    CHECK_NOT_NULL (callback);

    ErisTypeCacheEntry *entry, *tmp;
    HASH_ITER (hh, *cache, entry, tmp) {
        if (!(*callback) (cache, entry->typeinfo, userdata))
            break;
    }
}
