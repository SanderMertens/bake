
workspace "bake_lang_c"
  configurations { "debug", "release" }
  location "build"

  filter { "action:gmake" }
    buildoptions { "-std=c99", "-D_XOPEN_SOURCE=600" }

  project "bake_lang_c"
    kind "SharedLib"
    language "C"
    location "build"
    targetdir "."

    objdir ".bake_cache"

    files { "include/*.h", "src/*.c" }
    includedirs { ".", "$(BAKE_HOME)/include" }

    links { "bake_util" }
    libdirs { "$(BAKE_HOME)/lib" }

    filter "debug"
      defines { "DEBUG" }
      symbols "On"

    filter "release"
      defines { "NDEBUG" }
      optimize "On"
