#! /usr/bin/env lua
--
-- test-eol-sizeof.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require("eol")
local libtest = eol.load("libtest")

assert.Fields(eol, "sizeof")
assert.Callable(eol.sizeof)

assert.Equal(4, eol.sizeof(libtest.var_u32))
assert.Equal(4, eol.sizeof(libtest.var_u32.__type))

local inttype = libtest.intvar.__type
assert.Equal(inttype.sizeof, eol.sizeof(inttype))
assert.Equal(inttype.sizeof, eol.sizeof(libtest.intvar))

-- For functions and "void", returns "nil" (it does not have a size)
assert.Nil(eol.sizeof(libtest.add))
assert.Nil(eol.sizeof(libtest.voidptr.__type.type))

-- For other values, raises an error
local error_values = {
	1, 3.14,
	true, false,
	{ table = true },
	function () end,
	io.stdin,
}

for _, value in ipairs(error_values) do
	assert.Error(function ()
		local _ = eol.sizeof(value)
	end)
end
