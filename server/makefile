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
#* @file makefile
#* @brief Makefile to build C code
#*         
#*
#*
#* This is a top level makefile, which internally calls different makefiles
#* to build C files needed for various assignments.
#*
#* @author Sujoy Ray
#* @date January 28, 2023
#* @version 1.0
#*
#* CREDIT: Header credit: University of Colorado coding standard
#*/



#Set compiler options
COMPILER_PREFIX=
CC=$(CROSS_COMPILE)gcc
AS=$(CROSS_COMPILE)as
LD=$(CROSS_COMPILE)gcc
OBJCOPY=$(CROSS_COMPILE)objcopy

# Get all directories lined up
export C_SOURCES=
export ASM_SOURCES=
export SRC_BASE=$(PWD)
export SRC_DIR=
export OBJOUTPUTDIR =$(PWD)
export BUILD_INC_DIR=$(SRC_BASE)/include
export Output_DIR= $(PWD)

RM      = rm -f
CFLAGS =-g -Wall -Werror
LIBS =-lpthread -lrt

all: aesdsocket

aesdsocket: aesdsocket.o
	$(CC) -o aesdsocket ${CFLAGS} aesdsocket.o ${LIBS}	

clean:
	-$(RM) *.o*
	-$(RM) aesdsocket
	-$(RM) *.txt

.PHONY: all clean

