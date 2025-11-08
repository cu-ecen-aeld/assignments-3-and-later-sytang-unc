#!/bin/sh

if [ $# -lt 2 ]
then
    echo "Invalid argument count: #?, expected at least 2"
    exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d ${filesdir} ]
then
    echo "Argument ${filesdir} is not a valid directory"
    exit 1
fi

cd ${filesdir}
filecount=$(find * | wc -l)
matchcount=$(grep ${searchstr} * | wc -l)

echo "The number of files are ${filecount} and the number of matching lines are ${matchcount}"

exit 0