//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "file.h"
#include "spinlock.h"

typedef struct {
	struct spinlock lock;
	int state; // -1 => non allocated, 0 => allocated but not intiated, 1 mean active
	int counter;
} semaphore;

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;



semaphore semaphores[SEM_NMAX];

int sem_alloc(){
	// look for an available semaphore, return -1 if no slot available in semaphores.

	for(int i=0; i<SEM_NMAX; i++){
		acquire(&semaphores[i].lock);
		if(semaphores[i].state==-1){
			semaphores[i].state=1;
			release(&semaphores[i].lock);
			return i; // slot i is available, returning it.
		}
		release(&semaphores[i].lock);
	}
	return -1; // no slot available ... 
}

void sem_init(int sem_id, int c){
	acquire(&semaphores[sem_id].lock); // avoid concurrent init of same semaphore.
	if(semaphores[sem_id].state==0){ 
		semaphores[sem_id].counter=c;
		semaphores[sem_id].state=1;
		release(&semaphores[sem_id].lock);
	}else{ // unallocated or already active.
		release(&semaphores[sem_id].lock);
		//return -1; in fact the given question expect a void return ... so i don't notice if there is a error or not.
		return;
	}
}

void sem_wait(int sem_id){
	acquire(&semaphores[sem_id].lock); // this lock is very important (a lot of thread can be waiting and ready to rush this slot).
	while(semaphores[sem_id].counter<=0){ // while there is no available slot, sleep and recheck
		sleep(&semaphores[sem_id], &semaphores[sem_id].lock); // sleep will sleep the process and release the lock, and when wakeup the lock will be re-acquired
	}
	// here we are allowed to continue (and decrement the counter of course).
	semaphores[sem_id].counter--;
	release(&semaphores[sem_id].lock);
}

void sem_post(int sem_id){
	acquire(&semaphores[sem_id].lock);
	semaphores[sem_id].counter++; // increment the counter
	release(&semaphores[sem_id].lock);
	wakeup(&semaphores[sem_id]);
}

void sem_destroy(int sem_id){
	acquire(&semaphores[sem_id].lock);
	if(semaphores[sem_id].state==1){ 
		semaphores[sem_id].state=-1; // release the slot 
		release(&semaphores[sem_id].lock);
	}
}

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
  
  
  // init semaphores
  for(int i=0; i<SEM_NMAX; i++){
	  semaphores[i].state=-1; // NULL means that the slot is available.
	  
	  initlock(&semaphores[i].lock, ""); // init the spinlock ...
  }
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);
  
  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_trans();
    iput(ff.ip);
    commit_trans();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((LOGSIZE-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_trans();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      commit_trans();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}

