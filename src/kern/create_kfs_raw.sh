#! /usr/bin/bash


rm kfs.raw
cd ../user
make clean
make
cd ../util
make clean
make
./mkfs kfs.raw ../user/bin/init0 ../user/bin/init1 ../user/bin/init2 ../user/bin/trek ../user/bin/rule30 ../user/bin/init_trek_rule30 ../user/bin/init_fib_rule30 ../user/bin/init_fib_fib ../user/bin/fib ../user/bin/zork ../user/bin/rogue ../user/bin/test_refcnt ../user/bin/test_refcnt_file.txt ../user/bin/test_lock ../user/bin/test_lock_file.txt ../user/bin/greeting ../user/bin/cat ../user/bin/test_syscall ../user/bin/ahahaha.txt ../user/bin/HelloWorld.txt ../user/bin/HonorAndDeath.txt ../user/bin/HorusHeresyChOne.txt ../user/bin/HorusHeresyChTwo.txt ../user/bin/user_access_kern ../user/bin/syscall_ioctl ../user/bin/numbers.txt
mv kfs.raw ../kern
cd ../kern
