/**
 * \author Ingmar Malfait
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#ifndef RUN_AVG_LENGTH
#define RUN_AVG_LENGTH 5
#endif

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef uint16_t sensor_id_t;
typedef double sensor_value_t;
typedef time_t sensor_ts_t;         // UTC timestamp as returned by time() - notice that the size of time_t is different on 32/64 bit machine
typedef int sensor_count_t; 

/**
 * structure to hold sensor data
 */
typedef struct {
    sensor_id_t id;         /** < sensor id */
    sensor_id_t id_room;
    sensor_value_t value;   /** < sensor value */
    sensor_ts_t ts;         /** < sensor timestamp */
    sensor_value_t temp_temp[RUN_AVG_LENGTH];
    sensor_count_t run_length;
    bool sql_used;
    bool data_used;
} sensor_data_t;


#endif /* _CONFIG_H_ */
