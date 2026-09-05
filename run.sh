#!/bin/bash

echo "========== Compiling =========="

gcc userProgram.c ./libraries/function.c ./libraries/fileFunction.c -o userProgram || exit 1
gcc system.c ./libraries/function.c ./libraries/fileFunction.c -o system || exit 1
gcc mouth.c ./libraries/function.c -o mouth || exit 1
gcc ear.c ./libraries/function.c -o ear || exit 1

echo "Compilation successful."


if [ "$1" = "system" ]; then
    echo "Starting SYSTEM..."
    ./system 5000 systemDir < systemInput.txt     

elif [ "$1" = "userProgram" ]; then
    echo "Starting User program...."
    ./userProgram 5000 6000 userDir < userInput.txt

else
    echo "Starting SYSTEM..."
    ./system 5000 systemDir < systemInput.txt &

    SYSTEM_PID=$!
    sleep 1

    echo "Starting user program..."
    ./userProgram 5000 6000 userDir < userInput.txt

    wait $SYSTEM_PID

fi
