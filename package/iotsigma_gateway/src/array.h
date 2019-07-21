#ifndef __ARRAY_H__
#define __ARRAY_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _array_item
{
    struct _array_item *next;
    struct _array_item *prev;

    void *element;
}ArrayNode;

typedef struct _array
{
    ArrayNode *node;
    uint32_t size;
}Array;

typedef enum
{
    ARRAY_ORDER_SEQUENCE = 0,
    ARRAY_ORDER_REVERSE
}ArrayIteratorOrder;

typedef struct _array_iterator
{
    ArrayIteratorOrder order;
    ArrayNode *current;
    ArrayNode *next;
    ArrayNode *end;
}ArrayIterator;

Array *array_create(void);
Array *array_init(Array *array);
void array_clear(Array *array);

void array_push(Array *array, void *data);
void array_unshift(Array *array, void *data);
void array_insert(Array *array, uint32_t idx, void *data);
void *array_remove(Array *array, uint32_t idx);

void *array_at(Array *array, uint32_t idx);

ArrayIterator *array_iterator_create(const Array *array, ArrayIteratorOrder order);
void array_iterator_release(ArrayIterator *it);
int array_iterator_next(ArrayIterator *it);
void *array_iterator_value(ArrayIterator *it);

#ifdef __cplusplus
}
#endif

#endif
