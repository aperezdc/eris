#! /usr/bin/env lua
--
-- libtest-create-var-from-type.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require "eol"
local libtest = eol.load "libtest"
local int32_t = eol.type(libtest, "int32_t")

local value = int32_t()
assert.Equal(int32_t, eol.typeof(value))
assert.Equal(1, #value)
assert.Equal(0, value.__value)

value.__value = 42
assert.Equal(42, value.__value)

