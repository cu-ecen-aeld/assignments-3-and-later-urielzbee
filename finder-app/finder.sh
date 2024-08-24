#!/bin/bash

#echo "Hola mundo"
#echo "ola"
#echo "ola"
#echo "ola"

argNum=$#
directory=$1
name=$2

#echo ${argNum}
#echo ${directory}
#echo ${name}


if [ ${argNum} -lt 2 ]
then
	exit 1
fi

if [ -d ${directory} ]
then
	#grep -r ${name} ${directory}
	number_of_files=$(grep -Rl ${name} ${directory} -c | wc -l)
	number_of_lines=$(grep -r ${name} ${directory} | wc -l)
	echo "The number of files are ${number_of_files} and the number of matching lines are ${number_of_lines}"
else
	exit 1
fi
