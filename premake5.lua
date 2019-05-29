
workspace "bake"
  configurations { "debug", "release" }
  location ("build")

  configuration { "linux", "gmake" }
    buildoptions { "-std=c99", "-fPIC", "-D_XOPEN_SOURCE=600"}

  project "bake"
    kind "ConsoleApp"
    language "C"
    targetdir "."

    files { "include/*.h", "src/*.c", "util/src/*.c", "util/src/posix/*.c", "util/include/*.h", 
      "util/include/bake-util/*.h", "util/include/bake-util/posix/*.h"
    }

    includedirs { "include", "util/include" }

    objdir (".bake_cache")

    defines { "BAKE_IMPL" }

    configuration "debug"
      defines { "DEBUG" }
      symbols "On"

    configuration "release"
      defines { "NDEBUG" }
      optimize "On"

    filter { "system:macosx", "action:gmake"}
      toolset "clang"
      links { "dl", "pthread" }

    filter { "system:linux", "action:gmake"}
      links { "rt", "dl", "pthread", "m" }
