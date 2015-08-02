/*
 * eol-typecache.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eol-typecache.h"
#include "eol-util.h"
#include "uthash.h"


typedef struct _EolTypeCacheEntry EolTypeCacheEntry;

struct _EolTypeCacheEntry {
    uint32_t           offset;
    const EolTypeInfo *typeinfo;
    UT_hash_handle     hh;
};


void
eol_type_cache_init (EolTypeCache *cache)
{
    CHECK_NOT_NULL (cache);
    *cache = NULL;
}


void
eol_type_cache_free (EolTypeCache *cache)
{
    CHECK_NOT_NULL (cache);

    EolTypeCacheEntry *entry, *tmp;
    HASH_ITER (hh, *cache, entry, tmp) {
        /* XXX: This leaks the EolTypeInfo. */
        HASH_DEL (*cache, entry);
        free (entry);
    }
}



const EolTypeInfo*
eol_type_cache_lookup (EolTypeCache *cache,
                       uint32_t      offset)
{
    CHECK_NOT_NULL (cache);

    EolTypeCacheEntry *entry;
    HASH_FIND_INT (*cache, &offset, entry);
    return entry ? entry->typeinfo : NULL;
}


void
eol_type_cache_add (EolTypeCache      *cache,
                    uint32_t           offset,
                    const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (cache);
    CHECK_NOT_NULL (typeinfo);

    EolTypeCacheEntry *entry = malloc (sizeof (EolTypeCacheEntry));
    entry->offset   = offset;
    entry->typeinfo = typeinfo;

    HASH_ADD_INT (*cache, offset, entry);
}


void
eol_type_cache_foreach (EolTypeCache    *cache,
                        EolTypeCacheIter callback,
                        void            *userdata)
{
    CHECK_NOT_NULL (cache);
    CHECK_NOT_NULL (callback);

    EolTypeCacheEntry *entry, *tmp;
    HASH_ITER (hh, *cache, entry, tmp) {
        if (!(*callback) (cache, entry->typeinfo, userdata))
            break;
    }
}
