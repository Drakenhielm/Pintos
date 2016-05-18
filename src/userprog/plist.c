#include <stddef.h>

#include "plist.h"

static struct process_info plist[PLIST_SIZE];
static struct lock plist_lock;


void plist_init()
{
	int i;
	for(i = 0; i < PLIST_SIZE; i++)
	{
		plist[i].free = 1;
	}
	lock_init(&plist_lock);
}

void plist_print()
{
	printf("# ### Process List: ####\n");
	lock_acquire(&plist_lock);
	int i;
	int counter = 0;
	for(i = 0; i < PLIST_SIZE; i++)
	{
		if(plist[i].free == 0)
		{
			counter++;
			printf("# pid:%i, parent:%i, parent_alive:%i, result:%i, alive:%i, waiting:%i\n",
				plist[i].pid, plist[i].parent, plist[i].parent_alive, plist[i].result, plist[i].alive, plist[i].waiting);
		}
	}
	lock_release(&plist_lock);
	printf("# Number of processes: %d\n", counter);
	printf("# ### End Of Process List: ####\n");
}

int plist_is_parent_alive(int parent)
{
	if(parent == 1)
		return 1;
	
	int i;
	for(i = 0; i < PLIST_SIZE; i++)
	{
		if(plist[i].pid == parent)
		{
			if(plist[i].alive == 1)
				return 1;
			else
				return 0;
		}
	}
	return 0;
}

int plist_insert(int pid, int parent)
{
	int i;
	lock_acquire(&plist_lock);
	for(i = 0; i < PLIST_SIZE; i++)
	{
		if(plist[i].free == 1)
		{
			plist[i].free = 0;
			plist[i].pid = pid;
			plist[i].parent = parent;
			plist[i].parent_alive = plist_is_parent_alive(parent);
			sema_init(&(plist[i].sema), 0);
			plist[i].alive = 1;
			plist[i].waiting = -1;
			lock_release(&plist_lock);
			return 1;
		}
	}
	lock_release(&plist_lock);
	return 0;
}

void plist_remove_dead_children(int parent)
{
	int i;
	for(i = 0; i < PLIST_SIZE; i++)
	{
		if(plist[i].parent == parent)
		{
			plist[i].parent_alive = 0;
			if(plist[i].alive == 0)
				plist[i].free = 1;
		}
	}
}

int plist_kill(int pid)
{
	int i;
	lock_acquire(&plist_lock);
	for(i = 0; i < PLIST_SIZE; i++)
	{
		if(plist[i].pid == pid)
		{
			if(plist_is_parent_alive(plist[i].parent) == 1)
			{
				plist[i].alive = 0;
			}
			else
			{
				plist[i].free = 1;
			}
			sema_up(&(plist[i].sema));
			plist_remove_dead_children(pid);
			lock_release(&plist_lock);
			return 1;
		}
	}
	lock_release(&plist_lock);
	return 0;
}

int plist_is_child(int parent, int child)
{
	int i;
	for(i = 0; i < PLIST_SIZE; i++)
	{
		if(plist[i].pid == child && plist[i].parent == parent && plist[i].free != 1)
		{
			return i;
		}	
	}		
	return -1;
}

int plist_wait(int pid, int wait_pid)
{
	lock_acquire(&plist_lock);
	int i;
	int child_index = plist_is_child(pid, wait_pid);
	if(pid != 1)
	{
		for(i = 0; i < PLIST_SIZE; i++)
		{
			if(plist[i].pid == pid)
			{
				if(child_index != -1)
				{
					plist[i].waiting = wait_pid;
					lock_release(&plist_lock);
					sema_down(&(plist[child_index].sema));
					lock_acquire(&plist_lock);
					plist[i].waiting = -1;
					plist[child_index].free = 1;
					lock_release(&plist_lock);
					return plist[child_index].result;
				}
			}
		}
	}
	else if (child_index != -1)
	{
		lock_release(&plist_lock);
		sema_down(&(plist[child_index].sema));
		lock_acquire(&plist_lock);
		plist[child_index].free = 1;
		lock_release(&plist_lock);
		return plist[child_index].result;
	}
	lock_release(&plist_lock);
	return -1;
}

void plist_set_result(int pid, int result)
{
	int i;
	lock_acquire(&plist_lock);
	for(i = 0; i < PLIST_SIZE; i++)
	{
		if(plist[i].pid == pid)
		{
			plist[i].result = result;
		}	
	}
	lock_release(&plist_lock);
}

int plist_get_result(int pid)
{
	int i;
	lock_acquire(&plist_lock);
	for(i = 0; i < PLIST_SIZE; i++)
	{
		if(plist[i].pid == pid)
		{
			lock_release(&plist_lock);
			return plist[i].result;
		}	
	}
	lock_release(&plist_lock);	
	return -1;
}


