if not modules then modules = { } end modules ['util-tmr'] = {
    version   = 1.001,
    comment   = "companion to trac-inf.*",
    author    = "Hans Hagen, PRAGMA-ADE, Hasselt NL",
    copyright = "PRAGMA ADE / ConTeXt Development Team",
    license   = "see context related readme files"
}

-- A built in variant as in luametatex is only more efficient when we do a lot of
-- starting and stopping. Also, the final wrapping in statistics is adding some
-- overhead. But if needed, we can be fast.

-- The luajittex ffi based variant is gone as we don't really use that engine
-- any longer.

local ticks   = lua.getpreciseticks
local seconds = lua.getpreciseseconds

if not ticks or not seconds then
    ticks   = os.gettimeofday or os.clock
    seconds = function(n) return n or 0 end
end

if not timer then

    timer = {

        new = function()
            return { timing = 0, total = 0, offset = 0, start = 0 }
        end,

        reset = function(t)
            if t then
                t.timing = 0
                t.total  = 0
                t.offset = 0
                t.start  = 0
            end
        end,

        start = function(t,reset)
            if t then
                local timing = t.timing
                if reset then
                    timing  = 0
                    t.total = 0
                end
                if timing == 0 then
                    t.start = ticks()
                    if not t.total then
                        t.total = 0
                    end
                end
                t.timing = timing + 1
            end
        end,

        stop = function(t)
            if t then
                local timing = t.timing
                if timing > 1 then
                    t.timing = timing - 1
                else
                    local start = t.start
                    if start > 0 then
                        local stop  = ticks()
                        local total = stop - start
                        t.total  = t.total + total
                        t.timing = 0
                        t.start  = 0
                    end
                end
            end
        end,

        setoffset = function(t,offset)
            if t then
                t.offset = offset * (ticks() - t.start)
            end
        end,

        istiming = function(t)
            return t.timing > 0
        end,

        elapsed = function(t)
            if t then
                return seconds(t.total - t.offset)
            end
        end,

        current = function(t)
            if t then
                local total = t.total
                if t.timing > 0 then
                    total = total + ticks() - t.start
                end
                return seconds(total - t.offset)
            end
        end,

    }

end
