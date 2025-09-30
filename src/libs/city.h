#ifndef __CITY_H_
#define __CITY_H_

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

/* ----- Error handling ----- */
typedef enum status_code {
  STATUS_OK,
  STATUS_FAIL,
} status_code_t;

/* ----- Public API ----- */

typedef struct city_data city_data_t;
struct city_data {
    char* name;       // City name
    char* url;        // API URL
    double lat;
    double lon;
    double temp;
    double windspeed;
    double rel_hum;
    time_t cached_at;      // timestamp of last update (optional)
};

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
    unsigned int size; 
};

/* ----- Public Functions ----- */

int city_init(city_list_t** city_list);
// city_list_t* city_make_list();
city_node_t* city_make_node(city_data_t* city_data);
city_node_t* city_get(city_list_t* city_list);
void city_data_free(city_data_t* city_data);
void city_free_list(city_list_t* city_list);
//void city_boot(city_list_t* city_list);
int city_save_cache(city_data_t* city_data);
int city_read_cache(city_list_t* city_list);
int city_cache_age_seconds(char *filepath);
int city_load_cache(city_node_t* city_node, char* fp);

#endif /* __CITY_H_ */