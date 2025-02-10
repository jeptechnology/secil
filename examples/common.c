#include <secil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char buffer[1024];
static size_t buffer_size = 0;
static size_t start_pos = 0;
static size_t end_pos = 0;

static bool read_fn(void *user_data, unsigned char *buf, size_t required_count)
{
    if (start_pos + required_count > end_pos)
    {
        return false;
    }

    memcpy(buf, buffer + start_pos, required_count);
    start_pos += required_count;

    return true;
}

static bool write_fn(void *user_data, const unsigned char *buf, size_t count)
{
    if (buffer_size + count > sizeof(buffer))
    {
        return false;
    }

    memcpy(buffer + buffer_size, buf, count);
    return true;
}

static void log_fn(void *user_data, secil_log_severity_t severity, const char *message)
{
    printf("%s\n", message);
}

void inject_error(size_t bytes)
{
    // Inject random bytes
    for (size_t i = 0; i < bytes; i++)
    {
        unsigned char randomChar = rand() % 256;
        write_fn(NULL, &randomChar, 1);
    }
}

bool common_init()
{
    return secil_init(
        read_fn, 
        write_fn,
        log_fn,    
        NULL);
}