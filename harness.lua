#! /usr/bin/env lua
--
-- harness.lua
-- Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
--
-- Distributed under terms of the MIT license.
--

local tu = require "testutil"


local usage = [[
Usage: harness.lua <eris-dir> <output-dir> [options]
This script assumes that it is being run from the build output directory.
Options:

  --output=FORMAT   Output format (default: tap).
  --verbose         Print additional messages.
  --help            Display this help message.

Output formats:

  tap      Test Anything Protocol (http://www.testanything.org)

]]


if #arg < 2 then
	io.stderr:write(usage)
	os.exit(1)
end


local tests = {}
local options = {
	output  = "tap",
	verbose = false,
}


function printf(fmt, ...)
	io.stdout:write(fmt:format(...))
end

function verbose(fmt, ...)
	if options.verbose then
		io.stdout:write(fmt:format(...))
	end
end


for i = 3, #arg do
	local opt = arg[i]
	if opt == "-h" or opt == "--help" then
		io.stdout:write(usage)
		os.exit(0)
	end

	-- Try a flag with value first. If there is no match, then try for a
	-- boolean flag. If neither matches, assume the option is a test name.

	local name, value = opt:match("^%-%-(%w[%w%-]*)=(%w+)$")
	if name == nil then
		name = opt:match("^%-%-(%w[%w%-]*)$")
	end

	if name == nil then
		-- Test name.
		tests[#tests + 1] = opt
	elseif options[name] == nil then
		-- Flag looks like an option, but it is an unrecognized one.
		io.stderr:write("Invalid command line option: " .. opt .. "\n")
		io.stderr:write(usage)
		os.exit(1)
	elseif value == nil then
		-- Boolean flag.
		options[name] = not options[name]
	else
		-- Flag with value
		options[name] = value
	end
end


local object = {
	clone = function (self, t)
		local clone = {}
		setmetatable(clone, { __index = self })
		if type(t) == "table" then
			for k, v in pairs(t) do
				clone[k] = v
			end
		end
		return clone
	end;

	prototype = function(self)
		local meta = getmetatable(self)
		return meta and meta.__index or nil
	end;

	extends = function (self, obj)
		local meta = getmetatable(self)
		while true do
			if not (meta and meta.__index) then
				return false
			end
			if meta.__index == obj then
				return true
			end
			meta = getmetatable(meta.__index)
		end
	end;
}


local TAPOutput = object:clone {
	success = "ok %u - %s\n",
	skip    = "ok %u - # SKIP %s\n",
	failure = "not ok %u - %s\n",
}

function TAPOutput:setup(tests)
	self.counter = 0
	self.succeeded = {}
	self.failed = {}
	self.skipped = {}
	printf("1..%u\n", #tests)
end

function TAPOutput:start(test)
end

function TAPOutput:finish(test)
	self.counter = self.counter + 1
	if test.status == "success" then
		self.succeeded[#self.succeeded + 1] = test
		printf(self.success, self.counter, test.name)
	elseif test.status == "failure" then
		self.failed[#self.failed + 1] = test
		printf(self.failure, self.counter, test.name)
		if test.signal then
			verbose("# Exited due to signal %s, output:\n", test.exitcode)
		else
			verbose("# Exited with code %i, output:\n", test.exitcode)
		end
		if test.output ~= nil then
			verbose("# %s\n", test.output:gsub("\n", "\n# "))
		else
			verbose("# (no output)\n")
		end
	elseif test.status == "skip" then
		self.skipped[#self.skipped + 1] = test
		printf(self.skip, self.counter, test.name)
	else
		assert(false, "invalid status")
	end
end

-- Pick the output format
local outputs = { tap = TAPOutput }
local output = outputs[options.output]
if output == nil then
	io.stderr:write("Invalid output format: " .. options.output .. "\n")
	os.exit(2)
end


local Test = object:clone {
	status = "skip", -- Values: "skip", "success", "failure"
	skip   = false,  -- Set to "true" for skipped tests
	signal = false,  -- Set to "true"

	run = function (self)
		assert(not self.skip, "Test should have been skipped")

		local fd = io.popen("./eris '" .. self.file .. "' 2>&1")
		self.output = fd:read("*a")
		local success, reason, exitcode = fd:close()

		-- TODO: Support "expected-to-fail" tests, which invert
		--       the final resulting status.
		self.status   = success and "success" or "failure"
		self.signal   = reason == "signal"
		self.exitcode = exitcode
	end,
}


local scan_tests = #tests == 0
if scan_tests then
	-- No test names given in the command line: scan the tests directory.
	for i, filename in ipairs(tu.listdir(arg[1] .. "/test")) do
		local testname = filename:match("^([%w%-]+)%.lua$")
		if testname then
			-- Got a valid test file name, pick it.
			filename = tu.realpath(arg[1] .. "/test/" .. filename)
			tests[#tests + 1] = Test:clone { name = testname, file = filename }
		end
	end
else
	-- Test names given in the command line: validate that files exist.
	local valid = {}
	for i, testname in ipairs(tests) do
		local filename = arg[1] .. "/test/" .. testname .. ".lua"
		if tu.isfile(filename) then
			filename = tu.realpath(filename)
			valid[#valid + 1] = Test:clone { name = testname, file = filename }
		else
			io.stderr:write("Invalid test: " .. testname .. " (skipping)\n")
		end
	end
	tests = valid
end


output:setup(tests)
for i, test in ipairs(tests) do
	output:start(test)
	-- When test names are given in the command line (i.e. not
	-- scanned from the tests directory), tests are always run.
	if not test.skip or not scan_tests then
		test:run()
	end
	output:finish(test)
end
