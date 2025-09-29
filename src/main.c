#include "libs/HTTP.h"
#include "libs/city.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main() {
    /* INIT LIST */
    city_list_t *list = city_make_list();
    if (!list) {
        fprintf(stderr, "Failed to create city list\n");
        return 1;
    }

    /* BOOT-STRAP OR READ FROM CACHE TO LIST */
    city_parse(list);

    /* MAIN PROGRAM LOOP */
    while (1) {
  
        city_node_t *current = list->head;
        while (current != NULL) {
            printf("%s\n", current->data->name);
            current = current->next;
        }

        /* GET USER CITY */
        printf("Press 'q' to quit\n");
        printf("Enter a city name: ");
        city_node_t *user_city = city_get(list);
        if (!user_city) {
            printf("City not found.\n");
            continue;
        }

        printf("\nYou selected: %s\n", user_city->data->name);

        /* FILEPATH FOR CACHE */
        char fp[512];
        snprintf(fp, sizeof(fp), "./cities/%s_%.2f_%.2f.json",
                 user_city->data->name, user_city->data->lat, user_city->data->lon);

        char *http_response = NULL;

        /* 1) Check if in-memory data is old or uninitialized */
        if ((user_city->data->temp == 0.0) || http_is_old(user_city)) {
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

            free(http_response);  // only free if actually allocated
        }
        /* 2) If memory data is fresh but we still want to check cache file */
        else if (city_cache_age_seconds(fp) >= 0 &&
                 city_cache_age_seconds(fp) <= DATA_MAX_AGE_S) {
            printf("Using cached file for %s (age %d seconds)\n",
                   user_city->data->name, city_cache_age_seconds(fp));

            if (city_load_cache(user_city, fp) != 0) {
                fprintf(stderr, "Failed to read cached JSON for %s\n", user_city->data->name);
            }
        }

        /* 3) Print weather info (always print from struct, safe) */
        printf("\nCurrent Weather for %s:\n", user_city->data->name);
        printf("Temperature: %.2f °C\n", user_city->data->temp);
        printf("Wind speed: %.2f m/s\n", user_city->data->windspeed);
        printf("Humidity: %.2f %%\n\n", user_city->data->rel_hum);
    }

    city_free_list(list);
    return 0;
}
