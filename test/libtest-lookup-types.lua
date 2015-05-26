#! /usr/bin/env lua
--
-- libtest-lookup-types.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require "eris"
local libtest = eris.load "libtest"

local i32 = eris.type(libtest, "int32_t")
assert.Not.Nil(i32)
assert.Equal("int32_t", i32.name)
assert.Equal(4, i32.sizeof)
assert.False(i32.readonly)
