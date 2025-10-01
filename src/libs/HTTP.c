/* HTTP.c */
#include "city.h"
#include "HTTP.h"
#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "jansson.h"

/* ----- PRIVATE FUNCTIONS ----- */
int http_load_cache(city_node_t* city_node, char* fp);
int http_cache_age_seconds(char *filepath);
/* ----------------------------- */

int http_load_cache(city_node_t *city_node, char *fp) {
  if (!city_node || !fp) {
    return -1;
  }

  json_error_t error;
  json_t *root = json_load_file(fp, 0, &error);
  if (!root) {
    fprintf(stderr, "Failed to load JSON file %s: %s\n", fp, error.text);
    return -1;
  }

  json_t *jtemp = json_object_get(root, "temp");
  json_t *jwind = json_object_get(root, "windspeed");
  json_t *jhum = json_object_get(root, "rel_hum");
  json_t *jat = json_object_get(root, "cached_at");

  if (jtemp && json_is_number(jtemp))
    city_node->data->temp = json_number_value(jtemp);
  if (jwind && json_is_number(jwind))
    city_node->data->windspeed = json_number_value(jwind);
  if (jhum && json_is_number(jhum))
    city_node->data->rel_hum = json_number_value(jhum);
  if (jat && json_is_integer(jat))
    city_node->data->cached_at = (double)json_integer_value(jat);
  else
    city_node->data->cached_at = 0.0; // fallback if no cached_at

  json_decref(root); // free JSON object
  return 0;
}

int http_cache_age_seconds(char *filepath) {
  if (!filepath) {
    return -1;
  }
  json_error_t error;
  json_t *root = json_load_file(filepath, 0, &error);
  if (!root) {
    return -1;
  }
  json_t *jat = json_object_get(root, "cached_at");
  if (!json_is_integer(jat)) {
    json_decref(root);
    return -1;
  }
  int age = (int)(time(NULL) - json_integer_value(jat));
  json_decref(root);
  return age;
}

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

int http_is_old(city_node_t* city_node) {
    time_t now = time(NULL);
    double age = difftime(now, (time_t)city_node->data->cached_at);
    return age > (DATA_MAX_AGE_S);
}

/* NEW FUNCTION: http_get_weather_data() */
int http_get_weather_data(city_node_t* city_node) {
    
    // --- STEP 1: Check In-Memory Data (Highest Priority) ---
    // If data is initialized AND not old, we are done.
    if (city_node->data->temp != INIT_VAL && !http_is_old(city_node)) {
        printf("Using fresh in-memory data for %s (age %ld seconds).\n", 
               city_node->data->name, (long)difftime(time(NULL), city_node->data->cached_at));
        return STATUS_OK; 
    }

    // --- STEP 2: Check File Cache (Second Priority) ---
    int file_age = http_cache_age_seconds(city_node->data->fp);

    if (file_age >= 0 && file_age <= DATA_MAX_AGE_S) {
        
        // File exists and is young enough. Attempt to load it.
        if (http_load_cache(city_node, city_node->data->fp) == 0) {
            
            // CRITICAL CHECK: Ensure the loaded data is not just stale INIT_VALs
            if (city_node->data->temp != INIT_VAL) {
                printf("Using fresh cached file for %s (age %d seconds).\n", 
                       city_node->data->name, file_age);
                return STATUS_OK; // Success: Loaded fresh, initialized cache.
            }
            
            // If we are here, the cache file was fresh but contained uninitialized data.
            printf("Cache exist but has no weather data\n");
            
        } else {
            // Failure to load the cache file (e.g., corrupt JSON).
            fprintf(stderr, "Failed to read cached JSON for %s.\n", city_node->data->name);
        }
    }
    
    // --- STEP 3: Fallback - Fetch from Network ---
    printf("Data missing, old, or cache invalid. Fetching from Meteo...\n");
    
    char* http_response = http_get(city_node);
    if (!http_response) {
        fprintf(stderr, "HTTP request failed.\n");
        return STATUS_FAIL;
    }
    
    // Parse the new data
    if (http_json_parse(http_response, city_node) != 0) {
        fprintf(stderr, "Failed to parse HTTP response.\n");
        free(http_response);
        return STATUS_FAIL;
    }
    
    // Save the new data to cache
    if (city_save_cache(city_node->data) != 0)
        fprintf(stderr, "Failed to save cache for %s\n", city_node->data->name);
        
    free(http_response);

    return STATUS_OK;
}
