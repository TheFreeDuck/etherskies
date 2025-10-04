/* city.h */

#ifndef __CITY_H_
#define __CITY_H_
#define INIT_VAL -1000.0

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

/* ----- Error handling ----- */
typedef enum status_code {
  STATUS_OK,
  STATUS_FAIL,
  STATUS_EXIT,
} status_code_t;

/* ----- Struct for keeping city data ----- */
typedef struct city_data city_data_t;
struct city_data {
    char* name;
    char* url;
    char* fp;
    double lat;
    double lon;
    double temp;
    double windspeed;
    double rel_hum;
    time_t cached_at;
};
/* ----- Structs for linked list ----- */
typedef struct city_node city_node_t;
struct city_node {
    city_data_t* data;
    city_node_t* prev;
    city_node_t* next;
};

typedef struct city_list city_list_t;
struct city_list {
    city_node_t* head;
    city_node_t* tail;
    unsigned size; 
};

/* ----- Public Functions ----- */
int city_init(city_list_t** city_list);
int city_print_list(city_list_t** city_list);
int city_get(city_list_t* city_list, city_node_t** out_city);
int city_save_cache(city_data_t* city_data);
int city_dispose(city_list_t** city_list);

#endif /* __CITY_H_ */