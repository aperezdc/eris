#! /usr/bin/env lua
--
-- typeof-var.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require "eol"

local V = eol.load("libtest").var_i32
assert.Not.Nil(V)

local T = eol.typeof(V)
assert.Not.Nil(T)
assert.Equal(V.__type, T)
assert.Equal(eol.typeof "int32_t", T)
assert.Equal(4, T.sizeof)
