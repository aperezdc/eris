#! /usr/bin/env lua
--
-- modload.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eol = require "eol"
assert(eol, "could not load 'eol' module")
assert.Field(eol, "load")
