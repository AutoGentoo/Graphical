//
// Created by atuser on 10/22/17.
//

#ifndef HACKSAW_LOG_H
#define HACKSAW_LOG_H

#include <stdio.h>

#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_RESET   "\x1b[0m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_COLOR   "\x1b[39m"

void init_log() __attribute__((constructor));

void close_log() __attribute__((destructor));

void lset(FILE* target);

void lerror(char* format, ...);

void lwarning(char* format, ...);

void linfo(char* format, ...);

void lderror(char* format, ...);

void ldwarning(char* format, ...);

void ldinfo(char* format, ...);

#endif //HACKSAW_LOG_H