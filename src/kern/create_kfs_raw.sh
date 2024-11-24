#! /usr/bin/bash


rm kfs.raw
cd ../util
make clean
make
./mkfs kfs.raw ../user/bin/init0 ../user/bin/init1 ../user/bin/init2 ../user/bin/trek
mv kfs.raw ../kern
cd ../kern
bash mkcomp.sh kfs.raw
