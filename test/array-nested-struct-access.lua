#! /usr/bin/env lua
--
-- array-nested-struct-access.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local triangle = require("eris").load("libtest").triangle
assert.Not.Nil(triangle)
assert.Equal(3, #triangle)
assert.Equal(1, triangle[1].x)
assert.Equal(1, triangle[1].y)
assert.Equal(2, triangle[2].x)
assert.Equal(3, triangle[2].y)
assert.Equal(1, triangle[3].x)
assert.Equal(3, triangle[3].y)

triangle[2].x = 3
assert.Equal(3, triangle[2].x)
