#! /usr/bin/env lua
--
-- libtest-anon-struct.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local anon_struct = require("eris").load("libtest").anon_struct
assert.Not.Nil(anon_struct)
assert.Equal("anon_struct", anon_struct.__name)
