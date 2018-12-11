
workspace "driver_bake_c"
  configurations { "debug", "release" }
  location "build"

  configuration { "linux", "gmake" }
    buildoptions { "-std=c99", "-D_XOPEN_SOURCE=600" }

  project "driver_bake_c"
    kind "SharedLib"
    language "C"
    location "build"
    targetdir "."

    objdir ".bake_cache"

    files { "include/*.h", "src/*.c", "../platform/src/*.c" }
    includedirs { ".", "../platform", "../builder", "$(BAKE_HOME)/include" }

    configuration "debug"
      defines { "DEBUG" }
      symbols "On"

    configuration "release"
      defines { "NDEBUG" }
      optimize "On"
