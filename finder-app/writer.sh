#!/bin/sh

# Check for number of arguments
if [ ! $# -eq 2 ]
then
	echo "Invalid number of arguments! Need to be 2"
	exit 1
fi

WRITEFILE=$1
WRITESTR=$2

# Get the directory name
FILEDIR=$(dirname $WRITEFILE)

# Check if the directory exists
if [ ! -d $FILEDIR ]
then
	# Create the path if it doesn't exist
	mkdir -p $FILEDIR
	
	if [ ! $? -eq 0 ]
	then
		echo "Error while creating the directory!"
		exit 1
	fi
fi

# Create the file
echo $WRITESTR > $WRITEFILE


if [ ! $? -eq 0 ]
then
	echo "Error writing the string to the file!"
	exit 1
fi

# References:
# https://linuxconfig.org/how-to-create-a-new-subdirectory-with-a-single-command-on-linux
