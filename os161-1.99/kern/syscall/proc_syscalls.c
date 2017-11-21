#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <vfs.h>
#include <kern/fcntl.h>
  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);


	//telling child, parent is dying and clean up any zombie child//
	spinlock_acquire(&p->p_lock);
	struct child_proc_list * cur = p->child_list;
	struct child_proc_list * next = NULL;
	if (cur != NULL) {
		next = cur->next;
	}
	while (next != NULL) {
	
		//child in zombin state//	
		if (next->child->state == 0) {
			cur->next = next->next;
			cv_destroy(next->child->p_wait_cv);
			lock_destroy(next->child->p_wait_lock);
			proc_destroy(next->child);
			next = cur->next;
		}
		else {
			next->child->parent = NULL;
			cur = cur->next;
			next = next->next;
		}
	}
	cur = p->child_list;
	if (cur != NULL) {
	if (cur->child->state == 0){
		p->child_list = cur->next;
		cv_destroy(cur->child->p_wait_cv);
		lock_destroy(cur->child->p_wait_lock);
		proc_destroy(cur->child);
	}
	else {
		cur->child->parent = NULL;
	}
	}
	spinlock_release(&p->p_lock);

  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
	if (p->parent == NULL){
		cv_destroy(p->p_wait_cv);
		lock_destroy(p->p_wait_lock);
  		proc_destroy(p);
	}

	else {
		p->state = 0;
		p->exit_status = _MKWAIT_EXIT(exitcode);
		
		lock_acquire(p->p_wait_lock);	
		cv_broadcast(p->p_wait_cv, p->p_wait_lock);	
		lock_release(p->p_wait_lock);
	}
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
struct proc temp = *curproc; 
 *retval = temp.pid;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

	struct proc * child = NULL;
	struct proc * p = curproc;

	if (options != 0) {
    		return(EINVAL);
  	}


	spinlock_acquire(&p->p_lock);
	struct child_proc_list * temp = curproc->child_list;
	while (temp) {
		if (temp->child->pid == pid) {
			child = temp->child;
			break;
		}
		temp = temp->next;

	}

	spinlock_release(&p->p_lock);
	
	if (child == NULL) {
		return ECHILD;
	}

	lock_acquire(child->p_wait_lock);
	if (child->state == 0) {
		exitstatus = child->exit_status;		
	}
	
	else {
		while (child->state != 0) {
			cv_wait(child->p_wait_cv, child->p_wait_lock);
		}
		exitstatus = child->exit_status;
	}
	lock_release(child->p_wait_lock);
	

  /* for now, just pretend the exitstatus is 0 */
  //exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }

	//deleting the child//
	/*spinlock_acquire(&p->p_lock);
		struct child_proc_list * pre = curproc->child_list;
		struct child_proc_list * cur = pre->next;
		if (pre == NULL) {}
		else if (pre->child == child) {
			curproc->child_list = cur;
			proc_destroy(child);
		}
		else {
			while (cur) {
				if (cur->child == child) {
					pre->next = cur->next;
					proc_destroy(child);
					break;
				}
				pre = pre->next;
				cur = cur->next;
			}
		}

	spinlock_release(&p->p_lock);*/

  *retval = pid;
  return(0);
}

//changing memory space for the given process
void
anyproc_setas(struct addrspace *newas, struct proc * p) 
{
	struct addrspace *oldas;
	
	spinlock_acquire(&p->p_lock);
	oldas = p->p_addrspace;
	p->p_addrspace = newas;
	if (oldas != NULL) as_destroy(oldas);
	spinlock_release(&p->p_lock);

}

void entry_forked_process (void *local_tf, unsigned long int i) {

	(void)i;

	struct trapframe tf = *(struct trapframe *)local_tf;
	tf.tf_a3 = 0;
	tf.tf_v0 = 0;
	tf.tf_epc += 4;
	//as_activate();
	mips_usermode(&tf);


}
	
int
sys_fork(struct trapframe *tf, int32_t *retval)
{
	//creating the new process//
	struct proc * new_proc = NULL;
	//int new_pid;
	struct addrspace * new_addrspace = NULL;
	int thread_fork_result;
	
	new_proc = proc_create_runprogram("haha");
		//if coudl not create a new process//

		if (new_proc == NULL) {
			return ENPROC;

		}
	
	int new_mem_rv = as_copy(curproc_getas(), &new_addrspace);
	if (new_mem_rv == ENOMEM) {
		proc_destroy(new_proc);
		return ENPROC;
	}

	//setting address space//
	anyproc_setas(new_addrspace, new_proc);
	
	
	//getting up the thread needed//
	
	struct trapframe * local_tf = kmalloc(sizeof(*tf));
	if (local_tf == NULL) {
		proc_destroy(new_proc);
		return ENPROC;
	}	
	memcpy(local_tf, tf, sizeof(*tf));

	thread_fork_result = thread_fork("new_thread", new_proc, &entry_forked_process, local_tf, 1);
	if (thread_fork_result != 0) {
		kfree(local_tf);
		proc_destroy(new_proc);
		return ENPROC;
	}


	//setting parent and child relationship//
	
	//add child to parent//
	spinlock_acquire(&new_proc->p_lock);
	struct child_proc_list * c = kmalloc(sizeof(struct child_proc_list));
	
	if (c == NULL){
		proc_destroy(new_proc);
		return ENPROC;

	}
	c->child = new_proc;
	c->next = curproc->child_list;
	curproc->child_list = c;
	spinlock_release(&new_proc->p_lock);
	
	//add parent to child//
	new_proc->parent = curproc;
	
	//copying new thread's pid into return value;
	memcpy(retval, &(new_proc->pid), sizeof(pid_t));
	
	return 0;	





}

int sys_execv (const char * pname, char **argv, int *retval) {

	size_t pname_got;
	char * pname_ker = NULL;
	struct vnode *v;	
	struct addrspace *as = NULL;
	struct addrspace *old_as = NULL;
	vaddr_t entrypoint, stackptr;
	
	//arg varaible
	char **argv_ker = NULL;
	int argnum = 0;
	size_t len;


	if (pname == NULL) {
		*retval = -1; return EFAULT;
	}

	pname_ker = kmalloc(strlen(pname) + 1);
	if (pname_ker == NULL) {
		*retval = -1;
		return ENOMEM;
	}

	copyinstr((const_userptr_t)pname, pname_ker, strlen(pname) + 1, &pname_got); 

	//if (pname_got != strlen(pname) + 1) {
	//	*retval = -1;
	//	return EFAULT;
	//}

	//if (argv != NULL) {
		while (argv[argnum] != NULL) {
			argnum++;
		}
	//}
	
	argv_ker = kmalloc(sizeof(char*) * (argnum + 1));
	if (argv_ker == NULL) {
		*retval = -1;
		return ENOMEM;
	}
	
	size_t arglen;

	for (int i = 0; i < argnum; i++) {
		len = strlen(argv[i]) + 1;
		argv_ker[i] = kmalloc(len);
		if (argv_ker[i] == NULL) {
			*retval = -1;
			return ENOMEM;
		}
		copyinstr((const_userptr_t)argv[i], argv_ker[i], len, &arglen);

	}

	argv_ker[argnum] = NULL;	

	int result = 0;
	char *fname_temp;
	fname_temp = kstrdup(pname_ker);
	result = vfs_open(fname_temp, O_RDONLY, 0, &v);
	if (result){
		*retval = -1;
		return result;
	}

//	KASSERT(curproc_getas() != NULL);

	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		*retval = -1;
		return ENOMEM;
	}

	old_as = curproc_getas();
	curproc_setas(as);
	as_activate();

	result = load_elf(v, &entrypoint);
	if (result) {
		vfs_close(v);
		*retval = -1;
		return result;
	}

	vfs_close(v);

	result = as_define_stack(as, &stackptr);
	if (result) {
		*retval = -1;
		return result;
	}

	vaddr_t stack_addr[argnum + 1];
	stack_addr[argnum] = (vaddr_t)NULL;
	size_t stack_len;

	for (int i = argnum - 1; i >= 0; i--) {
		len = strlen(argv_ker[i]) + 1;
		stackptr -= len;
		stack_addr[i] = stackptr;
		copyoutstr(argv_ker[i], (userptr_t)stackptr, len, &stack_len); 
	}

	while (stackptr%4 != 0) stackptr--;

	for (int i = argnum; i >= 0; i--) {
		stackptr -= ROUNDUP(sizeof(vaddr_t), 4);
		copyout(&stack_addr[i], (userptr_t)stackptr, sizeof(vaddr_t));
		
	}
	
//	kfree(pname_ker);
//	for (int i = 0; i < argnum + 1; i++) {
//		kfree(argv[i]);
//	}

//	kfree(argv);
	as_destroy(old_as);
	
	enter_new_process(argnum /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
			  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;



} 

