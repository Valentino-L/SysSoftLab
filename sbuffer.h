/**
 * \author Ingmar Malfait
 */
#include <semaphore.h> 

#define FIFO_NAME 	"logFifo" 
#define MAX 		240

#ifndef DEBUG_PRINT
#define DEBUG_PRINT 0 //0 for no debug printing and 1 for debug printing
#endif

#ifndef DB_NAME
#define DB_NAME Sensor.db
#endif

#ifndef TABLE_NAME
#define TABLE_NAME SensorData
#endif

#ifndef RUN_AVG_LENGTH
#define RUN_AVG_LENGTH 5
#endif
 
#ifndef SET_MAX_TEMP 
#error SET_MAX_TEMP not set
#endif

#ifndef SET_MIN_TEMP
#error SET_MIN_TEMP not set
#endif

#ifndef TIMEOUT 
#error TIMEOUT not set
#endif

#ifndef _SBUFFER_H_
#define _SBUFFER_H_

#include <stdio.h>
#include "config.h"

#define SBUFFER_FAILURE -1
#define SBUFFER_SUCCESS 0
#define SBUFFER_NO_DATA 1

typedef struct sbuffer sbuffer_t;
typedef struct sbuffer_node sbuffer_node_t;

typedef struct sbuffer_node 
{
    struct sbuffer_node *next;  /**< a pointer to the next node*/
    sensor_data_t data;         /**< a structure containing the data */
} sbuffer_node_t;

/**
 * a structure to keep track of the buffer
 */
typedef struct sbuffer {
    pthread_cond_t *condition;
    pthread_mutex_t *buffer_cleanup_lock;
    sem_t* sema_lock;
    int* thread_alive;
    sbuffer_node_t *head;       /**< a pointer to the first node in the buffer */
    sbuffer_node_t *tail;       /**< a pointer to the last node in the buffer */
} sbuffer_t;

/**
 * Allocates and initializes a new shared buffer
 * \param buffer a double pointer to the buffer that needs to be initialized
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occurred
 */
int sbuffer_init(sbuffer_t **buffer);

/**
 * All allocated resources are freed and cleaned up
 * \param buffer a double pointer to the buffer that needs to be freed
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occurred
 */
int sbuffer_free(sbuffer_t **buffer);

/**
 * Removes the first sensor data in 'buffer' (at the 'head') and returns this sensor data as '*data'
 * If 'buffer' is empty, the function doesn't block until new sensor data becomes available but returns SBUFFER_NO_DATA
 * \param buffer a pointer to the buffer that is used
 * \param data a pointer to pre-allocated sensor_data_t space, the data will be copied into this structure. No new memory is allocated for 'data' in this function.
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occurred
 */
int sbuffer_remove(sbuffer_t *buffer);

/**
 * Inserts the sensor data in 'data' at the end of 'buffer' (at the 'tail')
 * \param buffer a pointer to the buffer that is used
 * \param data a pointer to sensor_data_t data, that will be copied into the buffer
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
*/
int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data);

sensor_data_t* sbuffer_get_last_node_data(sbuffer_t **buffer);

sensor_data_t* sbuffer_get_first_node_data(sbuffer_t **buffer);

void debug_print_buffer(sbuffer_t *buffer,FILE* fifo);

void write_logger(FILE* fifo, char* message, bool not_debugging);

sbuffer_node_t* sbuffer_get_first_node(sbuffer_t **buffer);

sbuffer_node_t* sbuffer_get_last_node(sbuffer_t **buffer);

#endif  //_SBUFFER_H_
