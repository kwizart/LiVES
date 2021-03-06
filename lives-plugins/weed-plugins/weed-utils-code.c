/* WEED is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   Weed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this source code; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA

   Weed is developed by:
   Gabriel "Salsaman" Finch - http://lives.sourceforge.net


   mainly based on LiViDO, which is developed by:

   Niels Elburg - http://veejay.sf.net
   Gabriel "Salsaman" Finch - http://lives-video.com
   Denis "Jaromil" Rojo - http://freej.dyne.org
   Tom Schouten - http://zwizwa.fartit.com
   Andraz Tori - http://cvs.cinelerra.org
   reviewed with suggestions and contributions from:
   Silvano "Kysucix" Galliani - http://freej.dyne.org
   Kentaro Fukuchi - http://megaui.net/fukuchi
   Jun Iio - http://www.malib.net
   Carlo Prelz - http://www2.fluido.as:8080/
*/

/* (C) Gabriel "Salsaman" Finch, 2005 - 2019 */

/* #include <string.h> */
/* #include <stdlib.h> */

/* #include <weed/weed.h> */
/* #include <weed/weed-effects.h> */
/* #include <weed/weed-utils.h> */

// uses only functions weed_leaf_get, weed_leaf_set, weed_leaf_num_elements
// plus weed_malloc, weed_free

/////////////////////////////////////////////////////////////////

#ifndef __HAVE_WEED_UTILS_CODE_C__
#define __HAVE_WEED_UTILS_CODE_C__

#ifndef __WEED_PLUGIN__
#error This code is designed to be included only by Weed plugins
#endif

#ifndef WEED_ABI_200
#error This code requires at least ABI version 200 of Weed
#endif

#ifndef WEED_UTILS_API_200
#error This code requires at least API version 200 of weed-utils
#endif

#define weed_get_value(plant, key, value) weed_leaf_get(plant, key, 0, value)
#define weed_check_leaf(plant, key) weed_get_value(plant, key, NULL)

static int weed_plant_has_leaf(weed_plant_t *plant, const char *key) {
  // check for existence of a leaf, must have a value and not just a seed_type
  if (plant != NULL) {
    weed_error_t err = weed_check_leaf(plant, key);
    if (err == WEED_SUCCESS) return WEED_TRUE;
  }
  return WEED_FALSE;
}

static int weed_leaf_exists(weed_plant_t *plant, const char *key) {
  // check for existence of a leaf, may have only a seed_type but no value set
  if (plant != NULL) {
    weed_error_t err = weed_check_leaf(plant, key);
    if (err != WEED_ERROR_NOSUCH_LEAF) return WEED_TRUE;
  }
  return WEED_FALSE;
}

static weed_error_t weed_set_int_value(weed_plant_t *plant, const char *key, int32_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_INT, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_double_value(weed_plant_t *plant, const char *key, double value) {
  return weed_leaf_set(plant, key, WEED_SEED_DOUBLE, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_boolean_value(weed_plant_t *plant, const char *key, int32_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_BOOLEAN, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_int64_value(weed_plant_t *plant, const char *key, int64_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_INT64, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_string_value(weed_plant_t *plant, const char *key, const char *value) {
  return weed_leaf_set(plant, key, WEED_SEED_STRING, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_voidptr_value(weed_plant_t *plant, const char *key, weed_voidptr_t value) {
  return weed_leaf_set(plant, key, WEED_SEED_VOIDPTR, 1, (weed_voidptr_t)&value);
}

static weed_error_t weed_set_plantptr_value(weed_plant_t *plant, const char *key, weed_plant_t *value) {
  return weed_leaf_set(plant, key, WEED_SEED_PLANTPTR, 1, (weed_voidptr_t)&value);
}

static inline weed_error_t weed_leaf_check(weed_plant_t *plant, const char *key, int32_t seed_type) {
  weed_error_t err;
  if ((err = weed_check_leaf(plant, key)) != WEED_SUCCESS) return err;
  if (weed_leaf_seed_type(plant, key) != seed_type) {
    return WEED_ERROR_WRONG_SEED_TYPE;
  }
  return WEED_SUCCESS;
}

static inline weed_error_t weed_value_get(weed_plant_t *plant, const char *key, int32_t seed_type, weed_voidptr_t retval) {
  weed_error_t err;
  if ((err = weed_leaf_check(plant, key, seed_type)) != WEED_SUCCESS) return err;
  return weed_get_value(plant, key, retval);
}

static int32_t weed_get_int_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t retval = 0;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_INT, &retval);
  if (error != NULL) *error = err;
  return retval;
}

static double weed_get_double_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  double retval = 0.;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_DOUBLE, &retval);
  if (error != NULL) *error = err;
  return retval;
}

static int32_t weed_get_boolean_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t retval = WEED_FALSE;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_BOOLEAN, &retval);
  if (error != NULL) *error = err;
  return retval;
}

static int64_t weed_get_int64_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int64_t retval = 0;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_INT64, &retval);
  if (error != NULL) *error = err;
  return retval;
}

static char *weed_get_string_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_size_t size;
  char *retval = NULL;
  weed_error_t err = weed_check_leaf(plant, key);
  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }
  if (weed_leaf_seed_type(plant, key) != WEED_SEED_STRING) {
    if (error != NULL) *error = WEED_ERROR_WRONG_SEED_TYPE;
    return NULL;
  }
  if ((retval = (char *)weed_malloc((size = weed_leaf_element_size(plant, key, 0)) + 1)) == NULL) {
    if (error != NULL) *error = WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }
  if ((err = weed_value_get(plant, key, WEED_SEED_STRING, &retval)) != WEED_SUCCESS) {
    if (retval != NULL) {
      weed_free(retval);
      retval = NULL;
    }
  }
  if (error != NULL) *error = err;
  return retval;
}

static weed_voidptr_t weed_get_voidptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_voidptr_t retval = NULL;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_VOIDPTR, &retval);
  if (error != NULL) *error = err;
  return retval;
}

static weed_plant_t *weed_get_plantptr_value(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_plant_t *retval = NULL;
  weed_error_t err = weed_value_get(plant, key, WEED_SEED_PLANTPTR, &retval);
  if (error != NULL) *error = err;
  return retval;
}

static inline weed_error_t weed_get_values(weed_plant_t *plant, const char *key, size_t dsize, char **retval) {
  weed_error_t err;
  weed_size_t num_elems = weed_leaf_num_elements(plant, key);
  int i;

  if ((*retval = (char *)weed_malloc(num_elems * dsize)) == NULL) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  for (i = 0; i < num_elems; i++) {
    if ((err = weed_leaf_get(plant, key, i, (weed_voidptr_t) & (*retval)[i * dsize])) != WEED_SUCCESS) {
      weed_free(*retval);
      *retval = NULL;
      return err;
    }
  }
  return WEED_SUCCESS;
}

static inline weed_voidptr_t weed_get_array(weed_plant_t *plant, const char *key,
    int32_t seed_type, weed_size_t typelen, weed_voidptr_t retvals, weed_error_t *error) {
  weed_error_t err = weed_leaf_check(plant, key, seed_type);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  err = weed_get_values(plant, key, typelen, (char **)&retvals);
  if (error != NULL) *error = err;
  return retvals;
}

static int32_t *weed_get_int_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t *retvals = NULL;
  return (int32_t *)(weed_get_array(plant, key, WEED_SEED_INT, 4, (uint8_t **)&retvals, error));
}

static double *weed_get_double_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  double *retvals = NULL;
  return (double *)(weed_get_array(plant, key, WEED_SEED_DOUBLE, 8, (uint8_t **)&retvals, error));
}

static int32_t *weed_get_boolean_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int32_t *retvals = NULL;

  weed_error_t err = weed_leaf_check(plant, key, WEED_SEED_BOOLEAN);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  err = weed_get_values(plant, key, 4, (char **)&retvals);
  if (error != NULL) *error = err;
  return retvals;
}

static int64_t *weed_get_int64_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  int64_t *retvals = NULL;

  weed_error_t err = weed_leaf_check(plant, key, WEED_SEED_INT64);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  err = weed_get_values(plant, key, 8, (char **)&retvals);
  if (error != NULL) *error = err;
  return retvals;
}

static char **weed_get_string_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_size_t num_elems, size;
  char **retvals = NULL;
  int i;

  weed_error_t err = weed_leaf_check(plant, key, WEED_SEED_STRING);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  if ((num_elems = weed_leaf_num_elements(plant, key)) == 0) return NULL;

  if ((retvals = (char **)weed_malloc(num_elems * sizeof(char *))) == NULL) {
    if (error != NULL) *error = WEED_ERROR_MEMORY_ALLOCATION;
    return NULL;
  }

  for (i = 0; i < num_elems; i++) {
    if ((retvals[i] = (char *)weed_malloc((size = weed_leaf_element_size(plant, key, i)) + 1)) == NULL) {
      for (--i; i >= 0; i--) weed_free(retvals[i]);
      if (error != NULL) *error = WEED_ERROR_MEMORY_ALLOCATION;
      weed_free(retvals);
      return NULL;
    }
    if ((err = weed_leaf_get(plant, key, i, &retvals[i])) != WEED_SUCCESS) {
      for (--i; i >= 0; i--) weed_free(retvals[i]);
      if (error != NULL) *error = err;
      weed_free(retvals);
      return NULL;
    }
    retvals[i][size] = '\0';
  }
  if (error != NULL) *error = WEED_SUCCESS;
  return retvals;
}

static weed_voidptr_t *weed_get_voidptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_voidptr_t *retvals = NULL;

  weed_error_t err = weed_leaf_check(plant, key, WEED_SEED_VOIDPTR);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  err = weed_get_values(plant, key, WEED_VOIDPTR_SIZE, (char **)&retvals);
  if (error != NULL) *error = err;
  return retvals;
}

static weed_plant_t **weed_get_plantptr_array(weed_plant_t *plant, const char *key, weed_error_t *error) {
  weed_plant_t **retvals = NULL;

  weed_error_t err = weed_leaf_check(plant, key, WEED_SEED_PLANTPTR);

  if (err != WEED_SUCCESS) {
    if (error != NULL) *error = err;
    return NULL;
  }

  err = weed_get_values(plant, key, WEED_VOIDPTR_SIZE, (char **)&retvals);
  if (error != NULL) *error = err;
  return retvals;
}

static weed_error_t weed_set_int_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int32_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_INT, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_double_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, double *values) {
  return weed_leaf_set(plant, key, WEED_SEED_DOUBLE, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_boolean_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int32_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_BOOLEAN, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_int64_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, int64_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_INT64, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_string_array(weed_plant_t *plant, const char *key, weed_size_t num_elems, char **values) {
  return weed_leaf_set(plant, key, WEED_SEED_STRING, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_voidptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems,
    weed_voidptr_t *values) {
  return weed_leaf_set(plant, key, WEED_SEED_VOIDPTR, num_elems, (weed_voidptr_t)values);
}

static weed_error_t weed_set_plantptr_array(weed_plant_t *plant, const char *key, weed_size_t num_elems,
    weed_plant_t **values) {
  return weed_leaf_set(plant, key, WEED_SEED_PLANTPTR, num_elems, (weed_voidptr_t)values);
}

#endif

