-- group

return {
    category  = "presentations",
    targets   = {
        tree  = {
            sources = "doc/context/sources/presentations",
            results = "doc/context/presentations",
            objects = { "sources", "results" },
        },
        site = {
            sources = "sources/presentations", -- placeholder
            results = "presentations",
            objects = { "results" },
        },
    },
    documents = {

        ["readme"] = {
            status  = "okay",
            group   = "general",
            sources = {
                "present-readme.tex",
            },
            results = {
                "present-readme.pdf",
            },
        },

        ["examples"] = {
            status  = "okay",
            group   = "general/styles",
            sources = {
                "examples/present-original-001.tex",
                "examples/present-green-001.tex",
                "examples/present-funny-001.tex",
                "examples/present-colorful-001.tex",
                "examples/present-fuzzy-001.tex",
                "examples/present-grow-001.tex",
                "examples/present-tiles-001.tex",
                "examples/present-windows-001.tex",
                "examples/present-punk-001.tex",
                "examples/present-stepper-001.tex",
                "examples/present-random-001.tex",
                "examples/present-shaded-001.tex",
                "examples/present-split-001.tex",
                "examples/present-balls-001.tex",
                "examples/present-organic-001.tex",
                "examples/present-weird-001.tex",
                "examples/present-steps-001.tex",
            },
            results = {
                "examples/present-original-001.pdf",
                "examples/present-green-001.pdf",
                "examples/present-funny-001.pdf",
                "examples/present-colorful-001.pdf",
                "examples/present-fuzzy-001.pdf",
                "examples/present-grow-001.pdf",
                "examples/present-tiles-001.pdf",
                "examples/present-windows-001.pdf",
                "examples/present-punk-001.pdf",
                "examples/present-stepper-001.pdf",
                "examples/present-random-001.pdf",
                "examples/present-shaded-001.pdf",
                "examples/present-split-001.pdf",
                "examples/present-balls-001.pdf",
                "examples/present-organic-001.pdf",
                "examples/present-weird-001.pdf",
                "examples/present-steps-001.pdf",
            },
        },

        ["bachotex-2005"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2005/bachotex-2005-fonts.tex",
                "bachotex/2005/bachotex-2005-hyphenation.tex",
            },
            results = {
         --     "bachotex/2005/bachotex-2005-fonts.pdf",
         --     "bachotex/2005/bachotex-2005-hyphenation.pdf },
            },
        },
        ["bachotex-2008"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2008/bachotex-2007-luatex.tex",
                "bachotex/2008/bachotex-2007-mplib.tex",
                "bachotex/2008/bachotex-2007-opentype.tex",
            },
            results = {
         --     "bachotex/2008/bachotex-2008-luatex.pdf",
         --     "bachotex/2008/bachotex-2008-mplib.pdf",
         --     "bachotex/2008/bachotex-2008-opentype.pdf",
            },
        },
        ["bachotex-2009"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2009/bachotex-2009-luatex.tex",
                "bachotex/2009/bachotex-2009-math.tex",
                "bachotex/2009/bachotex-2009-opentype.tex",
            },
            results = {
         --     "bachotex/2009/bachotex-2009-luatex.pdf",
         --     "bachotex/2009/bachotex-2009-math.pdf",
         --     "bachotex/2009/bachotex-2009-opentype.pdf",
            },
        },
        ["bachotex-2010"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2010/bachotex-2010-clash.tex",
                "bachotex/2010/bachotex-2010-move.tex",
            },
            results = {
                "bachotex/2010/bachotex-2010-clash.pdf",
                "bachotex/2010/bachotex-2010-move.pdf",
            },
        },
        ["bachotex-2011"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2011/bachotex-2011-cld-and-mkvi.tex",
                "bachotex/2011/bachotex-2011-math.tex",
                "bachotex/2011/bachotex-2011-ebook.tex",
                "bachotex/2011/bachotex-2011-metapost.tex",
            },
            results = {
                "bachotex/2011/bachotex-2011-cld-and-mkvi.pdf",
         --     "bachotex/2011/bachotex-2011-math.pdf",
         --     "bachotex/2011/bachotex-2011-ebook.pdf",
         --     "bachotex/2011/bachotex-2011-metapost.pdf",
            },
        },
        ["bachotex-2012"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2012/bachotex-2012-context.tex",
                "bachotex/2012/bachotex-2012-future.tex",
            },
            results = {
         --     "bachotex/2012/bachotex-2012-context.pdf",
         --     "bachotex/2012/bachotex-2012-future.pdf",
            },
        },
        ["bachotex-2013"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2013/bachotex-2013-bits.tex",
                "bachotex/2013/bachotex-2013-luatex.tex",
                "bachotex/2013/bachotex-2013-sense.tex",
                "bachotex/2013/bachotex-2013-speed.tex",
            },
            results = {
                "bachotex/2013/bachotex-2013-bits.pdf",
                "bachotex/2013/bachotex-2013-luatex.pdf",
                "bachotex/2013/bachotex-2013-sense.pdf",
                "bachotex/2013/bachotex-2013-speed.pdf",
            },
        },
        ["bachotex-2014"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2014/bachotex-2014-luatex.tex",
                "bachotex/2014/bachotex-2014-metapost.tex",
            },
            results = {
         --     "bachotex/2014/bachotex-2014-luatex.pdf",
         --     "bachotex/2014/bachotex-2014-metapost.pdf",
            },
        },
        ["bachotex-2015"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2015/bachotex-2015-ligatures.tex",
            },
            results = {
                "bachotex/2015/bachotex-2015-ligatures.pdf",
            },
        },
        ["bachotex-2016"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2016/bachotex-2016-toolbox.tex",
                "bachotex/2016/bachotex-2016-opentype.tex",
            },
            results = {
                "bachotex/2016/bachotex-2016-toolbox.pdf",
                "bachotex/2016/bachotex-2016-opentype.pdf",
            },
        },
        ["bachotex-2017"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2017/bachotex-2017-emoji.tex",
                "bachotex/2017/bachotex-2017-emoji-demo.tex",
                "bachotex/2017/bachotex-2017-variablefonts.tex",
                "bachotex/2017/bachotex-2017-variablefonts-demo.tex",
            },
            results = {
                "bachotex/2017/bachotex-2017-emoji.pdf",
                "bachotex/2017/bachotex-2017-emoji-demo.pdf",
                "bachotex/2017/bachotex-2017-variablefonts.pdf",
                "bachotex/2017/bachotex-2017-variablefonts-demo.pdf",
            },
        },
        ["bachotex-2018"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2018/bachotex-2018-fonteffects.tex",
                "bachotex/2018/bachotex-2018-mp.tex",
            },
            results = {
                "bachotex/2018/bachotex-2018-fonteffects.pdf",
                "bachotex/2018/bachotex-2018-mp.pdf",
            },
        },
        ["bachotex-2019"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2019/bachotex-2019-followingup.tex",
            },
            results = {
                "bachotex/2019/bachotex-2019-followingup.pdf",
            },
        },
        ["bachotex-2023"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2023/bachotex-2023-green.tex",
            },
            results = {
                "bachotex/2023/bachotex-2023-green.pdf",
            },
        },
        ["bachotex-2024"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2024/bachotex-2024-accessibility.tex",
                "bachotex/2024/bachotex-2024-fonts.tex",
            },
            results = {
                "bachotex/2024/bachotex-2024-accessibility.pdf",
         --     "bachotex/2024/bachotex-2024-fonts.pdf",
            },
        },
        ["bachotex-2025"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "bachotex/2025/bachotex-2025-dimensions.tex",
                "bachotex/2025/bachotex-2025-flagging.tex",
                "bachotex/2025/bachotex-2025-pages.tex",
            },
            results = {
                "bachotex/2025/bachotex-2025-dimensions.pdf",
                "bachotex/2025/bachotex-2025-flagging.pdf",
                "bachotex/2025/bachotex-2025-pages.pdf",
            },
        },

        ["context-2007"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2007/context-2007-luatex.tex",
                "context/2007/context-2007-mkiv.tex",
            },
            results = {
             -- "context/2007/context-2007-luatex.pdf",
             -- "context/2007/context-2007-mkiv.pdf",
            },
        },
        ["context-2010"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2010/context-2010-just-in-time-1.tex",
                "context/2010/context-2010-just-in-time-2.tex",
                "context/2010/context-2010-requirements.tex",
                "context/2010/context-2010-structure-matters.tex",
                "context/2010/context-2010-workflows.tex",
            },
            results = {
             -- "context/2010/context-2010-just-in-time-1.pdf",
             -- "context/2010/context-2010-just-in-time-2.pdf",
             -- "context/2010/context-2010-requirements.pdf",
             -- "context/2010/context-2010-structure-matters.pdf",
             -- "context/2010/context-2010-workflows.pdf",
            },
        },
        ["context-2011"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2011/context-2011-mathml-update.tex",
                "context/2011/context-2011-metapost-how-we-adapt.tex",
                "context/2011/context-2011-ebook-export.tex",
                "context/2011/context-2011-sorting-registers.tex",
            },
            results = {
                "context/2011/context-2011-mathml-update.pdf",
                "context/2011/context-2011-metapost-how-we-adapt.pdf",
                "context/2011/context-2011-ebook-export.pdf",
                "context/2011/context-2011-sorting-registers.pdf",
            },
        },
        ["context-2012"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2012/context-2012-xml-news.tex",
                "context/2012/context-2012-after-the-cleanup.tex",
                "context/2012/context-2012-lexing-sources.tex",
                "context/2012/context-2012-mixed-columns.tex",
                "context/2012/context-2012-the-script.tex",
                "context/2012/context-2012-visual-debugging.tex",
            },
            results = {
                "context/2012/context-2012-xml-news.pdf",
                "context/2012/context-2012-after-the-cleanup.pdf",
                "context/2012/context-2012-lexing-sources.pdf",
                "context/2012/context-2012-mixed-columns.pdf",
                "context/2012/context-2012-the-script.pdf",
                "context/2012/context-2012-visual-debugging.pdf",
            },
        },
        ["context-2013"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2013/context-2013-math.tex",
                "context/2013/context-2013-speed.tex",
            },
            results = {
                "context/2013/context-2013-math.pdf",
                "context/2013/context-2013-speed.pdf",
            },
        },
        ["context-2015"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2015/context-2015-status.tex",
            },
            results = {
                "context/2015/context-2015-status.pdf",
            },
        },
        ["context-2016"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2016/context-2016-luatex.tex",
            },
            results = {
                "context/2016/context-2016-luatex.pdf",
            },
        },
        ["context-2017"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2017/context-2017-css-selectors.tex",
                "context/2017/context-2017-features.tex",
                "context/2017/context-2017-features-chaintest.tex",
                "context/2017/context-2017-features-kerntest.tex",
                "context/2017/context-2017-features-pairtest.tex",
                "context/2017/context-2017-features-singletest.tex",
                "context/2017/context-2017-features-spacetest.tex",
                "context/2017/context-2017-features-substitutiontest.tex",
                "context/2017/context-2017-performance.tex",
                "context/2017/context-2017-synctex.tex",
                "context/2017/context-2017-tables.tex",
            },
            results = {
                "context/2017/context-2017-css-selectors.pdf",
                "context/2017/context-2017-features.pdf",
                "context/2017/context-2017-features-chaintest.pdf",
                "context/2017/context-2017-features-kerntest.pdf",
                "context/2017/context-2017-features-pairtest.pdf",
                "context/2017/context-2017-features-singletest.pdf",
                "context/2017/context-2017-features-spacetest.pdf",
                "context/2017/context-2017-features-substitutiontest.pdf",
                "context/2017/context-2017-performance.pdf",
                "context/2017/context-2017-synctex.pdf",
                "context/2017/context-2017-tables.pdf",
            },
        },
        ["context-2019"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2019/context-2019-lmtx.tex",
            },
            results = {
                "context/2019/context-2019-lmtx.pdf",
            },
        },
        ["context-2020"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2020/context-2020-mkii-mkiv-mkxl.tex",
                "context/2020/context-2020-luametatex.tex",
                "context/2020/context-2020-tokens.tex",
                "context/2020/context-2020-concepts.tex",
                "context/2020/context-2020-datatypes.tex",
                "context/2020/context-2020-ecmascript.tex",
                "context/2020/context-2020-svg.tex",
                "context/2020/context-2020-mp.tex",
                "context/2020/context-2020-implementers.tex",

                "context/2020/context-2020-halslegacy.jpg",
                "context/2020/context-2020-sin.svg",
                "context/2020/context-2020-gpdemo.gp",
                "context/2020/context-2020-gpdemo.svg",
            },
            results = {
                "context/2020/context-2020-mkii-mkiv-mkxl.pdf",
                "context/2020/context-2020-luametatex.pdf",
                "context/2020/context-2020-tokens.pdf",
                "context/2020/context-2020-concepts.pdf",
                "context/2020/context-2020-datatypes.pdf",
                "context/2020/context-2020-ecmascript.pdf",
                "context/2020/context-2020-svg.pdf",
                "context/2020/context-2020-mp.pdf",
                "context/2020/context-2020-implementers.pdf",
            },
        },
        ["context-2021"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2021/context-2021-compactfonts.tex",
                "context/2021/context-2021-localcontrol.tex",
                "context/2021/context-2021-luametafun.tex",
                "context/2021/context-2021-math.tex",
                "context/2021/context-2021-overloadprotection.tex",
                "context/2021/context-2021-paragraphs.tex",
                "context/2021/context-2021-programming.tex",
            },
            results = {
                "context/2021/context-2021-compactfonts.pdf",
                "context/2021/context-2021-localcontrol.pdf",
                "context/2021/context-2021-luametafun.pdf",
                "context/2021/context-2021-math.pdf",
                "context/2021/context-2021-overloadprotection.pdf",
                "context/2021/context-2021-paragraphs.pdf",
                "context/2021/context-2021-programming.pdf",
            },
        },
        ["context-2022"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2022/context-2022-style.tex",
                "context/2022/context-2022-stepping-up-metapost.tex",
                "context/2022/context-2022-metapost-and-sql.tex",
                "context/2022/context-2022-demo-sql-mps.tex",
                "context/2022/context-2022-lmtx-distribution.tex",
                "context/2022/context-2022-upgrading-the-math-engine.tex",
                "context/2022/context-2022-injecting-stuff.tex",
            },
            results = {
                "context/2022/context-2022-stepping-up-metapost.pdf",
                "context/2022/context-2022-metapost-and-sql.pdf",
             -- "context/2022/context-2022-demo-sql-mps.pdf",
                "context/2022/context-2022-lmtx-distribution.pdf",
             -- "context/2022/context-2022-upgrading-the-math-engine.pdf",
             -- "context/2022/context-2022-injecting-stuff.pdf",
            },
        },
        ["context-2023"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2023/context-2023-style.tex",
                "context/2023/context-2023-potrace.tex",
            },
            results = {
                "context/2023/context-2023-potrace.pdf",
            },
        },
        ["context-2024"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2024/context-2024-style.tex",
                "context/2024/context-2024-compactfonts.tex",
                "context/2024/context-2024-distributions.tex",
                "context/2024/context-2024-drawlines.tex",
                "context/2024/context-2024-improvements.tex",
                "context/2024/context-2024-landscape.tex",
                "context/2024/context-2024-pdf-2.tex",
                "context/2024/context-2024-segments.tex",
                "context/2024/context-2024-accessibility.tex",
                "context/2024/kingjames-001.png",
                "context/2024/kingjames-002.png",
            },
            results = {
                "context/2024/context-2024-compactfonts.pdf",
             -- "context/2024/context-2024-distributions.pdf",
             -- "context/2024/context-2024-drawlines.pdf",
             -- "context/2024/context-2024-improvements.pdf",
             -- "context/2024/context-2024-landscape.pdf",
                "context/2024/context-2024-pdf-2.pdf",
                "context/2024/context-2024-segments.pdf",
                "context/2024/context-2024-accessibility.pdf",
            },
        },
        ["context-2025"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "context/2025/context-2025-style.tex",
                "context/2025/context-2025-compact.tex",
                "context/2025/context-2025-future.tex",
                "context/2025/context-2025-signals.tex",
            },
            results = {
             -- "context/2025/context-2025-compact.pdf",
                "context/2025/context-2025-future.pdf",
             -- "context/2025/context-2025-future.pdf",
                "context/2025/context-2025-signals.pdf",
            },
        },
        ["context-2026"] = {
            status  = "okay",
            group   = "general/meetings",
            comment = "also bachotex",
            sources = {
                "context/2026/context-2026-sailaway.tex",
                "context/2026/context-2026-playfullmath.tex",
             -- "context/2026/context-2026-useful.tex",
             -- "context/2026/context-2026-hai.tex",
            },
            results = {
                "context/2026/context-2026-sailaway.pdf",
                "context/2026/context-2026-playfullmath.pdf",
             -- "context/2026/context-2026-useful.pdf",
             -- "context/2026/context-2026-hai.pdf",
            },
        },

        ["tug-2001"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "tug/2001/tug-2001-ideas.tex",
            },
            results = {
                "tug/2001/tug-2001-ideas.pdf",
            },
        },
        ["tug-2007"] = {
            status  = "okay",
            group   = "general/meetings",
            sources = {
                "tug/2007/tug-2007-fonts.tex",
            },
            results = {
                "tug/2007/tug-2007-fonts.pdf",
            },
        },

    },
}
