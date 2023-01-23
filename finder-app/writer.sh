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
#* @file  writer.sh
#* @brief write a pattern to a file. If the file and directory doesn't exist, 
#* this script will create a new one. 
#  
#*
#* @author Sujoy Ray
#* @date January 25, 2023
#* @version 1.0
#* CREDIT: Header credit: University of Colorado coding standard
#* CREDIT: Consulted find and grep command options from the manual and book.  
#*
#*/
if [ $# -lt 2 ]; then 
  echo "Please check arguments"
  echo "	Argument 1: <dir>/<file>"
  echo "	Argument 2: text to insert"
  exit 1;
fi

# Get the directory from the file path
dirpath=$(dirname "$1")

# Create the director
mkdir -p $dirpath

#Check the error code
if [ $? -gt 0 ]; then
  echo "Unable to create directory": $dirpath
  echo "Please check permission"
  exit 1
fi

#The will create file, if there is no file and then write content
echo $2 > $1

#Check the error code
if [ $? -gt 0 ]; then
  echo "Unable to create/ write to file": $1
  echo "Please check permission"
  exit 1
fi



