#! /usr/bin/env lua
--
-- test-assertions.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

assert(type(assert) == "table", "assert is not a table")

-- Check metatable
local meta = getmetatable(assert)
assert(meta ~= nil, "assert does not have a metatable")
assert(meta.__call ~= nil, "assert metatable does not have __call")
assert(type(meta.__call) == "function", "assert() is not callable")

assert.True(true)
assert.False(false)
assert.Not.True(false)
assert.Not.False(true)

-- Try calling a function that raises an error
assert.Error(function() error("meh") end)
assert.Error("meh", function() error("meh") end)
assert.Not.Error("meow", function() error("meh") end)
