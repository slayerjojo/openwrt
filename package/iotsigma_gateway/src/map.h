#ifndef __MAP_H__
#define __MAP_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "array.h"

typedef int (*MapComparer)(const void *key1, const void *key2);

typedef struct _map_node
{
    struct _map_node *_parent;
    struct _map_node *_left;
    struct _map_node *_right;

    unsigned char _color;

    const void *key;
    void *data;
}MapNode;

typedef struct _map
{
    MapNode *node;
    uint32_t size;
    
    MapComparer compare;
}Map;

typedef struct
{
    Array stack;
    MapNode *current;
}MapIterator;

Map *map_create(MapComparer compare);
Map *map_init(Map *map, MapComparer compare);
void map_clear(Map *map);

void *map_add(Map *map, const void *key, void *data);
void *map_remove(Map *map, const void **key);
void *map_get(Map *map, const void *key);

MapIterator *map_iterator_create(Map *map);
void map_iterator_release(MapIterator *it);
int map_iterator_next(MapIterator *it);
const void *map_iterator_key(MapIterator *it);
void *map_iterator_value(MapIterator *it);

#ifdef __cplusplus
}
#endif

#endif
