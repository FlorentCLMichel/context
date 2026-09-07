if not modules then modules = { } end modules ['trac-tmr'] = {
    version   = 1.001,
    comment   = "companion to trac-inf.*",
    author    = "Hans Hagen, PRAGMA-ADE, Hasselt NL",
    copyright = "PRAGMA ADE / ConTeXt Development Team",
    license   = "see context related readme files"
}

-- This is the regular interface. At some point I might in some spots use the more
-- direct functions.

local format = string.format

statistics           = statistics or { }
local statistics     = statistics

statistics.threshold = statistics.threshold or 0.01

local timernew       = timer.new
local timerstart     = timer.start
local timerstop      = timer.stop
local timerreset     = timer.reset
local timersetoffset = timer.setoffset
local timerelapsed   = timer.elapsed
local timercurrent   = timer.current
local timeristiming  = timer.istiming

local timers         = { }

table.setmetatableindex(timers,function(t,k)
    local v = timernew()
    t[k] = v
    return v
end)

function statistics.resettiming(instance)
    timerreset(timers[instance or "notimer"])
end

function statistics.starttiming(instance,reset)
    timerstart(timers[instance or "notimer"],reset)
end

function statistics.stoptiming(instance)
    timerstop(timers[instance or "notimer"])
end

function statistics.benchmarktimer(instance)
    timersetoffset(timers[instance or "notimer"],2)
end

function statistics.istiming(instance)
    return timeristiming(timers[instance or "notimer"])
end

local function elapsed(instance)
    if type(instance) == "number" then
        return instance
    else
        return timerelapsed(timers[instance or "notimer"])
    end
end

statistics.elapsed = elapsed

function statistics.elapsedindeed(instance)
    return elapsed(instance) > statistics.threshold
end

function statistics.currenttime(instance)
    if type(instance) == "number" then
        return instance
    else
        return timercurrent(timers[instance or "notimer"])
    end
end

function statistics.elapsedtime(instance)
    return format("%0.3f",elapsed(instance))
end

function statistics.elapsedseconds(instance,rest) -- returns nil if 0 seconds
    local e = elapsed(instance)
    if e > statistics.threshold then
        return format("%0.3f seconds %s",e,rest or "")
    end
end
