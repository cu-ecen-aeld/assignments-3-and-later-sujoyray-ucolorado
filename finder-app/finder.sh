#!/bin/sh
#/*****************************************************************************
#* Copyright (C) 2023 by Sujoy Ray
#*
#* Redistribution, modification or use of this software in source or binary
#* forms is permitted as long as the files maintain this copyright. Users are
#* permitted to modify this and use it to learn about the field of embedded
#* software. Sujoy Ray and the University of Colorado are not liable for
#* any misuse of this material.
#*
#*****************************************************************************/
#/**
#* @file  finder.sh
#* @brief : The script recursively finds the patter and prints the total occurrences of 
#*          the string and file count.
#*
#* @author Sujoy Ray
#* @date January 25, 2023
#* @version 1.0
#* CREDIT: Header credit: University of Colorado coding standard
#* CREDIT: Consulted find and grep command options from the manual and book.  
#*
#*/

#Step 1: check if all the required inputs are available, if not return error
if [ $# -lt 2 ]; then 
  echo "Please check arguments"
  echo "	Argument 1: <dir>"
  echo "	Argument 2: text to find"
  exit 1;
fi

filesdir=$1

# Step 2: check if the directory exists, if not return error
if [ ! -d $filesdir ]; then
  echo "Directory doesn't exist"
  exit 1
fi

# Step 3: grep -Rl lists all the files containing the string, pipe it to wc to get the count
# save it to filecnt varaible.  
filecnt=$(find  $filesdir -type f | xargs grep -Rl "${2}" | wc -l)

# Step 4: grep -Rhc lists total counts of the tring in each file , pipe it to awk to calculate the sum
# In the end, save it to variable.  
value=$(find  $filesdir -type f | xargs grep -Rhc "${2}" | awk '{ number += $0; } END { print number }')

#Step 5: Print the data
echo "The number of files are $filecnt and the number of matching lines are $value"

