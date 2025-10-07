/* HTTP.h */

#define DATA_MAX_AGE_S 900
#ifndef __HTTP_H_
#    define __HTTP_H_

#    include "city.h"
#    include "meteo.h"
#    include <stdio.h>

/* ----- Struct for CURL callback ----- */
typedef struct http_membuf http_membuf_t;
struct http_membuf {
    char*  data;
    size_t size;
};

/* ----- Public functions ----- */
int    http_get_weather_data(city_node_t* city_node);
char*  http_get(city_node_t* city_node);
size_t http_write_data(void* buffer, size_t size, size_t nmemb, void* userp);
int    http_json_parse(char* http_response, city_node_t* city_node);
int    http_is_old(city_node_t* city_node);

#endif /* __HTTP_H_ */