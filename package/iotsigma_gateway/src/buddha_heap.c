#include "buddha_heap.h"
#include "log.h"

#undef BUDDHA_HEAP_DEBUG

#undef BUDDHA_HEAP_MAGIC

#ifndef BUDDHA_HEAP_DEBUG

typedef struct
{
#ifdef BUDDHA_HEAP_MAGIC
    uint8_t magic;
#endif
    uint8_t mark:7;
    uint8_t alloc:1;
    uint16_t size;
}__attribute__((packed)) BuddhaHeapHeader;

static uint8_t *_heap = 0;
static uint32_t _size = 0;
static uint32_t _start = 0;

void buddha_heap_init(uint8_t *heap, size_t size)
{
	BuddhaHeapHeader *header = (BuddhaHeapHeader *)heap;
    
	_heap = heap;
    _size = size;
    _start = 0;
#ifdef BUDDHA_HEAP_MAGIC
    header->magic = 0x55;
#endif
    header->alloc = 0;
    header->mark = 0;
    header->size = size - sizeof(BuddhaHeapHeader);
}

void *buddha_heap_alloc(uint32_t size, uint8_t mark)
{
    uint32_t free = 0;
    uint32_t start = 0;
	BuddhaHeapHeader *header;
	void *addr;
    while (start < _size)
    {
        free = 0;
        while (
            (free < (size + sizeof(BuddhaHeapHeader))) && 
            (free < (_size - (_start + start) % _size)) &&
            (!((BuddhaHeapHeader *)(_heap + (_start + start + free) % _size))->alloc)
        )
        {
            free += sizeof(BuddhaHeapHeader) + ((BuddhaHeapHeader *)(_heap + (_start + start + free) % _size))->size;
        }
        if (free >= (sizeof(BuddhaHeapHeader) + size))
            break;
        start += sizeof(BuddhaHeapHeader) + ((BuddhaHeapHeader *)(_heap + (_start + start) % _size))->size;
    }
    if (start >= _size)
    {
        LogError("out of memory");
        return 0;
    }
    header = (BuddhaHeapHeader *)(_heap + (_start + start) % _size);
#ifdef BUDDHA_HEAP_MAGIC
    header->magic = 0x55;
#endif
    header->alloc = 1;
    header->size = free - sizeof(BuddhaHeapHeader);
    header->mark = mark;
    addr = header + 1;
    if ((free - size - sizeof(BuddhaHeapHeader)) > sizeof(BuddhaHeapHeader))
    {
        header->size = size;
        
        _start = (_start + start + sizeof(BuddhaHeapHeader) + header->size) % _size;

        header = (BuddhaHeapHeader *)(((uint8_t *)(header + 1)) + header->size);
#ifdef BUDDHA_HEAP_MAGIC
        header->magic = 0x55;
#endif
        header->alloc = 0;
        header->size = free - sizeof(BuddhaHeapHeader) - size - sizeof(BuddhaHeapHeader);
        header->mark = 0;
    }
    else
    {
        _start = (_start + start + sizeof(BuddhaHeapHeader) + header->size) % _size;
    }
    //LogDebug("addr:%p start:%d mark:%u size:%u", addr, _start, mark, size);
    return addr;
}

void *buddha_heap_talloc(uint32_t size, uint8_t mark)
{
	void *addr = 0;
    if (!mark)
        mark = HEAP_MARK_TEMPORARY;
    addr = buddha_heap_alloc(size, mark);
    if (addr)
        buddha_heap_free(addr);
    return addr;
}

void *buddha_heap_malloc(size_t size)
{
    return buddha_heap_alloc(size, 0);
}

void *buddha_heap_realloc(void *addr, uint32_t size)
{
    uint8_t *heap = ((uint8_t *)addr) - sizeof(BuddhaHeapHeader);

    BuddhaHeapHeader *header = (BuddhaHeapHeader *)heap;
#ifdef BUDDHA_HEAP_MAGIC
    if (0x55 != header->magic)
    {
        buddha_heap_check();
        LogHalt("wrong magic number");
        return 0;
    }
#endif
    if (header->size < size)
    {
        uint32_t free = sizeof(BuddhaHeapHeader) + header->size;
        while (
            (free < (size + sizeof(BuddhaHeapHeader))) &&
            (heap + free < _heap + _size) && 
            (!((BuddhaHeapHeader *)(heap + free))->alloc)
        )
        {
            if (heap + free == _heap + _start)
                _start = (_start + sizeof(BuddhaHeapHeader) + ((BuddhaHeapHeader *)(_heap + _start))->size) % _size;
            free += sizeof(BuddhaHeapHeader) + ((BuddhaHeapHeader *)(heap + free))->size;
        }
        if (free < (sizeof(BuddhaHeapHeader) + size))
        {
            void *new_addr = buddha_heap_alloc(size, header->mark);
            if (!new_addr)
            {
                LogError("out of memory");
                return 0;
            }
            memcpy(new_addr, addr, header->size);
            header->alloc = 0;
            header->mark = 0;
            //LogDebug("addr:%p size:%u start:%u", new_addr, size, _start);
            return new_addr;
        }
        else
        {
            header->size = free - sizeof(BuddhaHeapHeader);
        }
    }

    if (header->size > sizeof(BuddhaHeapHeader) + size)
    {
        BuddhaHeapHeader *next = (BuddhaHeapHeader *)((uint8_t *)(header + 1) + size);
#ifdef BUDDHA_HEAP_MAGIC
        next->magic = 0x55;
#endif
        next->size = header->size - size - sizeof(BuddhaHeapHeader);
        next->alloc = 0;
        next->mark = 0;
        header->size = size;
        if ((uint8_t *)(next + 1) + next->size == _heap + _start)
            _start = (uint8_t *)next - _heap;
    }
    //LogDebug("addr:%p size:%u start:%u", header + 1, size, _start);
    return header + 1;
}

void *buddha_heap_reuse(void *addr)
{
    BuddhaHeapHeader *header = (BuddhaHeapHeader *)(((uint8_t *)addr) - sizeof(BuddhaHeapHeader));
#ifdef BUDDHA_HEAP_MAGIC
    if (header->magic != 0x55)
    {
        buddha_heap_check();
        LogHalt("wrong magic number");
        return 0;
    }
#endif
    header->alloc = 1;
    return header + 1;
}

void buddha_heap_mark_set(void *addr, uint8_t mark)
{
    BuddhaHeapHeader *header = (BuddhaHeapHeader *)((uint8_t *)addr - sizeof(BuddhaHeapHeader));
#ifdef BUDDHA_HEAP_MAGIC
    if (header->magic != 0x55)
    {
        buddha_heap_check();
        LogHalt("wrong magic number");
        return;
    }
#endif
    header->mark = mark;
}

void *buddha_heap_flesh(void *addr)
{
    void *new_addr = 0;
	BuddhaHeapHeader *new_header;
    BuddhaHeapHeader *header = (BuddhaHeapHeader *)((uint8_t *)addr - sizeof(BuddhaHeapHeader));
#ifdef BUDDHA_HEAP_MAGIC
    if (header->magic != 0x55)
    {
        buddha_heap_check();
        LogHalt("wrong magic number");
        return 0;
    }
#endif
    new_addr = buddha_heap_alloc(header->size, header->mark);
    if (!new_addr)
        return addr;
    memcpy(new_addr, addr, header->size);
    new_header = (BuddhaHeapHeader *)((uint8_t *)new_addr - sizeof(BuddhaHeapHeader));
#ifdef BUDDHA_HEAP_MAGIC
    new_header->magic = 0x55;
#endif
    new_header->alloc = header->alloc;
    new_header->mark = header->mark;
    header->alloc = 0;
    header->mark = 0;
    return new_addr;
}

void buddha_heap_free(void *addr)
{
    BuddhaHeapHeader *header = (BuddhaHeapHeader *)((uint8_t *)addr - sizeof(BuddhaHeapHeader));
#ifdef BUDDHA_HEAP_MAGIC
    if (header->magic != 0x55)
    {
        buddha_heap_check();
        LogHalt("wrong magic number");
        return;
    }
#endif
    header->alloc = 0;
    if ((!header->mark) && ((_heap + _start) == ((uint8_t *)addr + header->size)))
        _start = (uint8_t *)header - _heap;
    //LogDebug("addr:%p start:%u", addr, _start);
}

void buddha_heap_destory(void *addr)
{
    BuddhaHeapHeader *header = (BuddhaHeapHeader *)((uint8_t *)addr - sizeof(BuddhaHeapHeader));
#ifdef BUDDHA_HEAP_MAGIC
    if (header->magic != 0x55)
    {
        buddha_heap_check();
        LogHalt("wrong magic number");
        return;
    }
#endif
    header->mark = 0;
    buddha_heap_free(addr);
}

void *buddha_heap_find(void *addr, uint8_t mark)
{
    uint8_t *heap = _heap;
    if (addr)
    {
        heap = ((uint8_t *)addr) - sizeof(BuddhaHeapHeader);
#ifdef BUDDHA_HEAP_MAGIC
        if (((BuddhaHeapHeader *)heap)->magic != 0x55)
        {
            buddha_heap_check();
            LogHalt("wrong magic number");
            return 0;
        }
#endif
        heap += sizeof(BuddhaHeapHeader) + ((BuddhaHeapHeader *)heap)->size;
    }
    while (heap < (_heap + _size))
    {
        BuddhaHeapHeader *header = (BuddhaHeapHeader *)heap;
#ifdef BUDDHA_HEAP_MAGIC
        if (header->magic != 0x55)
        {
            buddha_heap_check();
            LogHalt("wrong magic number");
            return 0;
        }
#endif
        if (header->mark == mark)
            return heap + sizeof(BuddhaHeapHeader);
        heap += sizeof(BuddhaHeapHeader) + header->size;
    }
    return 0;
}

void buddha_heap_check(void)
{
#ifdef DEBUG
	uint32_t pos = 0;
    LogError("buddha memory check start");
    while (pos < _size)
    {
        BuddhaHeapHeader *header = (BuddhaHeapHeader *)(_heap + pos);
        LogError("memory addr:%u magic:%02x alloc:%d size:%d mark:%d", 
            pos, 
#ifdef BUDDHA_HEAP_MAGIC
            header->magic, 
#else
            0,
#endif
            header->alloc, 
            header->size, 
            header->mark);
        pos += sizeof(BuddhaHeapHeader) + header->size;
    }
    LogError("buddha memory check end\r\n");
#endif
}

#else

typedef struct buddha_heap_header
{
    struct buddha_heap_header *next;

    uint8_t mark;
    uint8_t temporary;
    uint32_t size;
}BuddhaHeapHeader;

static BuddhaHeapHeader *_header = 0;
static size_t _size = 0;
static size_t _pos = 0;

void buddha_heap_init(uint8_t *heap, size_t size)
{
    _size = size;
    _pos = 0;
}

void *buddha_heap_alloc(uint32_t size, uint8_t mark)
{
    BuddhaHeapHeader *mem = (BuddhaHeapHeader *)malloc(sizeof(BuddhaHeapHeader) + size);
    if (!mem)
        return 0;
    mem->size = size;
    mem->mark = mark;
    mem->temporary = 0;
    mem->next = _header;
    _header = mem;
    
    _pos += size;

    if (_pos > _size)
    {
        BuddhaHeapHeader *prev = 0, *cur = _header, *last = 0, *last_prev = 0;
        while (cur)
        {
            if (cur->temporary)
            {
                last_prev = prev;
                last = cur;
            }
            prev = cur;
            cur = cur->next;
        }
        if (last)
        {
            _pos -= last->size;
            if (last_prev)
            {
                last_prev->next = last->next;
            }
            else
            {
                _header = last->next;
            }
            free(last);
        }
    }
    return mem + 1;
}

void *buddha_heap_talloc(uint32_t size, uint8_t mark)
{
	BuddhaHeapHeader *mem = 0;
    void *addr = buddha_heap_alloc(size, mark);
    if (!addr)
        return 0;
    mem = (BuddhaHeapHeader *)(((uint8_t *)addr) - sizeof(BuddhaHeapHeader));
    mem->temporary = 1;
    return addr;
}

void *buddha_heap_malloc(size_t size)
{
    return buddha_heap_alloc(size, 0);
}

void *buddha_heap_realloc(void *addr, uint32_t size)
{
	BuddhaHeapHeader *mem, *n;
    if (!addr)
        return 0;
	mem = (BuddhaHeapHeader *)(((uint8_t *)addr) - sizeof(BuddhaHeapHeader));
    n = (BuddhaHeapHeader *)realloc(mem, sizeof(BuddhaHeapHeader) + size);
    if (n == mem)
        return addr;
    if (mem == _header)
    {
        _header = n;
    }
    else
    {
        BuddhaHeapHeader *prev = _header;
        while (prev->next != mem)
            prev = prev->next;
        prev->next = n;
    }
    return n + 1;
}

void *buddha_heap_reuse(void *addr)
{
    BuddhaHeapHeader *mem = (BuddhaHeapHeader *)(((uint8_t *)addr) - sizeof(BuddhaHeapHeader));
    mem->temporary = 0;
    return addr;
}

void buddha_heap_mark_set(void *addr, uint8_t mark)
{
    BuddhaHeapHeader *mem = (BuddhaHeapHeader *)(((uint8_t *)addr) - sizeof(BuddhaHeapHeader));
    mem->mark = mark;
}

void *buddha_heap_flesh(void *addr)
{
    return addr;
}

void buddha_heap_free(void *addr)
{
    BuddhaHeapHeader *mem = (BuddhaHeapHeader *)(((uint8_t *)addr) - sizeof(BuddhaHeapHeader));
    mem->temporary = 1;
}

void buddha_heap_destory(void *addr)
{
    BuddhaHeapHeader *mem = (BuddhaHeapHeader *)(((uint8_t *)addr) - sizeof(BuddhaHeapHeader));

    BuddhaHeapHeader *prev = 0, *cur = _header;
    while (cur)
    {
        if (cur == mem)
            break;
        prev = cur;
        cur = cur->next;
    }
    if (cur)
    {
        _pos -= cur->size;
        if (prev)
        {
            prev->next = cur->next;
        }
        else
        {
            _header = cur->next;
        }
        free(cur);
    }
}

void *buddha_heap_find(void *addr, uint8_t mark)
{
    BuddhaHeapHeader *mem = _header;
    if (addr)
        mem = ((BuddhaHeapHeader *)(((uint8_t *)addr) - sizeof(BuddhaHeapHeader)))->next;
    while (mem)
    {
        if (mem->mark == mark)
            return mem + 1;
        mem = mem->next;
    }
    return 0;
}

void buddha_heap_check(void)
{
	BuddhaHeapHeader *mem;
    LogAction("buddha memory check start");
    mem = _header;
    while (mem)
    {
        LogAction("memory addr:%p alloc:%d size:%d mark:%d", mem + 1, !mem->temporary, mem->size, mem->mark);

        mem = mem->next;
    }
    LogAction("buddha memory check end\r\n");
}

#endif
