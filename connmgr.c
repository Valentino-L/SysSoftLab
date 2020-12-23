/**
 * \author Ingmar Malfait
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>  

#include "config.h"
#include "connmgr.h"
#include "./lib/tcpsock.h"
#include "./lib/dplist.h"
#include "poll.h"
#include "sbuffer.h"
#include "errmacros.h" //this is the professors code as taken from Toledo


/**
 * Implements a sequential test server (only one connection at the same time)
 */

FILE * sensor_data_recv = NULL; 
tcpsock_t *server;
dplist_t* poll_fd;
dplist_t* socket_node_list;
FILE* fifo;

void connmgr_listen(int port_number, sbuffer_t* buffer)
{
    printf("----------------------BEGINING OF CONNMGR----------------------\n");
    int result = mkfifo(FIFO_NAME, 0666);
    CHECK_MKFIFO(result); 
    fifo = fopen(FIFO_NAME, "w");
    FILE_OPEN_ERROR(fifo);
    sensor_data_recv = fopen("sensor_data_recv","w+");
    setbuf(sensor_data_recv, NULL);
    poll_fd = dpl_create(&element_copy_poll,&element_free_poll,&element_compare_poll);
    socket_node_list = dpl_create(&element_copy_node,&element_free_node,&element_compare_node);
    int i;
    int alive = 0;
    int a;
    int bytes;
    int server_attempts = 0;

    printf("CONNMANAGER: Test server is started\n");
    if(tcp_passive_open(&server, port_number) != TCP_NO_ERROR) exit(EXIT_FAILURE); //starts the server

    pollfd_t* temp_poll = NULL;
    temp_poll = malloc(sizeof(pollfd_t));
    memset(temp_poll, 0, sizeof(pollfd_t));
    temp_poll->events = POLLIN;
    tcp_get_sd(server,&a);
    temp_poll->fd=a;
    
    dpl_insert_at_index(poll_fd,temp_poll,0,true);
    free(temp_poll);
    alive++;
    while(alive>0 && *(buffer->thread_alive)) // keep polling and reading until all pipes are closed
    {
        if(DEBUG_PRINT)
        {
            printf("CONNMANAGER: nodelist=>");
            for (int j = 0; j < dpl_size(socket_node_list); j++)
            {
                tcp_node_t* temp_node = (tcp_node_t*)dpl_get_element_at_index(socket_node_list,j);
                char* temp_char;
                asprintf(&temp_char,"||%ld|%d|%p||",temp_node->last_active,temp_node->sensor_id,temp_node->socket);
                printf("%s",temp_char);
                //write_logger(fifo,temp_char);
                free(temp_char);
            }
            printf("<= nodelist\n");

            write_logger(fifo, "CONNMANAGER: polllist=>", DEBUG_PRINT);
            printf("CONNMANAGER: polllist=>");
            for (int j = 0; j < dpl_size(poll_fd); j++)
            {
                pollfd_t* temp_poll_2 = (pollfd_t*)dpl_get_element_at_index(poll_fd,j);
                char* temp_string;
                asprintf(&temp_string, "||%d|%hd|%hd||",temp_poll_2->fd,temp_poll_2->events,temp_poll_2->revents);
                printf("%s",temp_string);
                write_logger(fifo, temp_string, DEBUG_PRINT);
                free(temp_string);
            }
            write_logger(fifo, "<= polllist\n", DEBUG_PRINT);
            printf("<= polllist\n");
        }

        if(server_attempts == 2)
        {
            printf("CONNMANAGER: server has been idle for too long (two attempts)\n");
            write_logger(fifo, "CONNMANAGER: server has been idle for too long (two attempts)\n", DEBUG_PRINT);
            alive = 0;
        }

        for (int j = 0; j < dpl_size(socket_node_list); j++)
        {
            tcp_node_t* temp_sock = ((tcp_node_t*)(dpl_get_element_at_index(socket_node_list,j)));
            time_t temp_ts = 0;
            int temp_sd = 0;
            tcp_get_last_active(temp_sock,&temp_ts);
            tcp_get_sd(temp_sock->socket,&temp_sd);
            if (temp_ts <= (time(NULL) - TIMEOUT) && temp_ts != 0)
            {
                char* temp_string = NULL;
                asprintf(&temp_string,"CONNMANAGER: socket %d has been idle for too long \n", temp_sd);
                write_logger(fifo, "CONNMANAGER: socket %d has been idle for too long \n", DEBUG_PRINT);
                printf("%s",temp_string);
                free(temp_string);
                tcp_close(&((tcp_node_t*)dpl_get_element_at_index(socket_node_list,j))->socket);
                dpl_remove_at_index(socket_node_list,j,true);
                dpl_remove_at_index(poll_fd,j+1,true);
                alive--;
                printf("CONNMANAGER: connection closed\n");
            }
        }
        
        struct pollfd poll_fd_temp[dpl_size(poll_fd)+1];
        
        for(int y = 0; y < dpl_size(poll_fd)+1;y++)//conversion from DPLIST TO ARRAY
        {
            poll_fd_temp[y] = *(pollfd_t*)dpl_get_element_at_index(poll_fd,y);
        }

        int result = -99;

        if(server_attempts < 2) result = poll(poll_fd_temp,alive, TIMEOUT*1000);
        
        for(int y = 0; y < dpl_size(poll_fd);y++)//conversion from array to dplist
        {
            dpl_remove_at_index(poll_fd,y,true);
            pollfd_t* temp_poll = NULL;
            temp_poll = malloc(sizeof(pollfd_t));
            memset(temp_poll, 0, sizeof(pollfd_t));
            temp_poll->events = poll_fd_temp[y].events;
            temp_poll->revents = poll_fd_temp[y].revents;
            temp_poll->fd = poll_fd_temp[y].fd;
            dpl_insert_at_index(poll_fd,temp_poll,y,true);
            free(temp_poll);
        }

        if(result == -1)
        {
            printf("CONNMANAGER: problem while polling\n"); 
            fflush(stdout);
            exit(1);
        }

        for (i = 0; i < dpl_size(poll_fd); i++)
        {
            pollfd_t* temp_poll_2 = NULL;
            temp_poll_2 = (pollfd_t*)dpl_get_element_at_index(poll_fd,0);
            if(i==0 && temp_poll_2->revents == POLLIN)//ask server if there is a new tcp connection to connect
            {
                server_attempts = 0;
                printf("CONNMANAGER: waiting for new socket\n");
                fflush(stdout);
                tcp_node_t *temp_client = malloc(sizeof(tcp_node_t));
                temp_client->last_active = 0;
                temp_client->sensor_id = 1;
                temp_client->socket = NULL;
                if(tcp_wait_for_connection(server, &(temp_client->socket))!=TCP_NO_ERROR)
                {
                    exit(1);
                }
                printf("CONNMANAGER: got a new socket\n");
                tcp_get_sd((temp_client->socket),&a);
                pollfd_t* temp_poll = NULL;
                temp_poll = malloc(sizeof(pollfd_t));
                memset(temp_poll, 0, sizeof(pollfd_t));
                temp_poll->revents = 0;
                temp_poll->events = POLLIN;
                temp_poll->fd=a;
                dpl_insert_at_index(poll_fd,temp_poll,alive,true);
                dpl_insert_at_index(socket_node_list,temp_client,alive-1,true);
                free(temp_poll);
                free(temp_client);
                printf("CONNMANAGER: inserted new socket\n");
                alive++;
            }
            if(i==0 && temp_poll_2->revents != POLLIN)
            {
                server_attempts++;
            }
            if(i > 0)
            {
                sensor_data_t* data = NULL;
                data = malloc(sizeof(sensor_data_t));
                memset(data, 0, sizeof(sensor_data_t));
                bytes = sizeof(data->id);
                temp_poll_2 = (pollfd_t*)dpl_get_element_at_index(poll_fd,i);
                if(temp_poll_2->revents == POLLIN) 
                {
                    tcp_node_t* temp_node = dpl_get_element_at_index(socket_node_list,i-1);
                    tcp_receive(temp_node->socket,&(data->id), &bytes);
                    bytes = sizeof(data->value);
                    tcp_receive(temp_node->socket,&(data->value), &bytes);
                    bytes = sizeof(data->ts);
                    result = tcp_receive(temp_node->socket,&(data->ts), &bytes);
                    if (result == TCP_NO_ERROR)
                    {
                        server_attempts = 0;
                        fprintf(sensor_data_recv,"%hu %lf %ld\n",data->id,data->value, data->ts);
                        sbuffer_insert(buffer,data);
                        printf("CONNMANAGER: Readers you are clear to read\n");
                        pthread_cond_signal(buffer->condition);
                        char* temp_string = NULL;
                        asprintf(&temp_string,"CONNMANAGER: TCP connection %d wrote the data to buffer %hu,%lf,%ld\n",i,data->id,data->value, data->ts);
                        write_logger(fifo,temp_string,DEBUG_PRINT);
                        printf("%s",temp_string);
                        free(temp_string);
                        
                        tcp_node_t* temp_node = (tcp_node_t*)dpl_get_element_at_index(socket_node_list,i-1);
                        if(temp_node->last_active == 0)
                        {
                            char* temp_string = NULL;
                            asprintf(&temp_string,"A sensor node with %d has opened a new connection\n",data->id);
                            printf("%s",temp_string);
                            write_logger(fifo,temp_string, true);
                            free(temp_string);
                            temp_node->last_active = time(NULL);
                            temp_node->sensor_id = data->id;
                        }

                        asprintf(&temp_string,"CONNMANAGER: lastactive set %ld \n",set_last_active((tcp_node_t*)dpl_get_element_at_index(socket_node_list,i-1)));
                        free(temp_string);
                    }
                    if (result == TCP_CONNECTION_CLOSED) 
                    {
                        tcp_node_t* temp_node = (tcp_node_t*)dpl_get_element_at_index(socket_node_list,i-1);
                        char* temp_string = NULL;
                        asprintf(&temp_string,"The sensor node with %d has closed the connection\n",temp_node->sensor_id);
                        printf("%s",temp_string);
                        write_logger(fifo,temp_string, true);
                        free(temp_string);
                        tcp_close(&(temp_node->socket));
                        dpl_remove_at_index(socket_node_list,i-1,true);
                        dpl_remove_at_index(poll_fd,i,true);
                        alive--;
                    }
                    if(result != TCP_NO_ERROR && result != TCP_CONNECTION_CLOSED)
                    {
                        char* temp_string = NULL;
                        asprintf(&temp_string,"CONNMANAGER: something went wrong while receiving data with err code : %d\n",result);
                        free(temp_string);
                    }
                }
                free(data);
            }
        }
    }
    *(buffer->thread_alive) = 0;
    pthread_cond_signal(buffer->condition);
    fclose(fifo);
    printf("CONNMANAGER: Test server is shutting down\n");
    fflush(stdout);
}

void connmgr_free()
{
    printf("CONNMANAGER: freeing everything\n");
    fclose(sensor_data_recv);
    tcp_close(&server);
    dpl_free(&poll_fd,true);
    dpl_free(&socket_node_list,true);
    printf("CONNMANAGER: freed everything\n");
    fflush(stdout);
}

//callbacks

void * element_copy_poll(void * element) 
{
    pollfd_t* copy = NULL;
    copy = malloc(sizeof(pollfd_t));
    copy->events = ((pollfd_t*)element)->events;
    copy->fd = ((pollfd_t*)element)->fd;
    copy->revents = ((pollfd_t*)element)->revents;
    return (void *) copy;
}

void element_free_poll(void ** element) 
{
    free(*element);
    *element = NULL;
}

int element_compare_poll(void * x, void * y) 
{
    return ((((pollfd_t*)x)->fd < ((pollfd_t*)y)->fd) ? -1 : (((pollfd_t*)x)->fd == ((pollfd_t*)y)->fd) ? 0 : 1);
}

int tcp_get_last_active(tcp_node_t* socket, __time_t *last_active)
{
    if(socket == NULL) return -1;
    *last_active = socket->last_active;
    return 0;
}

time_t set_last_active(tcp_node_t *socket) 
{
    return socket->last_active = time(NULL);
}

void* element_copy_node(void * element)
{
    tcp_node_t* copy = NULL;
    copy = malloc(sizeof(tcp_node_t));
    copy->last_active = ((tcp_node_t*)element)->last_active;
    copy->sensor_id = ((tcp_node_t*)element)->sensor_id;
    copy->socket = ((tcp_node_t*)element)->socket;
    //TODO check if valgrind is angry at not copying ip (char*)
    return (void *) copy;
}
void element_free_node(void ** element)
{
    free(*element);
    *element = NULL;
}
int element_compare_node(void * x, void * y)
{
    return ((((tcp_node_t*)x)->last_active < ((tcp_node_t*)y)->last_active) ? -1 : (((tcp_node_t*)x)->last_active == ((tcp_node_t*)y)->last_active) ? 0 : 1);
}