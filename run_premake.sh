rm -rf build-Darwin
rm -rf build-Linux
../premake5 --os=macosx gmake
mv build build-Darwin
../premake5 --os=linux gmake
mv build build-Linux
