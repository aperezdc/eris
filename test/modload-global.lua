#! /usr/bin/env lua
--
-- modload-global.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require "eris"
local libtest = eris.load("libtest", true) -- Request loading as global
assert.Not.Nil(libtest)

-- Loading a second library that uses a symbol defined in the first library.
local libtest = eris.load("libtest2")
assert.Not.Nil(libtest)
