#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <linux/k22info.h>

int main(void) {
    int cap = 64;
    struct k22info *buf = NULL;

    for (;;) {
        buf = malloc((size_t)cap * sizeof(*buf));
        if (!buf) { perror("malloc"); return 1; }

        int ne = cap;
        long total = syscall(467, buf, &ne);
        if (total == -1) {          // libc: -1 on error, sets errno
            perror("k22tree");
            free(buf);
            return 1;
        }

        if (total > cap) {          //if it does not fit -> double it
            free(buf);
            cap *= 2;
            continue;
        }

        // Everything fits
        printf("- User-space buf. size: %d\n", cap);
        printf("- syscall return val:   %ld\n", total);
        printf("--- OK ---\n\n");

        printf("#comm,pid,ppid,fcldpid,nsblpid,nvcsw,nivcsw,stime\n");
        for (int i = 0; i < total; i++) {
            struct k22info *p = &buf[i];
            printf("%s,%d,%d,%d,%d,%lu,%lu,%lu\n",
                   p->comm, p->pid, p->parent_pid,
                   p->first_child_pid, p->next_sibling_pid,
                   p->nvcsw, p->nivcsw, p->start_time);
        }
        free(buf);
        break;
    }
    return 0;
}

