/*
    Etherskies, a CLI weather-tool.
    This is a school-project from students
    at Chas Academy, SUVX25.
    - Team Stockholm 1
    2025-10
*/

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

    while (1) {

        if (city_print_list(&list) != STATUS_OK) {
            fprintf(stderr, "Failed to print list.\n");
            return STATUS_FAIL;
        }

        printf("Select a city: ");
        city_node_t* user_city        = NULL;
        unsigned     user_city_status = city_get(list, &user_city);
        if (user_city_status == STATUS_EXIT) {
            printf("User pressed 'q' to exit.\n");
            city_dispose(&list);
            return STATUS_OK;
        } else if (user_city_status == STATUS_FAIL) {
            printf("\nCity not found.\n");
            continue;
        }
        printf("\nYou selected: %s\n", user_city->data->name);

        if (http_get_weather_data(user_city) != STATUS_OK) {
            fprintf(stderr, "Failed to get weather data for %s.\n",
                    user_city->data->name);
            city_dispose(&list);
            return STATUS_FAIL;
        }

        printf("\nCurrent Weather for %s:\n", user_city->data->name);
        printf("Temperature: %.2f °C\n", user_city->data->temp);
        printf("Wind speed: %.2f m/s\n", user_city->data->windspeed);
        printf("Humidity: %.2f %%\n\n", user_city->data->rel_hum);
    }

    city_dispose(&list);
    return STATUS_OK;
}
