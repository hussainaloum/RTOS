// os.c
// Runs on LM4F120/TM4C123/MSP432
// Lab 3 starter file.
// Daniel Valvano
// March 24, 2016

#include <stdint.h>
#include "os.h"
#include "CortexM.h"
#include "BSP.h"

// function definitions in osasm.s
void StartOS(void);

#define NUMTHREADS  6        // maximum number of threads
#define NUMPERIODIC 2        // maximum number of periodic threads
#define STACKSIZE   100      // number of 32-bit words in stack per thread
#define FIFOSUCCESS 0
#define FIFOFULL    -1

struct tcb{
  int32_t *sp;       // pointer to stack (valid for threads not running
  struct tcb *next;  // linked-list pointer
   // nonzero if blocked on this semaphore
   // nonzero if this thread is sleeping
  int32_t *blocked;
  uint32_t sleep;
};
typedef struct tcb tcbType;

struct event {
  uint32_t period;
  void (*func)(void);    // function pointer to event thread function
  uint32_t countdown;
};
typedef struct event eventType;

tcbType tcbs[NUMTHREADS];
tcbType *RunPt;
int32_t Stacks[NUMTHREADS][STACKSIZE];
eventType event_threads[NUMTHREADS];
uint32_t event_thread_count;

void static runperiodicevents(void);

// ******** OS_Init ************
// Initialize operating system, disable interrupts
// Initialize OS controlled I/O: periodic interrupt, bus clock as fast as possible
// Initialize OS global variables
// Inputs:  none
// Outputs: none
void OS_Init(void){
  DisableInterrupts();
  BSP_Clock_InitFastest();// set processor clock to fastest speed
  // perform any initializations needed
  event_thread_count = 0;
  BSP_PeriodicTask_Init(&runperiodicevents, 1000, 6);
}

void SetInitialStack(int i){
  Stacks[i][STACKSIZE-1] = 0x01000000;   // Thumb bit 
  Stacks[i][STACKSIZE-3] = 0x14141414;   // R14 
  Stacks[i][STACKSIZE-4] = 0x12121212;   // R12 
  Stacks[i][STACKSIZE-5] = 0x03030303;   // R3 
  Stacks[i][STACKSIZE-6] = 0x02020202;   // R2 
  Stacks[i][STACKSIZE-7] = 0x01010101;   // R1 
  Stacks[i][STACKSIZE-8] = 0x00000000;   // R0 
  Stacks[i][STACKSIZE-9] = 0x11111111;   // R11 
  Stacks[i][STACKSIZE-10] = 0x10101010;  // R10 
  Stacks[i][STACKSIZE-11] = 0x09090909;  // R9 
  Stacks[i][STACKSIZE-12] = 0x08080808;  // R8 
  Stacks[i][STACKSIZE-13] = 0x07070707;  // R7 
  Stacks[i][STACKSIZE-14] = 0x06060606;  // R6 
  Stacks[i][STACKSIZE-15] = 0x05050505;  // R5 
  Stacks[i][STACKSIZE-16] = 0x04040404;  // R4 
  tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer 
}

//******** OS_AddThreads ***************
// Add six main threads to the scheduler
// Inputs: function pointers to six void/void main threads
// Outputs: 1 if successful, 0 if this thread can not be added
// This function will only be called once, after OS_Init and before OS_Launch
int OS_AddThreads(void(*thread0)(void),
                  void(*thread1)(void),
                  void(*thread2)(void),
                  void(*thread3)(void),
                  void(*thread4)(void),
                  void(*thread5)(void)){
  int32_t sr;

  sr = StartCritical();
  int i, j;
  void(*threads[NUMTHREADS])(void) = {
    thread0, thread1, thread2, thread3, thread4, thread5
  };

  // initialize TCB circular list (same as RTOS project)
  for (i = 0; i < NUMTHREADS; i++) {
    tcbs[i].next = &tcbs[(i+1)%NUMTHREADS];
  }
  // initialize TCB
  for (j = 0; j < NUMTHREADS; j++) {
    SetInitialStack(j);         // initialize stacks
    Stacks[j][STACKSIZE-2] = (int32_t) threads[j]; // initialize PC
    tcbs[j].blocked = 0;        // not blocked
    tcbs[j].sleep = 0;          // not sleeping
  }

  RunPt = &tcbs[0];      // thread 0 is first to run
  EndCritical(sr);

  return 1;               // successful
}

//******** OS_AddPeriodicEventThread ***************
// Add one background periodic event thread
// Typically this function receives the highest priority
// Inputs: pointer to a void/void event thread function
//         period given in units of OS_Launch (Lab 3 this will be msec)
// Outputs: 1 if successful, 0 if this thread cannot be added
// It is assumed that the event threads will run to completion and return
// It is assumed the time to run these event threads is short compared to 1 msec
// These threads cannot spin, block, loop, sleep, or kill
// These threads can call OS_Signal
// In Lab 3 this will be called exactly twice
int OS_AddPeriodicEventThread(void(*thread)(void), uint32_t period){
  uint32_t i;
  int thread_added;

  thread_added = 0;
  if (event_thread_count < NUMTHREADS && period > 0) {
    i = event_thread_count;
    event_threads[i].func = thread;
    event_threads[i].period = period;
    event_threads[i].countdown = period;
    event_thread_count++;
    thread_added = 1;
  }

  return thread_added;
}

void static decrement_sleep_timer(void) {
  int i;
  for (i = 0; i < NUMTHREADS; i++) {
    if (tcbs[i].sleep)
      tcbs[i].sleep--;
  }
}

void static execute_event_threads(void) {
  int i;
  for (i = 0; i < event_thread_count; i++) {
    event_threads[i].countdown--;
    if (event_threads[i].countdown == 0) {
      event_threads[i].func();
      event_threads[i].countdown = event_threads[i].period;
    }
  }
}

void static runperiodicevents(void){
  decrement_sleep_timer(); 
  execute_event_threads();
}

//******** OS_Launch ***************
// Start the scheduler, enable interrupts
// Inputs: number of clock cycles for each time slice
// Outputs: none (does not return)
// Errors: theTimeSlice must be less than 16,777,216
void OS_Launch(uint32_t theTimeSlice){
  STCTRL = 0;                  // disable SysTick during setup
  STCURRENT = 0;               // any write to current clears it
  SYSPRI3 =(SYSPRI3&0x00FFFFFF)|0xE0000000; // priority 7
  STRELOAD = theTimeSlice - 1; // reload value
  STCTRL = 0x00000007;         // enable, core clock and interrupt arm
  StartOS();                   // start on the first task
}
// runs every ms
void Scheduler(void){ // every time slice
// ROUND ROBIN, skip blocked and sleeping threads
  RunPt = RunPt->next; 
  while (RunPt->blocked || RunPt->sleep) {
    RunPt = RunPt->next;
  }
}

//******** OS_Suspend ***************
// Called by main thread to cooperatively suspend operation
// Inputs: none
// Outputs: none
// Will be run again depending on sleep/block status
void OS_Suspend(void){
  STCURRENT = 0;        // any write to current clears it
  INTCTRL = 0x04000000; // trigger SysTick
// next thread gets a full time slice
}

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
  RunPt->sleep = sleepTime;
  OS_Suspend();
}

// ******** OS_InitSemaphore ************
// Initialize counting semaphore
// Inputs:  pointer to a semaphore
//          initial value of semaphore
// Outputs: none
void OS_InitSemaphore(int32_t *semaPt, int32_t value){
  *semaPt = value;
}

// ******** OS_Wait ************
// Decrement semaphore and block if less than zero
// Lab2 spinlock (does not suspend while spinning)
// Lab3 block if less than zero
// Inputs:  pointer to a counting semaphore
// Outputs: none
void OS_Wait(int32_t *semaPt){
  uint32_t sr;

  sr = StartCritical();
  (*semaPt)--;
  if (*semaPt < 0) {    // block current thread
    RunPt->blocked = semaPt;
    EndCritical(sr);
    OS_Suspend();
    return;             // OS_Suspend() has triggered SysTick 
  }
  EndCritical(sr);
}

// ******** OS_Signal ************
// Increment semaphore
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate
// Inputs:  pointer to a counting semaphore
// Outputs: none
void OS_Signal(int32_t *semaPt){
  uint32_t sr;
  tcbType *waitPt;

  sr = StartCritical();
  (*semaPt)++;
  if (*semaPt <= 0) {   // unblock a waiting thread
    waitPt = RunPt->next;
    while (waitPt->blocked != semaPt) {
      waitPt = waitPt->next;
    }
    waitPt->blocked = 0;
  }
  EndCritical(sr);
}

#define FSIZE 10    // can be any size
uint32_t PutI;      // index of where to put next
uint32_t GetI;      // index of where to get next
uint32_t Fifo[FSIZE];
int32_t CurrentSize;// 0 means FIFO empty, FSIZE means full
uint32_t LostData;  // number of lost pieces of data

// ******** OS_FIFO_Init ************
// Initialize FIFO.  
// One event thread producer, one main thread consumer
// Inputs:  none
// Outputs: none
void OS_FIFO_Init(void){
  OS_InitSemaphore(&CurrentSize, 0);
  PutI = 0;
  GetI = 0;
  LostData = 0;
}

// ******** OS_FIFO_Put ************
// Put an entry in the FIFO.  
// Exactly one event thread puts,
// do not block or spin if full
// Inputs:  data to be stored
// Outputs: 0 if successful, -1 if the FIFO is full
int OS_FIFO_Put(uint32_t data){
  int status;

  if (CurrentSize == FSIZE) {
    LostData++;
    status = FIFOFULL;
  } else {
    Fifo[PutI] = data;
    PutI = (PutI + 1) % FSIZE;
    OS_Signal(&CurrentSize);
    status = FIFOSUCCESS;
  }

  return status;
}

// ******** OS_FIFO_Get ************
// Get an entry from the FIFO.   
// Exactly one main thread get,
// do block if empty
// Inputs:  none
// Outputs: data retrieved
uint32_t OS_FIFO_Get(void){uint32_t data;
  OS_Wait(&CurrentSize);    // block if empty
  data = Fifo[GetI];
  GetI = (GetI + 1) % FSIZE;

  return data;
}



