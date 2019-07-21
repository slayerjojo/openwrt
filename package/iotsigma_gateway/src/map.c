#include "map.h"
#include <stdlib.h>
#include "log.h"

static const char black = 0;
static const char red   = 1;

static int map_comparer(const void *k1, const void *k2)
{
    return (size_t)k1 - (size_t)k2;
}

Map *map_create(MapComparer compare)
{
    if (!compare)
        compare = map_comparer;

    Map *map = (Map *)malloc(sizeof(Map));
    if (!map)
        LogHalt("out of memory");

    map->node = 0;
    map->size = 0;
    map->compare = compare;
    
    return map;
}

Map *map_init(Map *map, MapComparer compare)
{
    if (!map)
        return 0;

    if (!compare)
        compare = map_comparer;

    map->node = 0;
    map->size = 0;
    map->compare = compare;
    
    return map;
}

static inline void release_map_node(Map *map, MapNode *item)
{
    if(!item)
        return;

    if(item->_left)
        release_map_node(map, item->_left);
    if(item->_right)
        release_map_node(map, item->_right);

    free(item->data);
    free(item);
}

void map_clear(Map *map)
{
    if (!map)
        return;

    if (map->node)
        release_map_node(map, map->node);
    map->node = 0;
    map->size = 0;
}

static inline MapNode *get(MapNode *item, const void *key, MapComparer compare)
{
    if (!item)
        return 0;
    int comp = compare(item->key, key);
    if (!comp)
        return item;
    if (comp > 0)
        return get(item->_left, key, compare);
    return get(item->_right, key, compare);
}

static inline void left_rotate_map(Map *map, MapNode *x)
{
    MapNode *y  = x->_right;

    x->_right    = y->_left;
    if(y->_left)
        y->_left->_parent = x;

    y->_parent   = x->_parent;

    if(!x->_parent)
    {
        map->node   = y;
    }
    else
    {
        if(x->_parent->_left == x)
            x->_parent->_left     = y;
        else
            x->_parent->_right    = y;
    }

    y->_left     = x;
    x->_parent   = y;
}

static inline void right_rotate_map(Map *map, MapNode *y)
{
    MapNode *x  = y->_left;

    y->_left = x->_right;
    if(x->_right)
        x->_right->_parent    = y;

    x->_parent   = y->_parent;

    if(!y->_parent)
    {
        map->node   = x;
    }
    else
    {
        if(y == y->_parent->_right)
            y->_parent->_right    = x;
        else
            y->_parent->_left     = x;
    }

    x->_right    = y;

    y->_parent   = x;
}

void *map_add(Map *map, const void *key, void *data)
{
    MapNode *item = get(map->node, key, map->compare);
    if (item)
    {
        if (item->data == data)
            return 0;
        void *ret = item->data;
        item->data = data;
        return ret;
    }

    item = (MapNode *)malloc(sizeof(MapNode));
    if (!item)
        LogHalt("out of memory");
    item->key = key;
    item->data = data;
    item->_left = 0;
    item->_right = 0;
    item->_parent = 0;
    item->_color = black;

    MapNode *y = 0;
    MapNode *x = map->node;

    while(x)
    {
        y   = x;
        if(map->compare(item->key, x->key) < 0)
            x   = x->_left;
        else
            x   = x->_right;
    }

    item->_parent    = y;

    if(y)
    {
        if(map->compare(item->key, y->key) < 0)
            y->_left     = item;
        else
            y->_right    = item;
    }
    else
    {
        map->node   = item;
    }

    item->_color = red;

    MapNode *parent, *gparent;
    while((parent = item->_parent) && parent->_color == red)
    {
        gparent = parent->_parent;

        if(parent == gparent->_left)
        {
            MapNode *uncle = gparent->_right;
            if(uncle && uncle->_color == red)
            {
                uncle->_color    = black;
                parent->_color   = black;
                gparent->_color  = red;

                item    = gparent;
                continue;
            }

            if(parent->_right == item)
            {
                left_rotate_map(map, parent);

                MapNode *tmp    = parent;
                parent  = item;
                item    = tmp;
            }

            parent->_color   = black;
            gparent->_color  = red;
            right_rotate_map(map, gparent);
        }
        else
        {
            MapNode *uncle  = gparent->_left;
            if(uncle && uncle->_color == red)
            {
                uncle->_color    = black;
                parent->_color   = black;
                gparent->_color  = red;
                item    = gparent;
                continue;
            }

            if(parent->_left == item)
            {
                right_rotate_map(map, parent);

                MapNode *tmp;
                tmp = parent;
                parent  = item;
                item    = tmp;
            }

            parent->_color   = black;
            gparent->_color  = red;
            left_rotate_map(map, gparent);
        }
    }

    map->node->_color    = black;
    map->size++;
    return 0;
}

static inline void delete_fixup_map(Map *map, MapNode *node, MapNode *parent)
{
    MapNode *other;
    
    while((!node || node->_color == black) && node != map->node)
    {
        if(parent->_left == node)
        {
            other = parent->_right;
            if(red == other->_color)
            {
                other->_color    = black;
                parent->_color   = red;
                left_rotate_map(map, parent);
                other   = parent->_right;
            }
            if((!other->_left || black == other->_left->_color) && (!other->_right || black == other->_right->_color))
            {
                other->_color = red;
                node    = parent;
                parent  = node->_parent;
            }
            else
            {
                if(!other->_right || black == other->_right->_color)
                {
                    other->_left->_color = black;
                    other->_color = red;
                    right_rotate_map(map, other);
                    other   = parent->_right;
                }
                other->_color        = parent->_color;
                parent->_color       = black;
                other->_right->_color = black;
                left_rotate_map(map, parent);
                node    = map->node;
                break;
            }
        }
        else
        {
            other = parent->_left;
            if(red == other->_color)
            {
                other->_color    = black;
                parent->_color   = red;
                right_rotate_map(map, parent);
                other           = parent->_left;
            }
            if((!other->_left || black == other->_left->_color) && (!other->_right || black == other->_right->_color))
            {
                other->_color    = red;
                node    = parent;
                parent  = node->_parent;
            }
            else
            {
                if(!other->_left || black == other->_left->_color)
                {
                    other->_right->_color = black;
                    other->_color        = red;
                    left_rotate_map(map, other);
                    other   = parent->_left;
                }
                other->_color        = parent->_color;
                parent->_color       = black;
                other->_left->_color  = black;
                right_rotate_map(map, parent);
                node    = map->node;
                break;
            }
        }
    }
    if(node)
        node->_color = black;
}

void *map_remove(Map *map, const void **key)
{
    MapNode *item = get(map->node, *key, map->compare);
    if(!item)
        return 0;

    MapNode *child, *parent;
    unsigned char color;

    if (item->_left && item->_right)
    {
        MapNode *replace    = item;
        replace = replace->_right;
        while(replace->_left)
            replace = replace->_left;

        if(item->_parent)
        {
            if(item->_parent->_left == item)
                item->_parent->_left  = replace;
            else
                item->_parent->_right = replace;
        }
        else
            map->node   = replace;

        child   = replace->_right;
        parent  = replace->_parent;

        color   = replace->_color;

        if(parent == item)
        {
            parent  = replace;
        }
        else
        {
            if(child)
                child->_parent   = parent;
            parent->_left    = child;

            replace->_right  = item->_right;
            item->_right->_parent = replace;
        }

        replace->_parent = item->_parent;
        replace->_color  = item->_color;
        replace->_left   = item->_left;
        item->_left->_parent  = replace;

        if(black == color)
        {
            delete_fixup_map(map, child, parent);
        }

        void *ret = item->data;
        *key = item->key;
        free(item);
        map->size--;
        return ret;
    }

    if(item->_left)
        child   = item->_left;
    else
        child   = item->_right;

    parent  = item->_parent;

    color   = item->_color;

    if(child)
        child->_parent   = parent;

    if(parent)
    {
        if(parent->_left == item)
            parent->_left    = child;
        else
            parent->_right   = child;
    }
    else
    {
        map->node   = child;
    }

    if(black == color)
    {
        delete_fixup_map(map, child, parent);
    }

    void *ret = item->data;
    *key = item->key;
    free(item);
    map->size--;
    return ret;
}

void *map_get(Map *map, const void *key)
{
    MapNode *item = get(map->node, key, map->compare);
    if(!item)
        return 0;
    return item->data;
}

MapIterator *map_iterator_create(Map *map)
{
    if (!map || !map->size)
        return 0;
    MapIterator *it = (MapIterator *)malloc(sizeof(MapIterator));
    if (!it)
        return 0;
    Array *stack = array_init(&it->stack);
    array_unshift(stack, map->node);
    MapNode *p = map->node;
    while (p->_left)
    {
        p = p->_left;

        array_unshift(stack, p);
    }
    it->current = 0;
    return it;
}

void map_iterator_release(MapIterator *it)
{
    if (!it)
        return;
    array_clear(&(it->stack));
    free(it);
}

const void *map_iterator_key(MapIterator *it)
{
    if (!it || !it->current)
        return 0;
    return it->current->key;
}

void *map_iterator_value(MapIterator *it)
{
    if (!it || !it->current)
        return 0;
    return it->current->data;
}

int map_iterator_next(MapIterator *it)
{
    if (!it)
        return 0;
    Array *stack = &it->stack;
    it->current = 0;
    if (stack->size)
    {
        it->current = (MapNode *)array_at(stack, 0);
        array_remove(stack, 0);
        if (it->current->_right)
        {
            array_unshift(stack, it->current->_right);
            MapNode *p = it->current->_right;
            while (p->_left)
            {
                p = p->_left;

                array_unshift(stack, p);
            }
        }
    }
    return !!it->current;
}
