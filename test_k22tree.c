#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>
#include <linux/k22info.h>

#define __NR_k22tree 467 //k22 syscall number

static int find_parent(pid_t *stack, int top, pid_t parent_pid);

int main(void){
  int ret;
  int ne = 100;   //starting number of entries
  struct k22info *buf = NULL; //buffer to hold process information
  
  while (1){
        buf = malloc(ne * sizeof(struct k22info));
        if (!buf){
          perror("malloc");
          return 1;
        }
        
        int ne_temp = ne;
        ret = syscall(__NR_k22tree, buf, &ne_temp);
        
        if (ret < 0){
          perror("syscall k22tree");
          free(buf);
          return 1;
        }
        
        printf("- user-space buffer size: %d\n", ne);
        printf("- syscall return value: %d\n", ret);
        
        if(ret <= ne){
          printf("--- OK ---\n\n");
          break;
        }
        
        free(buf);
        ne *= 2; //buffer size isn't enough to fit all entries, double it and retry 

  }
  
  pid_t *stack = malloc(ne * sizeof(pid_t));
  if(!stack){
    perror("malloc stack");
    free(buf);
    return 1;
  }
  
  int top = 0;
  stack[top] = buf[0].pid; //root process
  
  printf("#comm, pid, ppid, fcldpid, nsblpid, nvcsw, nivcsw, stime\n");
   
  //printing root separately in case there are no children
  printf("%s, %d, %d, %d, %d, %lu, %lu, %lu\n",buf[0].comm, buf[0].pid, buf[0].parent_pid, buf[0].first_child_pid, buf[0].next_sibling_pid, buf[0].nvcsw, buf[0].nivcsw, buf[0].start_time);
  
  //printing the rest of processes if there are any
  for (int i = 1; i < ret; i++){
    struct k22info *p = &buf[i];
    int p_pos = find_parent(stack, top, p->parent_pid);
    
    //adjusting stack and printing depth
    top = p_pos + 1;
    stack[top] = p->pid;
    
    for(int j = 0; j < top; j++){
      printf("-");
    }
    printf("%s, %d, %d, %d, %d, %lu, %lu, %lu\n", p->comm, p->pid, p->parent_pid, p->first_child_pid, p->next_sibling_pid, p->nvcsw, p->nivcsw, p->start_time );
  }
  
  free(stack);
  free(buf);
  return 0;
}

//helper to find where parent is in stack
static int find_parent(pid_t *stack, int top, pid_t parent_pid){
  for(int i = top; i >= 0; i++){
    if(stack[i] == parent_pid){
      return i;
    }
  }
  return -1;
}

