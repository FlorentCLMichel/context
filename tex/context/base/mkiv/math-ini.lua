if not modules then modules = { } end modules ['math-ini'] = {
    version   = 1.001,
    comment   = "companion to math-ini.mkiv",
    author    = "Hans Hagen, PRAGMA-ADE, Hasselt NL",
    copyright = "PRAGMA ADE / ConTeXt Development Team",
    license   = "see context related readme files"
}

-- if needed we can use the info here to set up xetex definition files
-- the "8000 hackery influences direct characters (utf) as indirect \char's
--
-- isn't characters.data loaded already ... shortcut it here
--
-- replace code 7 by 0 as we don't use it anyway (chars with code 7 will adapt to
-- to the fam when set ... we use other means .. ok, we could use it for spacing but
-- then we also have to set the other characters (only a subset done now)

local next, type = next, type
local formatters, find = string.formatters, string.find
local utfchar, utfbyte, utflength = utf.char, utf.byte, utf.length
----- floor = math.floor
local sortedhash = table.sortedhash
local toboolean = toboolean

local context               = context
local commands              = commands
local implement             = interfaces.implement

local ctx_sprint            = context.sprint
local ctx_doifelsesomething = commands.doifelsesomething

local trace_defining        = false  trackers.register("math.defining", function(v) trace_defining = v end)

-- trace_defining = true

local report_math           = logs.reporter("mathematics","initializing")

mathematics                 = mathematics or { }
local mathematics           = mathematics

mathematics.extrabase       = fonts.privateoffsets.mathextrabase -- here we push some virtuals
mathematics.privatebase     = fonts.privateoffsets.mathbase      -- here we push the ex

local unsetvalue            = attributes.unsetvalue
local allocate              = utilities.storage.allocate
local chardata              = characters.data

local texsetattribute       = tex.setattribute
local setmathcode           = tex.setmathcode
local setdelcode            = tex.setdelcode

local families = allocate {
    mr = 0,
    mb = 1,
}

--- to be checked  .. afew defaults in char-def that should be alpha

local classes = allocate {
    ord          =  0, -- mathordcomm     mathord
    op           =  1, -- mathopcomm      mathop
    bin          =  2, -- mathbincomm     mathbin
    rel          =  3, -- mathrelcomm     mathrel
    open         =  4, -- mathopencomm    mathopen
    middle       =  4,
    close        =  5, -- mathclosecomm   mathclose
    punct        =  6, -- mathpunctcomm   mathpunct
    alpha        =  7, -- mathalphacomm   firstofoneargument
    accent       =  8, -- class 0
    radical      =  9,
    xaccent      = 10, -- class 3
    topaccent    = 11, -- class 0
    botaccent    = 12, -- class 0
    under        = 13,
    over         = 14,
    delimiter    = 15,
    division     = 15,
    inner        =  0, -- mathinnercomm   mathinner
    choice       =  0, -- mathchoicecomm  @@mathchoicecomm
    prime        =  0,
    differential =  0,
    exponential  =  0,
    limop        =  1, -- mathlimopcomm   @@mathlimopcomm
    nolop        =  1, -- mathnolopcomm   @@mathnolopcomm
    integral     =  1, -- new in lmtx but here still an operator
    --
    ordinary     =  0, -- ord
    operator     =  1, -- op
    alphabetic   =  7, -- alpha
    punctuation  =  6, -- punct
    opening      =  4, -- open
    closing      =  5, -- close
    binary       =  2, -- bin
    relation     =  3, -- rel
    diacritic    =  8, -- accent
    large        =  1, -- op
    variable     =  7, -- alphabetic
    number       =  7, -- alphabetic
    root         = 16, -- a private one
}

local engineclasses = table.setmetatableindex(function(t,k)
    if k then
        local c = tonumber(k) or classes[k] or 0
        local v = c < 8 and c or 0
        t[k] = v
        return v
    else
        return 0
    end
end)

local ordinary_class  = classes.ordinary
local open_class      = classes.open
local middle_class    = classes.middle
local close_class     = classes.close
local accent_class    = classes.accent
local radical_class   = classes.radical
local topaccent_class = classes.topaccent
local botaccent_class = classes.botaccent
local under_class     = classes.under
local over_class      = classes.over
local delimiter_class = classes.delimiter
local division_class  = classes.division
local root_class      = classes.root

local accents = allocate {
    accent    = true, -- some can be both
    topaccent = true,  [topaccent_class] = true,
    botaccent = true,  [botaccent_class] = true,
    under     = true,  [under_class]     = true,
    over      = true,  [over_class]      = true,
    unknown   = false,
}

-- engine subtypes get from elsewhere

local codes = allocate {
    ordinary       = 0,  [ 0] = "ordinary",
    largeoperator  = 1,  [ 1] = "largeoperator",
    binaryoperator = 2,  [ 2] = "binaryoperator",
    relation       = 3,  [ 3] = "relation",
    openingsymbol  = 4,  [ 4] = "openingsymbol",
    closingsymbol  = 5,  [ 5] = "closingsymbol",
    punctuation    = 6,  [ 6] = "punctuation",
 -- inner          = 7,  [ 7] = "inner",
 -- undersymbol    = 8,  [ 8] = "undersymbol",
 -- oversymbol     = 9,  [ 9] = "oversymbol",
 -- fractionsymbol = 10, [10] = "fractionsymbol",
 -- radicalsymbol  = 11, [11] = "radicalsymbol",
    middlesymbol   = 12, [12] = "middlesymbol",
}

local extensibles = allocate {
           unknown    = 0,
    l = 1, left       = 1,
    r = 2, right      = 2,
    h = 3, horizontal = 3,-- lr or rl
    u = 5, up         = 4,
    d = 5, down       = 5,
    v = 6, vertical   = 6,-- ud or du
    m = 7, mixed      = 7,
}

table.setmetatableindex(extensibles,function(t,k) t[k] = 0 return 0 end)

local virtualized = allocate {
}

function mathematics.virtualize(unicode,virtual)

    local function virtualize(k,v)
        local c = virtualized[k]
        if c == v then
            report_math("character %C is already virtualized to %C",k,v)
        elseif c then
            report_math("character %C is already virtualized to %C, ignoring mapping to %C",k,c,v)
        else
            virtualized[k] = v
        end
    end

    if type(unicode) == "table" then
        for k, v in next, unicode do
            virtualize(k,v)
        end
    elseif type(unicode) == "number" and type(virtual) == "number" then
        virtualize(unicode,virtual)
 -- else
        -- error
    end
end

mathematics.extensibles = extensibles
mathematics.classes     = classes
mathematics.codes       = codes
-----------.accents     = codes
mathematics.families    = families
mathematics.virtualized = virtualized

local escapes = characters.filters.utf.private.escapes

do

    local setmathcharacter = function(class,family,slot,unicode,mset,dset)
        if mset and codes[class] then -- regular codes < 7
            setmathcode("global",slot,class,family,unicode)
            mset = false
        end
        if dset and (class == open_class or class == close_class or class == middle_class or class == division_class) then
            setdelcode("global",slot,family,unicode,0,0)
            dset = false
        end
        return mset, dset
    end

    local function report(class,engine,family,unicode,name)
        local nametype = type(name)
        if nametype == "string" then
            report_math("class %a, engine %a, family %a, char %C, name %a",class,engine,family,unicode,name)
        elseif nametype == "number" then
            report_math("class %a, engine %a, family %a, char %C, number %U",class,engine,family,unicode,name)
        else
            report_math("class %a, engine %a, family %a, char %C",class,engine,family,unicode)
        end
    end

    local f_accent    = formatters[ [[\defUmathtopaccent \%s{%X}{%X}{%X}]] ]
    local f_topaccent = formatters[ [[\defUmathtopaccent \%s{%X}{%X}{%X}]] ]
    local f_botaccent = formatters[ [[\defUmathbotaccent \%s{%X}{%X}{%X}]] ]
    local f_over      = formatters[ [[\defUdelimiterover \%s{%X}{%X}{%X}]] ]
    local f_under     = formatters[ [[\defUdelimiterunder\%s{%X}{%X}{%X}]] ]
    local f_fence     = formatters[ [[\defUdelimiter     \%s{%X}{%X}{%X}]] ]
    local f_delimiter = formatters[ [[\defUdelimiter     \%s{%X}{%X}{%X}]] ]
    local f_radical   = formatters[ [[\defUradical       \%s{%X}{%X}]]     ]
    local f_root      = formatters[ [[\defUroot          \%s{%X}{%X}]]     ]
    local f_char      = formatters[ [[\defUmathchar      \%s{%X}{%X}{%X}]] ]

    local texmathchardef = tex.mathchardef

    local setmathsymbol = function(name,class,engine,family,slot) -- hex is nicer for tracing
        if class == accent_class then
            ctx_sprint(f_topaccent(name,0,family,slot))
        elseif class == topaccent_class then
            ctx_sprint(f_topaccent(name,0,family,slot))
        elseif class == botaccent_class then
            ctx_sprint(f_botaccent(name,0,family,slot))
        elseif class == over_class then
            ctx_sprint(f_over(name,0,family,slot))
        elseif class == under_class then
            ctx_sprint(f_under(name,0,family,slot))
        elseif class == open_class or class == close_class or class == middle_class then
            setdelcode("global",slot,{family,slot,0,0})
            ctx_sprint(f_fence(name,engine,family,slot))
        elseif class == delimiter_class then
            setdelcode("global",slot,{family,slot,0,0})
            ctx_sprint(f_delimiter(name,0,family,slot))
        elseif class == radical_class then
            ctx_sprint(f_radical(name,family,slot))
        elseif class == root_class then
            ctx_sprint(f_root(name,family,slot))
        elseif texmathchardef then
            texmathchardef(name,engine,family,slot,"permanent")
        else
            -- beware, open/close and other specials should not end up here
            ctx_sprint(f_char(name,engine,family,slot))
        end
    end

    function mathematics.define(family)
        family = family or 0
        family = families[family] or family
        local data = characters.data
        --
        local function remap(first,last)
            for unicode=utfbyte(first),utfbyte(last) do
                setmathcode("global",unicode,ordinary_class,family,unicode)
            end
        end
        remap("0","9")
        remap("A","Z")
        remap("a","z")
        --
        for unicode, character in sortedhash(data) do
            local symbol = character.mathsymbol
            local mset   = true
            local dset   = true
            if symbol then
                local other = data[symbol]
                local class = other.mathclass
                if class then
                    local engine = engineclasses[class]
                    if trace_defining then
                        report(class,engine,family,unicode,symbol)
                    end
                    mset, dset = setmathcharacter(engine,family,unicode,symbol,mset,dset)
                end
                local spec = other.mathspec
                if spec then
                    for i=1,#spec do
                        local m = spec[i]
                        local class = m.class
                        if class then
                            local engine = engineclasses[class]
                            -- todo: trace
                            mset, dset = setmathcharacter(engine,family,unicode,symbol,mset,dset)
                        end
                    end
                end
            end
            local class = character.mathclass
            local spec  = character.mathspec
            local name  = character.mathname
            if spec then
                local done = false
                if class then
                    if name then
                        report_math("fatal error, conflicting mathclass and mathspec for %C",unicode)
                        os.exit()
                    else
                        class = classes[class] or ordinary_class
                        local engine = engineclasses[class]
                        if trace_defining then
                            report(class,engine,family,unicode)
                        end
                        mset, dset = setmathcharacter(engine,family,unicode,unicode,mset,dset)
                        done = true
                    end
                end
                for i=1,#spec do
                    local m     = spec[i]
                    local name  = m.name
                    local class = m.class or class
                    if class then
                        class = classes[class] or ordinary_class
                    else
                        class = ordinary_class
                    end
                    if class then
                        local engine = engineclasses[class]
                        if name then
                            if trace_defining then
                                report(class,engine,family,unicode,name)
                            end
                            setmathsymbol(name,class,engine,family,unicode)
                        else
                            name = (class == classes.ordinary or class == classes.digit) and character.adobename -- bad
                            if name and trace_defining then
                                report(class,engine,family,unicode,name)
                            end
                        end
                        if not done then
                            mset, dset = setmathcharacter(engine,family,unicode,m.unicode or unicode,mset,dset) -- see solidus
                            done = true
                        end
                    end
                end
            else
                if class then
                    class = classes[class] or ordinary_class
                else
                    class = ordinary_class
                end
                if name ~= nil then
                    local engine = engineclasses[class]
                    if name == false then
                        if trace_defining then
                            report(class,engine,family,unicode,name)
                        end
                        mset, dset = setmathcharacter(engine,family,unicode,unicode,mset,dset)
                    else
                     -- if not name then
                     --     name = character.contextname -- too dangerous, we loose textslash and a few more
                     -- end
                        if name then
                            if trace_defining then
                                report(class,engine,family,unicode,name)
                            end
                            setmathsymbol(name,class,engine,family,unicode)
                        else
                            if trace_defining then
                                report(class,engine,family,unicode,character.adobename)
                            end
                        end
                        mset, dset = setmathcharacter(engine,family,unicode,unicode,mset,dset)
                    end
                elseif class ~= ordinary_class then
                    local engine = engineclasses[class]
                    if trace_defining then
                        report(class,engine,family,unicode,character.adobename)
                    end
                    mset, dset = setmathcharacter(engine,family,unicode,unicode,mset,dset)
                end
            end
        end
        if trace_defining then
            logs.stopfilelogging()
        end
    end

end

-- needed for mathml analysis
-- string with # > 1 are invalid
-- we could cache

local lpegmatch = lpeg.match

local utf8byte  = lpeg.patterns.utf8byte * lpeg.P(-1)

-- function somechar(c)
--     local b = lpegmatch(utf8byte,c)
--     return b and chardata[b]
-- end


local somechar = { }

table.setmetatableindex(somechar,function(t,k)
    if k then
        local b = lpegmatch(utf8byte,k)
        local v = b and chardata[b] or false
        t[k] = v
        return v
    end
end)

local function utfmathclass(chr, default)
    local cd = somechar[chr]
    return cd and cd.mathclass or default or "unknown"
end

local function utfmathlimop(chr)
    local cd = somechar[chr]
    return cd and (cd.mathclass == "operator" or cd.mathclass == "integral") or false
end

local function utfmathaccent(chr,default,asked1,asked2)
    local cd = somechar[chr]
    if not cd then
        return default or false
    end
    if asked1 and asked1 ~= "" then
        local mc = cd.mathclass
        if mc and (mc == asked1 or mc == asked2) then
            return true
        end
        local ms = cd.mathspec
        if not ms then
            local mp = cd.mathparent
            if mp then
                ms = chardata[mp].mathspec
            end
        end
        if ms then
            for i=1,#ms do
                local msi = ms[i]
                local mc = msi.class
                if mc and (mc == asked1 or mc == asked2) then
                    return true
                end
            end
        end
    else
        local mc = cd.mathclass
        if mc then
            return accents[mc] or default or false
        end
        local ms = cd.mathspec
        if ms then
            for i=1,#ms do
                local msi = ms[i]
                local mc = msi.class
                if mc then
                    return accents[mc] or default or false
                end
            end
        end
    end
    return default or false
end

local function utfmathstretch(chr,default) -- "h", "v", "b", ""
    local cd = somechar[chr]
    return cd and cd.mathstretch or default or ""
end

local function utfmathcommand(chr,default,asked1,asked2)
    local cd = somechar[chr]
    if not cd then
        return default or ""
    end
    if asked1 then
        local mn = cd.mathname
        local mc = cd.mathclass
        if mn and mc and (mc == asked1 or mc == asked2) then
            return mn
        end
        local ms = cd.mathspec
        if not ms then
            local mp = cd.mathparent
            if mp then
                ms = chardata[mp].mathspec
            end
        end
        if ms then
            for i=1,#ms do
                local msi = ms[i]
                local mn = msi.name
                if mn then
                    local mc = msi.class
                    if mc == asked1 or mc == asked2 then
                        return mn
                    end
                end
            end
        end
    else
        local mn = cd.mathname
        if mn then
            return mn
        end
        local ms = cd.mathspec
        if ms then
            for i=1,#ms do
                local msi = ms[i]
                local mn = msi.name
                if mn then
                    return mn
                end
            end
        end
    end
    return default or ""
end

local function utfmathfiller(chr, default)
    local cd = somechar[chr]
    local cmd = cd and cd.mathfiller -- or cd.mathname
    return cmd or default or ""
end

mathematics.utfmathclass   = utfmathclass
mathematics.utfmathstretch = utfmathstretch
mathematics.utfmathcommand = utfmathcommand
mathematics.utfmathfiller  = utfmathfiller
mathematics.utfmathaccent  = utfmathaccent

-- interfaced

implement {
    name      = "utfmathclass",
    actions   = { utfmathclass, context },
    arguments = "string"
}

implement {
    name      = "utfmathstretch",
    actions   = { utfmathstretch, context },
    arguments = "string"
}

implement {
    name      = "utfmathcommand",
    actions   = { utfmathcommand, context },
    arguments = "string"
}

implement {
    name      = "utfmathfiller",
    actions   = { utfmathfiller, context },
    arguments = "string"
}

implement {
    name      = "utfmathcommandabove",
    actions   = { utfmathcommand, context },
    arguments = { "string", false, "'topaccent'","'over'" }
}

implement {
    name      = "utfmathcommandbelow",
    actions   = { utfmathcommand, context },
    arguments = { "string", false, "'botaccent'","'under'" }
}

implement {
    name      = "utfmathcommandfiller",
    actions   = { utfmathfiller, context },
    arguments = "string"
}

-- todo: make this a helper:

implement {
    name      = "doifelseutfmathabove",
    actions   = { utfmathaccent, ctx_doifelsesomething },
    arguments = { "string", false, "'topaccent'", "'over'" }
}

implement {
    name      = "doifelseutfmathbelow",
    actions   = { utfmathaccent, ctx_doifelsesomething },
    arguments = { "string", false, "'botaccent'", "'under'" }
}

implement {
    name      = "doifelseutfmathaccent",
    actions   = { utfmathaccent, ctx_doifelsesomething },
    arguments = "string",
}

implement {
    name      = "doifelseutfmathfiller",
    actions   = { utfmathfiller, ctx_doifelsesomething },
    arguments = "string",
}

implement {
    name      = "doifelseutfmathlimop",
    actions   = { utfmathlimop, ctx_doifelsesomething },
    arguments = "string",
}

-- helpers
--
-- 1: step 1
-- 2: step 2
-- 3: htdp * 1.33^n
-- 4: size * 1.33^n

function mathematics.big(tfmdata,unicode,n,method)
    local t = tfmdata.characters
    local c = t[unicode]
    if c and n > 0 then
        local vv = c.vert_variants or c.next and t[c.next].vert_variants
        if vv then
            local vvn = vv[n]
            return vvn and vvn.glyph or vv[#vv].glyph or unicode
        elseif method == 1 or method == 2 then
            if method == 2 then -- large steps
                n = n * 2
            end
            local next = c.next
            while next do
                if n <= 1 then
                    return next
                else
                    n = n - 1
                    local tn = t[next].next
                    if tn then
                        next = tn
                    else
                        return next
                    end
                end
            end
        elseif method >= 3 then
            local size = 1.33^n
            if method == 4 then
                size = tfmdata.parameters.size * size
            else -- if method == 3 then
                size = (c.height + c.depth) * size
            end
            local next = c.next
            while next do
                local cn = t[next]
                if (cn.height + cn.depth) >= size then
                    return next
                else
                    local tn = cn.next
                    if tn then
                        next = tn
                    else
                        return next
                    end
                end
            end
        end
    end
    return unicode
end

-- experimental

-- local categories = { } -- indexed + hashed
--
-- local a_mathcategory = attributes.private("mathcategory")
--
-- local function registercategory(category,tag,data) -- always same data for tag
--     local c = categories[category]
--     if not c then
--         c = { }
--         categories[category] = c
--     end
--     local n = c[tag]
--     if not n then
--         n = #c + 1
--         c[n] = data
--         n = n * 1000 + category
--         c[tag] = n
--     end
--     return n
-- end
--
-- function mathematics.getcategory(n)
--     local category = n % 1000
--     return category, categories[category][floor(n/1000)]
-- end
--
-- mathematics.registercategory = registercategory
--
-- function commands.taggedmathfunction(tag,label)
--     if label then
--         texsetattribute(a_mathcategory,registercategory(1,tag,tag))
--         context.mathlabeltext(tag)
--     else
--         texsetattribute(a_mathcategory,1)
--         context(tag)
--     end
-- end

local categories       = { }
mathematics.categories = categories

local a_mathcategory = attributes.private("mathcategory")

local functions    = storage.allocate()
local noffunctions = 1000 -- offset

categories.functions = functions

implement {
    name      = "tagmfunctiontxt",
    arguments = { "string", "conditional" },
    actions   = function(tag,apply)
        local delta = apply and 1000 or 0
        texsetattribute(a_mathcategory,1000 + delta)
    end
}

implement {
    name      = "tagmfunctionlab",
    arguments = { "string", "conditional" },
    actions   = function(tag,apply)
        local delta = apply and 1000 or 0
        local n = functions[tag]
        if not n then
            noffunctions = noffunctions + 1
            functions[noffunctions] = tag
            functions[tag] = noffunctions
            texsetattribute(a_mathcategory,noffunctions + delta)
        else
            texsetattribute(a_mathcategory,n + delta)
        end
    end
}

--

local list

function mathematics.resetattributes()
    if not list then
        list = { }
        for k, v in next, attributes.numbers do
            if find(k,"^math") then
                list[#list+1] = v
            end
        end
    end
    for i=1,#list do
        texsetattribute(list[i],unsetvalue)
    end
end

implement {
    name    = "resetmathattributes",
    actions = mathematics.resetattributes
}

-- weird to do this here but it's a side affect of math anyway

interfaces.implement {
    name     = "enableasciimode",
    onlyonce = true,
    actions  = resolvers.macros.enablecomment,
}
