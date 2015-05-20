#! /usr/bin/env lua
--
-- libtest-fltvar.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local libtest = require("eris").load("libtest")

for _, varname in ipairs { "var_flt", "var_dbl" } do
	local variable = libtest[varname]
	assert.Not.Nil(variable)
	assert.Not.Callable(variable)
	assert.Equal(1.0, variable:get())

	-- Set a floating point value.
	variable:set(-42.5)
	assert.Not.Equal(1.0, variable:get())
	assert.Equal(-42.5, variable:get())

	-- Set an integral value (must work for floating point values).
	variable:set(42)
	assert.Not.Equal(1.0, variable:get())
	assert.Not.Equal(-42.5, variable:get())
	assert.Equal(42, variable:get())
end

