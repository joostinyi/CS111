# NAME: Justin Yi
# EMAIL: joostinyi00@gmail.com
# ID: 905123893

# prep test files
touch input
touch output
echo "test file redirection" > input
echo "default output" > output

# bad input output option usage
./lab0 --input=. --output=.. 2> /dev/null
if [ $? -ne 2 ]; then
    echo "Failed bad input check"
fi

# bad input output option usage
./lab0 --output=.. --input=.. 2> /dev/null
if [ $? -ne 3 ]; then
    echo "Failed bad output check"
fi

# segfault catch check
./lab0 --segfault --catch 2> /dev/null
if [ $? -ne 4 ]; then
    echo "Failed segfault catch check"
fi

# normal use case
dd if=/dev/urandom of=RANDOM bs=1024 count=1 2> /dev/null
./lab0 --input=RANDOM --output=output 2> /dev/null
cmp RANDOM output
if [ $? -ne 0 ]; then
    echo "Failed file redirection"
fi

# clean up
rm input output RANDOM;

echo "Success"