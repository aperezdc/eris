#! /usr/bin/env lua
--
-- typeof-var.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require "eris"

local V = eris.load("libtest").var_i32
assert.Not.Nil(V)

local T = eris.typeof(V)
assert.Not.Nil(T)
assert.Equal(V.__type, T)
assert.Equal(eris.typeof "int32_t", T)
assert.Equal(4, T.sizeof)
