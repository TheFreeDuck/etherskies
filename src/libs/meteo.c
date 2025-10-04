/* 
    meteo.c contains a function that builds the city url. 
*/

#include "meteo.h"
#include "city.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char* meteo_url(double lat, double lon) {

    char* base_url = "https://api.open-meteo.com/v1/forecast";

    /*We allocate space by figuring out how long the url is*/
    size_t size = snprintf(NULL, 0, "%s?latitude=%.2f&longitude=%.2f&current=temperature_2m,relative_humidity_2m,wind_speed_10m", base_url, lat, lon) + 1;
    char *url = (char *)malloc(size);
    if (!url) {
	/*Caller must free!*/
	printf("malloc failed in meteo_url\n");
	return NULL;
    }
    snprintf(url, size, "%s?latitude=%.2f&longitude=%.2f&current=temperature_2m,relative_humidity_2m,wind_speed_10m" , base_url, lat, lon);

    return url;
}
