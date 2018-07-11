#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "uproc.h"

#define MAX 64 // This is the maximum number of entries uproc array will allow.
#define NULL (void*)0

void display(int max, struct uproc ** procTable);           // Displays uproc array data.
void rel(int lower, int upper, struct uproc ** procTable);  // Releases dynamically allocated memory.

int
main(void)
{
    // Allocate memory for array of uproc structs.
    uproc ** procTable = (uproc**)malloc(MAX * sizeof(uproc*));
    for(int i = 0; i < MAX; ++i)
        procTable[i] = (uproc*)malloc(sizeof(uproc));
    int ptableSize = getprocs(MAX, *procTable);

    // If ptableSize is -1, that means MAX is smaller than the number of actives processes.
    if(ptableSize == -1){
        rel(0, MAX, procTable);
        free(procTable);
        printf(1, "Raise maximum entries in ps.c to view running processes.\n");
        exit();
    }

    // Free unused memory and display uproc array data.
    rel(ptableSize, MAX, procTable);
    display(ptableSize, procTable);
    rel(0, ptableSize, procTable);
    free(procTable);
    exit();
}

void display(int max, struct uproc ** procTable){
    printf(1, "PID\tUID\tGID\tPPID\tElapsed\t\tCPU\tState\tSize\tName\n");
    for(int i = 0; i < max; ++i){
        printf(1, "%d\t%d\t%d\t%d\t%d\t%d\t%s\t%d\t%s\n",
                procTable[i]->pid,
                procTable[i]->uid,
                procTable[i]->gid,
                procTable[i]->ppid,
                procTable[i]->elapsed_ticks,
                procTable[i]->CPU_total_ticks,
                procTable[i]->state,
                procTable[i]->size,
                procTable[i]->name);
    }
}

void rel(int lower, int upper, struct uproc ** procTable){
    for(int i = lower; i < upper; ++i){
        free(procTable[i]);
        procTable[i] = NULL;
    }
}

//struct uproc * procTable = (uproc*)malloc(max*sizeof(uproc));
#endif




















