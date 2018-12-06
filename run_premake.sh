rm -rf build-darwin
rm -rf build-linux
../premake5 --os=macosx gmake
mv build build-darwin
../premake5 --os=linux gmake
mv build build-linux
