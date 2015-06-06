/*
 * eris-libdwarf.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eris-libdwarf.h"
#include "eris-trace.h"

#include <stdlib.h>
#include <stdio.h>


#define DW_DEFINE_DEALLOC_FUNC(name, _, enumvalue)                 \
    void dw_dealloc_ ## name (void *location) {                    \
        dw_ ## name ## _t *dd = *((dw_ ## name ## _t**) location); \
        if (dd->name) {                                            \
            dwarf_dealloc (dd->debug, dd->name, enumvalue);        \
            dd->name = NULL;                                       \
        }                                                          \
    }

DW_DEALLOC_TYPES (DW_DEFINE_DEALLOC_FUNC)

#undef DW_DEFINE_DEALLOC_FUNC


char*
dw_die_repr (Dwarf_Debug dbg, Dwarf_Die die)
{
    CHECK_NOT_NULL (dbg);
    CHECK_NOT_NULL (die);

    uint32_t len = 1 + 3 + 1 + 20 + 1 + 1 + 1; /* <DIE offset name>\0 */

    dw_lerror_t  err  = { dbg };
    dw_lstring_t name = { dbg, dw_die_name (die, &err.error) };

    const char *stringrep = name.string;
    if (!stringrep) {
        /* try to get the DW_TAG_* name instead. */
        Dwarf_Half tag;
        dw_lerror_t err = { dbg };
        if (dwarf_tag (die, &tag, &err.error) == DW_DLV_OK)
            if (dwarf_get_TAG_name (tag, &stringrep) != DW_DLV_OK)
                stringrep = NULL;
    }

    if (!stringrep) stringrep = "?";
    len += strlen (stringrep);

    char *result = calloc (len + 1, sizeof (char));
    snprintf (result, len, "<DIE %#lx %s>",
              (unsigned long) dw_die_offset_ (die),
              stringrep);
    return result;
}


char*
dw_die_get_string_attr (Dwarf_Debug  dbg,
                        Dwarf_Die    die,
                        Dwarf_Half   tag,
                        Dwarf_Error *e)
{
    CHECK_NOT_NULL (dbg);
    CHECK_NOT_NULL (die);

    char *result;
    dw_lattr_t value = { dbg };
    if (dwarf_attr (die, tag, &value.attr, e) != DW_DLV_OK ||
        dwarf_formstring (value.attr, &result, e) !=  DW_DLV_OK)
            return NULL;
    return result;
}


bool
dw_die_get_uint_attr (Dwarf_Debug     dbg,
                      Dwarf_Die       die,
                      Dwarf_Half      tag,
                      Dwarf_Unsigned *out,
                      Dwarf_Error    *e)
{
    CHECK_NOT_NULL (dbg);
    CHECK_NOT_NULL (die);
    CHECK_NOT_NULL (out);

    dw_lattr_t value = { dbg };
    return dwarf_attr (die, tag, &value.attr, e) == DW_DLV_OK
        && dwarf_formudata (value.attr, out, e) == DW_DLV_OK;
}


bool
dw_die_get_sint_attr (Dwarf_Debug   dbg,
                      Dwarf_Die     die,
                      Dwarf_Half    tag,
                      Dwarf_Signed *out,
                      Dwarf_Error  *e)
{
    CHECK_NOT_NULL (dbg);
    CHECK_NOT_NULL (die);
    CHECK_NOT_NULL (out);

    dw_lattr_t value = { dbg };
    return dwarf_attr (die, tag, &value.attr, e) == DW_DLV_OK
        && dwarf_formsdata (value.attr, out, e) == DW_DLV_OK;
}


bool
dw_tue_array_get_n_items (Dwarf_Debug     dbg,
                          Dwarf_Die       tue,
                          Dwarf_Unsigned *out,
                          Dwarf_Error    *e)
{
    CHECK_NOT_NULL (dbg);
    CHECK_NOT_NULL (tue);
    CHECK_NOT_NULL (out);

    dw_ldie_t child = { dbg };
    if (dwarf_child (tue, &child.die, e) != DW_DLV_OK)
        return false;

    bool result = false;
    for (;;) {
        Dwarf_Half tag;
        if (dwarf_tag (child.die, &tag, e) != DW_DLV_OK)
            break;

        if ((result = (tag == DW_TAG_subrange_type) &&
             dw_die_get_uint_attrb (child, DW_AT_count, out, e)))
            break;

        dw_ldie_t prev = child;
        if (dwarf_siblingof (dbg, prev.die, &child.die, e) != DW_DLV_OK)
            break;
    }
    return result;
}
