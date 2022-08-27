#!/bin/sh

if [ ! $# -eq 2 ]
then
	echo "Invalid number of arguments! Needs to be 2"
	exit 1
fi

FILESDIR=$1
SEARCHSTR=$2

if [ ! -d $FILESDIR ]
then
	echo "Invalid directory path!"
	exit 1
fi

# Find number of files
FILENUM=$(find $FILESDIR -type f | wc -l)

# Number of matching lines
LINEMATCHES=$(grep $SEARCHSTR -r $FILESDIR | wc -l)

echo “The number of files are $FILENUM and the number of matching lines are $LINEMATCHES”

# References:
# https://devconnected.com/how-to-count-files-in-directory-on-linux/ --> Count files
# https://www.cyberciti.biz/faq/how-to-show-recursive-directory-listing-on-linux-or-unix/
