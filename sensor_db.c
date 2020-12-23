#define _GNU_SOURCE
#include <pthread.h>
#include <assert.h>
#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "config.h"
#include "sensor_db.h"
#include <sqlite3.h>
#include <unistd.h>
#include <string.h>
#include "sbuffer.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 

#include "errmacros.h" //this is the professors code as taken from Toledo

int result = -1;
FILE* fifo_logger = NULL;

int connect_to_database(char* db_name, sqlite3** db)
{
    int rc = sqlite3_open(db_name, db);
    if(rc == 0)
    {
        printf("succesfully opened database \n");
        write_logger(fifo_logger,"SQL: Connection to SQL server established.\n", true);
    }
    else
    {
        fprintf(stderr, "SQL: Unable to connect to SQL server.\n");
        printf("I'll retry after five seconds\n");
        usleep(5000000);
        char *logger_message = NULL;
        asprintf(&logger_message, "SQL: Unable to connect to SQL server.\n");
        write_logger(fifo_logger, logger_message, true);
        free(logger_message);
        sqlite3_close(*db);
    }
    return rc;
}

DBCONN *init_connection(char clear_up_flag, sbuffer_t* buffer)
{
    result = mkfifo(FIFO_NAME, 0666);
    CHECK_MKFIFO(result); 
    fifo_logger = fopen(FIFO_NAME, "w");
    FILE_OPEN_ERROR(fifo_logger);
    sqlite3 *db; 
    char *err_msg = 0;
    int rc = -1;
    int i = 0;
    do
    {
        if(i==3)
        {
            *(buffer->thread_alive) = 0;
            sqlite3_free(err_msg);
            return NULL;
        } 
        printf("SQL: attempt %d ", i+1);
        i++;
    } while(connect_to_database(TO_STRING(DB_NAME), &db) != 0);

    char *full_query = NULL;

    if (clear_up_flag == 0)
    {
        asprintf(&full_query, "CREATE TABLE IF NOT EXISTS %s(id INTEGER PRIMARY KEY AUTOINCREMENT, sensor_id INTEGER, sensor_value NUMERIC, timestamp INTEGER);", TO_STRING(TABLE_NAME));
        printf("SQL: we query :%s\n",full_query);
        rc = sqlite3_exec(db, full_query, 0, 0, &err_msg);
    }

    if (clear_up_flag == 1)
    {
        asprintf(&full_query, "DROP TABLE IF EXISTS %s; CREATE TABLE %s(id INTEGER PRIMARY KEY AUTOINCREMENT, sensor_id INTEGER, sensor_value NUMERIC, timestamp INTEGER);", TO_STRING(TABLE_NAME),TO_STRING(TABLE_NAME));
        printf("SQL: we query :%s\n",full_query);
        rc = sqlite3_exec(db, full_query, 0, 0, &err_msg);
    }

    char* temp_string;
    asprintf(&temp_string,"New table %s created.\n", TO_STRING(TABLE_NAME));
    write_logger(fifo_logger, temp_string, true);
    free(temp_string);

    free(full_query);

    if (rc != SQLITE_OK ) 
    {    
        fprintf(stderr, "Connection to SQL server lost.\n");
        char *logger_message = NULL;
        asprintf(&logger_message, "Connection to SQL server lost.\n");
        write_logger(fifo_logger, logger_message, true);
        free(logger_message);
        return NULL;
    }
    sqlite3_free(err_msg);
    return db;
}

void disconnect(DBCONN *conn)
{
    sqlite3_close(conn);
}

int insert_sensor(DBCONN *conn, sensor_id_t id, sensor_value_t value, sensor_ts_t ts)
{
    if(conn == NULL)
    {
        printf("SQL: something went wrong while inserting a sensor, the connection was NULL\n");
        write_logger(fifo_logger,"SQL: something went wrong while inserting a sensor, the connection was NULL\n", DEBUG_PRINT);
        fflush(stdout);
        fclose(fifo_logger);
        return -1;
    } 
    char *err_msg = 0;
    char *full_query = NULL;
    char *temp_string = NULL;
    asprintf(&full_query, "INSERT INTO main.%s(sensor_id,sensor_value,timestamp) VALUES (%hd,%lf,%ld);",TO_STRING(TABLE_NAME),id,value,ts);
    asprintf(&temp_string, "SQL: we query :%s\n",full_query);
    printf("%s",temp_string);
    write_logger(fifo_logger,temp_string, DEBUG_PRINT);
    int rc = sqlite3_exec(conn, full_query, 0, 0, &err_msg);
    free(temp_string);
    free(full_query);
    sqlite3_free(err_msg);
    return check_connection(rc);
}

int insert_sensor_from_buffer(DBCONN *conn, sbuffer_t* buffer)
{
    printf("---------------START OF SQL---------------\n");
    uint16_t sensor_id_temp;
    long temp_time; 
    double temp_temp_value;

    if (conn == NULL)
    {
        printf("SQL: problem opening database\n");
        fclose(fifo_logger);
        return -1;
    }

    if (buffer == NULL)
    {
        printf("SQL: problem opening sensor data\n");
        fclose(fifo_logger);
        return -1;
    }

    printf("SQL: The sensor temp is inserted\n");
    sbuffer_node_t* iterating_sensor = NULL;
    while(*(buffer->thread_alive) || sbuffer_get_first_node(&buffer) != NULL)
    {
        sem_wait(buffer->sema_lock);
        if(sbuffer_get_first_node(&buffer) != NULL)
        {
            if(iterating_sensor == NULL && sbuffer_get_first_node(&buffer) != NULL) iterating_sensor = sbuffer_get_first_node(&buffer);

            if(sbuffer_get_first_node_data(&buffer)->sql_used && sbuffer_get_first_node_data(&buffer)->data_used)
            {
                printf("SQL: SQL and DATA already used this data and it will be deleted\n");
                fflush(stdout);
                sbuffer_remove(buffer);
                printf("SQL: removed: ");
                write_logger(fifo_logger, "SQL: removed: ", DEBUG_PRINT);
                debug_print_buffer(buffer, fifo_logger);
                if(sbuffer_get_first_node(&buffer) == NULL) iterating_sensor = NULL;
                if(iterating_sensor == NULL) iterating_sensor = sbuffer_get_first_node(&buffer);
            }
            
            if(sbuffer_get_first_node(&buffer) != NULL && !sbuffer_get_first_node_data(&buffer)->sql_used)
            {
                iterating_sensor = sbuffer_get_first_node(&buffer);
            }
            
            if(iterating_sensor != NULL && !iterating_sensor->data.sql_used)
            {
                sem_post(buffer->sema_lock);
                sensor_data_t* temp_sensor_data = NULL;
                temp_sensor_data = &iterating_sensor->data;
                sensor_id_temp = temp_sensor_data->id;
                temp_temp_value = temp_sensor_data->value;
                temp_time = temp_sensor_data->ts;
                insert_sensor(conn,sensor_id_temp,temp_temp_value,temp_time);
                sem_wait(buffer->sema_lock);
                temp_sensor_data->sql_used = true;
                write_logger(fifo_logger, "SQL: used: ", DEBUG_PRINT);
                printf("SQL: used: ");
                debug_print_buffer(buffer, fifo_logger);
            }
            if(iterating_sensor != NULL && iterating_sensor->next != NULL) iterating_sensor = iterating_sensor->next; 
        }
        else
        {
            if(*(buffer->thread_alive))
            {
                printf("SQL: I am waiting on a signal from connmngr\n");
                pthread_cond_wait(buffer->condition, buffer->buffer_cleanup_lock); 
            }
            else
            {
                printf("SQL: I no longer need to be alive, I will close now\n");
            }
        }
        sem_post(buffer->sema_lock);
    }
    pthread_cond_signal(buffer->condition);
    fclose(fifo_logger);

    printf("The sensor temp has been inserted\n");
    printf("---------------END OF SQL---------------\n");
    return 0;
}

int find_sensor_all(DBCONN *conn, callback_t f)
{
    if(conn == NULL) return -1;
    char *err_msg = 0;
    char *full_query = NULL;
    asprintf(&full_query, "SELECT * FROM %s",TO_STRING(TABLE_NAME));
    printf("SQL: we query :%s\n",full_query);
    int rc = sqlite3_exec(conn,full_query , f, 0, &err_msg);
    free(full_query);
    sqlite3_free(err_msg);
    return check_connection(rc);
}

int find_sensor_by_value(DBCONN *conn, sensor_value_t value, callback_t f)
{
    if(conn == NULL) return -1;
    char *err_msg = 0;
    char *full_query = NULL;
    asprintf(&full_query, "SELECT * FROM %s WHERE sensor_value == %f",TO_STRING(TABLE_NAME),value);
    printf("SQL: we query :%s\n",full_query);
    int rc = sqlite3_exec(conn,full_query , f, 0, &err_msg);
    free(full_query);
    sqlite3_free(err_msg);
    return check_connection(rc);
}

int find_sensor_exceed_value(DBCONN *conn, sensor_value_t value, callback_t f)
{
    if(conn == NULL) return -1;
    char *err_msg = 0;
    char *full_query = NULL;
    asprintf(&full_query, "SELECT * FROM %s WHERE sensor_value > %f",TO_STRING(TABLE_NAME),value);
    printf("SQL: we query :%s\n",full_query);
    int rc = sqlite3_exec(conn,full_query , f, 0, &err_msg);
    free(full_query);
    sqlite3_free(err_msg);
    return check_connection(rc);
}

int find_sensor_by_timestamp(DBCONN *conn, sensor_ts_t ts, callback_t f)
{
    if(conn == NULL) return -1;
    char *err_msg = 0;
    char *full_query = NULL;
    asprintf(&full_query, "SELECT * FROM %s WHERE timestamp == %ld",TO_STRING(TABLE_NAME),ts);
    printf("SQL: we query :%s\n",full_query);
    int rc = sqlite3_exec(conn,full_query , f, 0, &err_msg);
    free(full_query);
    sqlite3_free(err_msg);
    return check_connection(rc);
}

int find_sensor_after_timestamp(DBCONN *conn, sensor_ts_t ts, callback_t f)
{
    if(conn == NULL) return -1;
    char *err_msg = 0;
    char *full_query = NULL;
    asprintf(&full_query, "SELECT * FROM %s WHERE timestamp > %ld",TO_STRING(TABLE_NAME),ts);
    printf("SQL: we query :%s\n",full_query);
    int rc = sqlite3_exec(conn,full_query , f, 0, &err_msg);
    free(full_query);
    sqlite3_free(err_msg);
    return check_connection(rc);
}

int check_connection(int rc)
{
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Connection to SQL server lost.\n");
        char *logger_message = NULL;
        asprintf(&logger_message, "Connection to SQL server lost.\n");
        write_logger(fifo_logger, logger_message, true);
        free(logger_message);
        return -1;
    }
    else return 0;
}