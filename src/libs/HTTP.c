/*
    HTTP.c contains functions that:
    - handles networking via libcurl
    - handles logic related to checking cache age
    - handles parsing JSON-string
*/

#include "HTTP.h"

#include "city.h"
#include "jansson.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ----- PRIVATE FUNCTIONS ----- */
int http_load_cache(city_node_t* city_node, char* fp);
int http_cache_age_seconds(char* filepath);

/* ------------------- */
/* ----- NETWORK ----- */
/*
http_get() uses standard CURL operations:
-calls http_write_data with every recived
-returns http_membuf_t data on success (dynamically allocated string)
*/
char* http_get(city_node_t* city_node) {

    CURL* curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Curled returned NULL\n");
        return NULL;
    }

    http_membuf_t chunk = {0};

    curl_easy_setopt(curl, CURLOPT_URL, city_node->data->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_data);
    /*This is *userp in http_write_data()*/
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Curl performed bad: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return NULL;
    }
    curl_easy_cleanup(curl);

    return chunk.data;
}

/*
http_write_data() is a callback used by libCURL. Each time a chunk of data
is received, it reallocates and appends it to the buffer in http_membuf_t.
*/
size_t http_write_data(void* buffer, size_t size, size_t nmemb, void* userp) {

    size_t bytes = size * nmemb;
    printf("\nRecived chunk: %zu bytes\n", bytes);
    /*Realloc with the size of recived chunk*/
    http_membuf_t* mem_t = userp;
    char*          ptr   = realloc(mem_t->data, mem_t->size + bytes + 1);
    if (!ptr) {
        printf("Returned 0 to CURL!\n");
        return STATUS_FAIL;
    }
    mem_t->data = ptr;
    /*Copies the new data into the buffer:
    data points to the start off the buffer -> size says how many bytes already
    exists
    -> new contents gets added to the end off buffer*/
    memcpy(mem_t->data + mem_t->size, buffer, bytes);
    mem_t->size += bytes;
    mem_t->data[mem_t->size] = '\0';

    return bytes;
}

/* ----- CACHING LOGIC ----- */
/*
http_get_weather_data handles logic for dealing with cache.
The priority is:
    1) data in struct is fresh
    2) cache files exist with fresh data
    3) no data in struct, no fresh cache data, fretch from network
If data needs to be fetched it calls http_get and the result is
passed to http_json_parse() which in turn passes its product to
city_save_cache().
*/
int http_get_weather_data(city_node_t* city_node) {

    /*Check if struct data is fresh*/
    if (city_node->data->temp != INIT_VAL && !http_is_old(city_node)) {
        printf("Using fresh in-memory data for %s (age %ld seconds).\n",
               city_node->data->name,
               (long)difftime(time(NULL), city_node->data->cached_at));
        return STATUS_OK;
    }

    /*Check if there is a file for city in cache and if the data is fresh*/
    int file_age = http_cache_age_seconds(city_node->data->fp);
    if (file_age >= 0 && file_age <= DATA_MAX_AGE_S) {

        if (http_load_cache(city_node, city_node->data->fp) == 0) {
            /*Check that data in fetched cache is not INIT_VAL*/
            if (city_node->data->temp != INIT_VAL) {
                printf("Using fresh cached file for %s (age %d seconds).\n",
                       city_node->data->name, file_age);
                return STATUS_OK;
            }
            printf("Cache exist but has no weather data\n");
        } else {
            fprintf(stderr, "Failed to read cached JSON for %s.\n",
                    city_node->data->name);
        }
    }

    /*All checks done, fetch from network*/
    printf("Data missing, old, or cache invalid. Fetching from Meteo...\n");
    char* http_response = http_get(city_node);
    if (!http_response) {
        fprintf(stderr, "HTTP request failed.\n");
        return STATUS_FAIL;
    }

    if (http_json_parse(http_response, city_node) != 0) {
        fprintf(stderr, "Failed to parse HTTP response.\n");
        free(http_response);
        return STATUS_FAIL;
    }

    if (city_save_cache(city_node->data) != 0)
        fprintf(stderr, "Failed to save cache for %s\n", city_node->data->name);

    free(http_response);

    return STATUS_OK;
}

/* ----- CACHING ----- */
/*
http_load_cache() loads weather/cache data for a city from a JSON file.
It updates the provided city_node's city_data_t fields (temp, windspeed,
rel_hum, cached_at) with the values found in the JSON file.
*/
int http_load_cache(city_node_t* city_node, char* fp) {
    if (!city_node || !fp) {
        return STATUS_FAIL;
    }

    json_error_t error;
    json_t*      root = json_load_file(fp, 0, &error);
    if (!root) {
        fprintf(stderr, "Failed to load JSON file %s: %s\n", fp, error.text);
        return STATUS_FAIL;
    }

    json_t* jtemp      = json_object_get(root, "temp");
    json_t* jwind      = json_object_get(root, "windspeed");
    json_t* jhum       = json_object_get(root, "rel_hum");
    json_t* jcached_at = json_object_get(root, "cached_at");

    if (jtemp && json_is_number(jtemp))
        city_node->data->temp = json_number_value(jtemp);
    if (jwind && json_is_number(jwind))
        city_node->data->windspeed = json_number_value(jwind);
    if (jhum && json_is_number(jhum))
        city_node->data->rel_hum = json_number_value(jhum);

    time_t cached = (jcached_at && json_is_integer(jcached_at))
                                     ? (time_t)json_integer_value(jcached_at)
                                     : 0;
    city_node->data->cached_at = cached;

    json_decref(root);
    return STATUS_OK;
}

/*
http_cache_age_seconds() fetches value from cached citys cached_at field and
calculates the age of the data which it then returns to caller on success.
*/
int http_cache_age_seconds(char* filepath) {
    if (!filepath) {
        return STATUS_FAIL;
    }

    json_error_t error;
    json_t*      root = json_load_file(filepath, 0, &error);
    if (!root) {
        return STATUS_FAIL;
    }

    json_t* jat = json_object_get(root, "cached_at");
    if (!json_is_integer(jat)) {
        json_decref(root);
        return STATUS_FAIL;
    }

    int age = (int)(time(NULL) - json_integer_value(jat));
    json_decref(root);

    return age;
}

int http_is_old(city_node_t* city_node) {
    time_t now = time(NULL);
    double age = difftime(now, (time_t)city_node->data->cached_at);
    return age > (DATA_MAX_AGE_S);
}

/* ----- JSON PARSING ----- */
/*
http_json_parse() parses the JSON-string and adds the parsed values
for temperatur, windspeed and relative humidity to a city data.
*/
int http_json_parse(char* http_response, city_node_t* city_node) {
    json_error_t error;
    json_t*      root = json_loads(http_response, 0, &error);

    if (!root) {
        fprintf(stderr, "JSON error at line %d: %s\n", error.line, error.text);
        return STATUS_FAIL;
    }

    json_t* current_weather = json_object_get(root, "current");
    if (!json_is_object(current_weather)) {
        json_decref(root);
        return STATUS_FAIL;
    }

    json_t* temperature = json_object_get(current_weather, "temperature_2m");
    json_t* windspeed   = json_object_get(current_weather, "wind_speed_10m");
    json_t* rel_humidity =
        json_object_get(current_weather, "relative_humidity_2m");

    if (json_is_number(temperature))
        city_node->data->temp = json_number_value(temperature);
    if (json_is_number(windspeed))
        city_node->data->windspeed = json_number_value(windspeed);
    if (json_is_number(rel_humidity))
        city_node->data->rel_hum = json_number_value(rel_humidity);

    json_decref(root);
    return STATUS_OK;
}
