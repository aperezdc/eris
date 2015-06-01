#! /usr/bin/env lua
--
-- struct-member-access-nested.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local screen = require("eris").load("libtest").screen
assert.Fields(screen, "tl", "br")
assert.Not.Nil(screen.tl)
assert.Not.Nil(screen.br)
assert.Equal(10, screen.tl.x)
assert.Equal(20, screen.tl.y)
assert.Equal(50, screen.br.x)
assert.Equal(80, screen.br.y)

screen.tl.x = 15
assert.Equal(15, screen.tl.x)
