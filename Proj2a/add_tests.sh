#!/bin/bash

# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

ADD_PGM=./lab2_add

for threads in 1 2 4 8 12
do
    for iterations in 10 20 40 80 100 1000 10000 100000
    do 
        $ADD_PGM --iterations=$iterations --threads=$threads >> lab2_add.csv
        $ADD_PGM --iterations=$iterations --threads=$threads --yield >> lab2_add.csv
        for s in m c s
        do
            if [[ "$s" == "s" && $iterations -ge 1000 ]]; then
                iterations=1000
            fi
            if [ $iterations -ge 10000 ]; then
                iterations=10000
            fi
            $ADD_PGM --iterations=$iterations --threads=$threads --sync=$s >> lab2_add.csv
            $ADD_PGM --iterations=$iterations --threads=$threads --yield --sync=$s >> lab2_add.csv
        done
    done 
done

