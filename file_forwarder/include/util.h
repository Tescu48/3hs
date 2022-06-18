

/* Includes */
#pragma once
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <3ds.h>

#define PROGRAM_TID 0x00004000001111100
#define FALSE 0
#define TRUE 1


/* Prototypes */
void fcpy(char *loc, char *dest);
char *read_file(char *fname);
void hang(char *message);
void self_destruct(void);
void mkdirp(char *loc);
