if not modules then modules = { } end modules ['data-tld'] = {
    version   = 1.001,
    comment   = "companion to m-texlive",
    author    = "Hans Hagen, PRAGMA-ADE, Hasselt NL",
    copyright = "PRAGMA ADE / ConTeXt Development Team",
    license   = "see context related readme files",
}

-- This is a variant on code I found in a \TEXLIVE\ installation in \type
-- {cont-sys.mkxl} in 2025 (probably written by Max). See \type {m-texlive.mkxl}.

function resolvers.checktexlive(texlive)
    local hashes = resolvers.gethashes()
    if not texlive then
        for i=1,#hashes do
            if string.find(hashes[i].name,"texmf%-dist") then
                texlive = true
                break
            end
        end
    end
    if not texlive then
        return
    end
 -- local found = { }
 -- for i=1,#hashes do
 --     local tree = string.match(hashes[i].name,"texmf%-([a-z]+)")
 --     if tree then
 --         found[tree] = true
 --     end
 -- end
 -- if not found.project or not found.fonts or not found.modules or not found.context then
 --     local report = logs.reporter("resolvers","caches")
 --     report("reference context uses texmf-[fonts|project|context|modules]")
 --     report("your installation uses texmf-[%|t]",table.sortedkeys(found))
 -- end
    local report = logs.reporter("resolvers","caches")
    local intime = true
    local window = 60
    report("")
    report("checking patched texlive setup (suboptimal)")
    for i=1,#hashes do
        local tree = hashes[i]
        local name = tree.name
        local lsrfile = file.join(resolvers.resolve(name),"ls-R")
        local lsrtime = lfs.isfile(lsrfile) and lfs.attributes(lsrfile,"modification")
        if lsrtime then
            local hashfile = file.addsuffix(caches.hashed(name),"lua")
            local hashpath = caches.getfirstreadablefile(hashfile,"trees")
            local hashtime = hashpath and lfs.attributes(hashpath,"modification")
            if hashtime then
                local delta = math.abs(os.difftime(lsrtime,hashtime))
                if delta < window then
                    -- Let's assume we're okay, we don't want some redundant generation
                    -- of files do we?
                else
                    if delta > 0 then
                        report("the %a files are newer than the %a files, updating","lsr","context")
                        resolvers.renewcache()
                        resolvers.load()
                    else
                        report("the %a files are newer than the %a files, updating","context","lsr")
                        os.execute("mktexlsr")
                    end
                    intime = false
                    break
                end
            else
                -- we probably have to generate
            end
        else
            -- no lsr file found, actually we can quit
        end
    end
    if intime then
        report("the file database and lsr files are within the %i seconds time window",window)
    end
    report("")
end
