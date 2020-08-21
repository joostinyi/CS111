#!/bin/bash

# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

LIST_PGM=./lab2_list

#	lab2b_1.png ... throughput vs. number of threads for mutex and spin-lock synchronized list operations.
#   lab2b_2.png ... mean time per mutex wait and mean time per operation for mutex-synchronized list operations.
#   lab2b_3.png ... successful iterations vs. threads for each synchronization method.
#   lab2b_4.png ... throughput vs. number of threads for mutex synchronized partitioned lists.
#   lab2b_5.png ... throughput vs. number of threads for spin-lock-synchronized partitioned lists.

# lab2b_1.png

for threads in 1 2 4 8 12 16 24
do
    $LIST_PGM --iterations=1000 --threads=$threads --sync=s >> lab2b_list.csv
    $LIST_PGM --iterations=1000 --threads=$threads --sync=m >> lab2b_list.csv
done

# lab2b_2.png
for threads in 1 2 4 8 12 16 24
do
    $LIST_PGM --iterations=1000 --threads=$threads --sync=m >> lab2b_list.csv
done

# lab2b_3.png
for threads in 1 4 8 12 16
do
    for iterations in 1 2 4 8 16
    do
        $LIST_PGM --iterations=$iterations --threads=$threads --yield=id --lists=4 >> lab2b_list.csv
    done
    for iterations in 10 20 40 80
    do 
        $LIST_PGM --iterations=$iterations --threads=$threads --yield=id --lists=4 --sync=s >> lab2b_list.csv
        $LIST_PGM --iterations=$iterations --threads=$threads --yield=id --lists=4 --sync=m >> lab2b_list.csv
    done
done

# lab2b_4.png and lab2b_3.png
for threads in 1 2 4 8 12
do
    for lists in 1 4 8 16
    do
        $LIST_PGM --iterations=1000 --threads=$threads --lists=$lists --sync=s >> lab2b_list.csv
        $LIST_PGM --iterations=1000 --threads=$threads --lists=$lists--sync=m >> lab2b_list.csv
    done
done