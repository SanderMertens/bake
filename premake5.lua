
workspace "bake"
  configurations { "debug", "release" }
  location ("build")

  configuration { "linux", "gmake" }
    buildoptions { "-std=c99", "-fPIC", "-D_XOPEN_SOURCE=600"}


  project "bake"
    kind "ConsoleApp"
    language "C"
    targetdir "."

    files { "include/*.h", "src/*.c", "../platform/include/*.h", "../platform/src/*.c"}
    includedirs { "include", "../platform/include" }

    prebuildcommands {
      "[ -d ../../platform ] || git clone https://github.com/cortoproject/platform ../platform"
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
