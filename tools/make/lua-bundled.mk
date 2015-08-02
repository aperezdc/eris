LUA_VERSION := 5.3.1
LUA_TARBALL := ${OUT}/downloads/lua-${LUA_VERSION}.tar.gz
LUA_SRCPATH := ${OUT}/lua-${LUA_VERSION}
LUA_LIB     := ${OUT}/liblua.a
LUA         := ${OUT}/lua
CPPFLAGS    += -I${LUA_SRCPATH}/src -DEOL_LUA_BUNDLED=1 -DLUA_USE_DLOPEN=1


LUA_LIB_SRCS := lapi.c lauxlib.c lbaselib.c lbitlib.c lcode.c lcorolib.c \
                lctype.c ldblib.c ldebug.c ldo.c ldump.c lfunc.c lgc.c   \
                linit.c liolib.c llex.c lmathlib.c lmem.c loadlib.c      \
                lobject.c lopcodes.c loslib.c lparser.c lstate.c ltm.c   \
                lstring.c lstrlib.c ltable.c ltablib.c lundump.c lvm.c   \
                lutf8lib.c lzio.c
LUA_LIB_SRCS := $(addprefix ${LUA_SRCPATH}/src/,${LUA_LIB_SRCS})
LUA_LIB_OBJS := $(patsubst %.c,%.o,${LUA_LIB_SRCS})

LUA_SRCS     := lua.c
LUA_SRCS     := $(addprefix ${LUA_SRCPATH}/src/,${LUA_SRCS})
LUA_OBJS     := $(patsubst %.c,%.o,${LUA_SRCS})

LUA_HEADERS  := lauxlib.h lualib.h lua.h
LUA_HEADERS  := $(addprefix ${LUA_SRCPATH}/src/,${LUA_HEADERS})


${LUA_LIB}: ${LUA_LIB_OBJS}
	${RUN_STATICLIB}

${LUA}: ${LUA_OBJS} ${LUA_LIB}
${LUA}: LDLIBS += -lm

${LUA_TARBALL}:
	${RUN_FETCH_URL}

${LUA_TARBALL}: URL = http://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz
${LUA_LIB_SRCS} ${LUA_SRCS} ${LUA_HEADERS}: ${LUA_TARBALL}
	${RUN_UNTARGZ}
	$Q touch ${LUA_LIB_SRCS} ${LUA_SRCS}


eol-lua.h: ${LUA_HEADERS}


clean-lua:
	$Q ${RM} ${LUA} ${LUA_LIB} ${LUA_LIB_OBJS} ${LUA_OBJS}

.PHONY: clean-lua
clean: clean-lua


distclean-lua: clean-lua
	$Q ${RM} -r ${LUA_SRCPATH}
	$Q ${RM} ${LUA_TARBALL}

.PHONY: distclean-lua
distclean: distclean-lua
