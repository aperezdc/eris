#! /usr/bin/env lua
--
-- modload.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require "eris"
assert(eris, "could not load eris module")
assert.Field(eris, "load")
