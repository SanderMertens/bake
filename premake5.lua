
workspace "bake"
  configurations { "debug", "release" }
  location ("build")

  configuration { "linux", "gmake" }
    buildoptions { "-std=c99", "-fPIC", "-D_XOPEN_SOURCE=600"}


  project "bake"
    kind "ConsoleApp"
    language "C"
    targetdir "."

    files { "include/*.h", "src/*.c", "../base/include/*.h", "../base/src/*.c"}
    includedirs { "include", "../base/include" }

    prebuildcommands {
      "[ -d ../../base ] || git clone https://github.com/cortoproject/base ../base"
    }

    postbuildcommands {
      "cd .. && ./bake setup"
    }

    objdir (".bake_cache")

    configuration "debug"
      defines { "DEBUG" }
      symbols "On"

    configuration "release"
      defines { "NDEBUG" }
      optimize "On"

    filter { "system:macosx", "action:gmake"}
      toolset "clang"
      links { "dl", "m", "ffi", "pthread" }

    filter { "system:linux", "action:gmake"}
      links { "rt", "dl", "m", "ffi", "pthread" }
