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

#Call respective makefiles to add sources and set up include path

# C entry point
C_SOURCES += writer.c


#Convert .C and .S files to .o files 
COBJS         := $(foreach source_file, $(C_SOURCES:.c=.obj), $(OBJOUTPUTDIR)/$(notdir $(source_file)) )
ASMOBJS       := $(foreach source_file, $(ASM_SOURCES:.S=.obj), $(OBJOUTPUTDIR)/$(notdir $(source_file)) )
OBJS          := $(COBJS) $(ASMOBJS)

PROJECT = writer
VPATH = $(SRC_DIR)


RM      = rm -f
MKDIR   = @mkdir -p $(@D) #creates folders if not present

#GCC FLAGS
CFLAGS =-g -Wall -Werror ${addprefix -I ,${BUILD_INC_DIR}} ${C_DEBUG_FLAG}
AFLAGS =


#LD FLAGS Flags

LDFLAGS = -lm

# Rules to build elf file
all: $(Output_DIR)/$(PROJECT)

$(COBJS) : $(OBJOUTPUTDIR)/%.obj : %.c
	@echo CC: ${<}
	$(MKDIR)
	@${CC} -o ${@} ${CFLAGS} -c $(<) 

$(ASMOBJS) : $(OBJOUTPUTDIR)/%.obj : %.S
	@echo AS: ${<}
	$(MKDIR)
	@${CC} -o ${@} ${AFLAGS} -c $(<) 

$(Output_DIR)/$(PROJECT): $(OBJS)
	@echo LD: ${@}
	$(MKDIR)
	@$(LD) -o ${@} $^ $(LDFLAGS)
	

clean:
	-$(RM) *.o*
	-$(RM) writer

.PHONY: all clean

