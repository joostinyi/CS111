# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

default: lab0.c
	/usr/local/cs/bin/gcc -g -Wall -Wextra -o lab0 lab0.c
check: lab0
	@chmod +x ./test.sh;\
	./test.sh
clean:
	rm -rf lab0 lab0-905123893.tar.gz 
dist: lab0.c Makefile backtrace.png breakpoint.png README
	tar -czf lab0-905123893.tar.gz lab0.c Makefile backtrace.png breakpoint.png README test.sh 
