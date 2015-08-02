#! /usr/bin/env lua
--
-- libtest-sizeof.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require("eol")
assert.Field(eol, "sizeof")

local libtest = eol.load("libtest")

assert.Equal(4, eol.sizeof(libtest.var_i32))
assert.Equal(4, eol.sizeof(libtest.var_i32.__type))
