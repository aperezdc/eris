#! /usr/bin/env lua
--
-- nanovg-demo.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local nvg = require "modularize" {
	"nanovg", prefix = "nvg", type_prefix = "NVG"
}

local W = 800
local H = 600

local function graph(vg, x, y, w, h, t)
	local samples = {
		1 + math.sin(t * 1.23450 + math.cos(t * 0.33457) * 0.44) * 0.5,
		1 + math.sin(t * 0.68363 + math.cos(t * 1.30000) * 1.55) * 0.5,
		1 + math.sin(t * 1.16442 + math.cos(t * 0.33457) * 1.24) * 0.5,
		1 + math.sin(t * 0.56345 + math.cos(t * 1.63000) * 0.14) * 0.5,
		1 + math.sin(t * 1.62450 + math.cos(t * 0.25400) * 0.30) * 0.5,
		1 + math.sin(t * 0.34500 + math.cos(t * 0.03000) * 0.60) * 0.5,
	}

	local dx = w / 5.0
	local sx = {}
	local sy = {}
	for i, sample in ipairs(samples) do
		sx[i] = x + i * dx
		sy[i] = y + h * sample * 0.8
	end

	local bg = nvg.LinearGradient(vg, x, y, x, y + h,
		nvg.RGBA(0, 160, 192, 0), nvg.RGBA(0, 160, 192, 0))
	nvg.BeginPath(vg)
	nvg.MoveTo(vg, sx[1], sy[1])
	for i = 2, #sx do
		nvg.BezierTo(vg, sx[i-1] + dx * 0.5, sy[i-1], sx[i] - dx * 0.5,
			sy[i], sx[i], sy[i])
	end
	nvg.LineTo(vg, x + w, y + h)
	nvg.LineTo(vg, x, y + h)
	nvg.FillPaint(vg, bg)
	nvg.Fill(vg)
end

local window = nvg.Window("Lua + Eris + NanoVG", W, H)
nvg.MakeCurrent(window)

local vg = nvg.Create(true)

while not nvg.Done(window) do
	nvg.FrameStart(window, vg)
	graph(vg, 0, H/2, W, H/2, nvg.Time())
	nvg.FrameEnd(window, vg)
end
nvg.Exit()
