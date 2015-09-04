#! /usr/bin/env lua
--
-- reflect-function-types.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local libtest = require("eol").load("libtest")
local func = libtest.add
assert.Not.Nil(func)
assert.Equal(2, #func)
assert.Fields(func, "__name", "__type", "__library")
assert.Equal("add", func.__name)
assert.Equal(libtest, func.__library)

local rettype = func.__type
assert.Not.Nil(rettype)

for i = 1, #func do
	local paramtype = func[i]
	assert.Not.Nil(paramtype)
	assert.Equal(rettype, paramtype)
end

assert.Error(function ()
	-- Access an invalid function parameter index
	local paramtype = func[42]
end)
