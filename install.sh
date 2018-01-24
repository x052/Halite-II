rm ./MyBot
cp CMakeLists.txt backup.CMakeLists.txt
rm -rf CMake*
mv backup.CMakeLists.txt CMakeLists.txt
set -e
cmake .
make MyBot
