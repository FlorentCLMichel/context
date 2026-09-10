if not modules then modules = { } end modules ['font-pat'] = {
    version   = 1.001,
    comment   = "companion to font-ini.mkiv",
    author    = "Hans Hagen, PRAGMA-ADE, Hasselt NL",
    copyright = "PRAGMA ADE / ConTeXt Development Team",
    license   = "see context related readme files"
}

-- This functiononality is nowadays provided via lfg files so what you see here
-- is more an example.

local match, lower = string.match, string.lower

local fonts    = fonts
local otf      = fonts.handlers.otf
local patches  = otf.enhancers.patches
local register = patches.register
local report   = patches.report

-- For some reason (either it's a bug in the font, or it's a problem in the
-- library) the palatino arabic fonts don't have the mkmk features properly
-- set up.

register("after","rehash features","^palatino.*arabic", function (data,filename)
    local gpos = data.gpos
    if gpos then
        for k=1,#gpos do
            local v = gpos[k]
            if not v.features and v.type == "gpos_mark2mark" then
                report("mkmk feature, name %a", v.name)
                v.features = {
                    {
                        scripts = {
                            arab = {
                                ["ara "] = true,
                                ["far "] = true,
                                ["urd "] = true,
                                ["dflt"] = true,
                            }
                        },
                        tag     = "mkmk",
                    }
                }
            end
        end
    end
end)
