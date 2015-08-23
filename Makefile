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
LDFLAGS += -fPIC -Wl,-E -Wl,--as-needed


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

include tools/make/lua-${lua_build}.mk
include tools/make/libdwarf-${libdwarf_build}.mk
include tools/make/dynasm-${jit_arch}.mk

# EOL module sources.
EOL_MODULE_SRCS := eol-module.c eol-trace.c eol-util.c eol-typing.c \
                   eol-typecache.c eol-libdwarf.c
EOL_MODULE_OBJS := $(patsubst %.c,${OUT}/%.o,${EOL_MODULE_SRCS})

# Testutil module source.
TESTUTIL_MODULE_SRCS := tools/harness-testutil.c
TESTUTIL_MODULE_OBJS := $(patsubst %.c,${OUT}/%.o,${TESTUTIL_MODULE_SRCS})

%.inc: %.gperf
	$P gperf $<
	$Q gperf -o $@ $<

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
	 ${OUT}/eol.so \
	 ${OUT}/testutil.so \
	 ${OUT}/libtest2.so \
	 ${OUT}/libtest.so
	$(if ${MAKE_TERMOUT},$Q echo)

clean:
	$Q ${RM} ${OUT}/lua ${LUA_OBJS}
	$Q ${RM} ${OUT}/eol.so ${EOL_MODULE_OBJS}
	$Q ${RM} ${OUT}/testutil.so ${TESTUTIL_MODULE_OBJS}
	$Q ${RM} ${OUT}/libtest.so ${OUT}/libtest.o
	$Q ${RM} ${OUT}/libtest2.so ${OUT}/libtest2.o

eol-module.c: eol-lua.h eol-libdwarf.h specials.inc eol-fcall-${eol_fcall}.c
tools/harness-testutil.c: eol-lua.h

${OUT}/eol.so: ${EOL_MODULE_OBJS} ${LIBDWARF}
${OUT}/eol.so: LDFLAGS += -shared
${OUT}/eol.so: LDLIBS += ${LIBDWARF_LDLIBS}

${OUT}/testutil.so: ${TESTUTIL_MODULE_OBJS}
${OUT}/testutil.so: LDFLAGS += -shared

${OUT}/libtest.so: ${OUT}/libtest.o
${OUT}/libtest.so: LDFLAGS += -shared

${OUT}/libtest2.so: ${OUT}/libtest2.o
${OUT}/libtest2.so: LDFLAGS += -shared

build.conf: configure
	./configure
