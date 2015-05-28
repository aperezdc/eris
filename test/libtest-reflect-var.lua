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
	                   expected_type, expected_type_sizeof)
	assert.Not.Nil(variable)
	assert.Equal(expected_name, variable.__name)
	assert.Equal(libtest, variable.__library)
	assert.Match(expected_type, variable.__type.name)
	assert.Equal(expected_readonly, variable.__type.readonly)
	if expected_type_sizeof ~= nil then
		assert.Equal(expected_type_sizeof, variable.__type.sizeof)
	end
end

check_variable(libtest.const_int, "const_int", true, "int")
check_variable(libtest.var_u16, "var_u16", false, "uint16_t", 2)
check_variable(libtest.intvar, "intvar", false, "int")
