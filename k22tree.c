#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>
#include <linux/k22info.h>
#include <linux/ktime.h> // Για να δουλέψουμε με χρόνο σε nanoseconds

/*
 * Αυτή η συνάρτηση κάνει το "περπάτημα" του process tree.
 * Δηλαδή, πάει από την αρχή (root) και πάει σε όλα τα παιδιά και τα αδέρφια τους
 * με DFS σειρά.
 */
static void fill_k22info(struct task_struct *task,
                         struct k22info __user *buf,
                         int *count,
                         int max_count)
{
    struct task_struct *child;
    struct task_struct *next_sibling;
    struct k22info info;

    // Αν γεμίσαμε το buffer, δεν συνεχίζουμε
    if (*count >= max_count)
        return;

    // Παίρνουμε μόνο τον "ηγέτη" της process (τον κύριο thread)
    task = thread_group_leader(task);

    // Καθαρίζουμε το struct πριν το γεμίσουμε
    memset(&info, 0, sizeof(info));

    // Βάζουμε το όνομα του προγράμματος
    strlcpy(info.comm, task->comm, sizeof(info.comm));

    // PID της process και του γονέα
    info.pid = task_pid_nr(task);
    info.parent_pid = task_pid_nr(task->real_parent);

    // Πόσες φορές έγινε voluntary και involuntary context switch
    info.nvcsw = task->nvcsw;
    info.nivcsw = task->nivcsw;

    // Χρόνος εκκίνησης σε nanoseconds
    info.start_time = ktime_to_ns(ktime_set(task->start_time / HZ, 0)); 

    // Πρώτο παιδί (αν υπάρχει)
    if (!list_empty(&task->children)) {
        child = list_first_entry(&task->children, struct task_struct, sibling);
        info.first_child_pid = task_pid_nr(thread_group_leader(child));
    } else {
        info.first_child_pid = 0;
    }

    // Επόμενο αδερφάκι (αν υπάρχει)
    next_sibling = list_next_entry(task, sibling);
    if (&next_sibling->sibling != &task->parent->children) {
        info.next_sibling_pid = task_pid_nr(thread_group_leader(next_sibling));
    } else {
        info.next_sibling_pid = 0;
    }

    // Στέλνουμε το struct στον χρήστη. Αν δεν πάει, σταματάμε.
    if (copy_to_user(&buf[*count], &info, sizeof(info)))
        return;

    // Αυξάνουμε το πόσα έχουμε στείλει μέχρι τώρα
    (*count)++;

    // Πάμε DFS στα παιδιά τώρα
    rcu_read_lock(); // κλειδώνουμε για ασφάλεια
    list_for_each_entry_rcu(child, &task->children, sibling) {
        fill_k22info(child, buf, count, max_count); // recursive call
    }
    rcu_read_unlock(); // ξεκλειδώνουμε όταν τελειώσουμε
}

/*
 * Η syscall μας, k22tree.
 * Παίρνει έναν user-space buffer και τον αριθμό θέσεων που χωράει.
 */
SYSCALL_DEFINE2(k22tree, struct k22info __user *, buf, int __user *, ne)
{
    int max_count;
    int count = 0;

    // Έλεγχος για έγκυρους pointers
    if (!buf || !ne)
        return -EINVAL;

    // Παίρνουμε από τον χρήστη το μέγεθος του buffer
    if (get_user(max_count, ne))
        return -EFAULT;

    if (max_count < 1)
        return -EINVAL;

    // Ξεκινάμε traversal από την init_task
    rcu_read_lock();
    fill_k22info(&init_task, buf, &count, max_count);
    rcu_read_unlock();

    // Ενημερώνουμε τον χρήστη πόσα entries καταφέραμε να συμπληρώσουμε
    if (put_user(count, ne))
        return -EFAULT;

    // Επιστρέφουμε τον αριθμό των processes που είδαμε
    return count;
}

/* 

 *
 * 1. Το επόμενο αδερφάκι μπορεί να  ξεφύγει αν είναι το τελευταίο παιδί.
 *    Ίσως χρειαστεί να προσέξουμε σε edge cases.
 *
 * 2. Ο χρόνος start_time μετατράπηκε απλά σε ns από jiffies.
 *    Θα ήταν καλύτερο να χρησιμοποιήσουμε clock monotonic ή κάποιο πιο ακριβές API;
 
* 3. Τι γίνεται αν ο user-space buffer είναι μικρότερος από τον αριθμό των processes;
 *    Θα χρειαστεί να ξανακαλέσει τη syscall με μεγαλύτερο buffer.
