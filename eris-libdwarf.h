/*
 * eris-libdwarf.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef ERIS_LIBDWARF_H
#define ERIS_LIBDWARF_H

#ifdef ERIS_WORKAROUND_DWARF_PUBTYPE_DIE_OFFSET
# if ERIS_WORKAROUND_DWARF_PUBTYPE_DIE_OFFSET
#  define dwarf_pubtype_die_offset dwarf_pubtype_type_die_offset
# endif
#endif /* ERIS_WORKAROUND_DWARF_PUBTYPE_DIE_OFFSET */

#if defined(ERIS_LIBDWARF_BUNDLED) && ERIS_LIBDWARF_BUNDLED
# include "libdwarf.h"
# include "dwarf.h"
#else
# if defined(ERIS_LIBDWARF_LIBDWARF_H) && ERIS_LIBDWARF_LIBDWARF_H
#  include <libdwarf/libdwarf.h>
#  include <libdwarf/dwarf.h>
# else
#  include <libdwarf.h>
#  include <dwarf.h>
# endif /* ERIS_LIBDWARF_LIBDWARF_H */
#endif /* ERIS_LIBDWARF_BUNDLED */

#define DW_TYPE_TAG_NAMES(F) \
    F (base_type)            \
    F (typedef)              \
    F (const_type)           \
    F (array_type)           \
    F (structure_type)

#endif /* !ERIS_LIBDWARF_H */
