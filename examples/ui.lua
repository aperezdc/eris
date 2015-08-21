#! /usr/bin/env lua
--
-- ui.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local ig = require "modularize" {
	"imgui", prefix = "ig", type_prefix = "ImGui"
}

local W = 800
local H = 600

local toplevel = ig.TopLevel("Lua + Eol + ImGui", W, H)
ig.MakeCurrent(toplevel)

while not ig.Done(toplevel) do
	ig.FrameStart(toplevel)
	ig.FrameEnd(toplevel)
end
ig.Exit()
