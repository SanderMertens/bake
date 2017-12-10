
workspace "bake"
  configurations { "Debug", "Release" }

  configuration { "linux", "gmake" }
    buildoptions { "-std=c99", "-fPIC", "-D_XOPEN_SOURCE=600"}

  project "bake"
    kind "ConsoleApp"
    language "C"
    targetdir "."

    files { "include/*.h", "src/*.c", "../base/include/*.h", "../base/src/*.c"}
    includedirs { "include", "../base/include" }

    prebuildcommands {
      "[ -d ../base ] || git clone https://github.com/cortoproject/base ../base"
    }

    postbuildcommands {
      "./bake setup"
    }

    if os.is64bit then
      objdir (".corto/obj/" .. os.target() .. "-64")
    else
      objdir (".corto/obj/" .. os.target() .. "-32")
    end

    configuration "macosx"
      links { "rt", "dl", "m", "ffi", "pthread" }

    configuration "linux"
      links { "rt", "dl", "m", "ffi", "pthread" }

    configuration "Debug"
      defines { "DEBUG" }
      symbols "On"

    configuration "Release"
      defines { "NDEBUG" }
      optimize "On"
