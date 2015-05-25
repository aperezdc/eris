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

# We want -Wall *before* the other CFLAGS, so we have to force its
# expansion and then re-assign to the variable.
EXPAND_CFLAGS := -fPIC -std=gnu99 -Wall ${CFLAGS}
CFLAGS = ${EXPAND_CFLAGS}

# Lua sources.
LUA_SRCS := lapi.c lauxlib.c lbaselib.c lbitlib.c lcode.c lcorolib.c \
            lctype.c ldblib.c ldebug.c ldo.c ldump.c lfunc.c lgc.c   \
            linit.c liolib.c llex.c lmathlib.c lmem.c loadlib.c      \
            lobject.c lopcodes.c loslib.c lparser.c lstate.c ltm.c   \
            lstring.c lstrlib.c ltable.c ltablib.c lundump.c lvm.c   \
            lutf8lib.c lzio.c lua.c
LUA_SRCS := $(addprefix lua/src/,${LUA_SRCS})
LUA_OBJS := $(patsubst %.c,${OUT}/%.o,${LUA_SRCS})

# Eris (non-JIT) module sources.
ERIS_MODULE_SRCS := eris-module.c eris-trace.c eris-util.c eris-typing.c
ERIS_MODULE_OBJS := $(patsubst %.c,${OUT}/%.o,${ERIS_MODULE_SRCS})

# Testutil module source.
TESTUTIL_MODULE_SRCS := testutil-module.c
TESTUTIL_MODULE_OBJS := $(patsubst %.c,${OUT}/%.o,${TESTUTIL_MODULE_SRCS})


${OUT}/%.o: %.c
	$P Compile $@
	$Q mkdir -p $(dir $@)
	$Q ${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${OUT}/%:
	$P Link $@
	$Q mkdir -p $(dir $@)
	$Q ${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^ ${LDLIBS}


all: ${OUT}/eris \
	 ${OUT}/eris.so \
	 ${OUT}/testutil.so \
	 ${OUT}/libtest.so
	$(if ${MAKE_TERMOUT},$Q echo)

clean:
	$Q ${RM} ${OUT}/eris ${LUA_OBJS}
	$Q ${RM} ${OUT}/eris.so ${ERIS_MODULE_OBJS}
	$Q ${RM} ${OUT}/testutil.so ${TESTUTIL_MODULE_OBJS}
	$Q ${RM} ${OUT}/libtest.so ${OUT}/libtest.o

${OUT}/eris: ${LUA_OBJS}
${OUT}/eris: LDLIBS += -lm

${OUT}/eris.so: ${ERIS_MODULE_OBJS}
${OUT}/eris.so: LDFLAGS += -shared
${OUT}/eris.so: LDLIBS += -ldwarf -lelf

${OUT}/testutil.so: ${TESTUTIL_MODULE_OBJS}
${OUT}/testutil.so: LDFLAGS += -shared

${OUT}/libtest.so: ${OUT}/libtest.o
${OUT}/libtest.so: LDFLAGS += -shared

${OUT}/lua/src/lua.o: CPPFLAGS += -DLUA_PROGNAME='"eris"' \
	                              -DLUA_PROMPT='"(eris) "' \
	                              -DLUA_PROMPT2='"  ...) "'

build.conf: configure
	./configure
