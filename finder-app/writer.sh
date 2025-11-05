#!/bin/bash

if [ $# -lt 2 ]
then
    echo "Invalid argument count: #?, expected at least 2"
    exit 1
fi

writefile=$1
writestr=$2

writedir=`dirname ${writefile}`

if [ ! -d ${writedir} ]
then
    mkdir -p ${writedir}
    if [ $? -ne 0 ]
    then
        echo "Failed to create directory ${writedir}"
        exit 1
    fi
fi 

echo "${writestr}" > ${writefile}
if [ $? -ne 0 ]
then
    echo "Failed to write to file ${writefile}"
    exit 1
fi

exit 0