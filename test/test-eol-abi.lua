#! /usr/bin/env lua
--
-- test-eol-abi.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require("eol")

for _, flag in ipairs { "le", "be", "32bit", "64bit", "win", "fpu", "softfpu", "eabi" } do
	-- We cannot make assumptions about the machine where the test suite runs,
	-- so just call the function with the flags to check that no errors are
	-- raised and a boolean value is returned.
	assert.Boolean(eol.abi(flag))
end

for _, flag in ipairs { 1, 3.14, false, true, { table=true }, "foobar" } do
	-- Check that an error is raised for non-string parameters and invalid
	-- string flags.
	assert.Error(function ()
		local _ = eol.abi(flag)
	end)
end
