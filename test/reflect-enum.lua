#! /usr/bin/env lua
--
-- reflect-enum.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local eris = require("eris")
local libtest = eris.load("libtest")
local Continent = eris.type(libtest, "Continent")

assert.Not.Nil(Continent)
assert.Equal(6, #Continent)

local data = {
	"AFRICA", "EUROPE", "ASIA", "AMERICA", "ANTARCTICA", "AUSTRALIA"
}

for i, name in ipairs(data) do
	local entry = Continent[i]
	assert.Equal(name, entry.name)
	assert.Equal(i - 1, entry.value)
end
