#! /usr/bin/env lua
--
-- libtest-struct-nested.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local Square = require("eol").load("libtest").screen.__type
assert.Not.Nil(Square)
assert.Equal(2, #Square)
assert.Equal("tl", Square[1].name)
assert.Equal(2, #Square[1].type)
assert.Equal("x", Square[1].type[1].name)
assert.Equal("y", Square[1].type[2].name)
assert.Equal("br", Square[2].name)
assert.Equal(Square[1].type, Square[2].type)
