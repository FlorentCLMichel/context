-- group

return {
    category  = "history",
    targets   = {
        tree  = {
            sources = "doc/context/sources/history",
            results = "doc/context/history",
            objects = { "sources", "results" },
        },
        site = {
            sources = "sources/history", -- placeholder
            results = "history",
            objects = { "results" },
        },
    },
    documents = {

        ["pdftex"] = {
            status  = "okay",
            group   = "general/pdftex",
            sources = {
                "pdftex/pdftex-t.tex",
                "pdftex/pdftex-t.txt",
            },
            results = {
                "pdftex/pdftex-a.pdf",
                "pdftex/pdftex-s.pdf",
                "pdftex/readme.txt",
            },
        },

        ["road"] = {
            status  = "okay",
            group   = "general/road",
            sources = {
                "road/roadshow.tex",
            },
            results = {
                "road/roadshow.pdf",
                "road/readme.txt",
            },
        },

        ["articles"] = {
            status  = "okay",
            group   = "general/articles",
            sources = {
            },
            results = {
                "articles/art-beyo.pdf",
                "articles/art-calc.pdf",
                "articles/art-colo.pdf",
                "articles/art-maps.pdf",
                "articles/art-prez.pdf",
                "articles/art-puzz.pdf",
                "articles/art-typo.pdf",
                "articles/art-visi.pdf",
                "articles/readme.txt",
            },
        },

        ["presentations"] = {
            status  = "okay",
            group   = "general/presentations",
            sources = {
            },
            results = {
                "presentations/readme.txt",
            },
        },

        ["presentations-talks"] = {
            status  = "okay",
            group   = "general/presentations/talks",
            sources = {
            },
            results = {
                "presentations/talks/fifteen.pdf",
                "presentations/talks/pre-2001.pdf",
                "presentations/talks/pre-mml.pdf",
            },
        },

        ["presentations-tug-2000"] = {
            status  = "okay",
            group   = "general/presentations/tug-2001",
            sources = {
            },
            results = {
                "presentations/tug-2000/logo-mil.gif",
                "presentations/tug-2000/logo-mil.htm",
                "presentations/tug-2000/pre-pdf1.pdf",
                "presentations/tug-2000/pre-pdf2.pdf",
                "presentations/tug-2000/pre-pdf3.pdf",
                "presentations/tug-2000/pre-pdf4.pdf",
                "presentations/tug-2000/pre-pdf5.pdf",
            },
        },

        ["presentations-euronts"] = {
            status  = "okay",
            group   = "general/presentations/euronts",
            sources = {
             -- "presentations/euronts/pre-nts.mp",
                "presentations/euronts/pre-nts.tex",
             -- "presentations/euronts/pre-ntsa.tex",
             -- "presentations/euronts/pre-ntsb.tex",
             -- "presentations/euronts/pre-ntsc.tex",
             -- "presentations/euronts/pre-ntsd.tex",
             -- "presentations/euronts/pre-ntsx.tex",
             -- "presentations/euronts/pre-ntsy.tex",
             -- "presentations/euronts/pre-ntsz.tex",
            },
            results = {
             -- "presentations/euronts/nts-all.pdf",
                "presentations/euronts/pre-nts.pdf",
                "presentations/euronts/pre-ntsa.pdf",
                "presentations/euronts/pre-ntsb.pdf",
                "presentations/euronts/pre-ntsc.pdf",
                "presentations/euronts/pre-ntsd.pdf",
                "presentations/euronts/pre-ntsx.pdf",
                "presentations/euronts/pre-ntsy.pdf",
                "presentations/euronts/pre-ntsz.pdf",
            },
        },

    },
}
