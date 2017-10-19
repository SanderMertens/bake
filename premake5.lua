
workspace "bake"
  configurations { "Debug", "Release" }
  location "build"

  configuration { "linux", "gmake" }
    buildoptions { "-std=c99", "-fPIC", "-D_XOPEN_SOURCE=600", "-rdynamic", "-Wl,--export-dynamic" }

  project "bake"
    kind "ConsoleApp"
    language "C"
    location "build"
    targetdir "."

    files { "include/*.h", "src/*.c", "../base/include/*.h", "../base/src/*.c"}
    includedirs { "include", "../base/include" }

    if os.is64bit then
      objdir (".corto/obj/" .. os.target() .. "-64")
    else
      objdir (".corto/obj/" .. os.target() .. "-32")
    end

    configuration "linux"
      links { "rt", "dl", "m", "ffi", "pthread" }    

    configuration "Debug"
      defines { "DEBUG" }
      symbols "On"

    configuration "Release"
      defines { "NDEBUG" }
      optimize "On"

