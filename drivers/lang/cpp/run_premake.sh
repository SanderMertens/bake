rm -rf build-Darwin
rm -rf build-Linux
rm -rf build-Mingw
rm -rf build-Emscripten
../../../../premake5 --os=macosx gmake
mv build build-Darwin
../../../../premake5 --os=linux gmake
mv build build-Linux
../../../../premake5 --os=windows gmake2
mv build build-Mingw
../../../../premake5 --os=emscripten gmake2
mv build build-Emscripten
