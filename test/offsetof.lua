#! /usr/bin/env lua
--
-- offsetof.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require "eris"
local Point = eris.type(eris.load "libtest", "Point")
assert.Not.Nil(Point)

-- First field always at offset 0; the offset of the second field must be
-- at least the size of the first field (it can be more, because padding).
assert.Equal(0, eris.offsetof(Point, 1))
assert.True(eris.offsetof(Point, 2) >= Point[1].type.sizeof)

-- The information gotten by field name must be the same.
assert.Equal(0, eris.offsetof(Point, "x"))
assert.True(eris.offsetof(Point, "y") >= Point[1].type.sizeof)

-- Offsets given by eris.offsetof() must match the ones given by indexing
-- the ErisTypeInfo userdatas.
assert.Equal(Point[1].offset, eris.offsetof(Point, 1))
assert.Equal(Point[1].offset, eris.offsetof(Point, "x"))
assert.Equal(Point[2].offset, eris.offsetof(Point, 2))
assert.Equal(Point[2].offset, eris.offsetof(Point, "y"))
