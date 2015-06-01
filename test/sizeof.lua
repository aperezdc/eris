#! /usr/bin/env lua
--
-- libtest-sizeof.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require("eris")
assert.Field(eris, "sizeof")

local libtest = eris.load("libtest")

assert.Equal(4, eris.sizeof(libtest.var_i32))
assert.Equal(4, eris.sizeof(libtest.var_i32.__type))
