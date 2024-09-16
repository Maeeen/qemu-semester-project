rm -rf ./afl_out/*

gcc ./coverage.c -O0
afl-fuzz -Q -i ./afl_in/ -o ./afl_out/ ./a.out