#! /usr/bin/env lua
--
-- test-eol-type.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require("eol")
assert.Field(eol, "type")
assert.Callable(eol.type)

local libtest = eol.load("libtest")

assert.Not.Nil(eol.type(libtest, "int"))  -- Existing type
assert.Nil(eol.type(libtest, "no no no")) -- Nonexistent type

-- Values other than a library for the first parameter raise errors.
for _, value in ipairs { 1, 3.14, false, true, "foo", { table=true } } do
	assert.Error(function ()
		local _ = eol.type(value, "int")
	end)
end

-- Values other than a string for the second parameter raise errors.
for _, value in ipairs { 1, 3.13, false, true, { table=true }, libtest } do
	assert.Error(function ()
		local _ = eol.type(libtest, value)
	end)
end
