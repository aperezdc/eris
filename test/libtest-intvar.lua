#! /usr/bin/env lua
--
-- libtest-intvar.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local libtest = require("eris").load("libtest")
local var = libtest:lookup("intvar")

assert.Equal(42, var:get())

var:set(32)
assert.Equal(32, var:get())

var:set(-1)
assert.Equal(-1, var:get())
