#! /usr/bin/env lua
--
-- libtest-fltvar.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local libtest = require("eol").load("libtest")

for _, varname in ipairs { "var_flt", "var_dbl" } do
	local variable = libtest[varname]
	assert.Not.Nil(variable)
	assert.Not.Callable(variable)
	assert.Equal(1.0, variable.__value)
	assert.Equal(libtest, variable.__library)

	-- Set a floating point value.
	variable.__value = -42.5
	assert.Not.Equal(1.0, variable.__value)
	assert.Equal(-42.5, variable.__value)

	-- Set an integral value (must work for floating point values).
	variable.__value = 42
	assert.Not.Equal(1.0, variable.__value)
	assert.Not.Equal(-42.5, variable.__value)
	assert.Equal(42, variable.__value)
end

