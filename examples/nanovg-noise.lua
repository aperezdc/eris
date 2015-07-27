#! /usr/bin/env lua
--
-- nanovg-noise.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require "eris"
local nvg = require "modularize" {
	"nanovg", prefix = "nvg", type_prefix = "NVG"
}
local inttype  = eris.type(nvg.__library, "long int")
local ucharptr = eris.type(nvg.__library, "unsigned char"):pointerto()

local W = 320
local H = 240

local window = nvg.Window("Noise", W, H)
nvg.MakeCurrent(window)

local vg = nvg.Create(false)
local bits = inttype(W * H / 2)
local bits_as_ptr = eris.cast(ucharptr, bits)
local image = nvg.CreateImageRGBA(vg, W, H, 0, bits_as_ptr)
local paint = nvg.ImagePattern(vg, 0, 0, W, H, 0.0, image, 1.0)

while not nvg.Done(window) do

	-- Fill with random data
	local r = bits[1] + 1
	for i = 1, #bits do
		r = r * 1103515245
		bits[i] = r ~ (bits[i] >> 16)
	end

	nvg.UpdateImage(vg, image, bits_as_ptr)

	nvg.FrameStart(window, vg)
	nvg.BeginPath(vg)
	nvg.Rect(vg, 0, 0, W, H)
	nvg.FillPaint(vg, paint)
	nvg.Fill(vg)
	nvg.FrameEnd(window, vg)
end
nvg.DeleteImage(vg, image)
nvg.Exit()
