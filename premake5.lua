
workspace "bake"
  configurations { "debug", "release" }
  location ("build")

  filter { "action:gmake" }
    buildoptions { "-std=c99", "-fPIC", "-D_XOPEN_SOURCE=600"}

  project "bake"
    kind "ConsoleApp"
    language "C"
    targetdir "."

    files { "include/*.h", "src/*.c", "util/src/*.c", "util/include/*.h", 
      "util/include/bake-util/*.h"
    }

    includedirs { "include", "util/include" }

    objdir (".bake_cache")

    defines { "BAKE_IMPL" }

    filter { "action:gmake" }
      files { "util/src/posix/*.c", "util/include/bake-util/posix/*.h" }

    filter { "action:gmake2" }
      files { "util/src/win/*.c", "util/include/bake-util/win/*.h" }

    filter "debug"
      defines { "DEBUG" }
      symbols "On"

    filter "release"
      defines { "NDEBUG" }
      optimize "On"

    filter { "system:macosx", "action:gmake"}
      toolset "clang"
      links { "dl", "pthread" }

    filter { "system:linux", "action:gmake"}
      links { "rt", "dl", "pthread", "m" }
