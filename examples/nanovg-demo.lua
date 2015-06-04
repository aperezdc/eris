#! /usr/bin/env lua
--
-- nanovg-demo.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require "eris"
local nvg = eris.load "nanovg"

local W = 800
local H = 600

local nvgBeginPath, nvgMoveTo, nvgBezierTo, nvgLineTo =
	nvg.nvgBeginPath, nvg.nvgMoveTo, nvg.nvgBezierTo, nvg.nvgLineTo
local nvgLinearGradient, nvgFillPaint, nvgFill =
	nvg.nvgLinearGradient, nvg.nvgFillPaint, nvg.nvgFill
local nvgRGBA = nvg.nvgRGBA

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
	local sx, sy
	for i, sample in ipairs(samples) do
		sx[i] = x + i * dx
		sy[i] = y + h * sample * 0.8
	end

	local bg = nvgLinearGradient(vg, x, y, x, y + h,
		nvgRGBA(0, 160, 192, 0), nvgRGBA(0, 160, 192, 0))
	nvgBeginPath(vg)
	nvgMoveTo(vg, sx[0], sy[0])
	for i = 2, #sx do
		nvgBezierTo(vg, dx[i-1] + dx * 0.5, sy[i-1] - dx * 0.5,
			sy[i], sx[i], sy[i])
	end
	nvgLineTo(vg, x + w, y + h)
	nvgLineTo(vg, x, y + h)
	nvgFillPaint(vg, bg)
	nvgFill(vg)
end

local window = nvg.nvgWindow ("Lua + Eris + NanoVG", W, H)
nvg.nvgMakeCurrent(window)

local nvgFrameStart, nvgFrameEnd, nvgDone, nvgTime =
	nvg.nvgFrameStart, nvg.nvgFrameEnd, nvg.nvgDone, nvg.nvgTime

local vg = nvg.nvgCreate(true)


while not nvgDone(window) do
	nvgFrameStart(window, vg)
	graph(vg, 0, H/2, W, H/2, nvgTime())
	nvgFrameEnd(window, vg)
end
nvg.nvgExit()
