#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"
#include "spinlock.h"

int stdout = 1;

int main(int argc, char *argv[])
{
	// in this test program, parent create semaphore (init counter 0) and wait on it
	// child 2 just wait on semaphore, and child1 sleep 100 ticks, call sem_post, and do it again.
	// so we can check if the counter is working fine.
  
 
  int sem=sem_alloc();
  
  printf(stdout, "[parent] semaphore created, with id =  %d\n", sem);
  sem_init(sem, 0);
  
  int pid=fork();
  if(pid==0){
	  int pid2=fork();
	  if(pid2==0){
		  printf(stdout, "[child2] call sem_wait ...\n");
		  sem_wait(sem);
		  printf(stdout, "[child2] wake up !\n");
	  }else{
		  printf(stdout, "[child1] sleeping 100 ticks sec in children.\n");
		  sleep(100);
		  printf(stdout, "[child1] sem_post ... \n");
		  sem_post(sem);
		  
		  printf(stdout, "[child1] sleeping again 100 ticks sec in children.\n");
		  sleep(100);
		  printf(stdout, "[child1] sem_post  again... \n");
		  sem_post(sem);
		  wait();
	  }
  }else{
	    printf(stdout, "[parent] calling sem_wait... \n");
	  sem_wait(sem);
	  printf(stdout, "[parent] wkae up !\n");
	  sem_destroy(sem);
	  wait(); // wait for child to avoid zombie.
  }
  exit();
}
