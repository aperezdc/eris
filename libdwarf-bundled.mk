LIBDWARF_VERSION := 20150507
LIBDWARF_TARBALL := ${OUT}/downloads/libdwarf-${LIBDWARF_VERSION}.tar.gz
LIBDWARF_SRCPATH := ${OUT}/libdwarf-${LIBDWARF_VERSION}
LIBDWARF         := ${LIBDWARF_SRCPATH}/libdwarf/libdwarf.a
LIBDWARF_LDLIBS  := -lelf
CPPFLAGS         += -I${LIBDWARF_SRCPATH}/libdwarf -DERIS_LIBDWARF_BUNDLED=1


${LIBDWARF}: ${LIBDWARF_SRCPATH}/libdwarf/Makefile
	$Q ${MAKE} -s -C $(dir $<)
	$Q touch $@

${LIBDWARF_SRCPATH}/libdwarf/Makefile \
${LIBDWARF_SRCPATH}/libdwarf/libdwarf.h: ${LIBDWARF_SRCPATH}/libdwarf/configure
	$Q cd ${LIBDWARF_SRCPATH}/libdwarf && \
		CC='${CC}' CFLAGS='-fPIC' ./configure --disable-shared

${LIBDWARF_SRCPATH}/libdwarf/configure \
${LIBDWARF_SRCPATH}/libdwarf/dwarf.h: ${LIBDWARF_TARBALL}
	${RUN_UNTARGZ}
	$Q touch $@

.NOTPARALLEL: ${LIBDWARF_SRCPATH}/libdwarf/configure \
              ${LIBDWARF_SRCPATH}/libdwarf/dwarf.h   \
			  ${LIBDWARF_SRCPATH}/libdwarf/Makefile  \
			  ${LIBDWARF_SRCPATH}/libdwarf/libdwarf.h

${LIBDWARF_TARBALL}: URL = https://github.com/Distrotech/libdwarf/archive/${LIBDWARF_VERSION}.tar.gz
${LIBDWARF_TARBALL}:
	${RUN_FETCH_URL}

${OUT}/eris-module.o: ${LIBDWARF_SRCPATH}/libdwarf/libdwarf.h


libdwarf-clean: ${LIBDWARF_SRCPATH}/libdwarf/Makefile
	$Q ${MAKE} -s -C ${LIBDWARF_SRCPATH}/libdwarf clean

.PHONY: libdwarf-clean
clean: libdwarf-clean


libdwarf-distclean: ${LIBDWARF_SRCPATH}/libdwarf/Makefile
	$Q ${RM} -r ${LIBDWARF_SRCPATH}
	$Q ${RM} ${LIBDWARF_TARBALL}

.PHONY: libdwarf-distclean
clean: libdwarf-distclean
