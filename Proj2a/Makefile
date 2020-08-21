# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

build:
	/usr/local/cs/bin/gcc -g -lz -pthread -Wall -Wextra -o lab2_add lab2_add.c
	/usr/local/cs/bin/gcc -g -lz -pthread -Wall -Wextra -o lab2_list lab2_list.c SortedList.c

tests: build 
	chmod +x add_tests.sh
	./add_tests.sh >/dev/null 2>&1
	chmod +x list_tests.sh
	./list_tests.sh >/dev/null 2>&1

graphs: 
	./lab2_add.gp
	./lab2_list.gp

dist: tests graphs
	tar -czf lab2a-905123893.tar.gz lab2_add.c lab2_list.c SortedList.c SortedList.h add_tests.sh list_tests.sh *.csv *.png lab2_add.gp lab2_list.gp Makefile README 

clean:
	rm -rf lab2_add lab2_list lab2a-905123893.tar.gz 