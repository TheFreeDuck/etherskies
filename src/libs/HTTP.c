#include "HTTP.h"
#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include "jansson.h"


char* http_get(city_node_t *city_node) {
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Curled returned NULL\n");
        return NULL;
    }
    
    

    http_membuf_t chunk = {0};

    curl_easy_setopt(curl, CURLOPT_URL, city_node->data->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk); /* This is chunk in http_write_data */
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); /* Follow redirects */

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Curl performed bad: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return NULL;
    }

    curl_easy_cleanup(curl);


    /* Return buffer of recived data and size of buffer */
    return chunk.data;
}

size_t http_write_data(void *buffer, size_t size, size_t nmemb, void *userp) {
    /* Call back function */
    size_t bytes = size * nmemb;
    printf("\nRecived chunk: %zu bytes\n", bytes);

    /* We grow the buffer with this size! */
    http_membuf_t* mem_t = userp;
    /* Grow buffer with recived chunk
       make room for nullterminator too! */
    char *ptr = realloc(mem_t->data, mem_t->size + bytes + 1);
    if (!ptr) {
        printf("Returned 0 to CURL!\n");
        return 0;
    }
    mem_t->data = ptr;
    /* Copies the new data into the buffer
       Data points to the start off the buffer
       size says how many bytes already exists
       New contents gets added to the end off buffer
    */
    memcpy(mem_t->data + mem_t->size, buffer, bytes);
    mem_t->size += bytes;
    mem_t->data[mem_t->size] = '\0'; /* nullterminator */
    return bytes;
}

int http_json_parse(char* http_response, city_node_t* city_node) {
    json_error_t error;
    json_t *root = json_loads(http_response, 0, &error);

    if (!root) {
        fprintf(stderr, "JSON error at line %d: %s\n", error.line, error.text);
        return -1;
    }

    json_t *current_weather = json_object_get(root, "current");
    if (!json_is_object(current_weather)) {
        json_decref(root);
        return -1;
    }

    json_t *temperature = json_object_get(current_weather, "temperature_2m");
    json_t *windspeed   = json_object_get(current_weather, "wind_speed_10m");
    json_t *rel_humidity= json_object_get(current_weather, "relative_humidity_2m");

    if (json_is_number(temperature)) city_node->data->temp = json_number_value(temperature);
    if (json_is_number(windspeed)) city_node->data->windspeed = json_number_value(windspeed);
    if (json_is_number(rel_humidity)) city_node->data->rel_hum = json_number_value(rel_humidity);

    json_decref(root);
    return 0;
}
