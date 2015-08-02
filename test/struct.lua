#! /usr/bin/env lua
--
-- libtest-anon-struct.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local origin = require("eol").load("libtest").origin
assert.Not.Nil(origin)
assert.Equal("origin", origin.__name)

local struct_type = origin.__type
assert.Equal("Point", struct_type.name)
assert.Equal(2, #struct_type)

local x_member = struct_type[1]
local y_member = struct_type[2]
assert.Equal("x", x_member.name)
assert.Equal("y", y_member.name)
assert.Equal(0, x_member.offset)
assert.Equal(x_member.type, y_member.type)
assert.True(x_member.offset < y_member.offset)
