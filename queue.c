#ifndef QUEUE
#define QUEUE
#include <stdio.h>
#include <stdlib.h>
#include "multi-lookup.h"
#include <pthread.h>
#include <semaphore.h>

struct queue {
    char *addr;
    int elements;
    int enqueueIdx;
    int dequeueIdx;
    FILE *servicedFile;
    FILE *resolvedFile;
    sem_t *space;
    sem_t *items;
    pthread_mutex_t *lock;
    pthread_mutex_t *fileIdxLock;
    pthread_mutex_t *resolverLock;
    pthread_mutex_t *requesterLock;
    pthread_mutex_t *resFileLock;
    pthread_mutex_t *servFileLock;
    int fileIdx;
    int finished;
    char **dataFiles;
    int fileNum;
};
// Abstracts buffer to act as an array with indices the size of
MAX_NAME_LENGTH
void get(int index, char *dest, struct queue *queue)
{
    int verbose = 0;
    if (index < 0 || index >= SHARED_BUFFER_ELM_SIZE)
    {
        printf("%d is not a valid index in get\n", index);
        exit(-1);
    }
    char *getAddr = queue->addr + (index * MAX_NAME_LENGTH);
    strcpy(dest, getAddr);
    if (verbose)
        printf("Getting value %s from index %d\n", dest, index);
}
char *set(int index, char *newDomain, struct queue *queue)
{
    int verbose = 0;
    if (index < 0 || index >= SHARED_BUFFER_ELM_SIZE)
    {
        printf("%d is not a valid index in set\n", index);
        exit(-1);
    }
    if (verbose)
        printf("Setting value at index %d to %s\n", index,
               newDomain);
    char *setAddr = queue->addr + (index * MAX_NAME_LENGTH);
    memset(setAddr, '\0', MAX_NAME_LENGTH); // resets buffer slot
    return memcpy(setAddr, newDomain, MAX_NAME_LENGTH);
}
void clearIdx(int index, struct queue *queue)
{
    char *setAddr = queue->addr + (index * MAX_NAME_LENGTH);
    memset(setAddr, '\0', MAX_NAME_LENGTH); // resets buffer slot
}
void enqueue(char *domainName, struct queue *queue)
{
    int verbose = 0;
    sem_wait(queue->space);
    if (queue->elements >= SHARED_BUFFER_ELM_SIZE)
    {
        if (verbose)
            printf("Queue full, can't add domain name: %s\n",
                   domainName);
        return;
    }
    pthread_mutex_lock(queue->requesterLock);
    set(queue->enqueueIdx, domainName, queue);
    queue->elements++;
    queue->enqueueIdx = (queue->enqueueIdx + 1) % SHARED_BUFFER_ELM_SIZE;
    pthread_mutex_unlock(queue->requesterLock);
    sem_post(queue->items);
}
void dequeue(char *dest, struct queue *queue)
{
    int verbose = 1;
    sem_wait(queue->items);
    if (queue->elements > SHARED_BUFFER_ELM_SIZE)
    {
        if (verbose)
            printf("Elements in queue can't exceed %d\n",
                   SHARED_BUFFER_ELM_SIZE);
    }
    pthread_mutex_lock(queue->resolverLock);
    get(queue->dequeueIdx, dest, queue);
    clearIdx(queue->dequeueIdx, queue);
    queue->elements--;
    queue->dequeueIdx = (queue->dequeueIdx + 1) % SHARED_BUFFER_ELM_SIZE;
    pthread_mutex_unlock(queue->resolverLock);
    sem_post(queue->space);
}
void printQueue(struct queue *queue)
{
    int i;
    int iterations = SHARED_BUFFER_ELM_SIZE;
    int idx;
    printf("\nPRINTING CIRCULAR QUEUE:\n");
    for (i = 0; i < iterations; i++)
    {
        idx = i;
        char domain[MAX_NAME_LENGTH];
        get(idx, domain, queue);
        printf("Index %d: [%s] \n", idx, domain);
    }
    printf("\n");
}
// char newDomain[MAX_NAME_LENGTH];
// enqueue("Hallo, please work mr queue", &masterQueue);
// enqueue("working1", &masterQueue);
// enqueue("working2", &masterQueue);
// enqueue("working3", &masterQueue);
// enqueue("working4", &masterQueue);
// dequeue(newDomain, &masterQueue);
// printf("%s\n", newDomain);
// enqueue("working5", &masterQueue);
// enqueue("working6", &masterQueue);
// printQueue(&masterQueue);
#endif