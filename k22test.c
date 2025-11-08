#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/k22info.h>

int main(void){
    struct k22info *buf = malloc(sizeof(struct k22info));
    int ne = 1;
    long ret = syscall(467, buf, &ne);
    printf("syscall ret=%ld, ne=%d\n", ret, ne);
    free(buf);
    return 0;
}
