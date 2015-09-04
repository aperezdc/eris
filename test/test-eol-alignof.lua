#! /usr/bin/env lua
--
-- test-eol-alignof.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require("eol")
assert.Field(eol, "alignof")
assert.Callable(eol.alignof)

local libtest = eol.load("libtest")
local u8type = eol.type(libtest, "uint8_t")

assert.Equal(1, eol.alignof(u8type))
assert.Equal(1, eol.alignof(libtest.var_u8))

-- Values other than variables or typeinfos raise an error
for _, value in ipairs { 1, 3.14, false, true, "str", { table=true } } do
	assert.Error(function ()
		local _ = eol.alignof(value)
	end)
end
