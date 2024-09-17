rm -rf ./afl_out/*
mkdir afl_in
echo s > afl_in/seed
afl-fuzz -Q -i ./afl_in/ -o ./afl_out/ /work/my-plugin/targets/coverage