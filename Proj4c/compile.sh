#!/bin/bash
# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

ret='uname -a | grep "armv7l"'
if uname -a | grep -q 'armv7l'; then
	/usr/bin/gcc -lmraa -lm -Wall -Wextra -o lab4c_tcp lab4c_tcp.c
    /usr/bin/gcc -lmraa -lm  -lcrypto -lssl Wall -Wextra -o lab4c_tls lab4c_tls.c
else
    /usr/bin/gcc -o lab4c_tcp -DDUMMY -lm lab4c_tcp.c 
    /usr/bin/gcc -o lab4c_tls -DDUMMY -lm -lcrypto -lssl lab4c_tls.c 
fi 
