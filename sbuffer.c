/**
 * \author Ingmar Malfait
 */
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "sbuffer.h"

/**
 * basic node for the buffer, these nodes are linked together to create the buffer
 */

sbuffer_node_t* sbuffer_get_first_node(sbuffer_t **buffer)
{
    if (*buffer == NULL) return NULL;
    if ((*buffer)->head == NULL) return NULL;
    if ((*buffer)->tail == NULL) return NULL;
    return (*buffer)->head;
}

sbuffer_node_t* sbuffer_get_last_node(sbuffer_t **buffer)
{
    if (*buffer == NULL) return NULL;
    if ((*buffer)->head == NULL) return NULL;
    if ((*buffer)->tail == NULL) return NULL;
    return (*buffer)->tail;
}

sensor_data_t* sbuffer_get_first_node_data(sbuffer_t **buffer)
{
    if (*buffer == NULL) return NULL;
    if ((*buffer)->head == NULL) return NULL;
    if ((*buffer)->tail == NULL) return NULL;
    return &(*buffer)->head->data;
}

sensor_data_t* sbuffer_get_last_node_data(sbuffer_t **buffer)
{
    if (*buffer == NULL) return NULL;
    if ((*buffer)->head == NULL) return NULL;
    if ((*buffer)->tail == NULL) return NULL;
    return &(*buffer)->tail->data;
}

int sbuffer_init(sbuffer_t **buffer) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;
    return SBUFFER_SUCCESS;
}

int sbuffer_free(sbuffer_t **buffer) {
    sbuffer_node_t *dummy = NULL;
    if ((buffer == NULL) || (*buffer == NULL)) {
        return SBUFFER_FAILURE;
    }
    while ((*buffer)->head) {
        dummy = (*buffer)->head;
        (*buffer)->head = (*buffer)->head->next;
        free(dummy);
    }
    free(*buffer);
    *buffer = NULL;
    return SBUFFER_SUCCESS;
}

int sbuffer_remove(sbuffer_t *buffer) {
    sbuffer_node_t *dummy;
    if (buffer == NULL) return SBUFFER_FAILURE;
    if (buffer->head == NULL) return SBUFFER_NO_DATA;
    dummy = buffer->head;
    if (buffer->head == buffer->tail) // buffer has only one node
    {
        buffer->head = buffer->tail = NULL;
    } else  // buffer has many nodes empty
    {
        buffer->head = buffer->head->next;
    }
    free(dummy);
    //buffer = NULL;
    return SBUFFER_SUCCESS;
}

int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy;
    if (buffer == NULL) return SBUFFER_FAILURE;
    dummy = malloc(sizeof(sbuffer_node_t));
    if (dummy == NULL) return SBUFFER_FAILURE;
    dummy->data = *data;
    dummy->next = NULL;
    if (buffer->tail == NULL) // buffer empty (buffer->head should also be NULL
    {
        buffer->head = buffer->tail = dummy;
    } else // buffer not empty
    {
        buffer->tail->next = dummy;
        buffer->tail = buffer->tail->next;
    }
    return SBUFFER_SUCCESS;
}

void debug_print_buffer(sbuffer_t *buffer,FILE* fifo)
{
    if(buffer == NULL) return;
    printf("buffer => ");
    write_logger(fifo, "buffer => ", DEBUG_PRINT);
    sbuffer_node_t *dummy = NULL;
    dummy = buffer->head;
    for( ;dummy != NULL;dummy=dummy->next)
    {
        printf("|d:%d|s:%d|%d|",dummy->data.data_used,dummy->data.sql_used,dummy->data.id);
        if(DEBUG_PRINT)
        {
            char* temp_string;
            asprintf(&temp_string,"|d:%d|s:%d|%d|",dummy->data.data_used,dummy->data.sql_used,dummy->data.id);
            write_logger(fifo, temp_string, DEBUG_PRINT);
            free(temp_string);
        } 
    }
    printf("<= buffer\n");
    write_logger(fifo, "<= buffer\n", DEBUG_PRINT);
    fflush(stdout);
}

void write_logger(FILE* fifo, char* message, bool not_debugging)
{
    if(fifo == NULL) return;
    mkfifo(FIFO_NAME, 0666);
    char* buffer;
    asprintf(&buffer, "%s", message);
    if (not_debugging && fputs( buffer, fifo ) == EOF )
    {
        fprintf( stderr, "Error writing data to fifo\n");
        exit( EXIT_FAILURE );
    }
    fflush(fifo);
    free(buffer);
}