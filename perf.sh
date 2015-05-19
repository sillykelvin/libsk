#!/bin/bash

min_size=32
max_size=1048576 # 1024 * 1024

printf "shm mgr performance test, minimum size: $min_size, maximum size: $max_size\n"

block_size=16
chunk_count=1
heap_size=104857600 # 1024 * 1024 * 100
printf "\n[1], heap, max block size: $block_size, chunk count: $chunk_count, heap size: $heap_size\n"
./sk -s $block_size -c $chunk_count -h $heap_size

block_size=1048576
chunk_count=100
heap_size=1048576
printf "\n[2], chunk, max block size: $block_size, chunk count: $chunk_count, heap size: $heap_size\n"
./sk -s $block_size -c $chunk_count -h $heap_size

block_size=256
chunk_count=100
heap_size=104857600
printf "\n[3], both, max block size: $block_size, chunk count: $chunk_count, heap size: $heap_size\n"
./sk -s $block_size -c $chunk_count -h $heap_size

block_size=1024
chunk_count=100
heap_size=104857600
printf "\n[4], both, max block size: $block_size, chunk count: $chunk_count, heap size: $heap_size\n"
./sk -s $block_size -c $chunk_count -h $heap_size

block_size=4096
chunk_count=100
heap_size=104857600
printf "\n[5], both, max block size: $block_size, chunk count: $chunk_count, heap size: $heap_size\n"
./sk -s $block_size -c $chunk_count -h $heap_size

block_size=16384
chunk_count=100
heap_size=104857600
printf "\n[6], both, max block size: $block_size, chunk count: $chunk_count, heap size: $heap_size\n"
./sk -s $block_size -c $chunk_count -h $heap_size

block_size=65536
chunk_count=100
heap_size=104857600
printf "\n[7], both, max block size: $block_size, chunk count: $chunk_count, heap size: $heap_size\n"
./sk -s $block_size -c $chunk_count -h $heap_size

block_size=262144
chunk_count=100
heap_size=104857600
printf "\n[8], both, max block size: $block_size, chunk count: $chunk_count, heap size: $heap_size\n"
./sk -s $block_size -c $chunk_count -h $heap_size
