if not modules then modules = { } end modules ['util-sig-imp-segment'] = {
    version   = 1.002,
    comment   = "companion to util-sig.lua",
    author    = "Hans Hagen, PRAGMA-ADE, Hasselt NL",
    copyright = "PRAGMA ADE / ConTeXt Development Team",
    license   = "see context related readme files"
}

local type = type

local serialwrite = serial and serial.write or os.serialwrite

if not serialwrite then
    return
end

-- Interfaces:

local trace    = environment.arguments.verbose
local signals  = utilities.signals

local report   = signals.report or logs.reporter("signal")
local state    = signals.loadstate()

local server   = state and state.servers[state.usage.server or "default"]
local protocol = server and server.protocol or "hue"

if protocol == "serial" then

    local port = false
    local baud = 115200

    if server then
        port = server.port or port
        baud = server.baud or baud
    end

    if not port then
        report("unknown serial post")
        return
    end

    local prefix = "s"

    local function squidsome(cmd,run)
        cmd = prefix .. cmd .. (run or "0") .. "\r"
        serialwrite(port,baud,cmd)
    end

    local function squidreset   (run) squidsome("r",run) end
    local function squidbusy    (run) squidsome("b",run) end
 -- local function squidstep    (run) squidsome("s",run) end
    local function squidstep    (run)                    end
    local function squiddone    (run) squidsome("d",run) end
    local function squidfinished(run) squidsome("f",run) end
    local function squidproblem (run) squidsome("p",run) end
    local function squiderror   (run) squidsome("e",run) end

    signals.squidinit     = squidreset
    signals.squidreset    = squidreset
    signals.squidbusy     = squidbusy
    signals.squidstep     = squidstep
    signals.squiddone     = squiddone
    signals.squidfinished = squidfinished
    signals.squidproblem  = squidproblem
    signals.squiderror    = squiderror

    return {
        name    = "squid",
        report  = report,
        trigger = function(state,run)
            if state == "reset" or state == "init" then
                squidreset(run)
            elseif state == "busy" then
                squidbusy(run)
            elseif state == "done" then
                squiddone(run)
            elseif state == "finished" then
                squidfinished(run)
            elseif state == "problem" or state == "maxruns" then
                squidproblem(run)
            elseif state == "error" then
                squiderror(run)
            end
        end,
        stepper = function(state,run)
            if state == "reset" then
                squidreset(run)
            elseif state == "busy" then
                squidbusy(run)
            elseif state == "step" then
                squidstep(run)
            elseif state == "finished" then
                squidfinished(run)
            elseif state == "problem" or state == "maxruns" then
                squidproblem(run)
            elseif state == "error" then
                squiderror(run)
            end
        end,
        signal = function(action)
            local cmd = "ar"
            if action == "busy" or action == "step" then
                cmd = "ab"
            elseif action == "done" then
                cmd = "ad"
            elseif action == "finished" then
                cmd = "af"
            elseif action == "problem" then
                cmd = "ap"
            elseif action == "error" then
                cmd = "ae"
            end
            squidsome(cmd)
       end,
    }

end

