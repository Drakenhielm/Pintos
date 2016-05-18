#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

/* header files you probably need, they are not used yet */
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


/* This array defined the number of arguments each syscall expects.
   For example, if you want to find out the number of arguments for
   the read system call you shall write:
   
   int sys_read_arg_count = argc[ SYS_READ ];
   
   All system calls have a name such as SYS_READ defined as an enum
   type, see `lib/syscall-nr.h'. Use them instead of numbers.
 */
const int argc[] = {
  /* basic calls */
  0, 1, 1, 1, 2, 1, 1, 1, 3, 3, 2, 1, 1, 
  /* not implemented */
  2, 1,    1, 1, 2, 1, 1,
  /* extended */
  0
};

static bool verify_fix_length(const void* start, const unsigned length)
{
	if (start == NULL || length < 0)
		return false;

	if (!is_user_vaddr(start + length))
		return false;

	char* cur_page = pg_round_down(start);
	for(;;)
	{
		if(pagedir_get_page(thread_current()->pagedir, cur_page) == NULL)
			return false;

		cur_page += PGSIZE;

		if(cur_page >= (char*)(start+length))
			return true;
	}

}

static bool verify_variable_length(const char* start)
{
	if (start == NULL)
		return false;
	
	char* cur_page = pg_round_down(start);
	char* cur_char = start;

	for(;;)
	{
		if(pagedir_get_page(thread_current()->pagedir, cur_page) == NULL)
			return false;

		for(;cur_char < cur_page + PGSIZE; cur_char++)
		{
			if (!is_user_vaddr(cur_char))
				return false;

			if(*cur_char == '\0')
				return true;

			//cur_char++;
		}
		cur_page += PGSIZE;
	}
	
}

static int read(int fd, char* buf, unsigned length)
{
  if(fd == STDIN_FILENO)
    {
      unsigned count = 0;
      char c;
			while(count < length)
			{
				c = input_getc();
				if (c == '\r')
					c = '\n';
			 
				buf[count] = c;
				printf("%c", c);
				count = count + 1;
			}
      return count;
    }
  else if(fd == STDOUT_FILENO)
    {
      return -1;
    }
  else
    {
      struct file* file = flist_find(fd, thread_current());
      if(file != NULL)
	{
	  return file_read(file, buf, length);
	}
      else
	return -1;
    }
}

static int write(int fd, const char* buf, unsigned length)
{
  if(fd == STDOUT_FILENO)
    {
      putbuf(buf, length);
      return length;
    }
  else if(fd == STDIN_FILENO)
    {
      return -1;
    }
  else
    {
      struct file* file = flist_find(fd, thread_current());
      if(file != NULL)
	return file_write(file, buf, length);
      else
	return -1;
    }
}

static int open (const char *file_name)
{
  struct file* file = filesys_open(file_name);

  if(file != NULL)
    return flist_insert(file, thread_current());
  else
    return -1;
}

static void seek(int fd, unsigned position)
{
  struct file* file = flist_find(fd, thread_current());
  if(file != NULL)
    if(position <= (unsigned)file_length(file))
      file_seek(file, (off_t)position);
}

static unsigned tell(int fd)
{
  struct file* file = flist_find(fd, thread_current());
  if(file != NULL)
    return file_tell(file);
  else
    return -1;
}

static int filesize(int fd)
{
   struct file* file = flist_find(fd, thread_current());
  if(file != NULL)
    return file_length(file);
  else
    return -1;
}

static int create(const char* buf, int size)
{
	if(buf == NULL)
	{
		process_exit(-1);
		return -1;
	}	
	else
		return filesys_create(buf, size);
}

static void
syscall_handler (struct intr_frame *f) 
{
  int32_t* esp = (int32_t*)f->esp;
  
	if(!verify_fix_length(esp, 4))
		process_exit(-1);
	
	int arg_count = argc[esp[0]];
	if(!verify_fix_length(esp+1, 4*arg_count))
		process_exit(-1);

  switch (esp[0])
    {
    case SYS_HALT:
      power_off();
      break;
    case SYS_EXIT:
				process_exit(esp[1]);
      break;
    case SYS_READ:
			if(!verify_fix_length((char*)esp[2], esp[3]))
				process_exit(-1);
      f->eax = read(esp[1], (char*)esp[2], esp[3]);
      break;
    case SYS_WRITE:
			if(!verify_fix_length((char*)esp[2], esp[3]))
				process_exit(-1);
      f->eax = write(esp[1], (char*)esp[2], esp[3]);
      break;
    case SYS_OPEN:
			if(!verify_variable_length((char*)esp[1]))
			{	
				process_exit(-1);
				f->eax = -1;
			}
			else
      f->eax = open((char*)esp[1]);
      break;
    case SYS_CLOSE:
      filesys_close(flist_remove(esp[1], thread_current()));
      break;
    case SYS_REMOVE:
			if(!verify_variable_length((char*)esp[1]))
				process_exit(-1);
      f->eax = filesys_remove((char*)esp[1]);
      break;
    case SYS_CREATE:
			if(!verify_variable_length((char*)esp[1]))	
				process_exit(-1);
			f->eax = create((const char*)esp[1], esp[2]);
      break;
    case SYS_SEEK:
      seek(esp[1], esp[2]);
      break;
    case SYS_TELL:
      f->eax = tell(esp[1]);
      break;
    case SYS_FILESIZE:
      f->eax = filesize(esp[1]);
      break;
    case SYS_EXEC:
			if(!verify_variable_length((char*)esp[1]))
			{	
				f->eax = -1;
				process_exit(-1);
			}
			else
      	f->eax = process_execute((const char*)esp[1]);
      break;
    case SYS_SLEEP:
      f->eax = timer_msleep(esp[1]);
      break;
		case SYS_PLIST:
      process_print_list();
      break;
		case SYS_WAIT:
      f->eax = process_wait(esp[1]);
      break;
    default:
      {
				printf ("Executed an unknown system call!\n");
						
				printf ("Stack top + 0: %d\n", esp[0]);
				printf ("Stack top + 1: %d\n", esp[1]);
						
				process_exit(-1);
    	}
  }
}


