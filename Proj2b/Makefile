# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

build:
	/usr/local/cs/bin/gcc -g -pthread -Wall -Wextra -o lab2_list lab2_list.c SortedList.c

tests: lab2_list 
	-@chmod +x list_tests.sh
	-@./list_tests.sh >/dev/null 2>&1

profile: lab2_list
	LD_PRELOAD=/lib64/libprofiler.so CPUPROFILE=./raw.gperf ./lab2_list --iterations=1000 --threads=12 --sync=s 
	/bin/pprof --test ./lab2_list ./raw.gperf > profile.out
	/bin/pprof --list=thread_worker ./lab2_list ./raw.gperf >> profile.out
	rm -f ./raw.gperf

graphs: lab2b_list.csv
	-@chmod +x lab2_list.gp
	./lab2_list.gp

dist: build tests graphs
	tar -czvf lab2b-905123893.tar.gz lab2_list.c SortedList.c SortedList.h list_tests.sh profile.out *.csv *.png lab2_list.gp Makefile README

clean:
	rm -rf lab2_list lab2b-905123893.tar.gz