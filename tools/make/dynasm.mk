BITOP_VERSION := 1.0.2
BITOP_TARBALL := ${OUT}/downloads/LuaBitOp-${BITOP_VERSION}.tar.gz
BITOP_PATH    := ${OUT}/LuaBitOp-${BITOP_VERSION}

${BITOP_TARBALL}: URL = http://bitop.luajit.org/download/LuaBitOp-${BITOP_VERSION}.tar.gz
${BITOP_TARBALL}:
	${RUN_FETCH_URL}

${BITOP_PATH}/bit.c: ${BITOP_TARBALL}
	${RUN_UNTARGZ}
	$Q touch $@

${OUT}/bit.so: ${BITOP_PATH}/bit.o
${OUT}/bit.so: CPPFLAGS += -DLUA_NUMBER_DOUBLE
${OUT}/bit.so: LDFLAGS += -shared

clean-bit:
	$Q ${RM} ${OUT}/bit.so ${OUT}/bit.o

.PHONY: clean-bit
clean: clean-bit


eol-fcall-${eol_fcall}.c: eol-fcall-${eol_fcall_in}.dasc | ${LUA} ${OUT}/bit.so
	$P 'DynASM(${jit_arch})' $<
	$Q LUA_CPATH=${OUT}/?.so ${LUA} dynasm/dynasm.lua ${dynasm_flags} -o $@ $<

