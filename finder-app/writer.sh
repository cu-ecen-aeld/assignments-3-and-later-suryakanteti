#!/bin/sh

if [ ! $# -eq 2 ]
then
	echo "Invalid number of arguments! Need to be 2"
	exit 1
fi

WRITEFILE=$1
WRITESTR=$2

# Get the directiry name
FILEDIR=$(dirname $WRITEFILE)

# Check if the directory exists
if [ ! -d $FILEDIR ]
then
	# Create the path if it doesn't exist
	mkdir -p $FILEDIR
fi

# Create the file
echo $WRITESTR > $WRITEFILE


# References:
# https://linuxconfig.org/how-to-create-a-new-subdirectory-with-a-single-command-on-linux
