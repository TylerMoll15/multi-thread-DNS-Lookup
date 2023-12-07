#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "util.h"
#define ARRAY_SIZE 8
#define MAX_INPUT_FILES 100
#define MAX_REQUESTER_THREADS 10
#define MAX_RESOLVER_THREADS 10
#define MAX_NAME_LENGTH 1025
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define PREDATA_LEN 5
#define SHARED_KEY 420
#define SHARED_BUFFER_ELM_SIZE 8
#define VERBOSE 1
                  
                  
int main(int argc, char *argv[]);
#endif