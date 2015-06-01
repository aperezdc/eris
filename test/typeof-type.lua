#! /usr/bin/env lua
--
-- typeof-type.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require "eris"
local T = eris.type(eris.load "libtest", "int32_t")
assert.Not.Nil(T)
assert.Equal(T, eris.typeof(T))
assert.Equal(4, eris.sizeof(eris.typeof(T)))
