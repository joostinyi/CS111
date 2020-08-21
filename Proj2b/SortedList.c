// NAME: Justin Yi
// EMAIL: joostinyi00@gmail.com
// ID: 905123893

#include "SortedList.h"
#include <sched.h>
#include <stdio.h>
#include <string.h>

/**
 * SortedList_insert ... insert an element into a sorted list
 *
 *	The specified element will be inserted in to
 *	the specified list, which will be kept sorted
 *	in ascending order based on associated keys
 *
 * @param SortedList_t *list ... header for the list
 * @param SortedListElement_t *element ... element to be added to the list
 */
void SortedList_insert(SortedList_t *list, SortedListElement_t *element)
{
    if(!list || !element)
        return;

    SortedListElement_t *p = list->next;

    while(p != list && strcmp(element->key, p->key) < 0)
    {
        if (opt_yield & INSERT_YIELD)
            sched_yield();
        p = p->next;
    }
    element->next = p->next;
    element->prev = p;
    element->next->prev = element;
    p->next = element;
}

/**
 * SortedList_delete ... remove an element from a sorted list
 *
 *	The specified element will be removed from whatever
 *	list it is currently in.
 *
 *	Before doing the deletion, we check to make sure that
 *	next->prev and prev->next both point to this node
 *
 * @param SortedListElement_t *element ... element to be removed
 *
 * @return 0: element deleted successfully, 1: corrtuped prev/next pointers
 *
 */
int SortedList_delete(SortedListElement_t *element)
{
    if (!element || element->next->prev != element || element->prev->next != element)
        return 1;
    
    element->next->prev = element->prev;
    if (opt_yield & DELETE_YIELD)
        sched_yield();
    element->prev->next = element->next;
    return 0;
}

/**
 * SortedList_lookup ... search sorted list for a key
 *
 *	The specified list will be searched for an
 *	element with the specified key.
 *
 * @param SortedList_t *list ... header for the list
 * @param const char * key ... the desired key
 *
 * @return pointer to matching element, or NULL if none is found
 */
SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key)
{
    if (!list || !key)
        return NULL;
    
    SortedListElement_t *p = list->next;

    while(p != list)
    {
        if (!strcmp(p->key, key))
            return p;

        if (opt_yield & LOOKUP_YIELD)
            sched_yield();
        
        p = p->next;
    }
    return NULL;
}

/**
 * SortedList_length ... count elements in a sorted list
 *	While enumeratign list, it checks all prev/next pointers
 *
 * @param SortedList_t *list ... header for the list
 *
 * @return int number of elements in list (excluding head)
 *	   -1 if the list is corrupted
 */
int SortedList_length(SortedList_t *list)
{
    if(!list)
        return -1;
    
    SortedListElement_t *p = list->next;

    int ret = 0;
    
    while(p != list)
    {
        if (!p || p->next->prev != p || p->prev->next != p)
            return -1;
        if (opt_yield & LOOKUP_YIELD)
            sched_yield();
        ret++;
        p = p->next;        
    }
    return ret;
}