#! /usr/bin/env lua
--
-- typeof-string.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require "eol"
local test = eol.load "libtest"
local T = eol.typeof "int32_t"
assert.Not.Nil(T)
assert.Equal(T, eol.typeof "int32_t")
assert.Equal(4, T.sizeof)
assert.Equal(4, eol.sizeof(T))
