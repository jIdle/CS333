#define STRMAX 32

typedef struct uproc {
    uint pid;
    uint uid;
    uint gid;
    uint ppid;
    uint elapsed_ticks;
    uint CPU_total_ticks;
    char state[STRMAX];
    uint size;
    char name[STRMAX];
    uint priority;
}uproc;
