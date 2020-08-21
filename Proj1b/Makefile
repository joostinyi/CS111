# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

default: client server

clean:
	rm -rf lab1b-client lab1b-server lab1b-905123893.tar.gz

dist: lab1b-client.c lab1b-server.c Makefile README
	tar -czf lab1b-905123893.tar.gz lab1b-client.c lab1b-server.c Makefile README

client: lab1b-client.c
	/usr/local/cs/bin/gcc -g -lz -Wall -Wextra -o lab1b-client lab1b-client.c

server: lab1b-server.c
	/usr/local/cs/bin/gcc -g -lz -Wall -Wextra -o lab1b-server lab1b-server.c