#! /usr/bin/env lua
--
-- offsetof.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require "eol"
local Point = eol.type(eol.load "libtest", "Point")
assert.Not.Nil(Point)

-- First field always at offset 0; the offset of the second field must be
-- at least the size of the first field (it can be more, because padding).
assert.Equal(0, eol.offsetof(Point, 1))
assert.True(eol.offsetof(Point, 2) >= Point[1].type.sizeof)

-- The information gotten by field name must be the same.
assert.Equal(0, eol.offsetof(Point, "x"))
assert.True(eol.offsetof(Point, "y") >= Point[1].type.sizeof)

-- Offsets given by eol.offsetof() must match the ones given by indexing
-- the EolTypeInfo userdatas.
assert.Equal(Point[1].offset, eol.offsetof(Point, 1))
assert.Equal(Point[1].offset, eol.offsetof(Point, "x"))
assert.Equal(Point[2].offset, eol.offsetof(Point, 2))
assert.Equal(Point[2].offset, eol.offsetof(Point, "y"))
