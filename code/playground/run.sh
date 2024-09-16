make -C /work/my-plugin/ clean
make -C /work/my-plugin/ TARGET_BIN=coverage plugin.so targets/fs-c targets/coverage
AFL_MAP_SIZE=65536 AFL_SKIP_BIN_CHECK=1 setarch `uname -m` -R afl-fuzz -i ./afl_in/ -o ./afl_out/ setarch `uname -m` -R /home/ubuntu/qemu/build/qemu-x86_64 -plugin /work/my-plugin/plugin.so /work/my-plugin/targets/fs-c