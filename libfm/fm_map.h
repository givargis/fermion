/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_map.h
 */

#ifndef FM_MAP_H
#define FM_MAP_H

typedef struct fm__map *fm__map_t;

fm__map_t fm__map_open(void);

void fm__map_close(fm__map_t map);

int fm__map_update(fm__map_t map, const char *key, void *val);

void *fm__map_lookup(fm__map_t map, const char *key);

#endif
