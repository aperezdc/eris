#! /usr/bin/env lua
--
-- libtest-lookup-variable.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local libtest = require("eris").load("libtest")
assert(libtest, "could not load libtest.so")

local intvar = libtest.intvar
assert.Not.Nil(intvar)
assert.Userdata(intvar, "org.perezdecastro.eris.Variable")
assert.Not.Callable(intvar)
