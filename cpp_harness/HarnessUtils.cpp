#include "HarnessUtils.hpp"
#include <list>
#include <iostream>

using namespace std;

void errexit (const char *err_str)
{
    fprintf (stderr, "%s\n", err_str);
    throw 1;
}

// from:
//http://stackoverflow.com/questions/77005/how-to-generate-a-stacktrace-when-my-gcc-c-app-crashes
void faultHandler(int sig) {
  void *array[30];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 30);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

// from:
// http://stackoverflow.com/questions/2844817/how-do-i-check-if-a-c-string-is-an-int
bool isInteger(const std::string & s){
   if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false ;

   char * p ;
   strtol(s.c_str(), &p, 10) ;

   return (*p == 0) ;
}


	
	// from:
	// http://stackoverflow.com/questions/504810/how-do-i-find-the-current-machines-full-hostname-in-c-hostname-and-domain-info
std::string machineName(){
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	return std::string(hostname);
}

int numCores(){
	return sysconf( _SC_NPROCESSORS_ONLN );
}

int archBits(){
	if(sizeof(void*) == 8){
		return 64;
	}
	else if(sizeof(void*) == 16){
		return 128;
	}
	else if(sizeof(void*) == 4){
		return 32;
	}
	else if(sizeof(void*) == 2){
		return 8;
	}
}

unsigned int nextRand(unsigned int last) {
	unsigned int next = last;
	next = next * 1103515245 + 12345;
	return((unsigned)(next/65536) % 32768);
}

int warmMemory(unsigned int megabytes){
	uint64_t preheat = megabytes*(2<<20);

	int blockSize = sysconf(_SC_PAGESIZE);
	int toAlloc = preheat / blockSize;
	list<void*> allocd;

	int ret = 0;

	for(int i = 0; i<toAlloc; i++){
		int32_t* ptr  = (int32_t*)malloc(blockSize);
		// ptr2,3 reserves space in case the list needs room
		// this prevents seg faults, but we may be killed anyway
		// if the system runs out of memory ("Killed" vs "Segmentation Fault")
		int32_t* ptr2  = (int32_t*)malloc(blockSize); 
		int32_t* ptr3  = (int32_t*)malloc(blockSize);
		if(ptr==NULL || ptr2==NULL || ptr3==NULL){
			ret = -1;
			break;
		}
		free(ptr2); free(ptr3);
		ptr[0]=1;
		allocd.push_back(ptr);
	}
	for(int i = 0; i<toAlloc; i++){
		void* ptr = allocd.back();
		allocd.pop_back();
		free(ptr);
	}
	return ret;
}
