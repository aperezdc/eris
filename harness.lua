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

  --output=FORMAT   Output format (default: auto).
  --verbose         Print additional messages.
  --list            List names of all test cases.
  --commands        Print commands used to run the test cases.
  --debug           Run test case under a debugger.
  --debugger=CMD    Use CMD as debugger command (default: gdb --args).
  --help            Display this help message.

Output formats:

  uterm    UTF-8 terminal supporting ANSI color codes.
  tap      Test Anything Protocol (http://www.testanything.org)
  auto     Use "uterm" when outputting to a terminal, "tap" otherwise.

]]


local function die(fmt, ...)
	io.stderr:write(fmt:format(...))
	os.exit(1)
end


if #arg < 2 then
	die(usage)
end


local tests = {}
local options = {
	debugger = "gdb --args",
	output   = "auto",
	commands = false,
	verbose  = false,
	debug    = false,
	list     = false,
}


local function printf(fmt, ...)
	io.stdout:write(fmt:format(...))
end

local function verbose(fmt, ...)
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

	local name, value = opt:match("^%-%-(%w[%w%-]*)=(.+)$")
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


local BaseOutput = object:clone()

function BaseOutput:setup(tests)
	self.succeeded = {}
	self.failed = {}
	self.skipped = {}
end

function BaseOutput:start(test)
	-- No-op
end

function BaseOutput:finish(test)
	if test.status == "success" then
		self.succeeded[#self.succeeded + 1] = test
	elseif test.status == "failure" then
		self.failed[#self.failed + 1] = test
	elseif test.status == "skip" then
		self.skipped[#self.skipped + 1] = test
	else
		assert(false, "invalid status")
	end
end

function BaseOutput:report()
	-- No-op
end


local TAPOutput = BaseOutput:clone {
	success = "ok %u - %s\n",
	skip    = "ok %u - # SKIP %s\n",
	failure = "not ok %u - %s\n",
}

function TAPOutput:setup(tests)
	self:prototype().setup(self, tests)
	self.counter = 0
	printf("1..%u\n", #tests)
end

function TAPOutput:start(test)
	self:prototype().start(self, test)
end

function TAPOutput:finish(test)
	self:prototype().finish(self, test)
	self.counter = self.counter + 1
	if test.status == "success" then
		printf(self.success, self.counter, test.name)
		if options.verbose and test.output ~= nil and #test.output > 0 then
			printf("# %s\n", test.output:gsub("\n", "\n# "))
		end
	elseif test.status == "failure" then
		printf(self.failure, self.counter, test.name)
		if test.signal then
			verbose("# Exited due to signal %s, output:\n", test.exitcode)
		else
			verbose("# Exited with code %i, output:\n", test.exitcode)
		end
		-- Output of failed tests is always written.
		if test.output ~= nil and #test.output > 0 then
			printf("# %s\n", test.output:gsub("\n", "\n# "))
		else
			verbose("# (no output)\n")
		end
	elseif test.status == "skip" then
		printf(self.skip, self.counter, test.name)
	else
		assert(false, "invalid status")
	end
end


local CSI_data = {
	-- Formats for character attributes.
	F_normal   = "[%um%s[0;0m",
	F_bold     = "[1;%um%s[0;0m",

	-- Formats for cursor movement.
	M_up       = "[%uA",
	M_down     = "[%uB",
	M_right    = "[%uC",
	M_left     = "[%uD",
	M_column   = "[%uG",

	-- Color constants.
	C_black    = 30,
	C_red      = 31,
	C_green    = 32,
	C_brown    = 33,
	C_blue     = 34,
	C_magenta  = 35,
	C_cyan     = 36,
	C_white    = 37,

	-- Actions which do not require parameters.
	erase      = "[K",
	eraseline  = "[2K",
	savepos    = "[s",
	restorepos = "[u",
}

local CSI = setmetatable({}, { __index = CSI_data })
for k, v in pairs(CSI_data) do
	local function make_movement_func(format, move)
		return function (n)
			return format:format(n)
		end
	end

	if k:sub(0, 2) == "C_" then
		k = k:sub(3)
		CSI[k] = function (txt)
			return CSI_data.F_normal:format(v, txt)
		end
		CSI[k .. "bg"] = function (txt)
			return CSI_data.F_normal:format(v + 10, txt)
		end
		CSI["bold" .. k] = function (txt)
			return CSI_data.F_bold:format(v, txt)
		end
		CSI["bold" .. k .. "bg"] = function (txt)
			return CSI_data.F_bold:format(v + 10, txt)
		end
	elseif k:sub(0, 2) == "M_" then
		k = k:sub(3)
		CSI[k] = make_movement_func(v, k)
	end
end


local Utf8TermOutput = BaseOutput:clone {
	dot_success = CSI.boldgreen "●",
	dot_failure = CSI.boldred   "◼",
	dot_skip    = CSI.boldbrown "✱",
	dot_pending = CSI.boldcyan  "◌",
}

function Utf8TermOutput:setup(tests)
	self:prototype().setup(self, tests)
	self.counter = 0
	self.total = #tests
end

function Utf8TermOutput:start(test)
	self:prototype().start(self, test)
	self.counter = self.counter + 1
	local progress = CSI.boldblue(("[%u/%u]"):format(self.counter, self.total))
	printf("%s%s %s %s%s", CSI.column(2), self.dot_pending,
		test.name, progress, CSI.erase)
	io.stdout:flush()
end

function Utf8TermOutput:finish(test)
	self:prototype().finish(self, test)
	local statusdot = self["dot_" .. test.status]
	printf("%s%s%s%s%s", CSI.savepos, CSI.column(2),
		statusdot, CSI.restorepos, CSI.erase)
	if test.status ~= "success" then
		printf("\n")
		if test.status == "failure" and test.output ~= nil and #test.output > 0 then
			printf("%s", CSI.white(test.output))
		end
	end
	io.stdout:flush()
end

function Utf8TermOutput:report()
	self:prototype().report(self)
	if #self.failed > 0 or #self.skipped > 0 then
		printf("\n")
	else
		printf("%s%s", CSI.column(1), CSI.erase)
	end
	if options.verbose and #self.failed > 0 then
		printf("Failed tests:\n")
		for _, test in ipairs(self.failed) do
			printf("  %s\n", CSI.red(test.name))
		end
		printf("\n")
	end
	printf(" %s %u  %s %u  %s %u\n",
		self.dot_success, #self.succeeded,
		self.dot_skip,    #self.skipped,
		self.dot_failure, #self.failed)
end


-- Pick the output format
local outputs = { tap = TAPOutput, uterm = Utf8TermOutput }
local supports_uterm = { "xterm", "uxterm", "screen", "st", "tmux" }
function detect_uterm()
	if not tu.isatty(io.stdout) then
		return false
	end

	local term = os.getenv("TERM")
	if term == nil or #term == 0 then
		return false
	end

	for _, t in ipairs(supports_uterm) do
		if term == t then
			return true
		end
		if term:sub(0, #t + 1) == t .. "-" then
			return true
		end
	end
	return false
end

if options.output == "auto" then
	options.output = detect_uterm() and "uterm" or "tap"
end

local output = outputs[options.output]
if output == nil then
	io.stderr:write("Invalid output format: " .. options.output .. "\n")
	os.exit(2)
end


local Test = object:clone {
	status = "skip", -- Values: "skip", "success", "failure"
	skip   = false,  -- Set to "true" for skipped tests
	signal = false,  -- Set to "true"

	command = function (self)
		return "./eris '" .. self.file .. "'"
	end,

	run = function (self)
		assert(not self.skip, "Test should have been skipped")

		local fd = io.popen(self:command() .. " 2>&1")
		self.output = fd:read("*a")
		local success, reason, exitcode = fd:close()

		-- TODO: Support "expected-to-fail" tests, which invert
		--       the final resulting status.
		self.status   = success and "success" or "failure"
		self.signal   = reason == "signal"
		self.exitcode = exitcode
	end,
}


local scan_tests = options.list or #tests == 0
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


if options.list then
	for _, test in ipairs(tests) do
		print(test.name)
	end
elseif options.debug then
	if #tests > 1 then
		die("Running a debugger supports only a single test case.\n");
	end
	local command = options.debugger .. " " .. tests[1]:command()
	verbose("LUA_INIT='%s'\n", os.getenv("LUA_INIT") or "")
	printf("Debug command: %s\n", command)
	os.execute(command)
elseif options.commands then
	local lua_init = os.getenv("LUA_INIT")
	local cwd = tu.getcwd()
	if lua_init then
		lua_init = "LUA_INIT='" .. lua_init .. "' "
	else
		lua_init = ""
	end
	lua_init = "cd '" .. cwd .. "' && " .. lua_init
	for _, test in ipairs(tests) do
		if not test.skip or not scan_tests then
			print(lua_init .. test:command())
		end
	end
else
	output:setup(tests)
	for _, test in ipairs(tests) do
		output:start(test)
		-- When test names are given in the command line (i.e. not
		-- scanned from the tests directory), tests are always run.
		if not test.skip or not scan_tests then
			test:run()
		end
		output:finish(test)
	end
	output:report()
end
