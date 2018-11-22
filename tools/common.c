internal Buffer
read_entire_file(char *filename)
{
    Buffer result = {0};

    FILE *file = fopen(filename, "rb");
    // NOTE(michiel): Get the file size
    fseek(file, 0, SEEK_END);
    result.size = safe_truncate_to_u32(ftell(file));
    // NOTE(michiel): Reset the current file pointer to the beginning
    fseek(file, 0, SEEK_SET);
    // NOTE(michiel): Allocate memory for the file data
    result.data = allocate_array(result.size, u8, 0);
    // NOTE(michiel): Read the actual data from the file
    s64 bytesRead = fread(result.data, 1, result.size, file);
    i_expect(bytesRead == (s64)result.size);
    fclose(file);

    return result;
}

internal inline u32
log2_up(u32 value)
{
    u32 bitPos = 0;
    while (((u64)1 << bitPos) <= value)
        {
            ++bitPos;
        }
    return bitPos;
}

internal inline u32
string_length(char *cString)
{
    u32 length = 0;
    while (*cString++)
    {
        ++length;
    }
    return length;
}

internal b32
strings_are_equal(String a, String b)
{
    b32 result = (a.size == b.size);

    if (result)
    {
        for (u32 nameIndex = 0; nameIndex < a.size; ++nameIndex)
        {
            if (a.data[nameIndex] != b.data[nameIndex])
            {
                result = false;
                break;
            }
        }
    }

    return result;
}

internal b32
strings_are_equal_a(uptr aSize, char *a, String b)
{
    b32 result = (aSize == b.size);

    if (result)
    {
        for (u32 nameIndex = 0; nameIndex < aSize; ++nameIndex)
        {
            if (a[nameIndex] != b.data[nameIndex])
            {
                result = false;
                break;
            }
        }
    }

    return result;
}

// NOTE(michiel): For internal storage of strings
typedef struct InternString
{
    struct InternString *next;

    u32 size;
    char data[];
} InternString;

global Map gInternStrings;
global Arena gInternArena;

internal String
str_internalize(String str)
{
    u64 hash = hash_bytes(str.data, str.size);
    u64 key = hash ? hash : 1;
    InternString *intern = map_get_from_u64(&gInternStrings, key);
    InternString *it = intern;
    //for (InternString *it = intern; it; it = it->next)
    while (it)
    {
        if (strings_are_equal_a(it->size, it->data, str))
        {
            return (String){.size = it->size, .data=(u8 *)it->data};
        }
        it = it->next;
    }

    InternString *newIntern = arena_allocate(&gInternArena, offsetof(InternString, data) + str.size);
    newIntern->size = str.size;
    newIntern->next = intern;
    memcpy(newIntern->data, str.data, str.size);
    map_put_from_u64(&gInternStrings, key, newIntern);
    return (String){.size=newIntern->size, .data=(u8 *)newIntern->data};
}

internal inline String
create_string_(char *cString)
{
    String result = {0};
    result.size = string_length(cString);
    result.data = (u8 *)cString;
    return result;
}

internal inline String
create_string(char *cString)
{
    String str = create_string_(cString);
    String result = str_internalize(str);
    return result;
}

internal inline String
str_internalize_cstring(char *cString)
{
      String result = create_string(cString);
    return result;
}

internal inline String
create_string_fmt(char *fmt, ...)
{
    static char buffer[4096];
    
    va_list args;
    va_start(args, fmt);
    u64 written = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    i_expect(written < sizeof(buffer));
    buffer[written] = 0;
    return create_string(buffer);
}

internal s64
string_to_number(String s)
{
    s64 result = 0;
    s64 base = 10;
    if ((s.size > 2) &&
        (s.data[0] == '0'))
    {
        if ((s.data[1] == 'b') ||
            (s.data[1] == 'B'))
        {
            base = 2;
            --s.size;
            ++s.data;
        }
        else if ((s.data[1] == 'x') ||
                 (s.data[1] == 'X'))
        {
            base = 16;
            --s.size;
            ++s.data;
        }
        else
        {
            base = 8;
        }
        --s.size;
        ++s.data;
    }
    
    for (u32 sIdx = 0; sIdx < s.size; ++sIdx)
    {
        result *= base;
        s64 adding = 0;
        if (('0' <= s.data[sIdx]) &&
            (s.data[sIdx] <= '9'))
        {
        adding = s.data[sIdx] - '0';
        }
        else if (('a' <= s.data[sIdx]) && (s.data[sIdx] <= 'f'))
        {
            i_expect(base == 16);
            adding = (s.data[sIdx] - 'a') + 10;
        }
        else 
        {
            i_expect(('A' <= s.data[sIdx]) && (s.data[sIdx] <= 'F'));
            i_expect(base == 16);
            adding = (s.data[sIdx] - 'A') + 10;
        }
        i_expect(adding >= 0);
        i_expect(adding < base);
        result += adding;
    }
    return result;
}
