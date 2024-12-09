#! /usr/bin/bash

bash create_kfs_raw.sh
make clean
bash mkcomp.sh kfs.raw
make kernel.elf
make run-kernel