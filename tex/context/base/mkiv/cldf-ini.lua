if not modules then modules = { } end modules ['cldf-ini'] = {
    version   = 1.001,
    comment   = "companion to cldf-ini.mkiv",
    author    = "Hans Hagen, PRAGMA-ADE, Hasselt NL",
    copyright = "PRAGMA ADE / ConTeXt Development Team",
    license   = "see context related readme files"
}

-- todo: {token} -> 3 tokens

-- This started as an experiment: generating context code at the lua end. After all
-- it is surprisingly simple to implement due to metatables. I was wondering if
-- there was a more natural way to deal with commands at the lua end. Of course it's
-- a bit slower but often more readable when mixed with lua code. It can also be handy
-- when generating documents from databases or when constructing large tables or so.
--
-- In cldf-tod.lua I have some code that might end up here. In cldf-old.lua the
-- code that precedes more modern solutions (made possible by features in the engine),
-- most noticeably function handling, which worked well for a decade, but by now the
-- more efficient approach is stable enough to move the original code to the obsolete
-- module.
--
-- maybe:  (escape) or 0x2061 (apply function) or 0x2394 (software function ⎔) (old)
-- note : tex.print == line with endlinechar appended
--
-- During the crited project we ran into the situation that luajittex was 10-20 times
-- slower that luatex ... after 3 days of testing and probing we finally figured out that
-- the the differences between the lua and luajit hashers can lead to quite a slowdown
-- in some cases.
--
-- context(lpeg.match(lpeg.patterns.texescape,"${}"))
-- context(string.formatters["%!tex!"]("${}"))
-- context("%!tex!","${}")
--
-- We try not to polute the context namespace too much. For that reason the commands are
-- registered in the context.core namespace. You can call all context commands using
-- context.foo etc. and pipe to context with context("foo"). Defining a local command
-- foo being context.foo is okay, as is context.core.foo. We will have some definitions
-- that are plugged into the core namespace (mostly for speedup) but otherwise specialized
-- writers are in the context namespace only. In your documents you can best use the
-- context.foo(...) and context(...) variant but in some core modules we use the faster
-- ones in critical places (no one will notice of course). The standard syntax highlighter
-- that I use knows how to visualize ctx related code.

-- I cleaned this code up a bit when updating the cld manual but got distracted a bit by
-- watching Trio Hiromi Uehara, Anthony Jackson & Simon Phillips (discovered via the
-- later on YT). Hopefully the video breaks made the following better in the end.

-- This module is also a test bed for experimental features so the content changes over
-- time (for the better or worse). There have been no fundamental changes for many years
-- and performance has not changed much either.

-- The code here has evolved over time, and was already present in the early versions of
-- mkiv. However, it got adapted when feature were added to luatex, like the ability to
-- "print" nodes and later tokens. We use to have control over the method used (if only
-- for testing) but the older code has been removed now. The history is in the archives.
-- These were the controls:

-- local tokenflushmode    = true
-- local nodeflushmode     = true
-- local scannerdefmode    = true
-- local maxflushnodeindex = 0x10FFFF - 1

-- In earlier experiments a function tables was referred to as lua.calls and the primitive
-- \luafunctions was \luacall and we used our own implementation of a function table (more
-- indirectness).

local format, stripstring = string.format, string.strip
local next, type, tostring, tonumber, unpack, select, rawset = next, type, tostring, tonumber, unpack, select, rawset
local insert, remove, concat = table.insert, table.remove, table.concat
local lpegmatch, lpegC, lpegS, lpegP, lpegV, lpegCc, lpegCs, patterns = lpeg.match, lpeg.C, lpeg.S, lpeg.P, lpeg.V, lpeg.Cc, lpeg.Cs, lpeg.patterns

local formatters           = string.formatters -- using formatters is slower in this case
local setmetatableindex    = table.setmetatableindex
local setmetatablecall     = table.setmetatablecall
local setmetatablenewindex = table.setmetatablenewindex

context                 = context    or { }
commands                = commands   or { }
interfaces              = interfaces or { }

local context           = context
local commands          = commands
local interfaces        = interfaces

local loaddata          = io.loaddata

local tex               = tex
local texsprint         = tex.sprint    -- just appended (no space,eol treatment)
local texprint          = tex.print     -- each arg a separate line (not last in directlua)
----- texwrite          = tex.write     -- all 'space' and 'character'

-- In this stage we don't yet have nodes populated so we access the library directly ...

local isnode            = node.isnode   or node.is_node
local copynodelist      = node.copylist or node.copy_list
local writenode         = node.write
local tonut             = node.direct.todirect
local tonode            = node.direct.tonode

local newtoken          = token.new
----- createtoken       = token.create

local istoken           = token.istoken or token.is_token
local setluatoken       = token.setlua  or token.set_lua

-- ... till here.

local isprintable       = tex.isprintable or function(n)
    return n and (type(n) == "string" or isnode(n) or istoken(n))
end

if tex.runlocal then
    -- obselete names
    tex.runtoks  = tex.runlocal
    tex.quittoks = tex.quitlocal
else
    tex.runlocal  = tex.runtoks
    tex.quitlocal = tex.quittoks
end

local catcodenumbers    = catcodes.numbers

local ctxcatcodes       = catcodenumbers.ctxcatcodes
local prtcatcodes       = catcodenumbers.prtcatcodes
local texcatcodes       = catcodenumbers.texcatcodes
local txtcatcodes       = catcodenumbers.txtcatcodes
local vrbcatcodes       = catcodenumbers.vrbcatcodes
local xmlcatcodes       = catcodenumbers.xmlcatcodes

local flush             = texsprint   -- snippets
local flushdirect       = texprint    -- lines
----- flushraw          = texwrite

local report_context    = logs.reporter("cld","tex")
local report_cld        = logs.reporter("cld","stack")
----- report_template   = logs.reporter("cld","template")

local processlines      = true -- experiments.register("context.processlines", function(v) processlines = v end)

local trialtypesetting  = function() return false end

function context.settrialtypesettingmethod(f)
    trialtypesetting         = f
    context.trialtypesetting = f
end

context.trialtypesetting = function() return trialtypesetting() end -- can be aliased !

local knownfunctions = (lua.getfunctionstable or lua.get_functions_table)(true)
local showstackusage = false

trackers.register("context.stack",function(v) showstackusage = v end)

local freed    = { }
local nofused  = 0
local noffreed = 0

local usedstack = function()
    return nofused, noffreed
end

local flushfunction = function(slot,arg)
    if arg() or trialtypesetting() then
        -- keep
    else
        noffreed = noffreed + 1
        freed[noffreed] = slot
        knownfunctions[slot] = false
    end
end

local storefunction = function(arg)
    local f = function(slot) flushfunction(slot,arg) end
    if noffreed > 0 then
        local n = freed[noffreed]
        freed[noffreed] = nil
        noffreed = noffreed - 1
        knownfunctions[n] = f
        return n
    else
        nofused = nofused + 1
        knownfunctions[nofused] = f
        return nofused
    end
end

local flushnode = function(slot,arg)
    if trialtypesetting() then
        writenode(copynodelist(arg))
    else
        writenode(arg)
        noffreed = noffreed + 1
        freed[noffreed] = slot
        knownfunctions[slot] = false
    end
end

local storenode = function(arg)
    local f = function(slot) flushnode(slot,arg) end
    if noffreed > 0 then
        local n = freed[noffreed]
        freed[noffreed] = nil
        noffreed = noffreed - 1
        knownfunctions[n] = f
        return n
    else
        nofused = nofused + 1
        knownfunctions[nofused] = f
        return nofused
    end
end

storage.storedfunctions = storage.storedfunctions or { }
local storedfunctions   = storage.storedfunctions
local initex            = environment.initex

storage.register("storage/storedfunctions", storedfunctions, "storage.storedfunctions")

local f_resolve = nil
local p_resolve = ((1-lpegP("."))^1 / function(s) f_resolve = f_resolve[s] end * lpegP(".")^0)^1

local function resolvestoredfunction(str)
    if type(str) == "string" then
        f_resolve = _G
        lpegmatch(p_resolve,str)
        return f_resolve
    else
        return str
    end
end

local function expose(slot,f,...) -- so we can register yet undefined functions
    local func = resolvestoredfunction(f)
    if not func then
        func = function() report_cld("beware: unknown function %i called: %s",slot,f) end
    end
    knownfunctions[slot] = func
    return func(...)
end

if initex then
    -- todo: log stored functions
else
    local slots = table.sortedkeys(storedfunctions)
    local last  = #slots
    if last > 0 then
        -- we restore the references
        for i=1,last do
            local slot = slots[i]
            local data = storedfunctions[slot]
            knownfunctions[slot] = function(...)
                -- print(data) -- could be trace
                return expose(slot,data,...)
            end
        end
        -- we now know how many are defined
        nofused = slots[last]
        -- normally there are no holes in the list yet
        for i=1,nofused do
            if not knownfunctions[i] then
                noffreed = noffreed + 1
                freed[noffreed] = i
            end
        end
     -- report_cld("%s registered functions, %s freed slots",last,noffreed)
    end
end

local registerfunction = function(f,direct,slot) -- either f=code or f=namespace,direct=name
    local func
    if slot then
        -- already used
    elseif noffreed > 0 then
        slot = freed[noffreed]
        freed[noffreed] = nil
        noffreed = noffreed - 1
    else
        nofused = nofused + 1
        slot = nofused
    end
    if direct then
        if initex then
            func = function(...) return expose(slot,f,...) end
            storedfunctions[slot] = f
        else
            func = resolvestoredfunction(f)
        end
        if type(func) ~= "function" then
            func = function() report_cld("invalid resolve %A, case %s",f,1) end
        end
    elseif type(f) == "string" then
        func = loadstring(f)
        if type(func) ~= "function" then
            func = function() report_cld("invalid code %A, case %s",f,2) end
        end
    elseif type(f) == "function" then
        func = f
    else
        func = function() report_cld("invalid function %A, case %s",f,3) end
    end
    knownfunctions[slot] = func
    return slot
end

local unregisterfunction = function(slot)
    if knownfunctions[slot] then
        noffreed = noffreed + 1
        freed[noffreed] = slot
        knownfunctions[slot] = false
    else
        report_cld("invalid function slot %A",slot)
    end
end

local reservefunction = function()
    if noffreed > 0 then
        local n = freed[noffreed]
        freed[noffreed] = nil
        noffreed = noffreed - 1
        return n
    else
        nofused = nofused + 1
        return nofused
    end
end

local callfunctiononce = function(slot)
    knownfunctions[slot](slot)
    noffreed = noffreed + 1
    freed[noffreed] = slot
    knownfunctions[slot] = false
end

setmetatablecall(knownfunctions,function(t,n) return knownfunctions[n](n) end)

-- some protection

do

    local stub    = { }
    local done    = false
    local message = function()
        -- one time message
        if not done then
            report_cld("")
            report_cld("use : slot = context.functions.register(f)")
            report_cld("and : context.functions.unregister(slot)")
            report_cld("")
            done = true
        end
    end

    setmetatable(stub, {
        __index    = message,
        __newindex = message,
    })

    function lua.getfunctionstable()
        message()
        return stub
    end

    lua.get_functions_table = lua.getfunctionstable

end

-- The next hack is a convenient way to define scanners at the Lua end and get them
-- available at the TeX end. There is some dirty magic needed to prevent overload
-- during format loading. Nowadays we prefer to use the slightly less efficient way
-- of defining interfaces using the implementer. There is a little more overhead in
-- defining as well as runtime overhead, but we accept that.

-- interfaces.scanners.foo = function() context("[%s]",tokens.scanners.string()) end : \scan_foo

local storedscanners    = interfaces.storedscanners or { }
local namesofscanners   = interfaces.namesofscanners or { }
local interfacescanners = { }
local privatenamespace  = "clf_"

interfaces.storedscanners  = storedscanners
interfaces.namesofscanners = namesofscanners

storage.register("interfaces/storedscanners", storedscanners, "interfaces.storedscanners")

-- local function registerscanner(name,action,protected,public,usage) -- todo: combine value and condition
--     rawset(interfacescanners,name,action)
--     local n = storedscanners[name]
--     n = registerfunction("interfaces.scanners."..name,true,n)
--     storedscanners[name] = n
--     namesofscanners[n] = name
--     name = public and name or (privatenamespace .. name)
--  -- print(">>",name,protected and "protected" or "",usage or "macro")
--     setluatoken(name,n,"global",protected and "protected" or "",usage or "macro")
-- end

-- todo: bitmap

local registerscanner if CONTEXTLMTXMODE > 0 then

    -- always permanent but we can consider to obey permanent==false

    -- todo: make bitset instead of keys (nil is skipped anyway)

    local function toflags(specification)
        local protected = (specification.protected and "protected") or (specification.semiprotected and "semiprotected")
        local untraced  = specification.untraced and "untraced"
        local noaligned = specification.noaligned and "noaligned"
        local usage     = specification.usage
        if usage == "value" then
            return "global", "value", "permanent", "untraced", protected
        elseif usage == "condition" then
            return "global", "conditional", "permanent", "untraced", protected
        elseif specification.frozen then
            return "global", "frozen", untraced, protected
        elseif specification.permanent == false or specification.onlyonce then -- for now onlyonce here
            return "global", untraced, protected, semiprotected, noaligned
        else
            return "global", "permanent", untraced, protected, noaligned
        end
    end

    registerscanner = function(name,action,specification)
        rawset(interfacescanners,name,action)
        local n = registerfunction("interfaces.scanners."..name,true,storedscanners[name])
        storedscanners[name] = n
        namesofscanners[n] = name
        name = specification.public and name or (privatenamespace .. name)
     -- print(name,n,toflags(specification))
        setluatoken(name,n,toflags(specification))
    end

else

    registerscanner = function(name,action,specification)
        rawset(interfacescanners,name,action)
        local n = storedscanners[name]
        n = registerfunction("interfaces.scanners."..name,true,n)
        storedscanners[name] = n
        namesofscanners[n] = name
        name = specification.public and name or (privatenamespace .. name)
        setluatoken(name,n,"global",specification.protected and "protected" or "")
    end

end

interfaces.registerscanner = registerscanner

function interfaces.knownscanner(name)
    return interfacescanners[name]
end

function interfaces.nameofscanner(slot)
    return namesofscanners[slot] or slot
end

if CONTEXTLMTXMODE > 0 then

    callbacks.register("show_lua_call", function(what, slot)
        local name = namesofscanners[slot]
     -- return name and formatters["%s: \\%s, slot: %i"](what,name,slot) or ""
        return name and formatters["%s \\%s"](what,name) or ""
    end, "provide lua call details")

end

setmetatablenewindex(interfacescanners, function(t,k,v)
    report_cld("don't register scanner %a directly",k)
 -- registerscanner(k,v)
end)

interfaces.scanners = storage.mark(interfacescanners)

context.functions = {
    register   = function(qualifiedname) return registerfunction(qualifiedname) end, -- only one argument
    unregister = unregisterfunction,
    reserve    = reservefunction,
    known      = knownfunctions,
    callonce   = callfunctiononce,
}

function commands.ctxfunction(code,namespace)
    context(registerfunction(code,namespace))
end

function commands.ctxscanner(name,code,namespace)
    local n = registerfunction(code,namespace)
    if storedscanners[name] then
        storedscanners[name] = n
    end
    context(n)
end

local function dummy() end

local texsetmacro = token.setmacro or token.set_macro

function commands.ctxresetter(name) -- to be checked
    return function()
        if storedscanners[name] then
            rawset(interfacescanners,name,dummy)
         -- context.resetctxscanner(privatenamespace .. name)
            texsetmacro(privatenamespace .. name,"","global")
        end
    end
end

-- Should we keep the catcodes with the function?

local catcodestack    = { }
local currentcatcodes = ctxcatcodes
local contentcatcodes = ctxcatcodes

local catcodes = {
    ctx = ctxcatcodes, ctxcatcodes = ctxcatcodes, context  = ctxcatcodes,
    prt = prtcatcodes, prtcatcodes = prtcatcodes, protect  = prtcatcodes,
    tex = texcatcodes, texcatcodes = texcatcodes, plain    = texcatcodes,
    txt = txtcatcodes, txtcatcodes = txtcatcodes, text     = txtcatcodes,
    vrb = vrbcatcodes, vrbcatcodes = vrbcatcodes, verbatim = vrbcatcodes,
    xml = xmlcatcodes, xmlcatcodes = xmlcatcodes,
}

-- maybe just increment / decrement

-- local function pushcatcodes(c)
--     insert(catcodestack,currentcatcodes)
--     currentcatcodes = (c and catcodes[c] or tonumber(c)) or currentcatcodes
--     contentcatcodes = currentcatcodes
-- end
--
-- local function popcatcodes()
--     currentcatcodes = remove(catcodestack) or currentcatcodes
--     contentcatcodes = currentcatcodes
-- end

local catcodelevel = 0

local function pushcatcodes(c)
    catcodelevel = catcodelevel + 1
    catcodestack[catcodelevel] = currentcatcodes
    currentcatcodes = (c and catcodes[c] or tonumber(c)) or currentcatcodes
    contentcatcodes = currentcatcodes
end

local function popcatcodes()
    if catcodelevel > 0 then
        currentcatcodes = catcodestack[catcodelevel] or currentcatcodes
        catcodelevel = catcodelevel - 1
    end
    contentcatcodes = currentcatcodes
end

function context.unprotect()
    -- at the lua end
    catcodelevel = catcodelevel + 1
    catcodestack[catcodelevel] = currentcatcodes
    currentcatcodes = prtcatcodes
    contentcatcodes = prtcatcodes
    -- at the tex end
    flush("\\unprotect")
end

function context.protect()
    -- at the tex end
    flush("\\protect")
    -- at the lua end
    if catcodelevel > 0 then
        currentcatcodes = catcodestack[catcodelevel] or currentcatcodes
        catcodelevel = catcodelevel - 1
    end
    contentcatcodes = currentcatcodes
end

context.catcodes     = catcodes
context.pushcatcodes = pushcatcodes
context.popcatcodes  = popcatcodes

-- -- --

local newline       = patterns.newline
local space         = patterns.spacer
local spacing       = newline * space^0
local content       = lpegC((1-spacing)^1)            -- texsprint
local emptyline     = space^0 * newline^2             -- texprint("")
                    + newline * space^1 * newline^1
local endofline     = space^0 * newline * space^0     -- texsprint(" ")
local simpleline    = endofline * lpegP(-1)           --

local verbose       = lpegC((1-space-newline)^1)
local beginstripper = (lpegS(" \t")^1 * newline^1) / ""
local endstripper   = beginstripper * lpegP(-1)

local justaspace    = space * lpegCc("")
local justanewline  = newline * lpegCc("")

local function n_content   (s) flush      (contentcatcodes,s    ) end
local function n_verbose   (s) flush      (vrbcatcodes,    s    ) end
local function n_endofline ()  flush      (currentcatcodes," \r") end
local function n_emptyline ()  flushdirect(currentcatcodes,"\r" ) end
local function n_simpleline()  flush      (currentcatcodes," \r") end

local n_exception = ""

-- better a table specification

function context.newtexthandler(specification)
    specification = specification or { }
    --
    local s_catcodes   = specification.catcodes
    --
    local f_before     = specification.before
    local f_after      = specification.after
    --
    local f_endofline  = specification.endofline  or n_endofline
    local f_emptyline  = specification.emptyline  or n_emptyline
    local f_simpleline = specification.simpleline or n_simpleline
    local f_content    = specification.content    or n_content
    local f_space      = specification.space
    --
    local p_exception  = specification.exception
    --
    if s_catcodes then
        f_content = function(s)
            flush(s_catcodes,s)
        end
    end
    --
    local pattern
    if f_space then
        if p_exception then
            local content = lpegC((1-spacing-p_exception)^1)
            pattern =
              (
                    justaspace   / f_space
                  + justanewline / f_endofline
                  + p_exception
                  + content      / f_content
                )^0
        else
            local content = lpegC((1-space-endofline)^1)
            pattern =
                (
                    justaspace   / f_space
                  + justanewline / f_endofline
                  + content      / f_content
                )^0
        end
    else
        if p_exception then
            local content = lpegC((1-spacing-p_exception)^1)
            pattern =
                simpleline / f_simpleline
              +
              (
                    emptyline  / f_emptyline
                  + endofline  / f_endofline
                  + p_exception
                  + content    / f_content
                )^0
        else
            local content = lpegC((1-spacing)^1)
            pattern =
                simpleline / f_simpleline
                +
                (
                    emptyline / f_emptyline
                  + endofline / f_endofline
                  + content   / f_content
                )^0
        end
    end
    --
    if f_before then
        pattern = (P(true) / f_before) * pattern
    end
    --
    if f_after then
        pattern = pattern * (P(true) / f_after)
    end
    --
    return function(str) return lpegmatch(pattern,str) end, pattern
end

function context.newverbosehandler(specification) -- a special variant for e.g. cdata in lxml-tex
    specification = specification or { }
    --
    local f_line    = specification.line    or function() flushdirect("\r") end
    local f_space   = specification.space   or function() flush      (" ")  end
    local f_content = specification.content or n_verbose
    local f_before  = specification.before
    local f_after   = specification.after
    --
    local pattern =
        justanewline / f_line    -- so we get call{}
      + verbose      / f_content
      + justaspace   / f_space   -- so we get call{}
    --
    if specification.strip then
        pattern = beginstripper^0 * (endstripper + pattern)^0
    else
        pattern = pattern^0
    end
    --
    if f_before then
        pattern = (lpegP(true) / f_before) * pattern
    end
    --
    if f_after then
        pattern = pattern * (lpegP(true) / f_after)
    end
    --
    return function(str) return lpegmatch(pattern,str) end, pattern
end

local flushlines = context.newtexthandler {
    content    = n_content,
    endofline  = n_endofline,
    emptyline  = n_emptyline,
    simpleline = n_simpleline,
}

-- The next variant is only used in rare cases (buffer to mp):

local printlines_ctx = (
    (newline)     / function()  texprint("") end +
    (1-newline)^1 / function(s) texprint(ctxcatcodes,s) end * newline^-1
)^0

local printlines_raw = (
    (newline)     / function()  texprint("") end +
    (1-newline)^1 / function(s) texprint(s)  end * newline^-1
)^0

function context.printlines(str,raw)     -- todo: see if via file is useable
    if raw then
        lpegmatch(printlines_raw,str)
    else
        lpegmatch(printlines_ctx,str)
    end
end

function context.printtable(t,separator)     -- todo: see if via file is useable
    if separator == nil or separator == true then
        separator = "\r"
    elseif separator == "" or separator == false then
        separator = ""
    end
    local s = concat(t,separator)
    if s ~= "" then
        context(s)
    end
end

-- -- -- "{" .. ti .. "}" is somewhat slower in a cld-mkiv run than "{",ti,"}"

local containseol = patterns.containseol

local lua_call_code = tokens.commands.lua_expandable_call or tokens.commands.lua_call

local sortedhashindeed = false

directives.register("context.sorthash",function(v)
    sortedhashindeed = v and table.sortedhash or nil
end)

local function writer(parent,command,...) -- already optimized before call
    if type(command) == "string" then -- for now
        flush(currentcatcodes,command) -- todo: ctx|prt|texcatcodes
    else
        flush(command) -- todo: ctx|prt|texcatcodes
    end
    local direct = false
    for i=1,select("#",...) do
        local ti = select(i,...)
        if direct then
            local typ = type(ti)
            if typ == "string" or typ == "number" then
                flush(currentcatcodes,ti)
            else -- node.write
                report_context("error: invalid use of direct in %a, only strings and numbers can be flushed directly, not %a",command,typ)
            end
            direct = false
        elseif ti == nil then
            -- nothing
        elseif ti == "" then
            flush(currentcatcodes,"{}")
     -- elseif ti == 1 then
     --     flush(currentcatcodes,"{1}")
        else
            local typ = type(ti)
            if typ == "string" then
                -- is processlines seen ?
                if processlines and lpegmatch(containseol,ti) then
                    flush(currentcatcodes,"{")
                    flushlines(ti)
                    flush(currentcatcodes,"}")
                elseif currentcatcodes == contentcatcodes then
                    flush(currentcatcodes,"{",ti,"}")
                else
                    flush(currentcatcodes,"{")
                    flush(contentcatcodes,ti)
                    flush(currentcatcodes,"}")
                end
            elseif typ == "number" then
                -- numbers never have funny catcodes
                flush(currentcatcodes,"{",ti,"}")
            elseif typ == "table" then
                local tn = #ti
                if tn == 0 then
                    local done = false
                    if sortedhashindeed then
                        for k, v in sortedhashindeed(ti) do
                            if done then
                                if v == "" then
                                    flush(currentcatcodes,",",k,'=')
                                else
                                    flush(currentcatcodes,",",k,"={",v,"}")
                                end
                            else
                                if v == "" then
                                    flush(currentcatcodes,"[",k,"=")
                                else
                                    flush(currentcatcodes,"[",k,"={",v,"}")
                                end
                                done = true
                            end
                        end
                    else
                        for k, v in next, ti do
                            if done then
                                if v == "" then
                                    flush(currentcatcodes,",",k,'=')
                                else
                                    flush(currentcatcodes,",",k,"={",v,"}")
                                end
                            else
                                if v == "" then
                                    flush(currentcatcodes,"[",k,"=")
                                else
                                    flush(currentcatcodes,"[",k,"={",v,"}")
                                end
                                done = true
                            end
                        end
                    end
                    if done then
                        flush(currentcatcodes,"]")
                    else
                        flush(currentcatcodes,"[]")
                    end
                elseif tn == 1 then -- some 20% faster than the next loop
                    local tj = ti[1]
                    if type(tj) == "function" then
                        tj = storefunction(tj)
                        flush(currentcatcodes,"[",newtoken(tj,lua_call_code),"]")
                    else
                        flush(currentcatcodes,"[",tj,"]")
                    end
                else
                    flush(currentcatcodes,"[")
                    for j=1,tn do
                        local tj = ti[j]
                        if type(tj) == "function" then
                            tj = storefunction(tj)
                            flush(currentcatcodes,"[",newtoken(tj,lua_call_code),j == tn and "]" or ",")
                        else
                            if j == tn then
                                flush(currentcatcodes,tj,"]")
                            else
                                flush(currentcatcodes,tj,",")
                            end
                        end
                    end
                end
            elseif typ == "function" then
                -- todo: ctx|prt|texcatcodes
                ti = storefunction(ti)
                flush(currentcatcodes,"{",newtoken(ti,lua_call_code),"}")
            elseif typ == "boolean" then
                if ti then
                    flushdirect(currentcatcodes,"\r")
                else
                    direct = true
                end
            elseif typ == "thread" then
                report_context("coroutines not supported as we cannot yield across boundaries")
         -- elseif isnode(ti) or istoken(ti) then
            elseif isprintable(ti) then
                flush(currentcatcodes,"{",ti,"}")
            else
                local s = tostring(ti)
                if s then
                    flushdirect(currentcatcodes,s)
                else
                    report_context("error: %a gets a weird argument %a",command,ti)
                end
            end
     -- else
     --     local n = isnode(ti)
     --     if n then
     --         flush(ti)
     --     else
     --         report_context("error: %a gets a weird argument %a",command,ti)
     --     end
        end
    end
end

local toks = tokens.cache
context.tokenizedcs = toks

local core = setmetatableindex(function(parent,k)
    local t
    local f = function(first,...)
        if not t then
            t = toks[k]
        end
        if first == nil then
            flush(t)
        else
            return writer(context,t,first,...)
        end
    end
    parent[k] = f
    return f
end)

core.cs = setmetatableindex(function(parent,k)
    local t
    local f = function()
        if not t then
            t = toks[k]
        end
        flush(t)
    end
    parent[k] = f
    return f
end)

local indexer = function(parent,k)
    if type(k) == "string" then
        return core[k]
    else
        return context -- catch
    end
end

context.core = core

-- only for internal usage:

-- local prtindexer = nil
--
-- do
--
--     -- the only variant is not much faster than the full but it's more
--     -- memory efficient
--
--     local protected     = { }
--     local protectedcs   = { }
--     context.protected   = protected
--     context.protectedcs = protectedcs
--
--     local function fullindexer(t,k)
--         local c = "\\" .. k -- tostring(k)
--         local v = function(first,...)
--             if first == nil then
--                 flush(prtcatcodes,c)
--             else
--                 return prtwriter(c,first,...)
--             end
--         end
--         rawset(t,k,v) -- protected namespace
--         return v
--     end
--
--     local function onlyindexer(t,k)
--         local c = "\\" .. k -- tostring(k)
--         local v = function()
--             flush(prtcatcodes,c)
--         end
--         rawset(protected,k,v)
--         rawset(t,k,v)
--         return v
--     end
--
--     protected.cs = setmetatableindex(function(parent,k)
--         local c = "\\" .. k -- tostring(k)
--         local v = function()
--             flush(prtcatcodes,c)
--         end
--         parent[k] = v
--         return v
--     end
--
--     setmetatableindex(protected,fullindexer)
--     setmetatablecall (protected,prtwriter)
--
--     setmetatableindex(protectedcs,onlyindexer)
--     setmetatablecall (protectedcs,prtwriter)
--
-- end

-- local splitformatters = utilities.strings.formatters.new(true) -- not faster (yet)

local caller = function(parent,f,a,...)
    if not parent then
        -- so we don't need to test in the calling (slower but often no issue)
    elseif f ~= nil then
        local typ = type(f)
        if typ == "string" then
            if f == "" then
                -- new, can save a bit sometimes
             -- if trace_context then
             --     report_context("empty argument to context()")
             -- end
            elseif a then
                flush(contentcatcodes,formatters[f](a,...)) -- was currentcatcodes
             -- flush(contentcatcodes,splitformatters[f](a,...)) -- was currentcatcodes
            elseif processlines and lpegmatch(containseol,f) then
                flushlines(f)
            else
                flush(contentcatcodes,f)
            end
        elseif typ == "number" then
            if a then
                flush(currentcatcodes,f,a,...)
            else
                flush(currentcatcodes,f)
            end
        elseif typ == "function" then
            -- ignored: a ...
            f = storefunction(f)
            flush(currentcatcodes,"{",newtoken(f,lua_call_code),"}")
        elseif typ == "boolean" then
            if f then
                if a ~= nil then
                    flushlines(a)
                else
                    flushdirect(currentcatcodes,"\n") -- no \r, else issues with \startlines ... use context.par() otherwise
                end
            else
                if a ~= nil then
                    -- no command, same as context(a,...)
                    writer(parent,"",a,...)
                else
                    -- ignored
                end
            end
        elseif typ == "thread" then
            report_context("coroutines not supported as we cannot yield across boundaries")
     -- elseif isnode(f) or istoken(f) then
        elseif isprintable(f) then
            flush(f)
        else
            local s = tostring(f)
            if s then
                flushdirect(currentcatcodes,s)
            else
                report_context("error: %a gets a weird argument %a","context",f)
            end
        end
 -- else
 --     local n = isnode(f)
 --     if n then
 --         flush(f)
 --     else
 --         report_context("error: %a gets a weird argument %a","context",f)
 --     end
    end
end

context.nodes = { -- todo
    store = storenode,
    flush = function(n)
        flush(n)
    end,
}

context.nuts = { -- todo
    store = function(n)
        return storenode(tonut(n))
    end,
    flush = function(n,d)
        flush(tonode(n))
    end,
}

local defaultcaller = caller

setmetatableindex(context,indexer)
setmetatablecall (context,caller)

function  context.sprint(...) -- takes catcodes as first argument
    flush(...)
end

function context.fprint(first,second,third,...)
    if type(first) == "number" then
        if third then
            flush(first,formatters[second](third,...))
        else
            flush(first,second)
        end
    else
        if second then
            flush(formatters[first](second,third,...))
        else
            flush(first)
        end
    end
end

tex.fprint = context.fprint

-- logging

local trace_stack       = { report_context }

local normalflush       = flush
local normalflushdirect = flushdirect
----- normalflushraw    = flushraw
local normalwriter      = writer
local currenttrace      = report_context
local nofwriters        = 0
local nofflushes        = 0

local tracingpermitted  = true

local visualizer = lpeg.replacer {
    { "\n", "<<newline>>" },
    { "\r", "<<par>>" },
}

statistics.register("traced context lua functions", function()
    local used, freed = usedstack()
    local unreachable = used - freed
    if nofwriters > 0 or nofflushes > 0 then
        return format("writers: %s, flushes: %s, maxstack: %s",nofwriters,nofflushes,used,freed,unreachable)
    elseif showstackusage or unreachable > 0 then
        return format("maxstack: %s, freed: %s, unreachable: %s",used,freed,unreachable)
    end
end)

-- The cmd names were synchronized with the normal call cmd names.

local luacalls = {
    lua_function_call  = true,
    lua_protected_call = true,
    lua_value          = true,
    lua_local_call     = true,
    lua_call           = true,
}

local function userdata(argument)
    if isnode(argument) then
        return formatters["<< %s node %i>>"](nodes.nodecodes[argument.id],tonut(argument))
    elseif istoken(argument) then
        local csname = argument.csname
        if csname then
         -- return formatters["<<\\%s>>"](csname)
            return formatters["\\%s"](csname)
        end
        if luacalls[argument.cmdname] then
            return "<<function>>" -- argument.mode
        end
        return "<<token>>"
    else
        return "<<userdata>>"
    end
end


local tracedwriter = function(parent,...) -- also catcodes ?
    nofwriters = nofwriters + 1
    local savedflush       = flush
    local savedflushdirect = flushdirect -- unlikely to be used here
    local t = { "w : - : " }
    local n = 1
    local traced = function(catcodes,...) -- todo: check for catcodes
        local s = type(catcodes) == "number" and { ... } or { catcodes, ... }
        for i=1,#s do
            local argument = s[i]
            local argtype  = type(argument)
            if argtype == "string" then
                s[i] = lpegmatch(visualizer,argument)
            elseif argtype == "number" then
                s[i] = argument
            elseif argtype == "userdata" then
                s[i] = userdata(argument)
            else
                s[i] = formatters["<<%S>>"](argument)
            end
        end
        s = concat(s)
        s = lpegmatch(visualizer,s)
        n = n + 1
        t[n] = s
    end
    flush = function(...)
        normalflush(...)
        if tracingpermitted then
            traced(...)
        end
    end
    flushdirect = function(...)
        normalflushdirect(...)
        if tracingpermitted then
            traced(...)
        end
    end
    normalwriter(parent,...)
    flush       = savedflush
    flushdirect = savedflushdirect
    currenttrace(concat(t))
end

-- we could reuse collapsed

local traced = function(one,two,...)
    if two ~= nil then
        -- only catcodes if 'one' is number
        local catcodes  = type(one) == "number" and one
        local arguments = catcodes and { two, ... } or { one, two, ... }
        local collapsed = { formatters["f : %s : "](catcodes or '-') }
        local c         = 1
        for i=1,#arguments do
            local argument = arguments[i]
            local argtype = type(argument)
            c = c + 1
            if argtype == "string" then
                collapsed[c] = lpegmatch(visualizer,argument)
            elseif argtype == "number" then
                collapsed[c] = argument
            elseif argtype == "userdata" then
                collapsed[c] = userdata(argument)
            else
                collapsed[c] = formatters["<<%S>>"](argument)
            end
        end
        currenttrace(concat(collapsed))
    elseif one ~= nil then
        -- no catcodes
        local argtype = type(one)
        if argtype == "string" then
            currenttrace(formatters["f : - : %s"](lpegmatch(visualizer,one)))
        elseif argtype == "number" then
            currenttrace(formatters["f : - : %s"](one))
        elseif argtype == "userdata" then
            currenttrace(formatters["F : - : %s"](userdata(one)))
        else
            currenttrace(formatters["f : - : <<%S>>"](one))
        end
    end
end

local tracedflush = function(one,two,...)
    nofflushes = nofflushes + 1
    if two ~= nil then
        normalflush(one,two,...)
    else
        normalflush(one)
    end
    if tracingpermitted then
        traced(one,two,...)
    end
end

local tracedflushdirect = function(one,two,...)
    nofflushes = nofflushes + 1
    if two ~= nil then
        normalflushdirect(one,two,...)
    else
        normalflushdirect(one)
    end
    if tracingpermitted then
        traced(one,two,...)
    end
end

function context.pushlogger(trace)
    trace = trace or report_context
    insert(trace_stack,currenttrace)
    currenttrace = trace
end

function context.poplogger()
    if #trace_stack > 1 then
        currenttrace = remove(trace_stack) or report_context
    else
        currenttrace = report_context
    end
end

function context.settracing(v)
    if v then
        flush       = tracedflush
        flushdirect = tracedflushdirect
        writer      = tracedwriter
    else
        flush       = normalflush
        flushdirect = normalflushdirect
        writer      = normalwriter
    end
    return flush, writer, flushdirect
end

function context.getlogger()
    return flush, writer, flushdirect
end

trackers.register("context.trace",context.settracing)

local trace_cld = false  trackers.register("context.files", function(v) trace_cld = v end)

do

    -- This is the most reliable way to deal with nested buffers and other
    -- catcode sensitive data.

    local resolve     = resolvers.savers.byscheme
    local validstring = string.valid
    local input       = context.input

    local function viafile(data,tag)
        if data and data ~= "" then
            local filename = resolve("virtual",validstring(tag,"viafile"),data)
         -- context.startregime { "utf" }
            input(filename)
         -- context.stopregime()
        end
    end

    context.viafile    = viafile

    -- experiment for xtables, don't use it elsewhere yet

    local collected    = nil
    local nofcollected = 0
    local sentinel     = string.char(26) -- ASCII SUB character : endoffileasciicode : ignorecatcode
    local level        = 0

    local function collect(c,a,...) -- can be optimized
        if type(c) == "userdata" then
            nofcollected = nofcollected + 1
         -- collected[nofcollected] = userdata(c)
            collected[nofcollected] = "\\" .. c.csname
        end
        if a then
            for i=1,select("#",a,...) do
                local c = select(i,a,...)
                nofcollected = nofcollected + 1
                collected[nofcollected] = type(c) == "userdata" and userdata(c) or c
            end
        end
    end

    local collectdirect = collect
    local permitted     = true

    -- doesn't work well with tracing do we need to avoid that when
    -- collecting stuff

    function context.startcollecting()
        if level == 0 then
            collected    = { }
            nofcollected = 0
            flush        = collect
            flushdirect  = collectdirect
            permitted    = tracingpermitted
        end
        level = level + 1
    end

    function context.stopcollecting()
        level = level - 1
        if level < 1 then
            local result     = concat(collected,sentinel)
            flush            = normalflush
            flushdirect      = normalflushdirect
            tracingpermitted = permitted
            collected        = nil
            nofcollected     = 0
            level            = 0
            viafile(result)
        end
    end

    local findtexfile = resolvers.findtexfile
    local findfile    = resolvers.findfile

    function context.runfile(filename)
        local foundname = findtexfile(file.addsuffix(filename,"cld")) or ""
        if foundname ~= "" then
            local ok = dofile(foundname)
            if type(ok) == "function" then
                if trace_cld then
                    report_context("begin of file %a (function call)",foundname)
                end
                ok()
                if trace_cld then
                    report_context("end of file %a (function call)",foundname)
                end
            elseif ok then
                report_context("file %a is processed and returns true",foundname)
            else
                report_context("file %a is processed and returns nothing",foundname)
            end
        else
            report_context("unknown file %a",filename)
        end
    end

    function context.loadfile(filename)
        context(stripstring(loaddata(findfile(filename))))
    end

    function context.loadviafile(filename)
        viafile(stripstring(loaddata(findfile(filename))))
    end

end

-- some functions

function context.direct(first,...)
    if first ~= nil then
        return writer(context,"",first,...)
    end
end

-- context.delayed (todo: lines)

do

    local delayed = { }

    local function indexer(parent,k)
        local f = function(...)
            local a = { ... } -- this also freezes ...
            return function()
             -- return context[k](unpack(a))
                return core[k](unpack(a))
            end
        end
        parent[k] = f
        return f
    end

    local function caller(parent,...) -- todo: nodes
        local a = { ... }
        return function()
         -- return context(unpack(a))
            return defaultcaller(context,unpack(a))
        end
    end

    setmetatableindex(delayed,indexer)
    setmetatablecall (delayed,caller)

    context.delayed = delayed

end

-- do
--
--     -- context.nested (todo: lines), creates strings
--
--     local nested = { }
--
--     local function indexer(parent,k) -- not ok when traced
--         local f = function(...)
--             local t, savedflush, n = { }, flush, 0
--             flush = function(c,f,s,...) -- catcodes are ignored
--                 n = n + 1
--                 t[n] = s and concat{f,s,...} or f -- optimized for #args == 1
--             end
--          -- context[k](...)
--             core[k](...)
--             flush = savedflush
--             return concat(t)
--         end
--         parent[k] = f
--         return f
--     end
--
--     local function caller(parent,...)
--         local t, savedflush, n = { }, flush, 0
--         flush = function(c,f,s,...) -- catcodes are ignored
--             n = n + 1
--             t[n] = s and concat{f,s,...} or f -- optimized for #args == 1
--         end
--      -- context(...)
--         defaultcaller(context,...)
--         flush = savedflush
--         return concat(t)
--     end
--
--     setmetatableindex(nested,indexer)
--     setmetatablecall (nested,caller)
--
--     context.nested = nested
--
-- end

context.nested = context.delayed

-- verbatim

function context.newindexer(catcodes,cmdcodes)
    local handler = { }

    local function indexer(parent,k)
        local command = core[k]
        local f = function(...)
            local savedcatcodes = contentcatcodes
            contentcatcodes = catcodes
            command(...)
            contentcatcodes = savedcatcodes
        end
        parent[k] = f
        return f
    end

    local function caller(parent,...)
        local savedcatcodes = contentcatcodes
        contentcatcodes = catcodes
        defaultcaller(parent,...)
        contentcatcodes = savedcatcodes
    end

    handler.cs = core.cs

    setmetatableindex(handler,indexer)
    setmetatablecall (handler,caller)

    return handler
end

context.verbatim  = context.newindexer(vrbcatcodes,ctxcatcodes)
context.puretext  = context.newindexer(txtcatcodes,ctxcatcodes)
context.protected = context.newindexer(prtcatcodes,prtcatcodes)

-- formatted

do

    local formatted = { }

    -- formatted.command([catcodes,]format[,...])

    local function formattedflush(parent,c,catcodes,fmt,...)
        if not catcodes then
            return writer(parent,c)
        elseif not fmt then
            return writer(parent,c,catcodes)
        elseif type(catcodes) == "number" then
            local result
            pushcatcodes(catcodes)
            result = writer(parent,c,formatters[fmt](...))
            popcatcodes()
            return result
        else
            return writer(parent,c,formatters[catcodes](fmt,...))
        end
    end

    local toks = tokens.cache

    local indexer = function(parent,k)
        if type(k) == "string" then
            local t
            local f = function(first,...)
                if not t then
                    t = toks[k]
                end
                if first == nil then
                    flush(t)
                else
                    return formattedflush(parent,t,first,...)
                end
            end
            parent[k] = f
            return f
        else
            return context -- catch
        end
    end

    -- formatted([catcodes,]format[,...])

    local function caller(parent,catcodes,fmt,...)
        if not catcodes then
            -- nothing
        elseif not fmt then
            flush(catcodes)
        elseif type(catcodes) == "number" then
            flush(catcodes,formatters[fmt](...))
        else
            flush(formatters[catcodes](fmt,...))
        end
    end

    setmetatableindex(formatted,indexer)
    setmetatablecall (formatted,caller)

    context.formatted = formatted

end

do

    -- metafun (this will move to another file)

    local metafun = { }

    function metafun.start()
        context.startMPcode()
    end

    function metafun.stop()
        context.stopMPcode()
    end

    setmetatablecall(metafun,defaultcaller)

    function metafun.color(name) -- obsolete
        return name -- formatters[ [[\MPcolor{%s}]] ](name)
    end

    -- metafun.delayed

    local delayed = { }

    local function indexer(parent,k)
        local f = function(...)
            local a = { ... }
            return function()
                return metafun[k](unpack(a))
            end
        end
        parent[k] = f
        return f
    end


    local function caller(parent,...)
        local a = { ... }
        return function()
            return metafun(unpack(a))
        end
    end

    setmetatableindex(delayed,indexer)
    setmetatablecall (delayed,caller)

    context.metafun = metafun
    metafun.delayed = delayed

end

-- helpers:

do

    function context.concat(...)
        context(concat(...))
    end

    local p_texescape = patterns.texescape
    local p_ctxescape = patterns.ctxescape

    function context.escaped   (s) if s then context(lpegmatch(p_texescape,s) or s) end end
    function context.ctxescaped(s) if s then context(lpegmatch(p_ctxescape,s) or s) end end

    function context.escape   (s) return (s and lpegmatch(p_texescape,s)) or s or "" end
    function context.ctxescape(s) return (s and lpegmatch(p_ctxescape,s)) or s or "" end

end

-- templates

do

    local single  = lpegP("%")
    local double  = lpegP("%%")
    local lquoted = lpegP("%[")
    local rquoted = lpegP("]%")
    local space   = lpegP(" ")

    local start = [[
    local texescape = lpeg.patterns.texescape
    local lpegmatch = lpeg.match
    return function(variables) return
    ]]

    local stop  = [[
    end
    ]]

    local replacer = lpegP { "parser",
        parser   = lpegCs(lpegCc(start) * lpegV("step") * (lpegCc("..") * lpegV("step"))^0 * lpegCc(stop)),
        unquoted = (lquoted*space/'')
                 * ((lpegC((1-space*rquoted)^1)) / "lpegmatch(texescape,variables%0 or '')" )
                 * (space*rquoted/'')
                 + (lquoted/'')
                 * ((lpegC((1-rquoted)^1)) / "lpegmatch(texescape,variables['%0'] or '')" )
                 * (rquoted/''),
        key      = (single*space/'')
                 * ((lpegC((1-space*single)^1)) / "(variables%0 or '')" )
                 * (space*single/'')
                 + (single/'')
                 * ((lpegC((1-single)^1)) / "(variables['%0'] or '')" )
                 * (single/''),
        escape   = double/'%%',
        step     = lpegV("unquoted")
                 + lpegV("escape")
                 + lpegV("key")
                 + lpegCc("\n[===[") * (1 - lpegV("unquoted") - lpegV("escape") - lpegV("key"))^1 * lpegCc("]===]\n"),
    }

    local templates = { }

    local function indexer(parent,k)
        local v = lpegmatch(replacer,k)
        if not v then
         -- report_template("invalid template:\n%s",k)
            v = "error: no valid template (1)"
        else
            local f = loadstring(v)
            if type(f) ~= "function" then
             -- report_template("invalid template:\n%s\n=>\n%s",k,v)
                v = "error: no valid template (2)"
            else
                f = f()
                if not f then
                 -- report_template("invalid template:\n%s\n=>\n%s",k,v)
                    v = "error: no valid template (3)"
                else
                    v = f
                end
            end
        end
        if type(v) == "function" then
            local f = function(first,second)
                if second then
                    pushcatcodes(first)
                    flushlines(v(second))
                    popcatcodes()
                else
                    flushlines(v(first))
                end
            end
            parent[k] = f
            return f
        else
            return function()
                flush(v)
            end
        end

    end

    local function caller(parent,k,...)
        return parent[k](...)
    end

    setmetatableindex(templates,indexer)
    setmetatablecall (templates,caller)

    context.templates = templates

end

-- The above is a bit over the top as we could also stick to a simple context.replace
-- which is fast enough anyway, but the above fits in nicer, also with the catcodes.
--
-- local replace = utilities.templates.replace
--
-- function context.template(template,variables)
--     context(replace(template,variables))
-- end

do

    -- not the best namespace -- some day maybe texs a la nodes and tokens .. also,
    -- we already have tex.modes so we need a different name

    local modelevels = tex.getmodevalues()
    local t          = table.keys(modelevels)
    tex.modelevels   = table.swapped(modelevels,modelevels) -- utilities.storage.allocate()

    for i=1,#t do local k = t[i] modelevels[-k] = modelevels[k] end

    if CONTEXTLMTXMODE > 0 then

        -- also elsewhere

        local flagcodes = tex.getflagvalues()
        tex.flagcodes   = table.swapped(flagcodes,flagcodes) -- utilities.storage.allocate()

    end

end
