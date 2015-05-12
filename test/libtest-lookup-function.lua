#! /usr/bin/env lua
--
-- libtest-lookup-function.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local libtest = require("eris").load("libtest")
assert(libtest, "could not load libtest.so")
assert.Field(libtest, "lookup")

local add = libtest:lookup("add")
assert.Not.Nil(add)
assert.Userdata(add)
assert.Callable(add)
