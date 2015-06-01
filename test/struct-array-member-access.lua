#! /usr/bin/env lua
--
-- struct-array-member-access.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local curve = require("eris").load("libtest").curve
assert.Not.Nil(curve)
assert.Fields(curve, "points", "tangential")
assert.Equal(4, #curve.points)
assert.Equal(1, curve.points[1].x)
curve.points[2].y = 5
assert.Equal(5, curve.points[2].y)
