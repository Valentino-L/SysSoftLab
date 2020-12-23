/**
 * \author Ingmar Malfait
 */
//#define _BSD_SOURCE
#define _GNU_SOURCE
#include <assert.h>
#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include "./lib/dplist.h"
#include "errmacros.h" //this is the professors code as taken from Toledo
#include "datamgr.h"
#include "config.h"
#include "sbuffer.h"

#define MOD(a,b) ((((a)%(b))+(b))%(b)) 
//this is the mathimatically correct mod, c is not sensitive to negative numbers
//I found this line (22) online on stackoverflow

dplist_t* sensor_list = NULL;
typedef struct sbuffer_node sbuffer_node_t;


void* element_copy(void * element);
void element_free(void ** element);
int element_compare(void * x, void * y);

void * element_copy(void * element) 
{
    sensor_data_t* copy = NULL;
    copy = malloc(sizeof(sensor_data_t));
    copy->id_room = ((sensor_data_t*)element)->id_room;
    copy->id = ((sensor_data_t*)element)->id;
    copy->value = ((sensor_data_t*)element)->value;
    copy->ts = ((sensor_data_t*)element)->ts;
    for (int i = 0; i < RUN_AVG_LENGTH; i++)
    {
        copy->temp_temp[i] = ((sensor_data_t*)element)->temp_temp[i];
    }
    copy->run_length = ((sensor_data_t*)element)->run_length;
    copy->data_used = ((sensor_data_t*)element)->data_used;
    copy->sql_used = ((sensor_data_t*)element)->sql_used;
    return (void *) copy;
}

void element_free(void ** element) 
{
    free(*element);
    *element = NULL;
}

int element_compare(void * x, void * y) 
{
    return ((((sensor_data_t*)x)->value < ((sensor_data_t*)y)->value) ? -1 : (((sensor_data_t*)x)->value == ((sensor_data_t*)y)->value) ? 0 : 1);
}

void datamgr_free()
{
    printf("DATAMANAGER: freeing everything\n");
    return;
}

uint16_t datamgr_get_room_id(sensor_id_t sensor_id_input)
{
    if(sensor_list==NULL) return -1; 
    if(sensor_id_input < 0) return -1;
    dplist_node_t* current = dpl_get_first_reference(sensor_list);
    sensor_data_t* element = dpl_get_element_at_index(sensor_list,0);
    for (size_t i = 0; i <= dpl_size(sensor_list); i++, current = dpl_get_next_reference(sensor_list,current)) 
    {
        element = dpl_get_element_at_index(sensor_list,i);
        if(element->id == sensor_id_input)
        {
            return element->id_room;
        }
    }
    return -1;
}

uint16_t datamgr_get_sensor_id_at_index(int index)
{
    if(sensor_list==NULL) return -1;
    if(index < 0) return -1;
    dplist_node_t* current = dpl_get_first_reference(sensor_list);
    sensor_data_t* element = dpl_get_element_at_index(sensor_list,0);
    int i = 0;
    for (i = 0; i != index; i++, current = dpl_get_next_reference(sensor_list,current)) {}
    element = dpl_get_element_at_index(sensor_list,i);
    return element->id;
}

sensor_value_t datamgr_get_avg(sensor_id_t sensor_id_input)
{
    if(sensor_list==NULL) return -1;
    if(sensor_id_input < 0) return -1;
    dplist_node_t* current = dpl_get_first_reference(sensor_list);
    sensor_data_t* element = dpl_get_element_at_index(sensor_list,0);
    for (size_t i = 0
    ;i <= dpl_size(sensor_list)
    ;i++, current = dpl_get_next_reference(sensor_list,current)) 
    {
        element = dpl_get_element_at_index(sensor_list,i);
        if(element->id == sensor_id_input)
        {
            return element->value;
        }
    }
    return -1;
}

time_t datamgr_get_last_modified(sensor_id_t sensor_id_input)
{
    if(sensor_list==NULL) return -1; 
    if(sensor_id_input < 0) return -1;
    dplist_node_t* current = dpl_get_first_reference(sensor_list);
    sensor_data_t* element = dpl_get_element_at_index(sensor_list,0);
    for (size_t i = 0; i <= dpl_size(sensor_list); i++, current = dpl_get_next_reference(sensor_list,current)) 
    {
        element = dpl_get_element_at_index(sensor_list,i);
        if(element->id == sensor_id_input)
        {
            return element->ts;
        }
    }
    return -1;
}

int datamgr_get_total_sensors()
{
    return dpl_size(sensor_list);
}

void datamgr_parse_buffer(FILE *fp_sensor_map, sbuffer_t* buffer)
{
    printf("---------------START OF DATAMANAGER---------------\n");
    sensor_list = dpl_create(&element_copy,&element_free,&element_compare);
    uint16_t id_sensor_temp;
    uint16_t id_room_temp;
    uint16_t sensor_id_temp = 0;
    long temp_time = 0; 
    double temp_value = 0;
    double temp_temp_value = 0;
    int result = 0; 

    result = mkfifo(FIFO_NAME, 0666);
    CHECK_MKFIFO(result);
    FILE* fifo = fopen(FIFO_NAME, "w");
    FILE_OPEN_ERROR(fifo);

    if (buffer == NULL)
    {
        printf("DATAMANAGER: problem getting sensor data\n");
    }
    if (fp_sensor_map == NULL)
    {
        printf("DATAMANAGER: problem opening room_sensor.map\n");
    } 
    printf("DATAMANAGER: The sensors data are inserted\n");
    sensor_data_t* sensor_data = NULL;
    for (int i = 0; fscanf(fp_sensor_map,"%hd %hd\n", &id_room_temp, &id_sensor_temp) != EOF; i++) 
    {
        sensor_data = malloc(sizeof(sensor_data_t));
        memset(sensor_data, 0, sizeof(sensor_data_t));
        sensor_data->id_room = id_room_temp;
        sensor_data->id = id_sensor_temp;
        dpl_insert_at_index(sensor_list,sensor_data,i,false);
    }
    printf("DATAMANAGER: The sensors data have been inserted\n");

    printf("DATAMANAGER: The sensor temps are inserted\n");
    sbuffer_node_t* iterating_sensor = NULL;
    while(*(buffer->thread_alive) || sbuffer_get_first_node(&buffer) != NULL)
    {
        sem_wait(buffer->sema_lock);
        if(sbuffer_get_first_node(&buffer) != NULL)
        {
            if(iterating_sensor == NULL && sbuffer_get_first_node(&buffer) != NULL) iterating_sensor = sbuffer_get_first_node(&buffer);

            if(sbuffer_get_first_node_data(&buffer)->sql_used && sbuffer_get_first_node_data(&buffer)->data_used)
            {
                printf("DATAMANAGER: SQL and DATA already used this data and it will be deleted\n");
                fflush(stdout);
                sbuffer_remove(buffer);
                printf("DATA: removed: ");
                write_logger(fifo, "DATA: removed: ", DEBUG_PRINT);
                debug_print_buffer(buffer, fifo);
                if(sbuffer_get_first_node(&buffer) == NULL) iterating_sensor = NULL;
                if(iterating_sensor == NULL) iterating_sensor = sbuffer_get_first_node(&buffer);
            }

            if(sbuffer_get_first_node(&buffer) != NULL && !sbuffer_get_first_node_data(&buffer)->data_used)
            {
                iterating_sensor = sbuffer_get_first_node(&buffer);
            }
            
            if(iterating_sensor != NULL && !iterating_sensor->data.data_used)
            {
                sem_post(buffer->sema_lock);
                sensor_data_t* temp_sensor_data = NULL;
                temp_sensor_data = &iterating_sensor->data;
                sensor_id_temp = temp_sensor_data->id;
                temp_temp_value = temp_sensor_data->value;
                temp_time = temp_sensor_data->ts;
                sensor_data_t* element = NULL;
                int j = 0;
                bool notfound = true;
                for(j = 0; j < datamgr_get_total_sensors() ; j++)
                { 
                    if(datamgr_get_sensor_id_at_index(j) == sensor_id_temp)
                    {
                        notfound = false;
                        char* temp_string = NULL;
                        asprintf(&temp_string,"DATAMANAGER: found sensor :%d in room: %d\n", datamgr_get_sensor_id_at_index(j), datamgr_get_room_id(sensor_id_temp));
                        printf("%s",temp_string);
                        free(temp_string);
                        element = dpl_get_element_at_index(sensor_list, j);
                        element->temp_temp[MOD(element->run_length,RUN_AVG_LENGTH)] = temp_temp_value;
                        element->run_length++;
                        if(element->ts < temp_time) element->ts = temp_time;
                        if(element->run_length < RUN_AVG_LENGTH) 
                        {
                            temp_value = 0;
                        }
                        else 
                        {
                            temp_value = 0;
                            for(int k = 0; k < RUN_AVG_LENGTH; k++)
                            {
                                temp_value = temp_value + element->temp_temp[MOD((k-RUN_AVG_LENGTH),(RUN_AVG_LENGTH))]/RUN_AVG_LENGTH;
                            }
                        }
                        element->value = temp_value;

                        printf("DATAMANAGER: "); 
                        for(int k = 0; k < RUN_AVG_LENGTH; k++)
                        {
                            char* temp_string; 
                            asprintf(&temp_string,"|%f|", element->temp_temp[k]);
                            printf("%s",temp_string);
                            free(temp_string);
                        } 
                        printf("\n");
                        printf("DATAMANAGER: avg: %f @ %d\n", element->value,element->id_room);
                        if(element->value > SET_MAX_TEMP && element->value != 0)  
                        {
                            char* temp_string;
                            asprintf(&temp_string,"The sensor node with %d reports it’s too hot (running avg temperature = %f)\n", element->id, element->value);
                            write_logger(fifo, temp_string, true);
                            //fprintf(stderr, "%s",temp_string);
                            free(temp_string);
                        }
                        if(element->value < SET_MIN_TEMP && element->value != 0) 
                        {
                            char* temp_string;
                            asprintf(&temp_string,"The sensor node with %d reports it’s too cold (running avg temperature = %f)\n", element->id, element->value);
                            write_logger(fifo, temp_string, true);
                            //fprintf(stderr, "%s",temp_string);
                            free(temp_string);
                        }
                    }
                    else
                    {
                        if(j == datamgr_get_total_sensors()-1 && notfound)
                        {
                            char* temp_string;
                            asprintf(&temp_string,"Received sensor data with invalid sensor node ID %d\n", sensor_id_temp);
                            write_logger(fifo, temp_string, true);
                            free(temp_string);
                        }
                    }
                }
                sem_wait(buffer->sema_lock);
                temp_sensor_data->data_used = true;
                
                printf("DATA: used: ");
                write_logger(fifo, "DATA: used: ", DEBUG_PRINT);
                debug_print_buffer(buffer, fifo);
            }

            if(iterating_sensor != NULL && iterating_sensor->next != NULL) iterating_sensor = iterating_sensor->next; 
            
        }
        else
        {
            if(*(buffer->thread_alive))
            {
                printf("DATA: I am waiting on a signal from connmngr\n");
                pthread_cond_wait(buffer->condition, buffer->buffer_cleanup_lock); 
            }
            else
            {
                printf("DATA: I no longer need to be alive, I will close now\n");
            }
        }
        sem_post(buffer->sema_lock);
    }
    printf("The sensor temp has been inserted\n");
    pthread_cond_signal(buffer->condition);
    fclose(fifo);
    dpl_free(&sensor_list, false);
    fclose(fp_sensor_map);
    printf("---------------END OF DATAMANAGER---------------\n");
    return;
}