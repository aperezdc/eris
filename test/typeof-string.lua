#! /usr/bin/env lua
--
-- typeof-string.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require "eris"
local test = eris.load "libtest"
local T = eris.typeof "int32_t"
assert.Not.Nil(T)
assert.Equal(T, eris.typeof "int32_t")
assert.Equal(4, T.sizeof)
assert.Equal(4, eris.sizeof(T))
