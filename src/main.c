#include "libs/HTTP.h"
#include "libs/city.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main() {
    
    /* INIT APP */
    city_list_t* list = NULL;
    if (city_init(&list) != STATUS_OK) {
        fprintf(stderr, "Failed to init app.\n");
        return STATUS_FAIL;
    }

    /* MAIN PROGRAM LOOP */
    while (1) {
        
        /* PRINT LINKED LIST */
        if (city_print_list(&list) != STATUS_OK) {
            fprintf(stderr, "Failed to print list.\n");
            return STATUS_FAIL;
        }

        /* USER SELECTED CITY */
        printf("Select a city: ");
        city_node_t* user_city = NULL;
        unsigned user_city_status = city_get(list, &user_city);
        if (user_city_status == STATUS_EXIT ) {
            printf("User pressed 'q' to exit.\n");
            return STATUS_EXIT;        
        } else if (user_city_status == STATUS_FAIL) {
            printf("\nCity not found.\n");
            continue;
        }
        printf("\nYou selected: %s\n", user_city->data->name);

        char* http_response = NULL;
        /* 1) Check if in-memory data is old or uninitialized */
        if ((user_city->data->temp == INIT_VAL) || http_is_old(user_city)) {
            printf("Data old or missing, fetching from Meteo...\n");

            http_response = http_get(user_city);
            if (!http_response) {
                fprintf(stderr, "HTTP request failed.\n");
                city_free_list(list);
                return 1;
            }

            if (http_json_parse(http_response, user_city) != 0) {
                fprintf(stderr, "Failed to parse HTTP response\n");
            } else {
                /* Save updated data to cache */
                if (city_save_cache(user_city->data) != 0)
                    fprintf(stderr, "Failed to save cache for %s\n", user_city->data->name);
            }

            free(http_response);

        } else if (city_cache_age_seconds(user_city->data->fp) >= 0 &&
                 city_cache_age_seconds(user_city->data->fp) <= DATA_MAX_AGE_S) {
            printf("Using cached file for %s (age %d seconds)\n",
                   user_city->data->name, city_cache_age_seconds(user_city->data->fp));

            if (city_load_cache(user_city, user_city->data->fp) != 0) {
                fprintf(stderr, "Failed to read cached JSON for %s\n", user_city->data->name);
            }
        }

        printf("\nCurrent Weather for %s:\n", user_city->data->name);
        printf("Temperature: %.2f °C\n", user_city->data->temp);
        printf("Wind speed: %.2f m/s\n", user_city->data->windspeed);
        printf("Humidity: %.2f %%\n\n", user_city->data->rel_hum);
    }

    city_free_list(list);
    return 0;
}
