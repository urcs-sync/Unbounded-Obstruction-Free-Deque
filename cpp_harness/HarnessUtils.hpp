#ifndef HARNESS_UTILS_HPP
#define HARNESS_UTILS_HPP

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h> 
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <cstdint>

// UTILITY CODE -----------------------------
void errexit (const char *err_str);
void faultHandler(int sig);
unsigned int nextRand(unsigned int last);
int warmMemory(unsigned int megabytes);
bool isInteger(const std::string & s);
std::string machineName();
int archBits();
int numCores();
#endif
