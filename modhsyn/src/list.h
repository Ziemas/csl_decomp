#ifndef LIST_H_
#define LIST_H_

#include "stddef.h"

struct ListElement {
	struct ListElement *next, *prev;
};

#define container_of(ptr, type, member)                    \
	({                                                     \
		const typeof(((type *)0)->member) *__mptr = (ptr); \
		(type *)((char *)__mptr - offsetof(type, member)); \
	})

#define list_entry(ptr, type, member) container_of(ptr, type, member)

static inline void
ListInit(struct ListElement *list)
{
	list->next = list;
	list->prev = list;
}

static inline void
ListInsert(struct ListElement *item, struct ListElement *list)
{
	struct ListElement *prev = list->prev, *next = list;
	item->next = next;
	item->prev = prev;
	prev->next = item;
	next->prev = item;
}

static inline void
ListRemove(struct ListElement *a)
{
	a->next->prev = a->prev;
	a->prev->next = a->next;
}

#endif // LIST_H_
