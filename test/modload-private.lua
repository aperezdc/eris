#! /usr/bin/env lua
--
-- modload-private.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require("eris")
local libtest = eris.load("libtest")
assert.Not.Nil(libtest)

local libtest2 = nil
assert.Error(function () libtest2 = eris.load("libtest2", false) end)
assert.Nil(libtest2)
