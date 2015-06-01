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
local struct_type = anon_struct.__type
assert.Equal(1, #struct_type)
assert.Equal("member", struct_type[1].name)
