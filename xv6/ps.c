#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "uproc.h"

#define MAX 64 // This is the maximum number of entries uproc array will allow.

void display(int activeProcs, uproc * procTable); // Displays uproc array data.

int
main(void)
{
    // Allocate memory for array of uproc structs.
    uproc * procTable = (uproc*)malloc(MAX*sizeof(uproc));
    int activeProcs = getprocs(MAX, procTable);

    // If activeProcs is -1, that means MAX is smaller than the number of actives processes.
    if(activeProcs == -1){
        free(procTable);
        printf(1, "Raise maximum entries in ps.c to view running processes.\n");
        exit();
    }
    display(activeProcs, procTable);
    free(procTable);
    exit();
}

void display(int activeProcs, uproc * procTable){
    printf(1, "PID\tName\tUID\tGID\tPPID\tPriority\tElapsed\t\tCPU\tState\tSize\n");
    int index = 0;
    int counter = 0;
    while(counter != activeProcs){
        if(procTable[index].pid){
            printf(1, "%d\t%s\t%d\t%d\t%d\t%d\t\t%d.%d\t\t%d.%d\t%s\t%d\n",
                    procTable[index].pid,
                    procTable[index].name,
                    procTable[index].uid,
                    procTable[index].gid,
                    procTable[index].ppid,
                    procTable[index].priority,
                    procTable[index].elapsed_ticks/1000,
                    procTable[index].elapsed_ticks%1000,
                    procTable[index].CPU_total_ticks/1000,
                    procTable[index].CPU_total_ticks%1000,
                    procTable[index].state,
                    procTable[index].size);
            ++counter;
        }
        ++index;
    }
}
#endif




















