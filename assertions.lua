#! /usr/bin/env lua
--
-- assertions.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local Steps = {}
local Chain = {}
local ChainMeta = {}

function ChainMeta:__index(key)
	local value = rawget(self, key)
	if value == nil then
		rawset(self, rawlen(self) + 1, Steps[key])
		value = self
	end
	return value
end


function ChainMeta:__call(...)
	local i = rawlen(self)
	local s = rawget(self, i)
	local m, r = s:apply("", ...)
	i = i - 1
	while i > 0 do
		s = rawget(self, i)
		m, r = s:apply(m, r)
		i = i - 1
	end
	if not r then
		error(m .. " expected", 2)
	end
end


function Chain:make()
	return setmetatable({}, ChainMeta)
end


Steps.True = {
	message = "boolean 'true'",
	apply = function (self, _, value)
		return self.message, type(value) == "boolean" and value == true
	end
}

Steps.False = {
	message = "boolean 'false'",
	apply = function (self, _, value)
		return self.message, type(value) == "boolean" and value == false
	end
}

Steps.Truthy = {
	message = "truthy value",
	apply = function (self, _, value)
		local result = false
		if value then
			result = true
		end
		return self.message, result
	end
}

Steps.Falsey = {
	message = "falsey value",
	apply = function (self, _, value)
		local result = true
		if value then
			result = false
		end
		return self.message, result
	end
}

Steps.Not = {
	apply = function (self, message, value)
		return message .. " not", not value
	end
}

Steps.Error = {
	apply = function (self, _, func_or_error, func)
		local expected_error = nil
		if func ~= nil then
			expected_error = func_or_error
		else
			func = func_or_error
		end
		local success, actual_error = pcall(func)
		if success then
			return "error", false
		end
		if expected_error ~= nil then
			-- Errors returned by pcall() contain the file name and line
			-- number where the error was produced. After that, the strings
			-- must match, and before the error string there is always a
			-- colon and a space (": ") which can be easily checked for.
			local err = actual_error:sub(-#expected_error - 2)
			return "error '" .. expected_error .. "' (got '" ..
				actual_error .. "' instead)", ": " .. expected_error == err
		end
		return "error", true
	end
}

Steps.Callable = {
	message = "callable object",
	apply = function (self, _, obj)
		if type(obj) == "function" then
			return self.message, true
		end
		local meta = getmetatable(obj)
		if meta ~= nil and type(meta.__call) == "function" then
			return self.message, true
		end
		return self.message, false
	end
}

Steps.Fields = {
	apply = function (self, _, obj, ...)
		local fields = { ... }
		for i, key in ipairs(fields) do
			if obj[key] == nil then
				return "table field '" .. key .. "'", false
			end
		end
		return "table fields { " .. table.concat(fields, ", ") .. " }", true
	end
}
Steps.Field = Steps.Fields

local function make_type_checker(typename)
	return {
		message = "value of type '" .. typename .. "'",
		apply = function (self, _, obj)
			return self.message, type(obj) == typename
		end
	}
end

local typenames = {
	-- Userdata is handled a bit differently, see below.
	"Nil", "Number", "String", "Boolean",
	"Table", "Function", "Thread",
}
for _, name in ipairs(typenames) do
	Steps[name] = make_type_checker(name:lower())
end
make_type_checker = nil
typenames = nil

Steps.Userdata = {
	message = "value of type 'userdata'",
	apply = function (self, _, obj, udatatype)
		if udatatype == nil then
			return self.message, type(obj) == "userdata"
		else
			local m = getmetatable(obj)
			local r = m ~= nil and m.__name == udatatype
			return self.message .. " with metatable '" .. udatatype .. "'", r
		end
	end
}

Steps.Equal = {
	apply = function (self, _, expected, obj)
		return tostring(expected) .. " (got '" .. tostring(obj) .. "')",
			expected == obj
	end
}

Steps.Match = {
	apply = function (self, _, match_re, obj)
		local re = "^" .. match_re .. "$"
		return "string matching '" .. re .. "' (got '" .. tostring(obj) .. "')",
			type(obj) == "string" and obj:match(re) ~= nil
	end
}


_G.assert = setmetatable({}, {
	__index = function (table, key)
		return Chain:make()[key]
	end,
	__call = function (table, thing, message, ...)
		if not thing then
			if message == nil then
				message = "Assertion failed!"
			end
			error(message:format(...), 2)
		end
	end,
})
return _G.assert
