#!/bin/sh
# Assignment 1 script for writing a string to a file
# Author: Malcolm McKellips

if [ $# -lt 2 ]
then
	echo "Not enough arguments supplied to writer script! Expected 2, got $#."
	exit 1
else 
	WRITEFILE=$1
	WRITESTR=$2
	
	DIRECTORY=$(dirname $WRITEFILE)
	
	#-p will create parent folders and not error if parts of path already exist
	mkdir -p $DIRECTORY
	
	if [ $? -ne 0 ]
	then
		echo "ERROR creating parent directory for WRITEFILE in writer!"
		exit 1
	fi 
	
	#Now create file with contents
	#Note, >> appends, > overwrites
	echo $WRITESTR > $WRITEFILE
	
	if [ $? -ne 0 ]
	then
		echo "ERROR writng/creating WRITEFILE in writer!"
		exit 1
	fi 
fi 


