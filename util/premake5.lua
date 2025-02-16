
workspace "bake_util"
  configurations { "debug", "release" }
  location "build"

  filter { "action:gmake" }
    buildoptions { "-std=c99", "-D_XOPEN_SOURCE=600" }

  project "bake_util"
    language "C"
    location "build"
    targetdir "."

    files { "include/*.h", "src/*.c", "include/bake-util/*.h" }

    includedirs { ".", "$(BAKE_HOME)/include" }

    objdir (".bake_cache")

    filter { "action:gmake" }
      files { "src/posix/*.c", "include/bake-util/posix/*.h" }

    filter { "action:gmake2" }
      files { "src/win/*.c", "include/bake-util/win/*.h" }

    filter "debug"
      defines { "UT_IMPL", "DEBUG" }
      symbols "On"

    filter "release"
      defines { "UT_IMPL", "NDEBUG" }
      optimize "On"

    filter { "system:macosx", "action:gmake"}
      toolset "clang"
      links { "dl", "pthread" }

    filter { "system:linux", "action:gmake"}
      links { "rt", "dl", "pthread", "m" }

    filter { "action:gmake2" }
      links { "shlwapi", "imagehlp", "dbghelp", "ws2_32" }

    filter { "system:not emscripten" }
      kind "SharedLib"

    filter { "system:emscripten" }
      kind "StaticLib"
	  pic "On"
