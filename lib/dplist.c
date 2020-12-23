/* Filename: dplist.c */
/**
 * \author Ingmar Malfait
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>
#include "dplist.h"

/*
 * definition of error codes
 * */
#define DPLIST_NO_ERROR 0
#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure
#define DPLIST_INVALID_ERROR 2 //error due to a list operation applied on a NULL list 

#ifdef DEBUG
#define DEBUG_PRINTF(...) 									                                        \
        do {											                                            \
            fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	    \
            fprintf(stderr,__VA_ARGS__);								                            \
            fflush(stderr);                                                                         \
                } while(0)
#else
#define DEBUG_PRINTF(...) (void)0
#endif


#define DPLIST_ERR_HANDLER(condition, err_code)                         \
    do {                                                                \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");      \
            assert(!(condition));                                       \
        } while(0)


/*
 * The real definition of struct list / struct node
 */

dplist_node_t *dpl_get_reference_of_element(dplist_t *list, void *element);

struct dplist_node 
{
    dplist_node_t *prev, *next;
    void *element;
};

struct dplist 
{
    dplist_node_t *head;

    void *(*element_copy)(void *src_element);

    void (*element_free)(void **element);

    int (*element_compare)(void *x, void *y);
};


dplist_t *dpl_create(// callback functions
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) 
{
    dplist_t *list;
    list = malloc(sizeof(struct dplist));
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_MEMORY_ERROR);
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

void dpl_free(dplist_t **list, bool free_element) 
{
    dplist_t* list_1 = *list;
    if(list_1 != NULL)
    {
        dplist_node_t* list_next = list_1->head;
        while(list_next != NULL)
        {
            dplist_node_t* list_temp = list_next->next;
            if (free_element)
            {
                list_1->element_free(&(list_next->element));
            }
            else
            {
                free(list_next->element);
            }
            
            free(list_next);
            list_next = list_temp;
        }
    }
    free(list_1);
    *list = NULL;
}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) 
{
    if(list == NULL) return NULL;
    if(list->head == NULL) return NULL;
    dplist_node_t* current = list->head;
    //DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    if(index < 0) index = 0;
    for (size_t i = 0; i != index && current->next != NULL; i++, current = current->next) { }
    return current;
}

int dpl_size(dplist_t *list) 
{
    int size = 0;
    if(list == NULL)
    {
        return -1;
    }
    if(list->head == NULL)
    {
        return 0;
    }
    size++;
    dplist_node_t* list_next = list->head->next; 
    while(list_next != NULL)
    {
        dplist_node_t* list_temp = list_next->next;
        list_next = list_temp;
        size++;
    }
    return size;
}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) 
{
    // not for different structs but for copies!!
    dplist_node_t *ref_at_index, *list_node;
    if (list == NULL) return NULL;
    if (element == NULL) return NULL;
    list_node = malloc(sizeof(dplist_node_t));
    DPLIST_ERR_HANDLER(list_node == NULL, DPLIST_MEMORY_ERROR);
    void* copy = element;
    if(insert_copy)
    {
        copy = list->element_copy(element);
    }
    list_node->element = copy;
    // pointer drawing breakpoint
    if (list->head == NULL) { // covers case 1
        list_node->prev = NULL;
        list_node->next = NULL;
        list->head = list_node;
        // pointer drawing breakpoint
    } else if (index <= 0) { // covers case 2
        list_node->prev = NULL;
        list_node->next = list->head;
        list->head->prev = list_node;
        list->head = list_node;
        // pointer drawing breakpoint
    } else {
        ref_at_index = dpl_get_reference_at_index(list, index);
        assert(ref_at_index != NULL);
        // pointer drawing breakpoint
        if (index < dpl_size(list)) { // covers case 4
            list_node->prev = ref_at_index->prev;
            list_node->next = ref_at_index;
            ref_at_index->prev->next = list_node;
            ref_at_index->prev = list_node;
            // pointer drawing breakpoint
        } else { // covers case 3
            assert(ref_at_index->next == NULL);
            list_node->next = NULL;
            list_node->prev = ref_at_index;
            ref_at_index->next = list_node;
            // pointer drawing breakpoint
        }
    }
    return list;
}

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) 
{
    if (list == NULL) return NULL;
    if (list->head == NULL) return list;
    if(index < 0)
    {
        index = 0;
    }
    dplist_node_t* current = dpl_get_reference_at_index(list, index);
    if(free_element)
    {
        list->element_free(&(current->element));
    }
    if (current == list-> head) 
    {
        list->head = current->next;
    }
    if (current->next != NULL) 
    {
        current->next->prev = current->prev;
    }
    if (current->prev != NULL) 
    {
        current->prev->next = current->next;
    }
    free(current);
    return list;
}

void *dpl_get_element_at_index(dplist_t *list, int index)
{
    if(list == NULL) return 0;
    if(list->head == NULL) return 0;
    if(index < 0) index = 0;
    dplist_node_t* current = list->head;
    for( int i = 0 ; i!=index && current->next != NULL; i++, current = current->next){}
    return current->element;
}

int dpl_get_index_of_element(dplist_t *list, void *element)
{
    if (list == NULL) return -1;
    if (element == NULL) return -1;
    if (list->head == NULL) return -1;
    size_t i = 0;
    for(dplist_node_t* current = list->head; 1 ; i++, current = current->next)
    {
        if (list->element_compare(element, current->element) == 0)
        {
            return i;
        }
        if (current->next == NULL)
        {
            return -1;
        }
    }
}

dplist_node_t *dpl_get_first_reference(dplist_t *list)
{
    return dpl_get_reference_at_index(list, 0);
}

dplist_node_t *dpl_get_last_reference(dplist_t *list) 
{
    return dpl_get_reference_at_index(list,dpl_size(list));
}

dplist_node_t *dpl_get_reference_of_element(dplist_t *list, void *element)
{
    if(list == NULL) return NULL;
    if(list->head == NULL) return NULL;
    dplist_node_t* current = list->head;
    for (size_t i = 0; element != current->element && current->next != NULL; i++, current = current->next) { }
    if(current->element != element) return NULL;
    return current;
}

dplist_node_t *dpl_get_next_reference(dplist_t *list, dplist_node_t *reference)// maybe check if the element is in list
{
    if (list == NULL) return NULL;
    if (list->head == NULL) return NULL;
    if (reference == NULL) return NULL;
    if(reference->next != NULL) return dpl_get_reference_of_element(list,reference->next->element);
    return NULL;
}

dplist_node_t *dpl_get_previous_reference(dplist_t *list, dplist_node_t *reference)
{
    if (list == NULL) return NULL;
    if (list->head == NULL) return NULL;
    if (reference == NULL) return NULL;
    if(reference->prev != NULL) return dpl_get_reference_of_element(list,reference->prev->element);
    return NULL;
}

int dpl_get_index_of_reference(dplist_t *list, dplist_node_t *reference) 
{
    if (list == NULL) return -1;
    if (list->head == NULL) return -1;
    if (reference == NULL) return -1;
    return dpl_get_index_of_element(list, reference->element);
}

dplist_t *dpl_insert_at_reference(dplist_t *list, void *element, dplist_node_t *reference, bool insert_copy) 
{
    if(list == NULL) return NULL;
    if(list->head == NULL) return NULL;
    if(reference == NULL) return NULL;
    dplist_node_t* current = list->head;
    size_t i = 0;
    for (i = 0; 1 ; i++, current = current->next) 
    { 
        if(current == reference) return dpl_insert_at_index(list,element,i,insert_copy);
        if(current != reference && current->next == NULL) return list;
    }
    return dpl_insert_at_index(list,element,i,insert_copy);
}

dplist_t *dpl_remove_at_reference(dplist_t *list, dplist_node_t *reference, bool free_element)
{
    if(reference == NULL) return NULL;
    if(list == NULL) return NULL;
    if(dpl_get_index_of_reference(list,reference) == -1) return list;
    return dpl_remove_at_index(list,dpl_get_index_of_reference(list,reference),free_element);
}

dplist_t *dpl_remove_element(dplist_t *list, void *element, bool free_element)
{
    if(element == NULL) return NULL;
    if(list == NULL) return NULL;
    if(dpl_get_index_of_element(list,element) == -1) return list;
    return dpl_remove_at_index(list,dpl_get_index_of_element(list,element),free_element); 
}



void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference)
{
    if(reference == NULL) return NULL;
    if(list == NULL) return NULL;
    if(list->head == NULL) return NULL;
    if(dpl_get_index_of_element(list, reference->element) == -1) return NULL;
    return reference->element;
}

dplist_t *dpl_insert_sorted(dplist_t *list, void *element, bool insert_copy)
{
    if(list == NULL) return NULL; 
    if(list->head == NULL) return dpl_insert_at_index(list,element,0,insert_copy);
    if(element == NULL) return NULL;
    int i = 0;
    if(insert_copy)
    {
        for(dplist_node_t* current = list->head; 1 ; i++, current = current->next)
        {
            if (list->element_compare(element, current->element) == 0 || list->element_compare(current->element, element) == 1)
            {
                return dpl_insert_at_index(list,element,i,insert_copy);
            }
            if (current->next == NULL)
            {
                return dpl_insert_at_index(list,element,i+1,insert_copy);
            }
        }
    }
    if(insert_copy != true)
    {
        for(dplist_node_t* current = list->head; 1 ; i++, current = current->next)
        {
            if (element <= current->element) 
            {
                return dpl_insert_at_index(list,element,i,insert_copy);
            }
            if (current->next == NULL)
            {
                return dpl_insert_at_index(list,element,i+1,insert_copy);
            }
        }
    }
    return NULL;
}