#! /usr/bin/env lua
--
-- type-pp.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local help = [[
Usage: %s file.so typename

This script uses the type information exposed by the Eris module to pretty
print C types. The output format can be (mostly) fed back to a C compiler,
and it also has some additional annotations in comments.
]]

if #arg ~= 2 then
	io.stderr:write(help:format(arg[0]))
	os.exit(1)
end

local function O(t)
	io.stdout:write(t)
end


local repr_type

local types = {
	has = function (self, t)
		assert(t.name, "Type does not have a name")
		return self[t.kind .. ":" .. t.name]
	end,
	add = function (self, t)
		assert(t.name, "Type does not have a name")
		local key = t.kind .. ":" .. t.name
		local rep = self[key]
		if rep == nil then
			print("/* Type: " .. key .. " */")
			rep = repr_type(t, true)
			self[key] = rep
			table.insert(self, key)
		end
		return rep
	end
}


local reprs = {}

repr_type = function (T, verbose)
	local r = reprs[T.kind]
	if r then
		return r(T, verbose)
	else
		return { T.name }
	end
end


reprs.enum = function (T, verbose)
	local r = { "enum ", T.name }
	if verbose or not T.name then
		table.insert(r, " ")
		table.insert(r, "{")
		table.insert(r, "\n")
		for i = 1, #T do
			local member = T[i]
			table.insert(r, member.name .. " = " .. tostring(member.value) .. ",")
			table.insert(r, "\n")
		end
		table.insert(r, "}")
	end
	return r
end

reprs.struct = function (T, verbose)
	local r = { T.kind .. " " }
	if T.name then
		table.insert(r, T.name .. " ")
	end
	if verbose or not T.name then
		table.insert(r, "{")
		table.insert(r, "\n")
		for i = 1, #T do
			local member = T[i]
			if member.type.kind == "array" then
				table.insert(r, repr_type(member.type.type))
				table.insert(r, " " .. member.name .. "[" .. tostring(#member.type) .. "]")
			else
				table.insert(r, repr_type(member.type))
				if member.name then
					table.insert(r, " " .. member.name)
				end
			end
			table.insert(r, ";")
			table.insert(r, "\n")
		end
		table.insert(r, "}")
	end
	return r
end
reprs.union = reprs.struct

reprs.array = function (T, verbose)
	return { repr_type(T.type, verbose), "[" .. tostring(#T) .. "]" }
end

reprs.typedef = function (T, verbose)
	if T.type.name then
		types:add(T.type)
		return { T.name }
	else
		return { "typedef ", repr_type(T.type), " " .. T.name }
	end
end

reprs.pointer = function (T, verbose)
	return { repr_type(T.type), "*" }
end


local PP = {
	indent = 0,
	indent_pending = false,

	Print = function (self, v)
		if self.indent_pending then
			for i = 1, self.indent do
				io.stdout:write("    ")
			end
			self.indent_pending = false
		end
		if type(v) == "string" then
			if v == "\n" then
				self.indent_pending = true
			elseif v == "{" then
				self.indent = self.indent + 1
			elseif v == "}" then
				self.indent = self.indent - 1
				v = "\b\b\b\b}"
			end
			O(v)
		elseif type(v) == "table" then
			for _, vv in ipairs(v) do
				self:Print(vv)
			end
		end
	end,
}


local eris = require("eris")
local T = eris.type(eris.load(arg[1]), arg[2])
types:add(T)

for _, tkey in ipairs(types) do
	PP:Print(types[tkey], #types == 1)
	O(";\n\n")
end

