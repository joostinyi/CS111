#!/bin/bash
# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

ret='uname -a | grep "armv7l"'
if uname -a | grep -q 'armv7l'; then
	/usr/bin/gcc -lmraa -lm -Wall -Wextra -o lab4b lab4b.c
else
    /usr/bin/gcc -o lab4b -DDUMMY -lm lab4b.c 
fi 
