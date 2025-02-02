
workspace "bake_lang_cpp"
  configurations { "debug", "release" }
  location "build"

  filter { "action:gmake" }
    buildoptions { "-std=c99", "-D_XOPEN_SOURCE=600" }

  project "bake_lang_cpp"
    language "C"
    location "build"
    targetdir "."

    filter { "system:not emscripten" }
      kind "SharedLib"
      links { "bake_util" }

    filter { "system:emscripten" }
      kind "ConsoleApp"

    objdir ".bake_cache"

    files { "include/*.h", "src/*.c" }
    includedirs { ".", "$(BAKE_HOME)/include" }

    libdirs { "$(BAKE_HOME)/lib" }

    filter "debug"
      defines { "DEBUG" }
      symbols "On"

    filter "release"
      defines { "NDEBUG" }
      optimize "On"

    filter { "system:emscripten", "action:gmake" }
      buildoptions { "-sSIDE_MODULE" }
      linkoptions { "-sSIDE_MODULE" }
