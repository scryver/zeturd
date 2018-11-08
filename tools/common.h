#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <memory.h>
#include <assert.h>

#define internal  static
#define global    static
#define persist   static
#define unused(x) (void)x

#define false     0
#define true      1

#define i_expect  assert
#define INVALID_CODE_PATH    i_expect(0 && "Invalid code path")
#define INVALID_DEFAULT_CASE default: { i_expect(0 && "Invalid default case"); } break

#define array_count(x)       (sizeof(x) / sizeof(x[0]))
#define is_pow2(x)           (((x) != 0) && (((x) & ((x)-1)) == 0))
#define align_down(x, a)     ((x) & ~((a) - 1))
#define align_up(x, a)       align_down((x) + (a) - 1, (a))
#define align_ptr_down(p, a) ((void *)align_down((uptr)(p), (a)))
#define align_ptr_up(p, a)   ((void *)align_up((uptr)(p), (a)))

#define minimum(a, b)        ((a) < (b) ? (a) : (b))
#define maximum(a, b)        ((a) > (b) ? (a) : (b))

#define U8_MAX    0xFF
#define U16_MAX   0xFFFF
#define U32_MAX   0xFFFFFFFF
#define U64_MAX   0xFFFFFFFFFFFFFFFF

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;

typedef int8_t    b8;
typedef int16_t   b16;
typedef int32_t   b32;
typedef int64_t   b64;

typedef float     f32;
typedef double    f64;

typedef size_t    uptr;

typedef struct Buffer
{
    u32 size;
    u8 *data;
} Buffer;
typedef Buffer String;

// TODO(michiel): Own struct for this
typedef struct FileStream
{
    b32 verbose;
    FILE *file;
} FileStream;

internal inline u32 safe_truncate_to_u32(u64 value)
{
    i_expect(value <= U32_MAX);
    return (u32)(value & U32_MAX);
}

typedef enum AllocateFlags
{
    ALLOC_NOCLEAR = 0x01,  // NOTE(michiel): This is a NO so a flag value of 0 clears the memory by default
} AllocateFlags;

#define allocate_array(count, type, flags) (type *)allocate_size(sizeof(type) * count, flags)
#define allocate_struct(type, flags) (type *)allocate_size(sizeof(type), flags)
internal inline void *allocate_size(u32 size, u32 flags)
{
    void *result = NULL;

    b32 clear = !(flags & ALLOC_NOCLEAR);

    if (clear)
    {
        result = calloc(size, 1);
    }
    else
    {
        result = malloc(size);
    }

    return result;
}

internal inline Buffer allocate_buffer(u32 size, u32 flags)
{
    Buffer result = {.size = size};
    result.data = allocate_array(size, u8, flags);
    if (result.data == NULL)
    {
        result.size = 0;
    }
    return result;
}

internal inline void *
reallocate_size(void *data, u32 size)
{
    void *result = NULL;
    result = realloc(data, size);
    return result;
}

internal inline void deallocate(void *mem)
{
    if (mem)
    {
        free(mem);
    }
}

typedef struct BufferHeader
{
    u32 len;
    u32 cap;
    u8 data[];
} BufferHeader;

#define buf_free(buf)           ((buf) ? deallocate(buf_hdr(buf)), NULL : NULL)
#define buf_clear(buf)          ((buf) ? (buf_len_(buf) = 0, (buf)) : 0)
#define buf_push(buf, val)      (buf_maybegrow(buf, 1), (buf)[buf_len_(buf)++] = (val))
#define buf_add(buf, n)         (buf_maybegrow(buf, n), buf_len_(buf) += (n), &(buf)[buf_len_(buf) - (n)])
#define buf_last(buf)           ((buf)[buf_len(buf) - 1])
#define buf_end(buf)            ((buf) + buf_len(buf))
#define buf_len(buf)            ((buf) ? buf_len_(buf) : 0)
#define buf_cap(buf)            ((buf) ? buf_cap_(buf) : 0)

#define buf_hdr(buf)            ((BufferHeader *)((u8 *)(buf) - offsetof(BufferHeader, data)))
#define buf_len_(buf)           (buf_hdr(buf)->len)
#define buf_cap_(buf)           (buf_hdr(buf)->cap)

#define buf_needgrow(buf, len)  (((buf) == 0) || ((buf_len_(buf) + (len)) >= buf_cap_(buf)))
#define buf_maybegrow(buf, len) (buf_needgrow(buf, (len)) ? buf_grow(buf, len), 0 : 0)

#define BUFFER_ADDRESS_MOD 1

#if BUFFER_ADDRESS_MOD
#define buf_grow(buf, len)      (buf_grow_((void **)&(buf), (len), sizeof(*(buf))))
#else
#define buf_grow(buf, len)      (*((void **)&(buf)) = buf_grow_((buf), (len), sizeof(*(buf))))
#endif // BUFFER_ADDRESS_MOD
//#define buf_put(buf, val)  (buf_put_(&buf, val, 1, sizeof(*buf)))

#if BUFFER_ADDRESS_MOD
internal inline void
buf_grow_(void **bufPtr, u32 increment, u32 elemSize)
#else
internal inline void *
buf_grow_(void *buf, u32 increment, u32 elemSize)
#endif
{
#if BUFFER_ADDRESS_MOD
    void *buf = *bufPtr;
#endif
    i_expect(buf_cap(buf) <= ((U32_MAX - 1) / 2));
    u32 newCap = 2 * buf_cap(buf);
    u32 newLength = buf_len(buf) + increment;
    if (newCap < 16)
    {
        newCap = 16;
    }
    if (newCap < newLength)
    {
        newCap = newLength;
    }
    i_expect(newLength <= newCap);
    i_expect(newCap <= ((U32_MAX - offsetof(BufferHeader, data)) / elemSize));
    u32 newSize = offsetof(BufferHeader, data) + newCap * elemSize;
    BufferHeader *newHeader = reallocate_size(buf ? buf_hdr(buf) : 0, newSize);
    i_expect(newHeader);
    if (!buf)
    {
        newHeader->len = 0;
    }
    newHeader->cap = newCap;
#if BUFFER_ADDRESS_MOD
    *bufPtr = newHeader->data;
#else
    return newHeader->data;
#endif
}

#if 0
internal inline void
buf_put_(void **bufPtr, void *data, u32 newLength, u32 elemSize)
{
    void *buf = *bufPtr;
    if (buf_len(buf) == buf_cap(buf))
    {
        buf_grow_(buf, newLength, elemSize);
    }
    memcpy((u8 *)buf + buf_hdr(buf)->len * elemSize, data, newLength * elemSize);
    buf_hdr(buf)->len += newLength;
}
#endif

typedef struct Arena
{
    char *at;
    char *end;
    char **blocks;
} Arena;

#define ARENA_ALIGNMENT      8
#define ARENA_ALLOC_MIN_SIZE (1024 * 1024)

internal inline void
arena_grow(Arena *arena, uptr newSize)
{
    uptr size = align_up(newSize < ARENA_ALLOC_MIN_SIZE ? ARENA_ALLOC_MIN_SIZE : newSize, ARENA_ALIGNMENT);
    arena->at = allocate_size(size, 0);
    i_expect(arena->at == align_ptr_down(arena->at, ARENA_ALIGNMENT));
    arena->end = arena->at + size;
    buf_push(arena->blocks, arena->at);
}

internal inline void *
arena_allocate(Arena *arena, uptr newSize)
{
    if (newSize > (uptr)(arena->end - arena->at))
    {
        arena_grow(arena, newSize);
        i_expect(newSize <= (uptr)(arena->end - arena->at));
    }
    void *at = arena->at;
    arena->at = align_ptr_up(arena->at + newSize, ARENA_ALIGNMENT);
    i_expect(arena->at <= arena->end);
    i_expect(at == align_ptr_down(at, ARENA_ALIGNMENT));
    return at;
}

internal void
arena_free(Arena *arena)
{
    for (char **it = arena->blocks; it != buf_end(arena->blocks); ++it)
    {
        deallocate(*it);
    }
    buf_free(arena->blocks);
}

// NOTE(michiel): Better hash functions
internal inline u64
hash_u64(u64 x) {
    x *= 0xFF51AFD7ED558CCD;
    x ^= x >> 32;
    return x;
}
internal inline u64
hash_ptr(void *ptr) {
    return hash_u64((uptr)ptr);
}

internal inline u64
hash_mix(u64 x, u64 y) {
    x ^= y;
    return hash_u64(x);
}

internal inline u64
hash_bytes(void *ptr, uptr len) {
    u64 x = 0xCBF29CE484222325;
    char *buf = (char *)ptr;
    for (uptr i = 0; i < len; i++) {
        x ^= buf[i];
        x *= 0x100000001B3;
        x ^= x >> 32;
    }
    return x;
}

typedef struct Map
{
    u64 *keys;
    u64 *values;
    u32 len;
    u32 cap;
} Map;

#define map_get(map, key)               (void *)(uptr)map_get_u64_from_u64(map, (u64)(uptr)(key))
#define map_put(map, key, val)          map_put_u64_from_u64(map, (u64)(uptr)(key), (u64)(uptr)(val))
#define map_get_u64(map, key)           map_get_u64_from_u64(map, (u64)(uptr)(key))
#define map_put_u64(map, key, val)      map_put_u64_from_u64(map, (u64)(uptr)(key), val)
#define map_get_from_u64(map, key)      (void *)(uptr)map_get_u64_from_u64(map, key)
#define map_put_from_u64(map, key, val) map_put_u64_from_u64(map, key, (u64)(uptr)(val));

internal inline u64
map_get_u64_from_u64(Map *map, u64 key)
{
    u64 result = 0;
    if (map->len > 0)
    {
        i_expect(is_pow2(map->cap));
        uptr hash = (uptr)hash_u64(key);
        i_expect(map->len < map->cap);
        for (;;)
        {
            hash &= map->cap - 1;
            if (map->keys[hash] == key)
            {
                result = map->values[hash];
                break;
            }
            else if (!map->keys[hash])
            {
                result = 0;
                break;
            }
            ++hash;
        }
    }
    else
    {
        result = 0;
    }

    return result;
}

internal inline void map_put_u64_from_u64(Map *map, u64 key, u64 value);

internal inline void
map_grow(Map *map, uptr newCap)
{
    newCap = (newCap < 16) ? 16 : newCap;
    Map newMap = {
        .keys = allocate_array(newCap, u64, 0),
        .values = allocate_array(newCap, u64, ALLOC_NOCLEAR),
        .cap = newCap,
    };
    for (u32 mapIndex = 0; mapIndex < map->cap; ++mapIndex)
    {
        if (map->keys[mapIndex])
        {
            map_put_u64_from_u64(&newMap, map->keys[mapIndex], map->values[mapIndex]);
        }
    }
    deallocate(map->keys);
    deallocate(map->values);
    *map = newMap;
}

internal inline void
map_put_u64_from_u64(Map *map, u64 key, u64 value)
{
    i_expect(key);
    if (!value)
    {
        // NOTE(michiel): Don't put zeroes in
        return;
    }

    if ((2 * map->len) >= map->cap)
    {
        map_grow(map, 2 * map->cap);
    }

    i_expect(2 * map->len < map->cap);
    i_expect(is_pow2(map->cap));
    uptr hash = (uptr)hash_u64(key);
    for (;;)
    {
        hash &= map->cap - 1;
        if (!map->keys[hash])
        {
            ++map->len;
            map->keys[hash] = key;
            map->values[hash] = value;
            break;
        }
        else if (map->keys[hash] == key)
        {
            map->values[hash] = value;
            break;
        }
        ++hash;
    }
}
