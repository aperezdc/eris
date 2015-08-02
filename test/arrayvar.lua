#! /usr/bin/env lua
--
-- libtest-arrayvar.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local intarray = require("eol").load("libtest").intarray

local function check_items(a)
	for i, v in ipairs(a) do
		assert.Equal(v, intarray[i])
	end
end

assert.Not.Nil(intarray)
assert.Equal("intarray", intarray.__name)
assert.Equal(5, #intarray)
check_items { 1, 2, 3, 4, 5 }
intarray[3] = 42
check_items { 1, 2, 42, 4, 5 }
