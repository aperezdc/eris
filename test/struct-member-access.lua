#! /usr/bin/env lua
--
-- struct-member-access.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local max_pos = require("eol").load("libtest").max_pos
assert.Not.Nil(max_pos)

-- Indexed access
assert.Equal(800, max_pos[1])
assert.Equal(600, max_pos[2])

-- Named access
assert.Equal(800, max_pos.x)
assert.Equal(600, max_pos.y)
