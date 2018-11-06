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

internal u32
string_length(char *cString)
{
    u32 length = 0;
    while (*cString++)
    {
        ++length;
    }
    return length;
}

internal inline String
create_string(char *cString)
{
    String result = {0};
    result.size = string_length(cString);
    result.data = (u8 *)cString;
    return result;
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

internal String
str_internalize_cstring(char *cString)
{
    String str = create_string(cString);
    return str_internalize(str);
}
