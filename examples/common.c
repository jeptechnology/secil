#include <secil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char buffer[1024];
static size_t start_pos = 0;
static size_t end_pos = 0;

static void log_fn(void *user_data, secil_log_severity_t severity, const char *message)
{
    printf("%s\n", message);
}

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
    if (end_pos + count > sizeof(buffer))
    {
        return false;
    }

    memcpy(buffer + end_pos, buf, count);
    end_pos += count;

    return true;
}

/// @brief Inject an error into the loopback stream by appending random bytes to the buffer.
/// @param bytes Number of bytes to inject.
void inject_loopback_error(size_t bytes)
{
    // Inject random bytes
    for (size_t i = 0; i < bytes; i++)
    {
        unsigned char randomChar = rand() % 256;
        write_fn(NULL, &randomChar, 1);
    }
}

/// @brief Log the message received.
/// @param type 
/// @param payload 
void log_message_received(secil_message_type_t type, secil_message_payload* payload)
{
    switch (type)
    {
    case secil_message_type_accessoryState:
        printf("Received Accessory state: %d\n", payload->accessoryState.accessoryState);
        break;
    case secil_message_type_autoWake:
        printf("Received Auto wake: %d\n", payload->autoWake.autoWake);
        break;
    case secil_message_type_awayMode:
        printf("Received Away mode: %d\n", payload->awayMode.awayMode);
        break;
    case secil_message_type_currentTemperature:
        printf("Received Current temperature: %d\n", payload->currentTemperature.currentTemperature);
        break;
    case secil_message_type_demandResponse:
        printf("Received Demand response: %d\n", payload->demandResponse.demandResponse);
        break;
    case secil_message_type_localUiState:
        printf("Received Local UI state: %d\n", payload->localUiState.localUiState);
        break;
    case secil_message_type_relativeHumidity:
        printf("Received Relative humidity: %d\n", payload->relativeHumidity.relativeHumidity);
        break;
    case secil_message_type_supportPackageData:
        printf("Received Support package data: %s\n", payload->supportPackageData.supportPackageData);
        break;
    default:
        printf("Received unknown message type: %d\n", type);
        break;
    }
}

/// @brief Initialise the common example code with a ram based buffer for a loopback test.
/// @return true if the initialisation was successful, false otherwise.
bool init_secil_loopback()
{
    return secil_init(
        read_fn, 
        write_fn,
        log_fn,    
        NULL);
}