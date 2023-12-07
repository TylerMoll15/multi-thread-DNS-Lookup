#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "util.h"
#include "multi-lookup.h"
#include <sys/time.h>
#include <sys/shm.h>
#include "queue.h"
#include <semaphore.h>
#include <time.h>

void checkErr(int argc, char *argv[])
{
    // Error checking
    if (argc < 6)
    {
        printf("Incorrect Usage: multi-lookup <#requester> <#resolver>
        <request log> <resolver log> [<data file> ...]\n");
        exit(-1);
    }

    int DATAFILES_LEN = argc - PREDATA_LEN;

    if (DATAFILES_LEN > MAX_INPUT_FILES)
    {
        fprintf(stderr, "%d is too many input files!\n", DATAFILES_LEN);
        exit(-1);
    }
    if (atoi(argv[1]) > MAX_REQUESTER_THREADS)
    {
            fprintf(stderr, "ERROR: %d exceeds the limit of requester
            threads\n", atoi(argv[1]));
            exit(-1);
    }
    if (atoi(argv[2]) > MAX_RESOLVER_THREADS)
    {
        fprintf(stderr, "ERROR: %d exceeds the limit of resolver
        threads\n", atoi(argv[2]));
        exit(-1);
    }
}

void writeResolved(char *result, struct queue *queue)
{
    int verbose = 0;
    if (verbose)
        printf("Writing %s to resolved...\n", result);
    fprintf(queue->resolvedFile, "%s", result);
}

void writeServiced(char *result, struct queue *queue)
{
    int verbose = 0;
    if (strncmp(result, "\n", 2) && verbose) printf("Writing %s to serviced...\n", result);
    pthread_mutex_lock(queue->servFileLock);
    fprintf(queue->servicedFile, "%s", result);
    fprintf(queue->servicedFile, "\n");
    pthread_mutex_unlock(queue->servFileLock);
}

struct reqArgs
{
    char *dataFile;
    struct queue *queue;
};

void *requester(void *args)
{
    int verbose = 0;
    if (verbose) printf("Requester thread: 0x%x initialized\n", (unsigned int)pthread_self());
    struct reqArgs *reqArgs = args;
    struct queue *queue = reqArgs->queue;
    FILE *fp;
    char *dataFile;
    int fileCount = 0;
    while (queue->fileIdx < queue->fileNum)
    {
        pthread_mutex_lock(queue->fileIdxLock);
        dataFile = queue->dataFiles[queue->fileIdx];
        queue->fileIdx++;
        pthread_mutex_unlock(queue->fileIdxLock);
        fp = fopen(dataFile, "r");
        if (NULL != fp)
        {
            char line[MAX_NAME_LENGTH];
            fileCount++;
            do
            {
                fgets(line, MAX_NAME_LENGTH, fp);
                line[strcspn(line, "\n")] = '\0';
                if (strcmp(line, ""))
                {
                    enqueue(line, queue);
                    writeServiced(line, queue);
                }
                memset(line, '\0', MAX_NAME_LENGTH); // resets buffer slot
            } while (!feof(fp));
            queue->finished = 1;
            fclose(fp);
        }
        else
        {
            fprintf(stderr, "invalid file %s\n", dataFile);
        }
    }
    printf("thread 0x%x serviced %d files\n", (unsigned int)pthread_self(), fileCount);
    return NULL;
}

void *resolver(void *args)
{
    int verbose = 0;
    struct queue *queue = args;
    if (verbose) printf("Resolver thread: 0x%x initialized\n", (unsigned int)pthread_self());
    char domainToResolve[MAX_NAME_LENGTH];
    char ip[MAX_IP_LENGTH];
    int hostNameCount = 0;
    while (1)
    {
        dequeue(domainToResolve, queue);
        domainToResolve[strcspn(domainToResolve, "\n")] = '\0';
        if (strcmp(domainToResolve, ""))
        {
            if (!strcmp(domainToResolve, "DEATH"))
            {
                if (verbose)
                    printf("Death was had for this thread\n");
                break;
            }
            int stat = dnslookup(domainToResolve, ip, MAX_IP_LENGTH);
            if (stat == -1)
            {
                strncat(domainToResolve, ", NOT_RESOLVED\n",
                        MAX_NAME_LENGTH);
                pthread_mutex_lock(queue->lock);
                writeResolved(domainToResolve, queue);
                pthread_mutex_unlock(queue->lock);
                domainToResolve[0] = '\0';
            }
            else
            {
                sprintf(domainToResolve, "%s, %s\n", domainToResolve, ip);
                pthread_mutex_lock(queue->lock);
                writeResolved(domainToResolve, queue);
                pthread_mutex_unlock(queue->lock);
                domainToResolve[0] = '\0';
            }
            domainToResolve[0] = '\0';
            hostNameCount++;
        }
    }
    printf("thread 0x%x resolved %d hostnames\n", (unsigned int)pthread_self(), hostNameCount);
    return NULL;
}
int main(int argc, char *argv[])
{
    clock_t start, end;
    double cpu_time_used;
    start = clock();
    checkErr(argc, argv);
    int shmid = shmget(SHARED_KEY, SHARED_BUFFER_ELM_SIZE * MAX_NAME_LENGTH, IPC_CREAT | 0666);
    char *str = (char *)shmat(shmid, (void *)0, 0);
    // Command line arg reading
    int reqTnum = atoi(argv[1]);
    int resTnum = atoi(argv[2]);
    char *reqLog = argv[3];
    char *resLog = argv[4];
    // Arr of data file names
    int DATAFILES_LEN = argc - PREDATA_LEN;
    char **dataFiles = (char **)malloc(DATAFILES_LEN * sizeof(char *));
    if (dataFiles == NULL)
    {
        fprintf(stderr, "Malloc err\n");
        exit(-1);
    }
    int i;
    for (i = 0; i < DATAFILES_LEN; i++)
    {
        dataFiles[i] = argv[i + PREDATA_LEN];
    }
    // Create queue
    struct queue masterQueue;
    masterQueue.addr = str;
    masterQueue.elements = 0;
    masterQueue.dequeueIdx = 0;
    masterQueue.enqueueIdx = 0;
    masterQueue.resolvedFile = fopen(resLog, "w");
    masterQueue.servicedFile = fopen(reqLog, "w");
    masterQueue.finished = 0;
    masterQueue.fileNum = DATAFILES_LEN;
    masterQueue.dataFiles = dataFiles;
    if (masterQueue.resolvedFile == 0 || masterQueue.servicedFile == 0)
    {
        printf("Log open error\n");
        exit(-1);
    }
// Idea credit:
// https: // w3.cs.jmu.edu/kirkpams/OpenCSF/Books/csf/html/ProdCons.html
    sem_t space;
    sem_t items;
    pthread_mutex_t lock;
    pthread_mutex_t fileIdxLock;
    pthread_mutex_t resolverLock;
    pthread_mutex_t requesterLock;
    pthread_mutex_t resFileLock;
    pthread_mutex_t servFileLock;
    sem_init(&space, 0, SHARED_BUFFER_ELM_SIZE);
    sem_init(&items, 0, 0);
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&fileIdxLock, NULL);
    pthread_mutex_init(&resolverLock, NULL);
    pthread_mutex_init(&requesterLock, NULL);
    pthread_mutex_init(&resFileLock, NULL);
    pthread_mutex_init(&servFileLock, NULL);
    masterQueue.space = &space;
    masterQueue.items = &items;
    masterQueue.lock = &lock;
    masterQueue.fileIdxLock = &fileIdxLock;
    masterQueue.fileIdx = 0;
    masterQueue.dataFiles = dataFiles;
    masterQueue.resolverLock = &resolverLock;
    masterQueue.requesterLock = &requesterLock;
    masterQueue.resFileLock = &resFileLock;
    masterQueue.servFileLock = &servFileLock;
    // CONCURRENCY
    pthread_t *request;
    pthread_t *resolve;
    request = malloc(reqTnum * sizeof(pthread_t));
    resolve = malloc(resTnum * sizeof(pthread_t));
    struct reqArgs reqArgs;
    reqArgs.queue = &masterQueue;
    // Init requester threads
    for (i = 0; i < reqTnum; i++)
    {
        pthread_create(&request[i], NULL, &requester, &reqArgs);
    }
    // Init for resolver threads
    for (i = 0; i < resTnum; i++)
    {
        pthread_create(&resolve[i], NULL, &resolver, &masterQueue);
    }
    // Join req threads
    for (i = 0; i < reqTnum; i++)
    {
        pthread_join(request[i], NULL);
    }
    // kill resolver threads
    for (i = 0; i < resTnum; i++)
    {
        enqueue("DEATH", &masterQueue);
    }
    // Join res threads
    for (i = 0; i < resTnum; i++)
    {
        pthread_join(resolve[i], NULL);
    }
    fclose(masterQueue.resolvedFile);
    fclose(masterQueue.servicedFile);
    shmdt(str);
    shmctl(shmid, IPC_RMID, NULL); // destroy the shared memory
    free(resolve);
    free(request);
    free(dataFiles);
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&fileIdxLock);
    pthread_mutex_destroy(&resolverLock);
    pthread_mutex_destroy(&requesterLock);
    sem_destroy(&space);
    sem_destroy(&items);
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("multi-lookup ran in %f seconds\n", cpu_time_used);
    return 0;
}