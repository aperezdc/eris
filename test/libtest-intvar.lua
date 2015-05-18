#! /usr/bin/env lua
--
-- libtest-intvar.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local libtest = require("eris").load("libtest")

function check_variable (variable, expected_value)
	assert.Not.Nil(variable)
	assert.Not.Callable(variable)
	assert.Field(variable, "get")
	assert.Equal(expected_value, variable:get())
	variable:set(100)
	assert.Equal(100, variable:get())
end


for _, width in ipairs { 8, 16, 32, 64 } do
	for _, signedness in ipairs { "i", "u" } do
		local variable_name = "var_" .. signedness .. tostring(width)
		local variable = libtest:lookup(variable_name)
		check_variable(variable, signedness == "i" and -width or width)
	end
end
