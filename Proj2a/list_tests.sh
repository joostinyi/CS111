#!/bin/bash

# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

LIST_PGM=./lab2_list

for threads in 1 2 4 8 12
do
    for iterations in 1 10 100 1000 10000 20000
    do 
        if [[ $threads -gt 1 && $iterations -gt 1000 ]]; then 
            iterations=1000;
        fi
        $LIST_PGM --iterations=$iterations --threads=$threads >> lab2_list.csv
    done 
done

for threads in 2 4 8 12
do
    for iterations in 1 2 4 8 16 32
    do 
        # lab2_list-2.png
        $LIST_PGM --iterations=$iterations --threads=$threads --yield=i >> lab2_list.csv
        $LIST_PGM --iterations=$iterations --threads=$threads --yield=d >> lab2_list.csv
        $LIST_PGM --iterations=$iterations --threads=$threads --yield=il >> lab2_list.csv
        $LIST_PGM --iterations=$iterations --threads=$threads --yield=dl >> lab2_list.csv

        # lab2_list-3.png
        for s in m s
        do
            $LIST_PGM --iterations=$iterations --threads=$threads --yield=i --sync=$s>> lab2_list.csv
            $LIST_PGM --iterations=$iterations --threads=$threads --yield=d --sync=$s>> lab2_list.csv
            $LIST_PGM --iterations=$iterations --threads=$threads --yield=il --sync=$s>> lab2_list.csv
            $LIST_PGM --iterations=$iterations --threads=$threads --yield=dl --sync=$s>> lab2_list.csv
        done        
    done 
done

for threads in 1 2 4 8 12 16 24
do
    iterations=1000;
    # lab2_list-4.png
    for s in m s
    do
        $LIST_PGM --iterations=$iterations --threads=$threads --sync=$s>> lab2_list.csv
        $LIST_PGM --iterations=$iterations --threads=$threads --sync=$s>> lab2_list.csv
        $LIST_PGM --iterations=$iterations --threads=$threads --sync=$s>> lab2_list.csv
        $LIST_PGM --iterations=$iterations --threads=$threads --sync=$s>> lab2_list.csv
        $LIST_PGM --iterations=$iterations --threads=$threads --sync=$s>> lab2_list.csv
    done    
done