#!/usr/bin/env texlua

if not modules then modules = { } end

modules["mtx-fix_export_images"] = {
    version   = "0.1",
    comment   = "Convert exported image placeholders to <img> tags",
    author    = "OpenAI + Florent Michel",
}

local lfs = require("lfs")

--------------------------------------------------------------------------------
-- Helpers
--------------------------------------------------------------------------------

local function readfile(name)
    local f = assert(io.open(name,"rb"))
    local s = f:read("*all")
    f:close()
    return s
end

local function writefile(name,data)
    local f = assert(io.open(name,"wb"))
    f:write(data)
    f:close()
end

--------------------------------------------------------------------------------
-- Locate export directory
--------------------------------------------------------------------------------

local jobname = arg[1]

if not jobname then
    io.stderr:write("Usage:\n")
    io.stderr:write("    mtxrun --script fix_export_images <jobname>\n")
    os.exit(1)
end

local exportdir

if lfs.attributes(jobname,"mode") == "directory" then
    exportdir = jobname
else
    exportdir = jobname .. "-export"
end

local htmlfile = exportdir .. "/" ..
                 exportdir:gsub("%-export$","") ..
                 "-div.html"

local cssfile = exportdir .. "/styles/" .. jobname .. "-images.css"

--------------------------------------------------------------------------------
-- Read CSS
--------------------------------------------------------------------------------

local css = readfile(cssfile)

local images = { }

for id,url in css:gmatch('%[image="([^"]+)"%].-url%([\'"]([^\'"]+)[\'"]%)') do
    images[id] = url
end

print(string.format("Found %d image mappings.",
    (function()
        local n = 0
        for _ in pairs(images) do
            n = n + 1
        end
        return n
    end)()
))

--------------------------------------------------------------------------------
-- Rewrite HTML
--------------------------------------------------------------------------------

local html = readfile(htmlfile)

local replacements = 0

local ANY = "[%z\1-\255]"
html = html:gsub(
    "<div class=\"image[^\"]*\"(" .. ANY .. "-)image=\"([^\"]+)\"(" .. ANY .. "-)</div>",
    function(before,id,after)

        local src = images[id]

        if not src then
            return '<div class=' .. before ..
                   ' image="' .. id .. '"' ..
                   after .. '></div>'
        end

        before = before:gsub('%sid="([^"]+)"',' id="%1"')
        before = before:gsub('%sclass="([^"]+)"',' class="%1"')

        replacements = replacements + 1

        return string.format(
            '<img%s%s src="%s"%s',
            before,
            "",
            src:gsub("../images","images"),
            after:gsub('%simage="[^"]+"',"")
        )
    end
)
html = html:gsub(
    "<div class=\"formulacontent[^\"]*\"(" .. ANY .. "-)image=\"([^\"]+)\"(" .. ANY .. "-)</div>",
    function(before,id,after)

        local src = images[id]

        if not src then
            return '<div' .. before ..
                   ' image="' .. id .. '"' ..
                   after .. '></div>'
        end

        before = before:gsub('%sid="([^"]+)"',' id="%1"')
        before = before:gsub('%sclass="([^"]+)"',' class="%1"')

        replacements = replacements + 1

        return string.format(
            '<img%s%s src="%s"%s<div class="break"><!--empty--></div>',
            before,
            "",
            src:gsub("../images","images"),
            after:gsub('%simage="[^"]+"',""):gsub('<math[\1-\255]+</math>',"")
        )
    end
)

writefile(htmlfile,html)

print(string.format("Replaced %d image placeholders.",replacements))
