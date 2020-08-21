#!/bin/bash
# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

./lab4b &>/dev/null <<-EOF
SCALE=F
PERIOD=2
STOP
LOG test
OFF
EOF
if [[ $? -ne 0 ]]; then
    echo "Standard usage no args failed"
fi 

./lab4b --log=LOGFILE <<-EOF
SCALE=F
PERIOD=2
STOP
LOG test
OFF
EOF

if [[ ! -s LOGFILE || $(grep "PERIOD=2" LOGFILE &>/dev/null) -ne 0 ||
        $(grep "LOG test" LOGFILE &>/dev/null) -ne 0 ||
        $(grep "SHUTDOWN" LOGFILE &>/dev/null) -ne 0 ]]; then
        echo "logging usage failed"
fi 

echo "all tests passed"

rm -f LOGFILE