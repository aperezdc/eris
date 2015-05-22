#! /usr/bin/env lua
--
-- libtest-reflect-function.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local libtest = require("eris").load("libtest")
local func = libtest.add
assert.Not.Nil(func)
assert.Callable(func)
assert.Equal("add", func:name())
assert.Equal(libtest, func:library())
