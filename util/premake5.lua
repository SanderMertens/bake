
workspace "bake_util"
  configurations { "debug", "release" }
  location "build"

  configuration { "linux", "gmake" }
    buildoptions { "-std=c99", "-D_XOPEN_SOURCE=600" }

  project "bake_util"
    kind "SharedLib"
    language "C"
    location "build"
    targetdir "."

    objdir ".bake_cache"

    files { "include/*.h", "src/*.c" }
    includedirs { ".", "$(BAKE_HOME)/include" }

    configuration "debug"
      defines { "UT_IMPL", "DEBUG" }
      symbols "On"

    configuration "release"
      defines { "UT_IMPL", "NDEBUG" }
      optimize "On"

    filter { "system:macosx", "action:gmake"}
      toolset "clang"
      links { "dl", "pthread" }

    filter { "system:linux", "action:gmake"}
      links { "rt", "dl", "pthread", "m" }
