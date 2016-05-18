#include <stddef.h>

#include "flist.h"
#include "threads/malloc.h"
#include "threads/thread.h"

void flist_init(struct map* map)
{
  list_init(&map->content);
  map->next_key = 2;
}

int flist_insert(struct file* file, struct thread* thread)
{
  struct map* map = &(thread->open_files);

  struct association* a = (struct association*)malloc(sizeof(struct association));
  
  if(a == NULL)
    return -1;
  
  a->value = file;
  a->key = map->next_key++;

  list_push_back(&(map->content), &(a->elem));
  return a->key;
}

void* flist_find(int fd, struct thread* thread)
{
  struct map* map = &(thread->open_files);
  struct list_elem* list_elem;
  for(list_elem = list_begin(&(map->content)); list_elem != list_end(&(map->content)); list_elem = list_next(list_elem))
    {
      struct association* a = list_entry(list_elem, struct association, elem);
      if(a->key == fd)
	return a->value;
    }
  return NULL;
}

void* flist_remove(int fd, struct thread* thread)
{
  struct map* map = &(thread->open_files);
  struct list_elem* list_elem;
  for(list_elem = list_begin(&(map->content)); list_elem != list_end(&(map->content)); list_elem = list_next(list_elem))
    {
      struct association* a = list_entry(list_elem, struct association, elem);
      if(a->key == fd)
	{
	  list_remove(list_elem);
	  void* ret = a->value;
	  free(a);
	  return ret;
	}
    }
  return NULL;
}

void flist_for_each(struct map* m, void (*exec) (int key, struct file* file, int aux), int aux)
{
  struct map* map = m;
  struct list_elem* list_elem;
  for(list_elem = list_begin(&(map->content)); list_elem != list_end(&(map->content)); list_elem = list_next(list_elem))
    {
      struct association* a = list_entry(list_elem, struct association, elem);
      exec(a->key, a->value, aux);
    }
}

void flist_remove_if(struct map* m, bool (*cond) (int key, struct file* file, int aux), int aux)
{
  struct map* map = m;
  struct list_elem* list_elem;
  for(list_elem = list_begin(&(map->content)); list_elem != list_end(&(map->content)); list_elem = list_next(list_elem))
    {
      struct association* a = list_entry(list_elem, struct association, elem);
      if (cond(a->key, a->value, aux))
	{
	  list_remove(list_elem);
	  free(a);
	}
    }
}
