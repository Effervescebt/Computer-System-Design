#! /usr/bin/bash


make clean
bash mkcomp.sh kfs.raw
make kernel.elf
make run-kernel
