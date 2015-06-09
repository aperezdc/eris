#! /usr/bin/env lua
--
-- upnginfo.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local upng = require "modularize" { "upng", prefix = "upng_" }

local function enum_value_string(enum, value)
	for i = 1, #enum do
		local item = enum[i]
		if value == item.value then
			return item.name
		end
	end
	return tostring(value) .. " (no symbolic name?)"
end

local print_item_format = "[1;1m  %-10s[0;0m %s\n"
local function print_item(key, value)
	io.stdout:write(print_item_format:format(key, tostring(value)))
end

local function info(img)
	local fmt = upng.get_format(img)
	print_item("format",   enum_value_string(upng.types.format, fmt))
	print_item("channels", upng.get_components(img))
	print_item("width",    upng.get_width(img))
	print_item("height",   upng.get_height(img))
	print_item("bpp",      upng.get_bpp(img))
	print_item("depth",    upng.get_bitdepth(img))
end

for _, path in ipairs(arg) do
	local img = upng.new_from_file(path)
	if img ~= nil then
		if upng.decode(img) == 0 then
			print("[1;32m" .. path .. "[0;0m")
			info(img)
		else
			io.stderr:write(path .. ": error ")
			local err = upng.get_error(img)
			io.stderr:write(enum_value_string(upng.types.error, err))
		end
		upng.free(img)
	else
		io.stderr:write("Cannot open '" .. path .. "'\n")
	end
end

