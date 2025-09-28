#ifndef __CITY_H_
#define __CITY_H_

#include <stdio.h>

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
    double time;      // timestamp of last update (optional)
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

city_list_t* city_make_list();
city_node_t* city_make_node(city_data_t *city_data);
city_node_t* city_get(city_list_t *city_list);
void city_data_free(city_data_t* city_data);
void city_free_list(city_list_t *city_list);

/* Added functionality: city_parse now loads cache or bootstrap */
void city_parse(city_list_t* city_list);

/* Added functionality: cache helpers */
int city_save_cache(city_data_t *city_data);
int city_read_cache(city_list_t *city_list);

#endif /* __CITY_H_ */
