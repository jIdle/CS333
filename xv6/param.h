#define NPROC                  64 // maximum number of processes
#define KSTACKSIZE           4096 // size of per-process kernel stack
#define NCPU                    8 // maximum number of CPUs
#define NOFILE                 16 // open files per process
#define NFILE                 100 // open files per system
#define NINODE                 50 // maximum number of active i-nodes
#define NDEV                   10 // maximum major device number
#define ROOTDEV                 1 // device number of file system root disk
#define MAXARG                 32 // max exec arguments
#define MAXOPBLOCKS            10 // max # of blocks any FS op writes
#define LOGSIZE   (MAXOPBLOCKS*3) // max data blocks in on-disk log
#define NBUF      (MAXOPBLOCKS*3) // size of disk block cache
// #define FSSIZE            1000 // size of file system in blocks
#define FSSIZE               2000 // size of file system in blocks  // CS333 requires a larger FS.

#ifdef CS333_P5
#define DEFAULT_UID             0 // default uid for first process and files
#define DEFAULT_GID             0 // default gid for first process and files
#define DEFAULT_MODE         0755 // default mode for files
#elif CS333_P2
#define FUID                    0 // default uid assigned to the first process 
#define FGID                    0 // default gid assigned to the first process
#endif

#ifdef CS333_P3P4
#define NULL            (void *)0 // to set pointers to
#define MAXPRIO                 6 // number of priority levels the ready list will use
#define BUDGET                 50 // number of cpu ticks used by process before demotion
#define TICKS_TO_PROMOTE      100 // number of ticks before processes are promoted
#endif

