#define _GNU_SOURCE
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include "connmgr.h"
#include "sbuffer.h"
#include "config.h"
#include "datamgr.h"
#include "sensor_db.h"
#include "errmacros.h" //this is the professors code as taken from Toledo

void* writer_thread(void * arg);
void* data_thread(void * arg);
void* SQL_thread(void * arg);

sensor_data_t* dummy = NULL;
sbuffer_t* buff;
pthread_t w_thread;
pthread_t r_threads[2];
int thread_alive = 1;
int port = 1234;

pthread_cond_t condition;
pthread_mutex_t buffer_cleanup_lock;
sem_t sema_lock;

pid_t logger_pid;

int main(int argc, char const *argv[])
{
    if(argc == 2) 
    {
        port = atoi(argv[1]);
        printf("PORT = %d\n", port);
    }
    else 
    {
        printf("You typed in the port incorrectly and the port will be the default of 1234 now\n");
    }
    printf("DEBUGGING: %d (-DDEBUG_PRINT=1 for debugging)\n", DEBUG_PRINT);
    int result = mkfifo(FIFO_NAME, 0666);
    CHECK_MKFIFO(result);
    sem_init(&sema_lock, 0, 1);
    logger_pid = fork();
    if(logger_pid == 0)
    {
        char logger_buff[MAX];
        FILE* fifo = fopen(FIFO_NAME, "r");
        FILE_OPEN_ERROR(fifo);
        FILE* logger_file = fopen ("gateway.log","w");
        printf("syncing with writer ok\n");
        char* str_result = NULL;
        int incremental = 0;
        do 
        {
            str_result = fgets(logger_buff,MAX, fifo);
            if (str_result != 0)
            {
                time_t long_time;
                struct tm *struct_time;
                long_time = time(NULL);
                struct_time = localtime(&long_time);
                char* temp_time = asctime(struct_time);//time(NULL) also works if you want a normal long
                temp_time[strlen(temp_time)-1] = '0';
                fprintf(logger_file,"%d %s %s \n",incremental++,temp_time,logger_buff);
                fflush(logger_file);
            }
        } while ( str_result != 0 );
        fclose (fifo);
        fclose (logger_file);
    }
    else
    {
        FILE* fifo = fopen(FIFO_NAME, "w");
        while(1)
        {
            printf("---------------START OF MAIN---------------\n");
            printf("syncing with reader ok\n");

            write_logger(fifo, "MAIN: debugging is on and additional messages will be writen \n", DEBUG_PRINT);

            sbuffer_init(&buff);
            buff->buffer_cleanup_lock = &buffer_cleanup_lock;
            buff->condition = &condition;
            buff->sema_lock = &sema_lock;
            buff->thread_alive = &thread_alive;
            pthread_create(&w_thread,NULL,&writer_thread,NULL);
            pthread_create(&r_threads[0],NULL,&data_thread,NULL);
            pthread_create(&r_threads[1],NULL,&SQL_thread,NULL);

            pthread_join(w_thread,NULL);
            pthread_join(r_threads[0],NULL);
            pthread_join(r_threads[1],NULL);

            sbuffer_free(&buff);
            fclose(fifo);
            printf("---------------END OF MAIN---------------\n");
            pthread_exit(NULL);
            return 0;
        }
        
    }
    
}

void* writer_thread(void * arg)
{
    connmgr_listen(port, buff);
    connmgr_free();
    pthread_detach(w_thread);
    pthread_exit(NULL);
    return NULL;
}

void* data_thread(void * arg)
{
    FILE* sensor_map = fopen("room_sensor.map","r");
    datamgr_parse_buffer(sensor_map, buff);
    datamgr_free();
    pthread_detach(r_threads[0]);
    pthread_exit(NULL);
    return NULL;
}

void* SQL_thread(void * arg)
{
    DBCONN *conn = init_connection(1,buff);
    insert_sensor_from_buffer(conn,buff);
    disconnect(conn);
    pthread_detach(r_threads[1]);
    pthread_exit(NULL);
    return NULL;
}