#! /usr/bin/env lua
--
-- libtest-intvar.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local libtest = require("eris").load("libtest")
local variable = libtest.const_int

assert.Not.Nil(variable)
assert.Not.Callable(variable)
assert.Field(variable, "get")
assert.Equal(42, variable:get())

-- Read-only variables cannot be assigned to.
assert.Error(function () variable:set(100) end)
assert.Equal(42, variable:get())
