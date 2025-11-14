#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>
#include <linux/k22info.h>

#define __NR_k22tree 467   /* syscall number for k22tree */

/*
 * Find where in the stack the parent PID lives.
 * Returns index in [0..top] or -1 if not found.
 */
static int find_parent(pid_t *stack, int top, pid_t parent_pid)
{
    int i;

    for (i = top; i >= 0; i--) {
        if (stack[i] == parent_pid)
            return i;
    }
    return -1;
}

int main(void)
{
    int ret;
    int ne = 100;                 /* initial number of entries */
    struct k22info *buf = NULL;   /* buffer for process info */

    /* 1) Grow the userspace buffer until everything fits */
    for (;;) {
        int ne_temp;

        buf = malloc((size_t)ne * sizeof(*buf));
        if (!buf) {
            perror("malloc");
            return 1;
        }

        ne_temp = ne;
        ret = syscall(__NR_k22tree, buf, &ne_temp);
        if (ret < 0) {
            perror("syscall k22tree");
            free(buf);
            return 1;
        }

        printf("- user-space buffer size: %d\n", ne);
        printf("- syscall return value: %d\n", ret);

        if (ret <= ne) {
            printf("--- OK ---\n\n");
            break;          /* everything fits */
        }

        free(buf);
        ne *= 2;            /* not enough, double and retry */
    }

    /* 2) Build a stack of PIDs in order to print depth as dashes */
    {
        pid_t *stack;
        int top = 0;        /* index of the current “last” element in stack */
        int i;

        stack = malloc((size_t)ne * sizeof(*stack));
        if (!stack) {
            perror("malloc stack");
            free(buf);
            return 1;
        }

        /* root process (should be swapper/0) */
        stack[top] = buf[0].pid;

        printf("#comm, pid, ppid, fcldpid, nsblpid, nvcsw, nivcsw, stime\n");

        /* print root (no leading dashes) */
        printf("%s, %d, %d, %d, %d, %lu, %lu, %lu\n",
               buf[0].comm, buf[0].pid, buf[0].parent_pid,
               buf[0].first_child_pid, buf[0].next_sibling_pid,
               buf[0].nvcsw, buf[0].nivcsw, buf[0].start_time);

        /* print the rest */
        for (i = 1; i < ret; i++) {
            struct k22info *p = &buf[i];
            int p_pos = find_parent(stack, top, p->parent_pid);
            int j;

            /*
             * If parent is found in the stack:
             *   - everything after the parent is “below” it in the tree
             *   - so we keep stack[0..p_pos] and push this PID on top
             * If not found (should be rare if DFS is correct), treat as
             * new “root-level” process.
             */
            if (p_pos >= 0) {
                top = p_pos + 1;
            } else {
                top = 0;
            }

            stack[top] = p->pid;

            /* depth = top, print that many dashes */
            for (j = 0; j < top; j++)
                putchar('-');

            printf("%s, %d, %d, %d, %d, %lu, %lu, %lu\n",
                   p->comm, p->pid, p->parent_pid,
                   p->first_child_pid, p->next_sibling_pid,
                   p->nvcsw, p->nivcsw, p->start_time);
        }

        free(stack);
    }

    free(buf);
    return 0;
}
