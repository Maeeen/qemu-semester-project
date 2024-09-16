rm -rf ./afl_out/*

afl-gcc-fast ./coverage.c
afl-fuzz -i ./afl_in/ -o ./afl_out/ ./a.out