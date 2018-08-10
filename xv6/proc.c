#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "uproc.h"

#define NULL (void *)0

#ifdef CS333_P3P4
#define MAXPRIO 2
#define TICKS_TO_PROMOTE 5000
struct state_lists {
  struct proc * free;
  struct proc * free_tail;
  struct proc * embryo;
  struct proc * embryo_tail;
  struct proc * sleep;
  struct proc * sleep_tail;
  //struct proc * ready;
  //struct proc * ready_tail;
  struct proc * running;
  struct proc * running_tail;
  struct proc * zombie;
  struct proc * zombie_tail;
  struct proc * ready[MAXPRIO+1]; 
  struct proc * ready_tail[MAXPRIO+1];
} state_lists;
#endif

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
#ifdef CS333_P3P4
  struct state_lists pLists;
  uint PromoteAtTime;
#endif
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void *chan);

#ifdef CS333_P3P4
static int stateListVerify(int rmState, int insState, struct proc * toCheck);
static void initProcessLists(void);
static void initFreeList(void);
// __attribute__ ((unused)) suppresses warnings for routines that are not
// currently used but will be used later in the project. This is a necessary
// side-effect of using -Werror in the Makefile.
static void __attribute__ ((unused)) stateListAdd(struct proc** head, struct proc** tail, struct proc* p);
static void __attribute__ ((unused)) stateListAddAtHead(struct proc** head, struct proc** tail, struct proc* p);
static int __attribute__ ((unused)) stateListRemove(struct proc** head, struct proc** tail, struct proc* p);
#endif

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
#ifdef CS333_P3P4
  p = ptable.pLists.free;
  if(p)
      goto found;
#else
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
#endif
  release(&ptable.lock);
  return 0;

found:
#ifdef CS333_P3P4
  if(stateListRemove(&ptable.pLists.free, &ptable.pLists.free_tail, p) == -1)
    cprintf("\nstateListRemove() returned -1 in allocproc()\n");
  stateListAdd(&ptable.pLists.embryo, &ptable.pLists.embryo_tail, p);
  int from = p->state;
#endif
  p->state = EMBRYO;
#ifdef CS333_P3P4
  int to = p->state;
  if(!stateListVerify(from, to, p))
    panic("allocproc: state transition failure");
#endif
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

#ifdef CS333_P1
  p->start_ticks = ticks;
#endif
#ifdef CS333_P2
  p->cpu_ticks_in = 0;
  p->cpu_ticks_total = 0;
#endif
#ifdef CS333_P3P4
  p->priority = MAXPRIO;
#endif
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
#ifdef CS333_P3P4
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
#endif
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");
#ifdef CS333_P3P4
  acquire(&ptable.lock);
  if(stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryo_tail, p) == -1)
    cprintf("\nstateRemoveList() returned -1 in userinit()\n");
  stateListAdd(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p);
  int from = p->state;
#endif
  p->state = RUNNABLE;
#ifdef CS333_P3P4
  int to = p->state;
  if(!stateListVerify(from, to, p))
    panic("userinit: state transition failure");
  release(&ptable.lock);
#endif

#ifdef CS333_P2
  p->parent = NULL;
  p->uid = FUID;
  p->gid = FGID;
#endif
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;
#ifdef CS333_P2
  np->uid = proc->uid;
  np->gid = proc->gid;
#endif

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
#ifdef CS333_P3P4
  if(stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryo_tail, np) == -1)
    cprintf("\nstateListRemove() returned -1 in fork()\n");
  stateListAdd(&ptable.pLists.ready[np->priority], &ptable.pLists.ready_tail[np->priority], np);
  int from = np->state;
#endif
  np->state = RUNNABLE;
#ifdef CS333_P3P4
  int to = np->state;
  if(!stateListVerify(from, to, np))
    panic("fork: state transition failure");
#endif
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifndef CS333_P3P4
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#else
void
exit(void)
{
    struct proc *p;
    int fd;

    if(proc == initproc)
        panic("init exiting");

    // Close all open files.
    for(fd = 0; fd < NOFILE; fd++){
        if(proc->ofile[fd]){
            fileclose(proc->ofile[fd]);
            proc->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(proc->cwd);
    end_op();
    proc->cwd = 0;
    acquire(&ptable.lock);

    // Parent might be sleeping in wait().
    wakeup1(proc->parent);

    // Pass abandoned children to init. 
    for(p = ptable.pLists.embryo; p != NULL; p = p->next){
        if(p->parent == proc)
            p->parent = initproc;
    }
    for(p = ptable.pLists.sleep; p != NULL; p = p->next){
        if(p->parent == proc)
            p->parent = initproc;
    }
    for(int i = MAXPRIO; i >= 0; --i){
        for(p = ptable.pLists.ready[i]; p != NULL; p = p->next){
            if(p->parent == proc)
                p->parent = initproc;
        }
    }
    for(p = ptable.pLists.running; p != NULL; p = p->next){
        if(p->parent == proc)
            p->parent = initproc;
    }
    for(p = ptable.pLists.zombie; p != NULL; p = p->next){
        if(p->parent == proc){
            p->parent = initproc;
            wakeup1(initproc);
        }
    }

    // Jump into the scheduler, never to return.
    if(stateListRemove(&ptable.pLists.running, &ptable.pLists.running_tail, proc) == -1)
        cprintf("\nstateListRemove() has returned -1 in exit()\n");
    stateListAdd(&ptable.pLists.zombie, &ptable.pLists.zombie_tail, proc);
    int from = proc->state;
    proc->state = ZOMBIE;
    int to = proc->state;
    if(!stateListVerify(from, to, proc))
        panic("exit: state transition failure");
    sched();
    panic("zombie exit");
}
#endif

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifndef CS333_P3P4
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#else
int
wait(void)
{
    struct proc *p;
    int onGoing = 0;
    int pid = 0;

    acquire(&ptable.lock);
    for(;;){
        for(p = ptable.pLists.embryo; p != NULL && onGoing == 0; p = p->next){ // These first 4 loops are checking for children processes that AREN'T zombies. 
            if(p->parent == proc)
                onGoing = 1;
        }
        for(p = ptable.pLists.sleep; p != NULL && onGoing == 0; p = p->next){   
            if(p->parent == proc)
                onGoing = 1;
        }
        for(p = ptable.pLists.running; p != NULL && onGoing == 0; p = p->next){ 
            if(p->parent == proc)
                onGoing = 1;
        }
        for(int i = MAXPRIO; i >= 0; --i){
            for(p = ptable.pLists.ready[i]; p != NULL && onGoing == 0; p = p->next){   
                if(p->parent == proc)
                    onGoing = 1;
            }
        }
        for(p = ptable.pLists.zombie; p != NULL; p = p->next){  // Now, if proc DOES have any children, we know they will be in here. Otherwise, it has no children.
            if(p->parent != proc)
                continue;
            // Found one.
            pid = p->pid;
            kfree(p->kstack);
            p->kstack = 0;
            freevm(p->pgdir);
            if(stateListRemove(&ptable.pLists.zombie, &ptable.pLists.zombie_tail, p) == -1)
                cprintf("\nstateListRemove() returned -1 in wait()\n");
            stateListAdd(&ptable.pLists.free, &ptable.pLists.free_tail, p);
            int from = p->state;
            p->state = UNUSED;
            int to = p->state;
            if(!stateListVerify(from, to, p))
                panic("wait: state transition failure");
            p->pid = 0;
            p->parent = 0;
            p->name[0] = 0;
            p->killed = 0;
            release(&ptable.lock);
            return pid;
        }

        if(onGoing == 0 || proc->killed){ // This check means we have found no children in any of the process lists.
          release(&ptable.lock);
          return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(proc, &ptable.lock);  //DOC: wait-sleep
    }
}
#endif

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifndef CS333_P3P4
// original xv6 scheduler. Use if CS333_P3P4 NOT defined.
void
scheduler(void)
{
  struct proc *p;
  int idle;  // for checking if processor is idle

  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p;
      switchuvm(p);
      p->state = RUNNING;

#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif

      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }
}

#else
void
scheduler(void)
{
    struct proc *p;
    int idle;  // for checking if processor is idle

    for(;;){
     // Enable interrupts on this processor.
        sti();
        idle = 1;  // assume idle unless we schedule a process
        acquire(&ptable.lock);

        // Iterates through ready list queues.
        // Only iterates through lower queues if there is nothing in the highest queue.
        for(int i = MAXPRIO; i >= 0; --i){
            for(p = ptable.pLists.ready[i]; p != NULL; p = p->next){
                idle = 0;
                proc = p;
                switchuvm(p);

                if(stateListRemove(&ptable.pLists.ready[i], &ptable.pLists.ready_tail[i], p) == -1)
                    cprintf("\nstateListRemove() returned -1 in scheduler()\n");
                stateListAdd(&ptable.pLists.running, &ptable.pLists.running_tail, p);
                int from = p->state;
                p->state = RUNNING;
                int to = p->state;
                if(!stateListVerify(from, to, p))
                    panic("scheduler: state transition failure");
                p->cpu_ticks_in = ticks;

                swtch(&cpu->scheduler, proc->context);
                switchkvm();
                proc = NULL;

                // If there is a process in the highest priority and we are currently searching in a lower priority.
                if(ptable.pLists.ready[MAXPRIO] && i != MAXPRIO)
                    i = MAXPRIO+1;
                    break;
            }
        }
        release(&ptable.lock);

        // if idle, wait for next interrupt
        if (idle){
            sti();
            hlt();
        }
    }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
#ifdef CS333_P2
  proc->cpu_ticks_total += (ticks - proc->cpu_ticks_in);
#endif
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
#ifdef CS333_P3P4
  if(stateListRemove(&ptable.pLists.running, &ptable.pLists.running_tail, proc) == -1)
    cprintf("\nstateListRemove() returned -1 in yield()\n");
  stateListAdd(&ptable.pLists.ready[proc->priority], &ptable.pLists.ready_tail[proc->priority], proc);
  int from = proc->state;
#endif
  proc->state = RUNNABLE;
#ifdef CS333_P3P4
  int to = proc->state;
  if(!stateListVerify(from, to, proc))
    panic("yield: state transition failure");
#endif
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// 2016/12/28: ticklock removed from xv6. sleep() changed to
// accept a NULL lock to accommodate.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){
    acquire(&ptable.lock);
    if (lk) release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
#ifdef CS333_P3P4
  if(stateListRemove(&ptable.pLists.running, &ptable.pLists.running_tail, proc) == -1)
    cprintf("\nstateListRemove() returned -1 in sleep()\n");
  stateListAdd(&ptable.pLists.sleep, &ptable.pLists.sleep_tail, proc);
  int from = proc->state;
#endif
  proc->state = SLEEPING;
#ifdef CS333_P3P4
  int to = proc->state;
  if(!stateListVerify(from, to, proc))
    panic("sleep: state transition failure");
#endif
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

//PAGEBREAK!
#ifndef CS333_P3P4
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#else
static void
wakeup1(void *chan)
{
    struct proc *p;

    for(p = ptable.pLists.sleep; p != NULL; p = p->next){
        if(p->chan == chan){
            if(stateListRemove(&ptable.pLists.sleep, &ptable.pLists.sleep_tail, p) == -1)
                cprintf("\nstateListRemove() returned -1 in wakeup1()\n");
            stateListAdd(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p);
            int from = p->state;
            p->state = RUNNABLE;
            int to = p->state;
            if(!stateListVerify(from, to, p))
                panic("wakeup1: state transition failure");
        }
    }
}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifndef CS333_P3P4
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
    struct proc *p;

    acquire(&ptable.lock);
    for(p = ptable.pLists.embryo; p != NULL; p = p->next){
        if(p->pid == pid){
            p->killed = 1;
            release(&ptable.lock);
            return 0;
        }
    }
    for(p = ptable.pLists.running; p != NULL; p = p->next){
        if(p->pid == pid){
            p->killed = 1;
            release(&ptable.lock);
            return 0;
        }
    }
    for(int i = MAXPRIO; i >= 0; --i){
        for(p = ptable.pLists.ready[i]; p != NULL; p = p->next){
            if(p->pid == pid){
                p->killed = 1;
                release(&ptable.lock);
                return 0;
            }
        }
    }
    for(p = ptable.pLists.zombie; p != NULL; p = p->next){
        if(p->pid == pid){
            p->killed = 1;
            release(&ptable.lock);
            return 0;
        }
    }
    for(p = ptable.pLists.sleep; p != NULL; p = p->next){
        if(p->pid == pid){
            p->killed = 1;
            if(stateListRemove(&ptable.pLists.sleep, &ptable.pLists.sleep_tail, p) == -1)
                cprintf("\nstateListRemove() returned -1 in kill()\n");
            // p is placed on high priority list to process "killed" flag faster.
            stateListAdd(&ptable.pLists.ready[MAXPRIO], &ptable.pLists.ready_tail[MAXPRIO], p);
            p->priority = MAXPRIO;
            int from = p->state;
            p->state = RUNNABLE;
            int to = p->state;
            if(!stateListVerify(from, to, p))
                panic("kill: state transition failure");
            release(&ptable.lock);
            return 0;
        }
    }
    release(&ptable.lock);
    return -1;
}
#endif

static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep",
  [RUNNING]   "running",
  [ZOMBIE]    "zombie",
  [RUNNABLE]  "runnable"
};

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
#ifdef CS333_P1
  cprintf("\nPID\tName\tUID\tGID\tPPID\tElapsed\t\tCPU\tState\tSize\tPCs");
#endif
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
#ifndef CS333_P1
    cprintf("%d %s %s", p->pid, state, p->name);
#endif
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
#ifdef CS333_P1
      int dispTime = ticks - p->start_ticks;
      cprintf("\n%d\t%s\t%d\t%d\t%d\t%d.%d\t\t%d.%d\t%s\t%d\t", 
              p->pid, 
              p->name, 
              p->uid, 
              p->gid, 
              h_getppid(p), 
              dispTime/1000, 
              dispTime%1000,
              p->cpu_ticks_total/1000,
              p->cpu_ticks_total%1000,
              state,
              p->sz);
      for(i=0; i<10 && pc[i] != 0; i++){
        cprintf("%p ", pc[i]);
      }
#else
      for(i=0; i<10 && pc[i] != 0; i++){
        cprintf(" %p ", pc[i]);
      }
#endif
    }
#ifndef CS333_P1
    cprintf("\n");
#endif
  }
  cprintf("\n\n");
}

#ifdef CS333_P3P4
void
displayReady(void)
{
    cprintf("\nReady List Processes:\n");
    int first = 1;
    struct proc * current = NULL;
    acquire(&ptable.lock);
    for(int i = MAXPRIO; i >= 0; --i){
        for(current = ptable.pLists.ready[i]; current != NULL; current = current->next) {
            if(first){
                --first;
                cprintf("%d", current->pid);
                continue;
            }
            cprintf(" -> %d", current->pid);
        }
    }
    release(&ptable.lock);
    cprintf("\n");
}

void
displayFree(void)
{
    int counter = 0;
    struct proc * current = NULL;
    acquire(&ptable.lock);
    for(current = ptable.pLists.free; current != NULL; current = current->next) {
        ++counter;
    }
    release(&ptable.lock);
    cprintf("\nFree List Size: %d processes\n", counter);
}

void
displaySleep(void)
{
    cprintf("\nSleep List Processes:\n");
    int first = 1;
    struct proc * current = NULL;
    acquire(&ptable.lock);
    for(current = ptable.pLists.sleep; current != NULL; current = current->next) {
        if(first){
            --first;
            cprintf("%d", current->pid);
            continue;
        }
        cprintf(" -> %d", current->pid);
    }
    release(&ptable.lock);
    cprintf("\n");
}

void
displayZombie(void)
{
    cprintf("\nZombie List Processes:\n");
    int first = 1;
    struct proc * current = NULL;
    acquire(&ptable.lock);
    for(current = ptable.pLists.zombie; current != NULL; current = current->next) {
        if(first){
            --first;
            cprintf("(%d, %d)", current->pid, h_getppid(current));
            continue;
        }
        cprintf(" -> (%d, %d)", current->pid, h_getppid(current));
    }
    release(&ptable.lock);
    cprintf("\n");
}
#endif

#ifdef CS333_P2
int h_getppid(struct proc * p)
{
  if(!p->parent)
    return p->pid;
  if(p->parent->pid < 0 || p->parent->pid > 32767)
    return -1;
  return p->parent->pid;
}

int
copyprocs(int max, uproc * procTable)
{
  acquire(&ptable.lock);
  int activeProcs = 0;
  struct proc * pPtr = ptable.proc;

  // This loops copies data from the ptable processes my uproc structs.
  for(int i = 0; i < NPROC; ++i){
    if(i >= max){
      cprintf("\nProcess cap lower than ptable capacity. There may be processes missing in display.\n");
      break;
    }
    procTable[i].pid              = pPtr[i].pid;
    procTable[i].uid              = pPtr[i].uid;
    procTable[i].gid              = pPtr[i].gid;
    if(!pPtr[i].parent)
      procTable[i].ppid           = pPtr[i].pid;
    else
      procTable[i].ppid           = pPtr[i].parent->pid;
    procTable[i].elapsed_ticks    = ticks - pPtr[i].start_ticks;
    procTable[i].CPU_total_ticks  = pPtr[i].cpu_ticks_total;
    procTable[i].size             = pPtr[i].sz;

    char * temp = states[pPtr[i].state];
    for(int j = 0; j < strlen(temp); ++j)
      procTable[i].state[j] = temp[j];
    for(int j = 0; j < strlen(pPtr[i].name); ++j)
      procTable[i].name[j] = pPtr[i].name[j];

    if(strncmp(temp, "unused", 9) != 0 && strncmp(temp, "embryo", 9) != 0)
      ++activeProcs;
  }
  release(&ptable.lock);
  return activeProcs;
}
#endif

#ifdef CS333_P3P4

// stateListVerify will make sure that any process ACTUALLY has been removed from one list, and placed
// into another. It does this by taking as arguments: the int form of the state list the process was
// removed from, the int form of the state list the process was added to, and the process itself 
// which is used for comparison in both of those lists. The "int form" that I'm mentioning is used
// by accessing the pLists struct by address (I think this is pointer arithmetic?) so I can 
// abstract the process to access any one state list. Otherwise I would have to manually check each state 
// list for confirmation of removal and insertion.
//
// EDIT: stateListVerify has been changed in P4 to also check whether ready list processes are in
// the correct priority list.
static int
stateListVerify(int rmState, int insState, struct proc * toCheck)
{
    struct proc ** startAddr = (struct proc**)(&ptable.pLists);
    struct proc * current = NULL;
    rmState = 2*rmState;    // Multiplying by two to skip tail pointers in state list.
    insState = 2*insState;
    // Checking the list the process was removed from.
    if(rmState == 10){ // If we removed the process from the ready list.
        for(int i = MAXPRIO; i >= 0; --i){
            for(current = (startAddr + rmState)[i]; current != NULL; current = current->next){
                if(toCheck == current)
                    return 0;
            }
        }
    }
    else{
        for(current = *(startAddr + rmState); current != NULL; current = current->next){
            if(toCheck == current)
                return 0;
        }
    }
    // Checking the list the process was moved to.
    if(insState == 10){ // If we inserted the process into the ready list.
        for(int i = MAXPRIO; i >= 0; --i){
            for(current = (startAddr + insState)[i]; current != NULL; current = current->next){
                if(toCheck == current){
                    if(toCheck->priority != i)
                        return 0;   // Process priority does not match the priority list it is in.
                    return 1;
                }
            }
        }
    }
    else{
        for(current = *(startAddr + insState); current != NULL; current = current->next){
            if(toCheck == current)
                return 1;
        }
    }
    return 0;
}

static void
stateListAdd(struct proc** head, struct proc** tail, struct proc* p)
{
  if(*head == 0){
    *head = p;
    *tail = p;
    p->next = 0;
  } else{
    (*tail)->next = p;
    *tail = (*tail)->next;
    (*tail)->next = 0;
  }
}

static void
stateListAddAtHead(struct proc** head, struct proc** tail, struct proc* p)
{
  if(*head == 0){
    *head = p;
    *tail = p;
    p->next = 0;
  } else {
    p->next = *head;
    *head = p;
  }
}

static int
stateListRemove(struct proc** head, struct proc** tail, struct proc* p)
{
  if(*head == 0 || *tail == 0 || p == 0){
    if(*head == 0)
      cprintf("\nHead is NULL.\n");
    if(*tail == 0)
      cprintf("\nTail is NULL.\n");
    if(p == 0)
      cprintf("\nProcess is NULL.\n");
    return -1;
  }

  struct proc* current = *head;
  struct proc* previous = 0;

  if(current == p){
    *head = (*head)->next;
    // prevent tail remaining assigned when we've removed the only item
    // on the list
    if(*tail == p){
      *tail = 0;
    }
    return 0;
  }

  while(current){
    if(current == p){
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found, hit eject.
  if(current == 0){
    cprintf("\nThe process wasn't found.\n");
    return -1;
  }

  // Process found. Set the appropriate next pointer.
  if(current == *tail){
    *tail = previous;
    (*tail)->next = 0;
  } else{
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = 0;

  return 0;
}

static void
initProcessLists(void)
{ 
  for(int i = 0; i < MAXPRIO+1; ++i){
     ptable.pLists.ready[i] = NULL;
     ptable.pLists.ready_tail[i] = NULL;
  }  
  //ptable.pLists.ready = NULL;
  //ptable.pLists.ready_tail = NULL;
  ptable.pLists.free = NULL;
  ptable.pLists.free_tail = NULL;
  ptable.pLists.sleep = NULL;
  ptable.pLists.sleep_tail = NULL;
  ptable.pLists.zombie = NULL;
  ptable.pLists.zombie_tail = NULL;
  ptable.pLists.running = NULL;
  ptable.pLists.running_tail = NULL;
  ptable.pLists.embryo = NULL;
  ptable.pLists.embryo_tail = NULL;
}

static void
initFreeList(void)
{
  if(!holding(&ptable.lock)){
    panic("acquire the ptable lock before calling initFreeList\n");
  }

  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.pLists.free, &ptable.pLists.free_tail, p);
  }
}
#endif
