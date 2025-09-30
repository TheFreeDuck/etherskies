/*
    TO-DO: keep refactoring and turning functions into STATUS OK or FAIL
    TO-DO: clean up cache handling?
*/
#include "city.h"
#include "jansson.h"
#include "meteo.h"
#include "tinydir.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* ----- Private function prototypes ----- */
int city_boot(city_list_t *city_list);
city_list_t *city_make_list();
city_node_t *city_make_node(city_data_t *city_data);
city_data_t *city_make_data(char *city_name, char* fp, double lat, double lon,
                            double temp, double windspeed, double rel_hum);
void city_add_tail(city_node_t *city_node, city_list_t *city_list);
int cache_age_seconds(const char *filepath);

/* ----- Bootstrap cities ----- */
typedef struct {
  char name[32];
  char* fp;
  double lat;
  double lon;
} city_bootstrap_t;

city_bootstrap_t bootstrap_arr[] = {
    {"Stockholm",   "", 59.3293, 18.0686},
    {"Göteborg",    "", 57.7089, 11.9746},
    {"Malmö",       "", 55.6050, 13.0038},
    {"Uppsala",     "", 59.8586, 17.6389},
    {"Västerås",    "", 59.6099, 16.5448},
    {"Örebro",      "", 59.2741, 15.2066},
    {"Linköping",   "", 58.4109, 15.6216},
    {"Helsingborg", "", 56.0465, 12.6945},
    {"Jönköping",   "", 57.7815, 14.1562},
    {"Norrköping",  "", 58.5877, 16.1924},
    {"Lund",        "", 55.7047, 13.1910},
    {"Gävle",       "", 60.6749, 17.1413},
    {"Sundsvall",   "", 62.3908, 17.3069},
    {"Umeå",        "", 63.8258, 20.2630},
    {"Luleå",       "", 65.5848, 22.1567},
    {"Kiruna",      "", 67.8558, 20.2253}
};
/* ----------------------------------------- */

int city_init(city_list_t **city_list) {

  city_list_t *new_list = city_make_list();
  if (!new_list) {
    return STATUS_FAIL;
  }
  if (city_boot(new_list) != STATUS_OK) {
    return STATUS_FAIL;
  }
  *city_list = new_list; /* return through out-ptr */

  return STATUS_OK;
}

city_list_t *city_make_list() {
  city_list_t *list = malloc(sizeof(city_list_t));
  if (!list) {
    printf("Malloc failed\n");
    return NULL;
  }
  list->head = list->tail = NULL;
  list->size = 0;
  return list;
}

city_node_t *city_make_node(city_data_t *city_data) {
  city_node_t *node = malloc(sizeof(city_node_t));
  if (!node) {
    printf("Malloc failed\n");
    return NULL;
  }
  node->data = city_data;
  node->prev = node->next = NULL;
  return node;
}

int city_boot(city_list_t *city_list) {
  if (!city_list) {
    return STATUS_FAIL;
  }

  unsigned num_read_files = city_read_cache(city_list);
  if (num_read_files == STATUS_OK) {
    printf("number %u cities from cache\n", city_list->size);
    return STATUS_OK;
  }

  printf("Cache empty, using bootstrap and saving to cache\n");
  size_t n = sizeof(bootstrap_arr) / sizeof(bootstrap_arr[0]);
  for (size_t i = 0; i < n; i++) {
    city_bootstrap_t *b = &bootstrap_arr[i];
    city_data_t *data = city_make_data(b->name, b->fp, b->lat, b->lon, INIT_VAL, 0.0, 0.0);
    if (!data) {
      continue;
    }
    city_node_t *node = city_make_node(data);
    if (node) {
      city_add_tail(node, city_list);
      city_save_cache(data);
    } else {
      city_data_free(data);
    }
  }
  return STATUS_OK;
}

int city_read_cache(city_list_t *list) {

  if (!list) {
    return STATUS_FAIL;
  }

  /* no files */
  tinydir_dir dir;
  if (tinydir_open(&dir, "./cities") != 0) {
    return STATUS_FAIL;
  }

  int num_read_files = 0;
  while (dir.has_next) {
    tinydir_file file;
    tinydir_readfile(&dir, &file);
    if (!file.is_dir && strstr(file.name, ".json")) {
      json_error_t error;
      json_t *root = json_load_file(file.path, 0, &error);
      if (!root) {
        tinydir_next(&dir);
        continue;
      }

      json_t* jname = json_object_get(root, "name");
      json_t* jfp = json_object_get(root,"fp");
      json_t* jlat = json_object_get(root, "lat");
      json_t* jlon = json_object_get(root, "lon");
      json_t* jtemp = json_object_get(root, "temp");
      json_t* jwind = json_object_get(root, "windspeed");
      json_t* jhum = json_object_get(root, "rel_hum");
      json_t* jcached = json_object_get(root, "cached_at");

      /* needed? */
      if (!json_is_string(jname) || !json_is_string(jfp) || !json_is_number(jlat) ||
          !json_is_number(jlon)) {
        json_decref(root);
        tinydir_next(&dir);
        continue;
      }

      city_data_t *data = city_make_data(
          (char *)json_string_value(jname), (char*)json_string_value(jfp), json_number_value(jlat),
          json_number_value(jlon), jtemp ? json_number_value(jtemp) : INIT_VAL,
          jwind ? json_number_value(jwind) : INIT_VAL,
          jhum ? json_number_value(jhum) : INIT_VAL);

      if (jcached && json_is_integer(jcached)) {
        data->cached_at = (time_t)json_is_integer(jcached);
      } else {
        data->cached_at = 0;
      }

      if (data) {
        city_node_t *node = city_make_node(data);
        if (node) {
          city_add_tail(node, list);
          num_read_files++;
        } else {
          city_data_free(data);
        }
      }

      json_decref(root);
    }
    tinydir_next(&dir);
  }
  tinydir_close(&dir);
  list->size = num_read_files;
  return STATUS_OK;
}

int city_print_list(city_list_t **city_list) {
  /* Check that both the pointer-to-list
  and the actual list are not NULL */
  if (!city_list || !*city_list) {
    fprintf(stderr, "Pointer to list or list is NULL\n");
    return STATUS_FAIL;
  }

  city_node_t *current = (*city_list)->head;
  while (current != NULL) {
    printf("%s\n", current->data->name);
    current = current->next;
  }

  return STATUS_OK;
}

city_data_t *city_make_data(char* city_name, char* fp, double lat, double lon,
                            double temp, double windspeed, double rel_hum) {
    (void)fp;
    city_data_t *data = malloc(sizeof(city_data_t));
    if (!data) {
        printf("Malloc failed\n");
        return NULL;
    }
    /* setting and building valuis for datafields */
    data->lat = lat;
    data->lon = lon;
    data->temp = temp;
    data->windspeed = windspeed;
    data->rel_hum = rel_hum;
    data->name = malloc(strlen(city_name) + 1);
    if (!data->name) {
        free(data);
        return NULL;
    }
    strcpy(data->name, city_name);
    char fp_buf[128];
    snprintf(fp_buf, sizeof(fp_buf), 
        "./cities/%s_%.2f_%.2f.json", city_name, lat, lon);
    data->fp = malloc(strlen(fp_buf) + 1);
    if (!data->fp) {
        free(data->name);
        free(data);
        return NULL;
    }
    strcpy(data->fp, fp_buf);

    data->url = meteo_url(lat, lon);
    return data;
}

void city_add_tail(city_node_t *node, city_list_t *list) {
  if (!node || !list) {
    return;
  }
  if (!list->tail) {
    list->head = list->tail = node;
  } else {
    node->prev = list->tail;
    list->tail->next = node;
    list->tail = node;
  }
  list->size++;
}

int city_get(city_list_t *city_list, city_node_t **out_city) {

  if (!city_list || !out_city) {
    fprintf(stderr, "List or ptr to ptr is NULL\n");
    return STATUS_FAIL;
  }
  char buf[128];
  if (fgets(buf, sizeof(buf), stdin) == NULL) {
    return STATUS_FAIL;
  }
  buf[strcspn(buf, "\n")] = 0;
  if (strcmp(buf, "q") == 0) {
    return STATUS_EXIT;
  }
  city_node_t *current = city_list->head;
  while (current) {
    if (strcmp(current->data->name, buf) == 0) {
      /* return found city to ptr-ptr */
      *out_city = current;
      return STATUS_OK;
    }
    current = current->next;
  }
  /* show city not found */
  return STATUS_FAIL;
}

void city_data_free(city_data_t *data) {
  if (!data) {
    return;
  }
  if (data->name) {
    free(data->name);
  }
  if (data->url) {
    free(data->url);
  }
  free(data);
}

void city_free_list(city_list_t *list) {
  if (!list) {
    return;
  }
  city_node_t *current = list->head;
  while (current) {
    city_node_t *next = current->next;
    if (current->data) {
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

int city_save_cache(city_data_t *data) {
  if (!data) {
    return -1;
  }
  /* Makes folder "cities" if doesn't exist */
  if (mkdir("./cities", 0755) != 0 && errno != EEXIST) {
    perror("mkdir");
    return -1;
  }

  char filepath[512];
  snprintf(filepath, sizeof(filepath), "./cities/%s_%.2f_%.2f.json", data->name,
           data->lat, data->lon);

  json_t *root = json_object();
  json_object_set_new(root, "name", json_string(data->name));
  json_object_set_new(root, "fp", json_string(data->fp));
  json_object_set_new(root, "lat", json_real(data->lat));
  json_object_set_new(root, "lon", json_real(data->lon));
  json_object_set_new(root, "temp", json_real(data->temp));
  json_object_set_new(root, "windspeed", json_real(data->windspeed));
  json_object_set_new(root, "rel_hum", json_real(data->rel_hum));
  json_object_set_new(root, "cached_at", json_integer(time(NULL)));

  time_t now = time(NULL);
  data->cached_at = now; // so http_is_old() works immediately
  json_object_set_new(root, "cached_at", json_integer(now));

  if (json_dump_file(root, filepath, JSON_INDENT(2)) != 0) {
    json_decref(root);
    return -1;
  }
  json_decref(root);
  return 0;
}

int city_cache_age_seconds(char *filepath) {
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

/* reads user city */
int city_load_cache(city_node_t *city_node, char *fp) {
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
