
solution "bake"
  configurations { "Debug", "Release" }
  location "build"

  configuration { "linux", "gmake" }
    buildoptions { "-std=c99", "-fPIC", "-D_XOPEN_SOURCE=600" }

  project "bake"
    kind "ConsoleApp"
    language "C"
    location "build"

    files { "include/*.h", "src/*.c", "../base/include/*.h", "../base/src/*.c" }
    includedirs { ".", "../base" }

    if os.is64bit then
      objdir ("obj/" .. os.get() .. "-64")
    else
      objdir ("obj/" .. os.get() .. "-32")
    end

    configuration "linux"
      links { "rt", "dl", "m", "ffi", "pthread" }    

    configuration "Debug"
      defines { "DEBUG" }
      flags { "Symbols" }

    configuration "Release"
      defines { "NDEBUG" }
      flags { "Optimize" }

