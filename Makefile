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

ifeq (${__verbose},0)
  P := @printf ' %6s %s\n'
  Q := @
else
  P := @:
  Q :=
endif


-include build.conf

OUT     := ${obj}
PREFIX   = ${prefix}
LDLIBS   = ${libs}
CC       = ${cc}
LDFLAGS += -Wl,-E

# We want -Wall *before* the other CFLAGS, so we have to force its
# expansion and then re-assign to the variable.
EXPAND_CFLAGS := -std=gnu99 -Wall ${CFLAGS}
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
ERIS_MODULE_SRCS := eris-module.c
ERIS_MODULE_OBJS := $(patsubst %.c,${OUT}/%.o,${ERIS_MODULE_SRCS})


${OUT}/%.o: %.c
	$P CC $@
	$Q mkdir -p $(dir $@)
	$Q ${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

${OUT}/%:
	$P LD $@
	$Q mkdir -p $(dir $@)
	$Q ${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^ ${LDLIBS}


all: ${OUT}/eris \
	 ${OUT}/eris.so

clean:
	$P CLEAN
	$Q ${RM} ${OUT}/eris ${LUA_OBJS}
	$Q ${RM} ${OUT}/eris.so ${ERIS_MODULE_OBJS}

${OUT}/eris: ${LUA_OBJS}
${OUT}/eris: LDLIBS += -lm

${OUT}/eris.so: ${ERIS_MODULE_OBJS}
${OUT}/eris.so: LDFLAGS += -shared
${OUT}/eris.so: LDLIBS += -ldwarf -lelf

${OUT}/lua/src/lua.o: CPPFLAGS += -DLUA_PROGNAME='"eris"' \
	                              -DLUA_PROMPT='"(eris) "' \
	                              -DLUA_PROMPT2='"  ...) "'

build.conf: configure
	./configure
