#
# Makefile
# Adrian Perez, 2015-04-17 10:59
#

# Handle V=(0|1) in the command line. This must be the first thing done!
__verbose := 0
ifeq ($(origin V),command line)
  ifneq ($(strip $V),0)
    __verbose := 1
  endif
endif

P := @:
Q :=
ifeq (${__verbose},0)
  ifneq (${MAKE_TERMOUT},)
    P := @printf '[1G[K%s %s '
    Q := @
  endif
endif


-include build.conf

OUT     := ${obj}
PREFIX   = ${prefix}
LDLIBS   = ${libs}
CC       = ${cc}
LDFLAGS += -fPIC -Wl,-E


define RUN_FETCH_URL
$P Fetch ${URL}
$Q mkdir -p $(dir $@)
$Q curl -s -L -R -o '$@' '${URL}'
endef

define RUN_UNTARGZ
$P UnTarGz $<
$Q mkdir -p ${OUT}
$Q tar -xzf $< -C ${OUT}
endef

define RUN_STATICLIB
$P StaticLib $@
$Q mkdir -p $(dir $@)
$Q ar cr $@ $^
endef

# We want -Wall *before* the other CFLAGS, so we have to force its
# expansion and then re-assign to the variable.
EXPAND_CFLAGS := -fPIC -std=gnu99 -Wall ${CFLAGS}
CFLAGS = ${EXPAND_CFLAGS}

all:
clean:
distclean: clean
.PHONY: distclean

include lua-${lua_build}.mk
include libdwarf-${libdwarf_build}.mk


# Eris (non-JIT) module sources.
ERIS_MODULE_SRCS := eris-module.c eris-trace.c eris-util.c eris-typing.c \
                    eris-typecache.c
ERIS_MODULE_OBJS := $(patsubst %.c,${OUT}/%.o,${ERIS_MODULE_SRCS})

# Testutil module source.
TESTUTIL_MODULE_SRCS := testutil-module.c
TESTUTIL_MODULE_OBJS := $(patsubst %.c,${OUT}/%.o,${TESTUTIL_MODULE_SRCS})

${OUT}/%.o: ${OUT}/%.c
	$P Compile $@
	$Q mkdir -p $(dir $@)
	$Q ${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${OUT}/%.o: %.c
	$P Compile $@
	$Q mkdir -p $(dir $@)
	$Q ${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${OUT}/%:
	$P Link $@
	$Q mkdir -p $(dir $@)
	$Q ${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^ ${LDLIBS}


all: ${LUA} \
	 ${OUT}/eris.so \
	 ${OUT}/testutil.so \
	 ${OUT}/libtest.so
	$(if ${MAKE_TERMOUT},$Q echo)

clean:
	$Q ${RM} ${OUT}/eris.so ${ERIS_MODULE_OBJS}
	$Q ${RM} ${OUT}/testutil.so ${TESTUTIL_MODULE_OBJS}
	$Q ${RM} ${OUT}/libtest.so ${OUT}/libtest.o

eris-module.c: eris-lua.h eris-libdwarf.h
testutil-module.c: eris-lua.h

${OUT}/eris.so: ${ERIS_MODULE_OBJS} ${LIBDWARF}
${OUT}/eris.so: LDFLAGS += -shared
${OUT}/eris.so: LDLIBS += ${LIBDWARF_LDLIBS}

${OUT}/testutil.so: ${TESTUTIL_MODULE_OBJS}
${OUT}/testutil.so: LDFLAGS += -shared

${OUT}/libtest.so: ${OUT}/libtest.o
${OUT}/libtest.so: LDFLAGS += -shared


build.conf: configure
	./configure
