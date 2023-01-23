#!/bin/sh
# Assignment 1 script for finding matching lines in a file
# Author: Malcolm McKellips

if [ $# -lt 2 ]
then
	echo "Not enough arguments supplied to finder script! Expected 2, got $#."
	exit 1
else 
	FILESDIR=$1
	SEARCHSTR=$2
fi 

if [ ! -d $FILESDIR ]
then
	echo "Error supplied path: ${FILESDIR} is not a valid path to a directory"
	exit 1
else
	#Reference for this line: https://stackoverflow.com/questions/9157138/recursively-counting-files-in-a-linux-directory
	X=$(find ${FILESDIR} -type f | wc -l)
	Y=$(grep -R ${SEARCHSTR} ${FILESDIR} | wc -l)
	echo "The number of files are ${X} and the number of matching lines are ${Y}"
fi
	
