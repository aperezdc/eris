#! /usr/bin/env lua
--
-- libtest-reflect-var.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local libtest = require("eris").load("libtest")
assert.Not.Nil(libtest)

function check_variable(variable, expected_name, expected_readonly,
						expected_declared_type, expected_precise_type)
	assert.Not.Nil(variable)
	local precise_type, declared_type = variable:typenames()
	assert.Equal(expected_name, variable:name())
	assert.Equal(libtest, variable:library())
	assert.Equal(expected_readonly, variable:readonly())
	assert.Match(expected_precise_type, precise_type)
	assert.Match(expected_declared_type, declared_type)
end

check_variable(libtest.const_int, "const_int", true, "int", "int%d+_t")
check_variable(libtest.var_u16, "var_u16", false, "unsigned%s[%w%s]+", "uint16_t")
check_variable(libtest.intvar, "intvar", false, "int", "int%d+_t")
