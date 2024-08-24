#!/bin/bash


argNum=$#
filePath=$1
inputString=$2

if [ ${argNum} -lt 2 ]
then
	echo "Error: Missing arguments"
	exit 1
fi

directory=$(dirname ${filePath})

if [ -d ${directory} ]
then
	:
else
	mkdir -p ${directory}
fi

echo ${inputString} > ${filePath}
