set -e

git clone --no-checkout https://github.com/glennrp/libpng.git ./repo
git -C ./repo checkout a37d4836519517bdce6cb9d956092321eca3e73b
cd repo
autoreconf -f -i
./configure
make -j$(nproc) clean
make -j$(nproc) libpng16.la
cd ..
sed -i '1i#include <stdlib.h>' ./repo/contrib/oss-fuzz/libpng_read_fuzzer.cc

clang++ -std=c++11 -I./repo ./repo/contrib/oss-fuzz/libpng_read_fuzzer.cc -o ./libpng_read_fuzzer ./repo/.libs/libpng16.a -lz ../harness/afl_driver.cpp