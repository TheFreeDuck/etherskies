#include "city.h"
#include "meteo.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include "tinydir.h"
#include "jansson.h"

/* ----- Private function prototypes ----- */
city_data_t *city_make_data(char *city_name, double lat, double lon, double temp, double windspeed, double rel_hum);
void city_add_tail(city_node_t *city_node, city_list_t *city_list);
int cache_age_seconds(const char *filepath);

/* ----- Bootstrap cities ----- */
typedef struct
{
  char name[32];
  double lat;
  double lon;
} city_bootstrap_t;

city_bootstrap_t bootstrap_arr[] = {
    {"Stockholm", 59.3293, 18.0686},
    {"Göteborg", 57.7089, 11.9746},
    {"Malmö", 55.6050, 13.0038},
    {"Uppsala", 59.8586, 17.6389},
    {"Västerås", 59.6099, 16.5448},
    {"Örebro", 59.2741, 15.2066},
    {"Linköping", 58.4109, 15.6216},
    {"Helsingborg", 56.0465, 12.6945},
    {"Jönköping", 57.7815, 14.1562},
    {"Norrköping", 58.5877, 16.1924},
    {"Lund", 55.7047, 13.1910},
    {"Gävle", 60.6749, 17.1413},
    {"Sundsvall", 62.3908, 17.3069},
    {"Umeå", 63.8258, 20.2630},
    {"Luleå", 65.5848, 22.1567},
    {"Kiruna", 67.8558, 20.2253}};

/* ----------------------------------------- */

city_list_t *city_make_list()
{
  city_list_t *list = malloc(sizeof(city_list_t));
  if (!list)
  {
    printf("Malloc failed\n");
    return NULL;
  }
  list->head = list->tail = NULL;
  list->size = 0;
  return list;
}

city_node_t *city_make_node(city_data_t *city_data)
{
  city_node_t *node = malloc(sizeof(city_node_t));
  if (!node)
  {
    printf("Malloc failed\n");
    return NULL;
  }
  node->data = city_data;
  node->prev = node->next = NULL;
  return node;
}

city_data_t *city_make_data(char *city_name, double lat, double lon, double temp, double windspeed, double rel_hum)
{
  city_data_t *data = malloc(sizeof(city_data_t));
  if (!data)
  {
    printf("Malloc failed\n");
    return NULL;
  }

  data->lat = lat;
  data->lon = lon;
  data->temp = temp;
  data->windspeed = windspeed;
  data->rel_hum = rel_hum;
  data->name = malloc(strlen(city_name) + 1);
  if (!data->name)
  {
    free(data);
    return NULL;
  }
  strcpy(data->name, city_name);

  data->url = meteo_url(lat, lon);
  return data;
}

void city_add_tail(city_node_t *node, city_list_t *list)
{
  if (!node || !list)
  {
    return;
  }
  if (!list->tail)
  {
    list->head = list->tail = node;
  }
  else
  {
    node->prev = list->tail;
    list->tail->next = node;
    list->tail = node;
  }
  list->size++;
}

/* -------------------------------------------------------------------
 * city_parse
 * -------------------------------------------------------------------
 * 1) Loads existing cached JSON files (city_read_cache)
 * 2) If cache empty, uses bootstrap_arr and saves each to cache
 * Added functionality: preserves weather data across program runs
 */
void city_parse(city_list_t *city_list)
{
  if (!city_list)
  {
    return;
  }

  int loaded = city_read_cache(city_list);
  if (loaded > 0)
  {
    printf("Loaded %d cities from cache\n", loaded);
    return;
  }

  printf("Cache empty, using bootstrap and saving to cache\n");
  size_t n = sizeof(bootstrap_arr) / sizeof(bootstrap_arr[0]);
  for (size_t i = 0; i < n; i++)
  {
    city_bootstrap_t *b = &bootstrap_arr[i];
    city_data_t *data = city_make_data(b->name, b->lat, b->lon, 0.0, 0.0, 0.0);
    if (!data)
    {
      continue;
    }
    city_node_t *node = city_make_node(data);
    if (node)
    {
      city_add_tail(node, city_list);
      city_save_cache(data);
    }
    else
    {
      city_data_free(data);
    }
  }
}

city_node_t *city_get(city_list_t *city_list)
{
  if (!city_list)
  {
    return NULL;
  }
  
  char buf[128];
  
  if (!fgets(buf, sizeof(buf), stdin))
  {
    return NULL;
  }
  
  buf[strcspn(buf, "\n")] = 0;
  
  city_node_t *current = city_list->head;
  
  while (current)
  {
    if (strcmp(current->data->name, buf) == 0)
    {
      return current;
    }
    current = current->next;
  }
  
  return NULL;
}

void city_data_free(city_data_t *data)
{
  if (!data)
  {
    return;
  }
  if (data->name)
  {
    free(data->name);
  }
  if (data->url)
  {
    free(data->url);
  }
  free(data);
}

void city_free_list(city_list_t *list)
{
  if (!list)
  {
    return;
  }
  city_node_t *current = list->head;
  while (current)
  {
    city_node_t *next = current->next;
    if (current->data)
    {
      city_data_free(current->data);
    }
    
    free(current);
    current = next;
  }
  free(list);
}

/* -------------------------------------------------------------------
 * Cache helpers
 * ------------------------------------------------------------------- */

int city_save_cache(city_data_t *data)
{
  if (!data)
  {
    return -1;
  }
  if (mkdir("./cities", 0755) != 0 && errno != EEXIST)
  {
    perror("mkdir");
    return -1;
  }

  char filepath[512];
  snprintf(filepath, sizeof(filepath), "./cities/%s_%.2f_%.2f.json", data->name, data->lat, data->lon);

  json_t *root = json_object();
  json_object_set_new(root, "name", json_string(data->name));
  json_object_set_new(root, "lat", json_real(data->lat));
  json_object_set_new(root, "lon", json_real(data->lon));
  json_object_set_new(root, "temp", json_real(data->temp));
  json_object_set_new(root, "windspeed", json_real(data->windspeed));
  json_object_set_new(root, "rel_hum", json_real(data->rel_hum));
  json_object_set_new(root, "cached_at", json_integer(time(NULL)));

  if (json_dump_file(root, filepath, JSON_INDENT(2)) != 0)
  {
    json_decref(root);
    return -1;
  }
  json_decref(root);
  return 0;
}

int city_read_cache(city_list_t *list)
{
  
  if (!list)
  {
    return 0;
  }
  
  tinydir_dir dir;
  if (tinydir_open(&dir, "./cities") != 0)
  {
    return 0;
  }

  int loaded = 0;
  while (dir.has_next)
  {
    tinydir_file file;
    tinydir_readfile(&dir, &file);
    if (!file.is_dir && strstr(file.name, ".json"))
    {
      json_error_t error;
      json_t *root = json_load_file(file.path, 0, &error);
      if (!root)
      {
        tinydir_next(&dir);
        continue;
      }

      json_t *jname = json_object_get(root, "name");
      json_t *jlat = json_object_get(root, "lat");
      json_t *jlon = json_object_get(root, "lon");
      json_t *jtemp = json_object_get(root, "temp");
      json_t *jwind = json_object_get(root, "windspeed");
      json_t *jhum = json_object_get(root, "rel_hum");

      if (!json_is_string(jname) || !json_is_number(jlat) || !json_is_number(jlon))
      {
        json_decref(root);
        tinydir_next(&dir);
        continue;
      }

      city_data_t *data = city_make_data((char *)json_string_value(jname),
                                         json_number_value(jlat),
                                         json_number_value(jlon),
                                         jtemp ? json_number_value(jtemp) : 0.0,
                                         jwind ? json_number_value(jwind) : 0.0,
                                         jhum ? json_number_value(jhum) : 0.0);
      if (data)
      {
        city_node_t *node = city_make_node(data);
        if (node)
        {
          city_add_tail(node, list);
          loaded++;
        }
        else
        {
          city_data_free(data);
        }
      }

      json_decref(root);
    }
    tinydir_next(&dir);
  }
  tinydir_close(&dir);
  return loaded;
}

int cache_age_seconds(const char *filepath)
{
  if (!filepath)
  {
    return INT_MAX;
  }
  json_error_t error;
  json_t *root = json_load_file(filepath, 0, &error);
  if (!root)
  {
    return INT_MAX;
  }
  json_t *jat = json_object_get(root, "cached_at");
  if (!json_is_integer(jat))
  {
    json_decref(root);
    return INT_MAX;
  }
  int age = (int)(time(NULL) - json_integer_value(jat));
  json_decref(root);
  return age;
}
