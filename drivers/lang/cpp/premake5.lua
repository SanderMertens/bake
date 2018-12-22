
workspace "bake_lang_cpp"
  configurations { "debug", "release" }
  location "build"

  configuration { "linux", "gmake" }
    buildoptions { "-std=c99", "-D_XOPEN_SOURCE=600" }

  project "bake_lang_cpp"
    kind "SharedLib"
    language "C"
    location "build"
    targetdir "."

    objdir ".bake_cache"

    files { "include/*.h", "src/*.c" }
    includedirs { ".", "$(BAKE_HOME)/include" }

    links { "bake_util" }
    libdirs { "$(BAKE_HOME)/lib" }

    configuration "debug"
      defines { "DEBUG" }
      symbols "On"

    configuration "release"
      defines { "NDEBUG" }
      optimize "On"
