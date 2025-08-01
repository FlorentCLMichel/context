# We use CMake but I bet we can also provide a simple make file for linux just because 
# when I look what gets compiled into CMake we need very little of that (and it 
# has a lot of dependencies - ssh related, compression, security, xml, etc), but maybe 
# there is a lightweight version of CMake. 

cmake_minimum_required(VERSION 3.9..3.28)

project(luametatex VERSION 2.11 LANGUAGES C)

# I need to check with Mojca of the compile farm can handle this. 

set(CMAKE_C_STANDARD 11)
# set(CMAKE_C_STANDARD 17)
# set(CMAKE_C_STANDARD 23)

# https://sourceforge.net/p/predef/wiki/OperatingSystems/
# https://sourceforge.net/p/predef/wiki/Architectures/

include(GNUInstallDirs)

# Optionals (maybe have a LMT_*_TOO for each of them). We might start out with only a very few
# optionals at some time, but for now we enable them (there is not not much code involved). The 
# idea behind these optionals is that we have very simple (!) interfaces, delegating as much as 
# possible to Lua. We will *not* add interfaces with many bindings because that will introduce
# dependencies (and looking at e.g. LuaTeX build updates shows that clearly: a no-go). 

set(LMT_KPSE_TOO 1) # In case we want to manage MKII scripts (etc) with mtxrun.
set(LMT_HB_TOO   1) # Maybe handy for Idris' font development (old converted ffi stuff)

# This triggere link time optimization, which adds to compile time and gains (at most) a few 
# percent on runtime. When set, because we're sparse, we also strip the binary.

# set(LMT_OPTIMIZE 1)

# This makes the binary some 135K smaller so it might become the default at some point which is 
# nice (the smaller as runner the better). After all, we don't load external (Lua) libraries 
# anyway. 

set(LMT_STRIP 1)

# When one wants to use Lua libraries, this is needed. But keep in mind that it's not supported, 
# so we won't look into issues that could result from that. Getting a matching library is upto 
# the user. Keep in mind that we might have set a different bytecode (luac) version during the 
# alpha/beta stages of a Lua development version. 

if (EXISTS "${CMAKE_SOURCE_DIR}/source/lua/lmthelperlib.c") 
    set(luametatex_use_helpers 1)
    add_definitions(-DLUAMETATEX_USE_HELPERS=1)
endif()

# set(LMT_PERMIT_LUA_LIBRARIES 1)

if (DEFINED LMT_PERMIT_LUA_LIBRARIES)
    unset(LMT_STRIP)
    unset(LMT_OPTIMIZE)
    add_definitions(-DLMT_PERMIT_LUA_LIBRARIES="yes")
endif ()

if (MSVC)

    if (CMAKE_C_COMPILER_ID MATCHES "Clang")

        add_compile_options(
            -Wall
            -O2

            -Wcast-align
            -Wcast-qual

            -Wno-unknown-pragmas
            -fno-strict-aliasing

            -Wno-pedantic
            -Wno-deprecated-declarations
            -Wno-missing-noreturn
            -Wno-shadow
        )

        add_definitions(-D_CRT_SECURE_NO_WARNINGS)

        add_definitions(-DLMT_COMPILER_USED="clang")

    else()

        add_compile_options(
            /Wall

            /wd4127 # constant conditional expression
            /wd4131 # old style declarator
            /wd4152 # function pointer cast
            /wd4201 # nonstandard extension used: nameless struct/union
            /wd4244 # assignment in conditional expression
            /wd4456 # local vars with same name as outer variable
            /wd4457 # local vars with same function parameter
            /wd4464 # relative include path
            /wd4668 # missing defines
            /wd4702 # unreachable code
            /wd4710 # inlining
            /wd4711 # inlining
            /wd4774 # sprint argument 2 warning
            /wd4777 # format argument 2 warning
            /wd4820 # local vars with same name as outer variable
            /wd4996 # strdup etc warnings
            /wd5045 # spectre

          # /GL     # whole program link optimization
          # /Gw     # whole program data optimization (a little smaller bin)

          # /Ob3    # more agressive inline, much larger bin, no gain

            /wd4061 # enumerator * in switch * is not explicitly handled (mp)
            /wd4701 # potentially unitialized local variable (lua)
            /wd4255 # no function prototype given

            /wd5105 # macro expansion producing 'defined' has undefined behavior

            /wd4548 # expression before comma has no effect; expected expression with side-effect

            # indeed a bit faster but also a much larger binary:

            # /fp:fast

            # okay for amd processors too but no difference in size so probably no gain:

          # /favor:INTEL64
          # /fsanitize:address
          # /std:c17

        )

        # We always optimize ... symbols are not in the binary anyway so there is no advantage
        # (like when accessing Lua api functions). We could have an additional luametatex-lua.dll
        # but that also creates a dependency (possible conflict). So just don't expect using 
        # Lua libraries in luametatex. 

      # if (DEFINED LMT_OPTIMIZE)
            add_compile_options(
                /GL # whole program link optimization
                /Gw # whole program data optimization (a little smaller bin)
            )
      # endif()

        add_definitions(-DLMT_COMPILER_USED="msvc")

    endif()

 else()

    if (CMAKE_C_COMPILER_ID MATCHES "Clang")

        # why not -03

        add_compile_options(
            -O2
        )

        add_definitions(-DLMT_COMPILER_USED="clang")

    else()

        add_compile_options(
            -O3
          # -g0
          # -mtune=nocona # fails on arm so more testing needed
        )

        add_definitions(-DLMT_COMPILER_USED="gcc")

      # add_compile_options(-pg)
      # add_link_options(-pg)

      # e.g. mp crashes when we have > 4K currentpicture clips in a row 

    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,stack-size=1000000")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,stack-size=2621440")

    endif()

    add_compile_options(
        -Wall

        -Wcast-align
        -Wcast-qual

        # we need to check for zeros anyway as not all compiles have this 

      # -fno-signed-zeros
      # -fno-trapping-math
      # -fno-math-errno

        -Wno-unknown-pragmas
        -Wno-unused-result
        -fno-strict-aliasing
    )

    # for c17
    #
    # add_definitions(-D__STDC_WANT_LIB_EXT2__=1)

    if ((DEFINED LMT_OPTIMIZE) OR (DEFINED LMT_STRIP))
        if (NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
            set(CMAKE_EXE_LINKER_FLAGS "-s")
        endif()
    endif()

endif()

if (CMAKE_C_COMPILER_ID MATCHES "Clang")

    add_compile_options(
        -Wno-unknown-warning-option
        -Wno-nonportable-include-path
        -Wno-nonportable-system-include-path
        -Wno-newline-eof
        -Wno-extra-semi-stmt
        -Wno-sign-conversion
        -Wno-unused-macros
        -Wno-reserved-id-macro
        -Wno-comma
        -Wno-switch-enum
        -Wno-shadow
        -Wno-missing-noreturn
        -Wno-implicit-fallthrough
      # -Wno-format
        -Wno-reserved-identifier
        -Wno-date-time
        -Wno-format-nonliteral
        -Wno-float-equal
      # too noisy, we just look at what gcc reports 
        -Wno-unsafe-buffer-usage
        -Wno-macro-redefined
        -Wno-undef
        -Wno-switch-default
        -Wno-class-varargs
    )

endif()

# Not that tested (converted ffi originals):

if ((DEFINED LMT_KPSE_TOO))
    add_definitions(-DLMT_KPSE_TOO=1)
endif()
if ((DEFINED LMT_HB_TOO))
    add_definitions(-DLMT_HB_TOO=1)
endif()

# This needs cmake >= 3.9 and produces a 60K smaller mingw binary but it take quite a bit of
# runtime to get there so it should become an option (apart from testing on all builders).

if (DEFINED LMT_OPTIMIZE)

    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_message)

    if (ipo_supported)
        #
        # We only have one program so we do it global (can become an -- option)
        #
        # set_property(TARGET luametatex PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        #
        # mingw64: 2865664, nocona: 2819584, lto: 2835968 (around 1% gain on manual)
        #
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        #
    else()
        # No message needed, just accept the fact.
    endif()

endif()

# Mimalloc is still under development, so we only support it on a few platforms. By the time it is
# stable we can probably remove some of the following tests. A bit of a hack:
#
# When the old osx version is dropped and armhf is upgraded we can enable unix except solaris which
# fails. So, only osx 10.6 and rpi 32 fail. But we will probably drop 32 bit in the future anyway.
#
# At the beginning of 2025 on Windows 10 the gain is some 5 percent on the 650 page luametatex 
# manual over the default memory allocator. 

# CMAKE_HOST_SYSTEM_PROCESSOR arm64 x86_64

# if (APPLE)
#     set(MI_OSX_ZONE 1)
# endif()

if (CMAKE_HOST_SOLARIS)
    # fails
elseif (MSVC)
    set(luametatex_use_mimalloc 1)
elseif (CMAKE_HOST_APPLE AND NOT (${CMAKE_C_COMPILER} MATCHES "arm"))
    # fails on the osx intel
elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv7l")
    # fails on the rpi 32 bit
elseif (CMAKE_C_COMPILER_ID MATCHES "AppleClang|Clang")
    # no motivation to figure specifics out
else()
    set(luametatex_use_mimalloc 1)
endif()

include_directories(${CMAKE_ROOT}/source)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/source)

if ((DEFINED luametatex_use_mimalloc))
    add_definitions(-DLUAMETATEX_USE_MIMALLOC=1)
  # add_definitions(-DMIMALLOC_RESET_DELAY=250)
  # set(luametatex_use_mimalloc 1)
    include(cmake/mimalloc.cmake)
endif()

include(cmake/tex.cmake)
include(cmake/lua.cmake)
include(cmake/mp.cmake)

include(cmake/luarest.cmake)
include(cmake/luasocket.cmake)
include(cmake/luaoptional.cmake)

include(cmake/pplib.cmake)
include(cmake/miniz.cmake)
include(cmake/softposit.cmake)
include(cmake/potrace.cmake)
include(cmake/qrcodegen.cmake)
include(cmake/triangles.cmake)

include(cmake/luametatex.cmake)
