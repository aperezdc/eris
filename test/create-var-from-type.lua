#! /usr/bin/env lua
--
-- libtest-create-var-from-type.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require "eris"
local libtest = eris.load "libtest"
local int32_t = eris.type(libtest, "int32_t")

local value = int32_t()
assert.Equal(int32_t, value.__type)
assert.Equal(1, #value)
assert.Equal(0, value.__value)

value[1] = 42
assert.Equal(42, value.__value)

