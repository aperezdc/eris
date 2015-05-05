#! /usr/bin/env lua
--
-- libtest-load.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local libtest = require("eris").load("libtest")
assert(libtest, "could not load libtest.so")
assert.Field(libtest, "lookup")

local add = libtest:lookup("add")
assert.Callable(add)
