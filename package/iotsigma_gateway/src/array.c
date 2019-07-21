#include "array.h"
#include <stdlib.h>
#include "log.h"

Array *array_create(void)
{
    Array *array = (Array *)malloc(sizeof(Array));
    if (!array)
        return 0;
    array->size = 0;
    array->node = 0;
    return array;
}

Array *array_init(Array *array)
{
    if (!array)
        return 0;
    array->size = 0;
    array->node = 0;
    return array;
}

void array_clear(Array *array)
{
    while (array->size)
        array_remove(array, 0UL);
}

void array_push(Array *array, void *data)
{
    ArrayNode *item = (ArrayNode *)malloc(sizeof(ArrayNode));
    if (!item)
        LogHalt("out of memory");
    item->element = data;

    if (!array->node)
    {
        array->node = item;
        item->next = item;
        item->prev = item;
    }
    else
    {
        item->next = array->node;
        item->prev = array->node->prev;

        array->node->prev = item;
        item->prev->next = item;
    }
    array->size++;
}

void array_unshift(Array *array, void *data)
{
    ArrayNode *item = (ArrayNode *)malloc(sizeof(ArrayNode));
    if (!item)
        LogHalt("out of memory");
    item->element = data;

    if (!array->node)
    {
        array->node = item;
        item->next = item;
        item->prev = item;
    }
    else
    {
        item->next = array->node;
        item->prev = array->node->prev;

        array->node->prev = item;
        item->prev->next = item;

        array->node = array->node->prev;
    }
    array->size++;
}

void array_insert(Array *array, uint32_t idx, void *data)
{
    if (idx >= array->size)
    {
        array_push(array, data);
        return;
    }

    if (!idx)
    {
        array_unshift(array, data);
        return;
    }

    ArrayNode *item = array->node;
    while (idx--)
        item = item->next;

    ArrayNode *newitem = (ArrayNode *)malloc(sizeof(ArrayNode));
    if (!newitem)
        LogHalt("out of memory");
    newitem->element = data;
    newitem->next = item;
    newitem->prev = item->prev;

    item->prev->next = newitem;
    item->prev = newitem;
}

void *array_remove(Array *array, uint32_t idx)
{
    if (idx >= array->size)
        return 0;

    ArrayNode *item = array->node;
    while(idx--)
        item = item->next;

    ArrayNode *prev = item->prev;
    ArrayNode *next = item->next;

    prev->next = item->next;
    next->prev = item->prev;

    array->size--;

    if (item == array->node)
        array->node = next;
    if (item == array->node)
        array->node = 0;

    void *ret = item->element;
    free(item);
    return ret;
}

void *array_at(Array *array, uint32_t idx)
{
    if (idx >= array->size)
        idx = array->size - 1;
    ArrayNode *item = 0;
    if (array->size >= 2 && idx >= (array->size / 2))
    {
        item = array->node->prev;
        idx = array->size - 1 - idx;
        while (idx--)
            item = item->prev;
    }
    else
    {
        item = array->node;
        while (idx--)
            item = item->next;
    }

    return item->element;
}

ArrayIterator *array_iterator_create(const Array *array, ArrayIteratorOrder order)
{
    ArrayIterator *it = (ArrayIterator *)malloc(sizeof(ArrayIterator));
    if (ARRAY_ORDER_SEQUENCE == order)
    {
        it->order = ARRAY_ORDER_SEQUENCE;
    }
    else if (ARRAY_ORDER_REVERSE == order)
    {
        it->order = ARRAY_ORDER_REVERSE;
    }
    it->end = 0;
    it->current = 0;
    it->next = array->node;
    return it;
}

void array_iterator_release(ArrayIterator *it)
{
    free(it);
}

void *array_iterator_value(ArrayIterator *it)
{
    if (!it || !it->current)
        return 0;
    return it->current->element;
}

int array_iterator_next(ArrayIterator *it)
{
    if (!it)
        return 0;
    if (it->current == it->next)
        return 0;
    it->current = it->next;
    if (it->end)
    {
        if (it->end == it->current)
            return 0;
    }
    else
    {
        it->end = it->current;
    }
    if (ARRAY_ORDER_SEQUENCE == it->order)
    {
        if (it->current)
            it->next = it->current->next;
    }
    else if (ARRAY_ORDER_REVERSE == it->order)
    {
        if (it->current)
            it->next = it->current->prev;
    }
    return !!it->current;
}
