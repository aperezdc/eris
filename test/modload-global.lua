#! /usr/bin/env lua
--
-- modload-global.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require "eol"
local libtest = eol.load("libtest", true) -- Request loading as global
assert.Not.Nil(libtest)

-- Loading a second library that uses a symbol defined in the first library.
local libtest = eol.load("libtest2")
assert.Not.Nil(libtest)
