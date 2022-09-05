/* Notes written by Qian Liu <qianlxc@outlook.com>
  If you find any bug, please contact with me.*/

#include <mmu.h>
#include <error.h>
#include <env.h>
#include <kerelf.h>
#include <sched.h>
#include <pmap.h>
#include <printf.h>

struct Env *envs = NULL;        // All environments
struct Env *curenv = NULL;            // the current env
struct Thread *curthread = NULL;

static struct Env_list env_free_list;    // Free list
// struct Env_list env_sched_list[2];      // Runnable list
struct Thread_list thread_sched_list[2];
 
extern Pde *boot_pgdir;
extern char *KERNEL_SP;

static u_int asid_bitmap[2] = {0}; // 64

static u_int asid_alloc() {
    int i, index, inner;
    for (i = 0; i < 64; ++i) {
        index = i >> 5;
        inner = i & 31;
        if ((asid_bitmap[index] & (1 << inner)) == 0) {
            asid_bitmap[index] |= 1 << inner;
            return i;
        }
    }
    panic("too many processes!");
}

static void asid_free(u_int i) {
    int index, inner;
    index = i >> 5;
    inner = i & 31;
    asid_bitmap[index] &= ~(1 << inner);
}

u_int mkenvid(struct Env *e) {
    u_int idx = e - envs;
    u_int asid = asid_alloc();
    return (asid << (1 + LOG2NENV)) | (1 << LOG2NENV) | idx;
}

u_int mkthreadid(struct Thread *t) {
    struct Env *e = t->thread_parent_env;
    u_int idx = t - e->env_threads;
    return (e->env_id << LOG2THREAD) | idx; 
}

int envid2env(u_int envid, struct Env **penv, int checkperm)
{
    struct Env *e;
    /* Hint: If envid is zero, return curenv.*/
    /* Step 1: Assign value to e using envid. */
	if (envid == 0){
		*penv = curenv;
		return 0;
	} else{
		e = &envs[ENVX(envid)];
	}

    if (e->env_id != envid) {   //no need env_free
        *penv = 0;
        return -E_BAD_ENV;
    }

	if(checkperm){
		if(e != curenv && e->env_parent_id != curenv->env_id){
			*penv = 0;
			return -E_BAD_ENV;
		}
	}

    *penv = e;
    return 0;
}

int threadid2thread(u_int threadid, struct Thread **pthread) {
    struct Thread *t;
    struct Env *e;

    if (threadid == 0) {
        *pthread = curthread;
        return 0;
    }

    e = &envs[ENVX(threadid >> LOG2THREAD)];
    t = &e->env_threads[THREADX(threadid)];
    if(threadid != t->thread_id) {
        *pthread = 0;
        return -E_BAD_THREAD;
    }
    *pthread = t;
    return 0;
}

void env_init(void)
{
	int i;
    /* Step 1: Initialize many lists. */
    LIST_INIT(&env_free_list);
	// LIST_INIT(&env_sched_list[0]);
	// LIST_INIT(&env_sched_list[1]);
    LIST_INIT(&thread_sched_list[0]);
    LIST_INIT(&thread_sched_list[1]);
	
    for(i = NENV - 1; i >= 0; i--){
    	// envs[i].env_status = ENV_FREE;
		LIST_INSERT_HEAD(&env_free_list, &envs[i], env_link);	
	}
}

static int env_setup_vm(struct Env *e)
{

    int i, r;
    struct Page *p = NULL;
    Pde *pgdir;

    /* Step 1: Allocate a page for the page directory
     *   using a function you completed in the lab2 and add its pp_ref.
     *   pgdir is the page directory of Env e, assign value for it. */
    if ((r = page_alloc(&p)) != 0) {
        panic("env_setup_vm - page alloc error\n");
        return r;
    }
	p->pp_ref++;
	pgdir = (Pde*)page2kva(p);

    /* Step 2: Zero pgdir's field before UTOP. */
	for(i = 0; i < PDX(UTOP); i++){
		pgdir[i] = 0;	
	}

    /* Step 3: Copy kernel's boot_pgdir to pgdir. */
	for(; i < PTE2PT; i++){
		pgdir[i] = boot_pgdir[i];	//no need UVPT, beacuse it will be recovered later
	}
    /* Hint:
     *  The VA space of all envs is identical above UTOP
     *  (except at UVPT, which we've set below).
     *  See ./include/mmu.h for layout.
     *  Can you use boot_pgdir as a template?
     */
	e->env_pgdir = pgdir;
	e->env_cr3 = PADDR(pgdir);

    /* UVPT maps the env's own page table, with read-only permission.*/
    e->env_pgdir[PDX(UVPT)]  = e->env_cr3 | PTE_V;
    return 0;
}

int thread_alloc(struct Thread **new, struct Env *e) {
    struct Thread *t;
    u_int i = 0;
    // overflow max thread count
    if(e->env_thread_count >= NTHREAD)
        return -E_MAX_THREAD;
    // get a free thread
    for(i = 0; ; i++) {
        if(i >= NTHREAD)
            return -E_MAX_THREAD;
        t = &e->env_threads[i];
        if(t->thread_status == ENV_FREE && t->thread_exit_ptr == NULL) {
            break;
        }
    }
    e->env_thread_count++;
    //init basic information
    t->thread_tf.cp0_status = 0x1000100C;
    t->thread_tf.regs[29] = USTACKTOP - THREADX(t->thread_id) * TSTACKSIZE;
    t->thread_parent_env = e;    
    t->thread_id = mkthreadid(t);
    t->thread_status = ENV_RUNNABLE;
    t->thread_pri = 1;  // will be replaced in sys_thread_alloc 
    //pthread info
    //join
    t->thread_detach = 0;
    t->thread_join_val_ptr = NULL;
    LIST_INIT(&t->thread_join_list);
    //exit
    t->thread_exit_val = 0;
    t->thread_exit_ptr = (void *)&(t->thread_exit_val);
    //cancel
    t->thread_cancelstate = THREAD_CANCEL_ENABLE;
    t->thread_canceltype = THREAD_CANCEL_ASYNCHRONOUS;
    t->thread_to_be_canceled = 0;

    *new = t;
    printf("alloc thread %08x\n", t->thread_id);
    return 0;
}

int env_alloc(struct Env **new, u_int parent_id)
{
    struct Env *e;
    struct Thread *t;
	u_int r;
    /* Step 1: Get a new Env from env_free_list*/
    if(LIST_EMPTY(&env_free_list)){
    	*new = NULL;
    	return -E_NO_FREE_ENV;
	}
	e = LIST_FIRST(&env_free_list);
    /* Step 2: Call a certain function (has been completed just now) to init kernel memory layout for this new Env.
     *The function mainly maps the kernel address to this new Env address. */
	if((r = env_setup_vm(e)) != 0){
    	return r;
	}
    /* Step 3: Initialize every field of new Env with appropriate values.*/
	e->env_id = mkenvid(e);
	e->env_parent_id = parent_id;
    // Step 4: 
    if((r = thread_alloc(&t, e)) < 0)
        return r;
    // e->env_tf.cp0_status = 0x1000100C;
	// e->env_tf.regs[29] = USTACKTOP;

    /* Step 5: Remove the new Env from env_free_list. */
	LIST_REMOVE(e, env_link);
	*new = e;
    printf("[%08x] alloc env %08x\n", parent_id, e->env_id);
	return 0;
}

static int load_icode_mapper(u_long va, u_int32_t sgsize,
                             u_char *bin, u_int32_t bin_size, void *user_data)
{
    struct Env *env = (struct Env *)user_data;
    struct Page *p = NULL;
    u_long i = 0;
    int r;
    u_long offset = va - ROUNDDOWN(va, BY2PG);
    u_long size;
    /* Step 1: load all content of bin into memory. */
    //begin align
    if(offset != 0){
    	if((p = page_lookup(env->env_pgdir, va - offset, NULL)) == NULL){
	    	if((r = page_alloc(&p)) != 0){
	    		return r;
			}
			if((r = page_insert(env->env_pgdir, p, va - offset, PTE_R)) != 0){
				return r;
			}	
		}
		size = MIN(BY2PG - offset, bin_size - i);
		bcopy((void *)bin, (void *)(page2kva(p) + offset), size);
		i += size;
	}
	//pages
    for (; i < bin_size; i += size) {
    	if((p = page_lookup(env->env_pgdir, va + i, NULL)) == NULL){
	    	if((r = page_alloc(&p)) != 0){
		    	return r;
			}
			if((r = page_insert(env->env_pgdir, p, va + i, PTE_R)) != 0){
				return r;
			}
		}
		size = MIN(BY2PG, bin_size - i);
		bcopy((void *)bin + i, (void *)page2kva(p), size);
    }
    /* Step 2: alloc pages to reach `sgsize` when `bin_size` < `sgsize`.
     * hint: variable `i` has the value of `bin_size` now! */
    //begin align
    //va + i has been align with BY2PG, p is the last p
    offset = (va + i) -  ROUNDDOWN(va + i, BY2PG);
    if(offset != 0){
		if((p = page_lookup(env->env_pgdir, va + i - offset, NULL)) == NULL){
	    	if((r = page_alloc(&p)) != 0){
		    	return r;
			}
			if((r = page_insert(env->env_pgdir, p, va + i - offset, PTE_R)) != 0){
				return r;
			}
		}
		size = MIN(BY2PG - offset, sgsize - i);
		//bzero((void *)(page2kva(p) + offset), size);
		i += size; 
	}
	//zero .bss    
    while (i < sgsize) {
    	if((p = page_lookup(env->env_pgdir, va + i, NULL)) == NULL){
			if((r = page_alloc(&p)) != 0){
		    	return r;
		    }
			if((r = page_insert(env->env_pgdir, p, va + i, PTE_R)) != 0){
				return r;
			} 		
		}
		size = MIN(BY2PG, sgsize - i);
		//bzero((void *)page2kva(p), size);
		i += size;
    }
    
    return 0;
}

static void load_icode(struct Env *e, u_char *binary, u_int size)
{
    /* Hint:
     *  You must figure out which permissions you'll need
     *  for the different mappings you create.
     *  Remember that the binary image is an a.out format image,
     *  which contains both text and data.
     */
    struct Page *p = NULL;
    u_long entry_point;
    u_long r;
    u_long perm = PTE_R;
    u_int va;
    
    for(va = USTACKTOP - TSTACKSIZE * NTHREAD; va < USTACKTOP; va += BY2PG){
        /* Step 1: alloc a page. */
        if((r = page_alloc(&p)) != 0){
            return;
        }
        /* Step 2: Use appropriate perm to set initial stack for new Env. */
        /* Hint: Should the user-stack be writable? */
        if((r = page_insert(e->env_pgdir, p, va, perm)) != 0){
            return;
        }
    }
    /* Step 3: load the binary using elf loader. */
	load_elf(binary, size, &entry_point, (void *)e, load_icode_mapper);
    /* Step 4: Set CPU's PC register as appropriate value. */
    // printf("entry point :%08x\n", entry_point);
    e->env_threads[0].thread_tf.pc = entry_point;
    // e->env_tf.pc = entry_point;
}

void env_create_priority(u_char *binary, int size, int priority)
{
    struct Env *e;
    /* Step 1: Use env_alloc to alloc a new env. */
	if(env_alloc(&e, 0) != 0){
    	return;
	}
	/* Step 2: assign priority to the new env. */
    e->env_threads[0].thread_pri = (u_int)priority;
    // load icode and insert thread
	load_icode(e, binary, size);
    LIST_INSERT_HEAD(&thread_sched_list[0], &e->env_threads[0], thread_sched_link);
	// LIST_INSERT_HEAD(&env_sched_list[0], e, env_sched_link);
}

void env_create(u_char *binary, int size)
{
     /* Step 1: Use env_create_priority to alloc a new env with priority 1 */
	env_create_priority(binary, size, 1);
}

/* Overview:
 *  Free env e and all memory it uses.
 */
void env_free(struct Env *e)
{
    Pte *pt;
    u_int pdeno, pteno, pa;
    u_int i;

    /* Hint: Note the environment's demise.*/
    printf("env [%08x] free env %08x\n", curenv ? curenv->env_id : 0, e->env_id);

    /* Hint: Flush all mapped pages in the user portion of the address space */
    for (pdeno = 0; pdeno < PDX(UTOP); pdeno++) {
        /* Hint: only look at mapped page tables. */
        if (!(e->env_pgdir[pdeno] & PTE_V)) {
            continue;
        }
        /* Hint: find the pa and va of the page table. */
        pa = PTE_ADDR(e->env_pgdir[pdeno]);
        pt = (Pte *)KADDR(pa);
        /* Hint: Unmap all PTEs in this page table. */
        for (pteno = 0; pteno <= PTX(~0); pteno++)
            if (pt[pteno] & PTE_V) {
                page_remove(e->env_pgdir, (pdeno << PDSHIFT) | (pteno << PGSHIFT));
            }
        /* Hint: free the page table itself. */
        e->env_pgdir[pdeno] = 0;
        page_decref(pa2page(pa));
    }
    /* Hint: free the page directory. */
    pa = e->env_cr3;
    e->env_pgdir = 0;
    e->env_cr3 = 0;
    /* Hint: free the ASID */
    asid_free(e->env_id >> (1 + LOG2NENV));
    page_decref(pa2page(pa));
    /* Hint: return the environment to the free list. */
    // e->env_status = ENV_FREE;
    for (i = 0; i < NTHREAD; i++) {
        e->env_threads[i].thread_status = ENV_FREE;
        e->env_threads[i].thread_exit_ptr = NULL;
    }
    e->env_thread_count = 0;
    e->env_runs = 0;
    LIST_INSERT_HEAD(&env_free_list, e, env_link);
    // LIST_REMOVE(e, env_sched_link);
}

void thread_free(struct Thread *t) {
    struct Env *e;
    e = t->thread_parent_env;
    printf("thread [%08x] free thread %08x in env %08x\n", curthread->thread_id, t->thread_id, e->env_id);
    e->env_thread_count--;
    if(e->env_thread_count <= 0)
        env_free(e);
    // remove from sched list
    // printf("free status set!\n");
    t->thread_status = ENV_FREE;
    LIST_REMOVE(t, thread_sched_link);
}

/* Overview:
 *  Free env e, and schedule to run a new env if e is the current env.
 */
void env_destroy(struct Env *e) // no need anymore
{
    //destroy all thread in env
    int i;
    for (i = 0; i < NTHREAD; i++) {
        if(e->env_threads[i].thread_status != ENV_FREE)
            thread_destroy(&e->env_threads[i]);
    }

    /* Hint: schedule to run a new environment. */
    if (curenv == e) {
        curenv = NULL;
        /* Hint: Why this? */
        bcopy((void *)KERNEL_SP - sizeof(struct Trapframe),
              (void *)TIMESTACK - sizeof(struct Trapframe),
              sizeof(struct Trapframe));
        printf("env is killed ... \n");
        sched_yield();  //new this?
    }
}

void thread_destroy(struct Thread *t) {
    thread_free(t); // LIST REMOVE in thread_free
    if(curthread == t) {
        curthread = NULL;
		bcopy((void *)KERNEL_SP - sizeof(struct Trapframe),
			  (void *)TIMESTACK - sizeof(struct Trapframe),
			  sizeof(struct Trapframe));
		printf("thread is killed ... \n");
		sched_yield();
    }
}

extern void env_pop_tf(struct Trapframe *tf, int id);
extern void lcontext(u_int contxt);

void
thread_run(struct Thread *t)  //thread_run
{
	struct Trapframe *old;
	old = (struct Trapframe *)(TIMESTACK - sizeof(struct Trapframe));
    /* Step 1: save register state of curthread. */
    if(curthread != NULL){
 		bcopy((void *)old, (void *)(&curthread->thread_tf), sizeof(struct Trapframe));
 		curthread->thread_tf.pc = curthread->thread_tf.cp0_epc;
	}

    /* Step 2: Set 'curthread' to the new environment. */
	curthread = t;
    curenv = t->thread_parent_env;
    curenv->env_runs++;

    /* Step 3: Use lcontext() to switch to its address space. */
	lcontext((u_int)curenv->env_pgdir);

    // print_thread(curthread);
    // print_env(curenv);
    // print_trapframe(&curthread->thread_tf);

    // Step 4
	env_pop_tf(&curthread->thread_tf, GET_ENV_ASID(curenv->env_id));
}

void print_thread(struct Thread* t) {
    printf("thread id: [%08x]\n", t->thread_id);
    printf("thread pri: [%08x]\n", t->thread_pri);
    printf("thread status: [%08x]\n", t->thread_status);
    printf("thread parent envid: [%08x]\n", t->thread_parent_env->env_id);
    printf("thread tf addr: [%08x]\n", &t->thread_tf);
}

void print_env(struct Env *e) {
    printf("env id: [%08x]\n", e->env_id);
    printf("env cr3: [%08x]\n", e->env_cr3);
    printf("env pgdir: [%08x]\n", e->env_pgdir);
    printf("env parentid: [%08x]\n", e->env_parent_id);
    printf("env threadcount: [%08x]\n", e->env_thread_count);
}

void print_trapframe(struct Trapframe *tf) {
    printf("tf pc: [%08x]\n", tf->pc);
    printf("tf epc: [%08x]\n", tf->cp0_epc);
    printf("tf cp0status: [%08x]\n", tf->cp0_status);
    printf("tf cp0cause: [%08x]\n", tf->cp0_cause);
    printf("tf badvaddr: [%08x]\n", tf->cp0_badvaddr);
    u_int i;
    for(i = 0; i < 32; i++)
        printf("reg[%d]:[%08x] ", i, tf->regs[i]);
    printf("\n");
}
