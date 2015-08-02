#! /usr/bin/env lua
--
-- typeof-type.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require "eol"
local T = eol.type(eol.load "libtest", "int32_t")
assert.Not.Nil(T)
assert.Equal(T, eol.typeof(T))
assert.Equal(4, eol.sizeof(eol.typeof(T)))
