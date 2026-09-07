if not modules then modules = { } end modules['mtx-testsuite'] = {
    version   = 1.001,
    comment   = "companion to mtxrun.lua",
    author    = "Hans Hagen, PRAGMA-ADE, Hasselt NL",
    copyright = "PRAGMA ADE / ConTeXt Development Team",
    license   = "see context related readme files"
}

-- runtestsparallel.cmd:

-- @echo off
--
-- REM ~ echo %path%
--
-- mtxrun  --generate %1
-- context --make en %1
-- mtxrun  --script font --reload --force
-- mtxrun  --script testsuite --parallel --pattern=**/*.tex --purge %1
--
-- echo.
-- echo results:
-- echo.
--
-- type testsuite-process.lua
--
-- echo.
-- echo.

-- runtestsparallel.sh:

-- #! /bin/sh
--
-- mtxrun  --generate $1
-- context --make en $1
-- mtxrun  --script font --reload --force
-- mtxrun  --script testsuite --parallel --pattern=**/*.tex --purge $1
--
-- cat testsuite-process.lua

-- (1) mtxrun --script testsuite --compare --oldname=foo --newname=bar --objects --pattern=*.tex
-- (2) move/mv bar.lua foo.lua
-- (3) mtxrun --script testsuite --compare --oldname=foo --newname=bar --objects --pattern=*.tex

local helpinfo = [[
<?xml version="1.0"?>
<application>
 <metadata>
  <entry name="name">mtx-testsuite</entry>
  <entry name="detail">Experiments with the testsuite</entry>
  <entry name="version">1.00</entry>
 </metadata>
 <flags>
  <category name="basic">
   <subcategory>
    <flag name="process"><short>process files (<ref name="pattern"/>)"/></short></flag>
   </subcategory>
   <subcategory>
    <flag name="compare"><short>compare files (<ref name="pattern"/> <ref name="newname"/> <ref name="oldname"/> <ref name="collect)"/></short></flag>
   </subcategory>
  </category>
 </flags>
 <examples>
  <category>
   <title>Example</title>
   <subcategory>
    <example><command>mtxrun --script testsuite --compare --objects --oldname=cld-compare-old  --newname=cld-compare-new  --pattern=**/*.cld</command></example>
    <example><command>mtxrun --script testsuite --compare --objects --oldname=mkiv-compare-old --newname=mkiv-compare-new --pattern=**/*.mkiv</command></example>
    <example><command>mtxrun --script testsuite --compare --objects --oldname=mkvi-compare-old --newname=mkvi-compare-new --pattern=**/*.mkvi</command></example>
    <example><command>mtxrun --script testsuite --compare --objects --oldname=tex-compare-old  --newname=tex-compare-new  --pattern=**/*.tex</command></example>
   </subcategory>
  </category>
 </examples>
</application>
]]

local application = logs.application {
    name     = "mtx-testsuite",
    banner   = "Experiments with the testsuite 1.00",
    helpinfo = helpinfo,
}

local gmatch, match, gsub, find, lower, format = string.gmatch, string.match, string.gsub, string.find, string.lower, string.format
local concat = table.concat
local split = string.split
local are_equal = table.are_equal
local tonumber = tonumber
local formatters = string.formatters
local clock = os.gettimeofday or os.clock

local report = application.report

scripts           = scripts           or { }
scripts.testsuite = scripts.testsuite or { }

local f_runner = formatters['%s --batch --nocompression --nodates --trailerid=1 --randomseed=1234 %s "%s"']
----- f_runner = formatters['%s --nocompression --nodates --trailerid=1 --randomseed=1234 "%s"']

function scripts.testsuite.process()
    local pattern = environment.argument("pattern")
    if pattern then
        local cleanup = environment.argument("cleanup")
        local jit     = environment.argument("jit")
        local engine  = environment.argument("luatex") and "--luatex" or ""
        local results = { }
        local start   = statistics.starttiming(scripts.testsuite.process)
        local files   = dir.glob(pattern)
        local start   = tonumber(environment.argument("start"))
        local suffix  = start and ("-" ..start) or ""
        local start   = start or 1
        local stop    = tonumber(environment.argument("stop"))  or #files
        local step    = tonumber(environment.argument("step"))  or 1
        local luaname = "testsuite-process" .. suffix .. ".lua"
        for i=start,stop,step do
            local filename = files[i]
            if filename then
                local dirname  = file.dirname(filename)
                local basename = file.basename(filename)
                local texname  = basename
                local pdfname  = file.replacesuffix(basename,"pdf")
                local tucname  = file.replacesuffix(basename,"tuc")
                local workdir  = lfs.currentdir()
                lfs.chdir(dirname)
                os.remove(pdfname)
                if cleanup then
                    os.remove(tucname)
                end
                if lfs.isfile(texname) then
                    local command = f_runner(jit and "contextjit" or "context",engine,texname)
                    local result  = tonumber(os.execute(command)) or 0
                    if result > 0 then
                        results[filename] = result
                    end
                end
                lfs.chdir(workdir)
            else
                break
            end
        end
        statistics.stoptiming(scripts.testsuite.process)
        results.runtime = statistics.elapsedtime(scripts.testsuite.process)
        io.savedata(luaname,table.serialize(results,true))
        report()
        report("files: %i, runtime: %s, overview: %s",#files,results.runtime,luaname)
        report()
    end
end

-- early september 2026 : Intel Xeon E3-1505M v6 @ 3.00GHz, 48GB, 2TB, Dell 7220
--
-- 2200 files, windows 11, popen,           828 sec after prepare
--                         process, native, 417 sec after prepare
--                         process, fork,   417 sec after prepare
--             ubuntu wsl, process, fork,   237 sec after prepare

local function preparerun(capped)
    statistics.starttiming("testsuite:prepare")
    if not capped then
        os.execute("mtxrun  --generate")
        if squid then
            squid.signal("busy")
            os.sleep(2)
            squid.signal("busy")
        end
        os.execute("context --make en")
        if squid then
            squid.signal("finished")
            os.sleep(2)
            squid.signal("busy")
        end
        os.execute("mtxrun  --script font  --reload --force")
        if squid then
            squid.signal("finished")
            os.sleep(2)
            squid.signal("reset")
        end
    end
    statistics.stoptiming("testsuite:prepare")
end

local function collectfiles(pattern,capped)
    statistics.starttiming("testsuite:collect")
    report()
    report("collecting test files")
    local files = dir.glob(pattern)
    local total = #files
    table.sort(files) -- unix
    report("%i files found",total)
    report()
    statistics.stoptiming("testsuite:collect")
    if capped then
        total = capped
    end
    return files, total
end

local function setsteps(runners)
    local squid = environment.argument("squid")
    if squid then
        squid = require("util-sig-imp-squid.lua")
    end
    if squid then
        squid.stepper("reset")
    end
    local steps = { }
    for i=1,runners do
        steps[i] = 0
    end
    if squid then
        squid.signal("busy")
    end
    return squid, steps
end

local function wrapuprun(squid,luaname,total,problem,results)
    if squid then
        squid.signal(problem and "error" or "finished")
    end
    results = {
        files = results,
        times = {
            collect = statistics.elapsedtime("testsuite:collect"),
            process = statistics.elapsedtime("testsuite:process"),
            prepare = statistics.elapsedtime("testsuite:prepare"),
        }
    }
    io.savedata(luaname,table.serialize(results,true))
    report()
    report("files: %i, collect: %s s, prepare: %s s, process: %s s, problems: %i, overview: %s",
        total,
        results.times.collect,
        results.times.prepare,
        results.times.process,
        table.count(results.files),
        luaname
    )
    report()
end

local function getlog(pi,exit,detail)
    local name = file.removesuffix(pi[2]) .. "-error.log"
    local log  = table.load(name)
    if type(log) == "table" then
        log = {
            lastluaerror = log.lastluaerror and match(log.lastluaerror,"([^\n\r]*)") or nil,
            lasttexerror = log.lasttexerror and match(log.lasttexerror,"([^\n\r]*)") or nil,
            linenumber   = log.linenumber,
        }
    else
        log = {
            detail = detail,
            exit   = exit
        }
    end
    return log
end

if not process or environment.argument("popen") then

    local popen  = io.popen
    local close  = io.close
    local read   = io.read
    local gobble = io.gobble

    function scripts.testsuite.parallel() -- quite some overlap but ...
        local pattern = environment.argument("pattern")
        if pattern then
            local cleanup = environment.argument("cleanup")
            local engine  = environment.argument("luatex") and "--luatex" or ""
            local capped  = tonumber(environment.argument("capped"))
            local runners = tonumber(environment.argument("parallel")) or 8
            local process = { }
            local results = { }
            local luaname = "testsuite-process.lua"
            local count   = 0
            local problem = false

            local files, total = collectfiles(pattern,capped)
            local squid, steps = setsteps(runners)

            preparerun(capped)

            statistics.starttiming("testsuite:process")
            while true do
                local done = false
                for i=1,runners do
                    local pi = process[i]
                    if pi then
                        local s = gobble(pi[1])
                        if s then
                            done = true
                            goto done
                        else
                            local r, detail, n = close(pi[1])
                            local bad = not r or n > 0
                            if bad then
                                results[pi[2]] = getlog(pi,exit,detail)
                            end
                            if bad then
                                problem = true
                            end
                            report("%02i : %04i : %s : %s : %0.3f ",i,pi[3],bad and "error" or "done ",pi[2],clock()-pi[4])
                            process[i] = false
                        end
                    end
                    count = count + 1
                    if count > total then
                        -- we're done
                    else
                        local filename = files[count]
                        local dirname  = file.dirname(filename)
                        local basename = file.basename(filename)
                        local texname  = basename
                        local pdfname  = file.replacesuffix(basename,"pdf")
                        local tucname  = file.replacesuffix(basename,"tuc")
                        local workdir  = lfs.currentdir()
                        lfs.chdir(dirname)
                        os.remove(pdfname)
                        if cleanup then
                            os.remove(tucname)
                        end
                        if lfs.isfile(texname) then
                            steps[i] = steps[i] + 1
                            if squid then
                                squid.stepper("busy",i,steps[i],problem)
                            end
                            local command = f_runner("context",engine,texname)
                            local result  = popen(command)
                            if result then
                                process[i] = { result, filename, count, clock(), 0, texname }
                            else
                                results[filename] = "error"
                                if squid then
                                    squid.stepper("busy",i,steps[i],problem)
                                end
                            end
                            report("%02i : %04i : %s : %s",i,count,result and "start" or "error",filename)
                        end
                        lfs.chdir(workdir)
                        done = true
                    end
                  ::done::
                end
                if not done then
                    break
                end
            end
            statistics.stoptiming("testsuite:process")

            wrapuprun(squid,luaname,total,problem,results)
        end
    end

else

    local open   = process.open
    local close  = process.close
    local read   = process.read
    local poll   = process.poll
    local gobble = io.gobble

    function scripts.testsuite.parallel() -- quite some overlap but ...
        local pattern = environment.argument("pattern")
        if pattern then
            local cleanup = environment.argument("cleanup")
            local engine  = environment.argument("luatex") and "--luatex" or ""
            local capped  = tonumber(environment.argument("capped"))
            local runners = tonumber(environment.argument("parallel")) or 8
            local process = { }
            local results = { }
            local luaname = "testsuite-process.lua"
            local count   = 0
            local problem = false
            local lookup  = { }

            local files, total = collectfiles(pattern,capped)
            local squid, steps = setsteps(runners)

            preparerun(capped)

            local function populated()
                local active = { }
                for i=1,runners do
                    local pi = process[i]
                    if pi then
                        local handle = pi[1]
                        active[#active+1] = handle
                        lookup[handle]    = pi
                    else
                        count = count + 1
                        if count > total then
                            -- we're done
                        else
                            local filename = files[count]
                            local dirname  = file.dirname(filename)
                            local basename = file.basename(filename)
                            local texname  = basename
                            local pdfname  = file.replacesuffix(basename,"pdf")
                            local tucname  = file.replacesuffix(basename,"tuc")
                            local workdir  = lfs.currentdir()
                            lfs.chdir(dirname)
                            os.remove(pdfname)
                            if cleanup then
                                os.remove(tucname)
                            end
                            if lfs.isfile(texname) then
                                steps[i] = steps[i] + 1
                                if squid then
                                    squid.stepper("busy",i,steps[i],problem)
                                end
                                local command = f_runner("context",engine,texname)
                                local handle  = open(command,true)
                                if handle then
                                    local pi   = { handle, filename, count, clock(), i, texname }
                                    process[i] = pi
                                    active[#active+1] = handle
                                    lookup[handle]    = pi
                                else
                                    results[filename] = "error"
                                    if squid then
                                        squid.stepper("busy",i,steps[i],problem)
                                    end
                                end
                                report("%02i : %04i : %s : %s",i,count,handle and "start" or "error",filename)
                            end
                            lfs.chdir(workdir)
                        end
                    end
                end
                return active
            end

            statistics.starttiming("testsuite:process")
            while true do
                local active = populated()
                if #active > 0 then
                    local ready = poll(active,1000)
                    for i=1,#ready do
                        local index  = ready[i]
                        local handle = active[index]
                        local pi     = lookup[handle]
                        if pi then
                            -- we dont handle output
                            local state = read(handle)
                            if state == true then
                                local exit = close(handle)
                                local bad  = exit ~= 0
                                if bad then
                                    results[pi[2]] = getlog(pi,exit)
                                end
                                if bad then
                                    problem = true
                                end
                                report("%02i : %04i : %s : %s : %0.3f ",i,pi[3],bad and "error" or "done ",pi[2],clock()-pi[4])
                                process[pi[5]] = false
                                lookup[handle] = nil
                            end
                        end
                    end
                else
                    break
                end
            end
            statistics.stoptiming("testsuite:process")

            wrapuprun(squid,luaname,total,problem,results)
        end
    end

end

function scripts.testsuite.compare()
    local pattern = environment.argument("pattern")
    local oldname = environment.argument("oldname")
    local newname = environment.argument("newname")
    local collect = environment.argument("collect")
    local bitmaps = environment.argument("bitmaps")
    local objects = environment.argument("objects")
    local cleanup = environment.argument("cleanup")
    local jit     = environment.argument("jit")
    local engine  = environment.argument("luatex") and "--luatex" or ""
    if pattern and newname then
        oldname = oldname and file.addsuffix(oldname,"lua")
        newname = file.addsuffix(newname,"lua")
        local files = dir.glob(pattern)
        local info  = table.load("testsuite-info.lua")
        local skip  = info and info.exceptions or { }
        local oldhashes = oldname and lfs.isfile(oldname) and dofile(oldname) or { }
        local newhashes = {
            version = 0.01,
            files   = { },
            data    = os.date(),
        }
        local old = oldhashes and oldhashes.files or {}
        local new = newhashes and newhashes.files or {}
        local err = { }

        local function compare(filename,olddata,name)
            local newhash = md5.HEX(io.loaddata(name))
            local oldhash = olddata and olddata.hash
            if not oldhash then
                new[filename] = { status = "new", hash = newhash }
            elseif oldhash == newhash then
                new[filename] = { status = "unchanged", hash = oldhash }
            else
                new[filename] = { status = "changed", hash = newhash }
            end
        end

        for i=1,#files do
            local filename = files[i]
            local olddata  = old[filename]
            if collect then
                new[filename] = olddata or { status = "collected" }
            elseif olddata and olddata.status == "skip" then
                new[filename] = olddata
            else
                local dirname  = file.dirname(filename)
                local basename = file.basename(filename)
                local texname  = basename
                local pdfname  = file.replacesuffix(basename,"pdf")
                local tucname  = file.replacesuffix(basename,"tuc")
                local pngname  = "temp.png"
                local oldname  = "old-" .. pdfname
                local workdir  = lfs.currentdir()
                lfs.chdir(dirname)
                os.remove(oldname)
                os.rename(pdfname,oldname)
                if cleanup then
                    os.remove(tucname)
                end
                os.remove(pngname)
                if lfs.isfile(texname) then
                    local command = f_runner(jit and "contextjit" or "context",engine,texname)
                    local result  = os.execute(command)
                    if result > 0 then
                        new[filename] = { status = "error", comment = "error code: " .. result }
                        err[filename] = result
                    elseif lfs.isfile(pdfname) then
                        local fullname = gsub(filename,"^%./","")
                        if skip[fullname] then
                            new[filename] = { status = "okay", comment = (bitmaps or objects) and "not compared" or nil }
                        elseif bitmaps then -- -A 8
                            local command = string.format('mutool draw -o %s -r 600 %s',pngname,pdfname)
                            local result = os.execute(command)
                            if lfs.isfile(pngname) then
                                compare(filename,olddata,pngname)
                            else
                                new[filename] = { status = "error", comment = "no png file" }
                            end
                        elseif objects then
                            compare(filename,olddata,pdfname)
                        else
                            new[filename] = { status = "okay" }
                        end
                    else
                        new[filename] = { status = "error", comment = "no pdf file" }
                    end
                else
                   new[filename] = { status = "error", comment = "no tex file" }
                end
                os.remove(pngname)
                lfs.chdir(workdir)
            end
        end
        io.savedata(newname,table.serialize(newhashes,true))
        if next(err) then
            for filename, data in table.sortedhash(err) do
                report("fatal error in file %a",filename)
            end
        else
            report("no fatal errors")
        end
    else
        report("provide --pattern --oldname --newname [--cleanup] [--bitmaps | --objects]")
    end
end

if environment.argument("compare") then
    scripts.testsuite.compare()
elseif environment.argument("process") then
    scripts.testsuite.process()
elseif environment.argument("parallel") then
    scripts.testsuite.parallel()
elseif environment.argument("exporthelp") then
    application.export(environment.argument("exporthelp"),environment.files[1])
else
    application.help()
end

