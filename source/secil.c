#include "secil.h"

#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "secil.pb.h"

static struct
{
    secil_read_fn read_callback;
    secil_write_fn write_callback;
    secil_log_fn logger;
    void *user_data;
} state;

/// @brief Check if the current state is valid.
/// @return True if the current state is valid, false otherwise.
static bool secil_is_state_valid()
{
    return state.read_callback && state.write_callback;
}

static void secil_log(secil_log_severity_t severity, const char *message)
{
    if (state.logger)
    {
        state.logger(state.user_data, severity, message);
    }
}

/// @brief Callback function for reading from the stream.
/// @param stream - The stream.
/// @param buf - The buffer.
/// @param count - The count.
/// @return true if the read was successful, false otherwise.
static bool secil_read_callback(pb_istream_t *stream, pb_byte_t *buf, size_t count)
{
    return state.read_callback(state.user_data, buf, count);
}

/// @brief Callback function for writing to the stream.
/// @param stream - The stream.
/// @param buf - The buffer.
/// @param count - The count.
static bool secil_write_callback(pb_ostream_t *stream, const pb_byte_t *buf, size_t count)
{
    return state.write_callback(state.user_data, buf, count);
}

/// @brief Creates an pb input stream from the given state.
/// @return An instance of a pb_istream_t structure.
static pb_istream_t secil_create_istream()
{
    pb_istream_t stream;
    stream.callback = &secil_read_callback;
    stream.state = state.user_data;
    stream.bytes_left = 4096; // 4KB max message size: adjust as needed
#ifndef PB_NO_ERRMSG
    stream.errmsg = NULL;
#endif
    return stream;
}

/// @brief Creates an pb output stream from the given state.
/// @return An instance of a pb_ostream_t structure.
static pb_ostream_t secil_create_ostream()
{
    pb_ostream_t stream;
    stream.callback = &secil_write_callback;
    stream.state = state.user_data;
    stream.max_size = 4096; // 4KB max message size: adjust as needed
    stream.bytes_written = 0;
    stream.errmsg = NULL;
    return stream;
}

static void secil_skip_to_next_null(pb_istream_t *stream)
{
    if (!secil_is_state_valid())
    {
        return;
    }
    pb_byte_t byte;
    while (state.read_callback(state.user_data, &byte, 1) && byte != 0);
}

/// @brief Initializes the eme_se_comms library.
/// @param read_callback The read callback function (required).
/// @param write_callback The write callback function (required).
/// @param on_message The on message callback function (required).
/// @param logger The logger callback function (optional - can be null).
/// @param user_data The user data.
/// @return True if the eme_se_comms library was initialized successfully, false otherwise.
bool secil_init(secil_read_fn read_callback,
                secil_write_fn write_callback,
                secil_log_fn logger,
                void *user_data)
{
    state.read_callback = read_callback;
    state.write_callback = write_callback;
    state.logger = logger;
    state.user_data = user_data;

    return secil_is_state_valid();
}

/// @brief Deinitializes the eme_se_comms library.
/// @param handle The handle to the eme_se_comms library.
void secil_deinit()
{
    state.read_callback = NULL;
    state.write_callback = NULL;
    state.logger = NULL;
    state.user_data = NULL;
}

/// @brief Receive a message from the stream.
/// @param type The type of the message (not null).
/// @param payload The payload of the message (not null).
/// @return True if the main loop ran successfully, false otherwise.
bool secil_receive(secil_message *message)
{
    if (!secil_is_state_valid())
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke loop - Invalid state. Have you initialised the library?");
        return false;
    }

    if (!message)
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke loop - message buffer is NULL.");
        return false;
    }

    pb_istream_t stream = secil_create_istream();

    secil_skip_to_next_null(&stream);

    // Decode a message
    if (!pb_decode_ex(&stream, secil_message_fields, message, PB_DECODE_NOINIT | PB_DECODE_DELIMITED))
    {
        secil_log(secil_LOG_WARNING, "Cannot decode message");
        secil_log(secil_LOG_WARNING, stream.errmsg ? stream.errmsg : "Unknown error");

        return false;
    }

    return true;
}

static bool secil_send(const secil_message *message)
{
    if (!secil_is_state_valid())
    {
        return false;
    }

    pb_ostream_t stream = secil_create_ostream();

    // Write null terminator, followed by the message to the stream
    pb_byte_t null_byte = 0;
    return pb_write(&stream, &null_byte, 1) && pb_encode_ex(&stream, secil_message_fields, message, PB_ENCODE_DELIMITED);
}

#define SECIL_SEND(FIELD, VALUE) \
    secil_message message = { \
        .which_payload = secil_message_##FIELD##_tag, \
        .payload = { .FIELD = { .FIELD = VALUE } } \
    }; \
    return secil_send(&message)

bool secil_send_currentTemperature(int8_t currentTemperature)   { SECIL_SEND(currentTemperature, currentTemperature);   }
bool secil_send_heatingSetpoint(int8_t heatingSetpoint)         { SECIL_SEND(heatingSetpoint, heatingSetpoint);         }
bool secil_send_awayHeatingSetpoint(int8_t awayHeatingSetpoint) { SECIL_SEND(awayHeatingSetpoint, awayHeatingSetpoint); }
bool secil_send_coolingSetpoint(int8_t coolingSetpoint)         { SECIL_SEND(coolingSetpoint, coolingSetpoint);         }
bool secil_send_awayCoolingSetpoint(int8_t awayCoolingSetpoint) { SECIL_SEND(awayCoolingSetpoint, awayCoolingSetpoint); }
bool secil_send_hvacMode(int8_t hvacMode)                       { SECIL_SEND(hvacMode, hvacMode);                       }
bool secil_send_relativeHumidity(bool relativeHumidity)         { SECIL_SEND(relativeHumidity, relativeHumidity);       }
bool secil_send_accessoryState(bool accessoryState)             { SECIL_SEND(accessoryState, accessoryState);           }
bool secil_send_demandResponse(bool demandResponse)             { SECIL_SEND(demandResponse, demandResponse);           }
bool secil_send_awayMode(bool awayMode)                         { SECIL_SEND(awayMode, awayMode);                       }
bool secil_send_autoWake(bool autoWake)                         { SECIL_SEND(autoWake, autoWake);                       }
bool secil_send_localUiState(int8_t localUiState)               { SECIL_SEND(localUiState, localUiState);               }
bool secil_send_dateTime(uint64_t dateTime)                     { SECIL_SEND(dateAndTime, dateTime);                    }

// NOTE: This message is different from the others, as it contains a string and cannot be directly assigned like the others.
bool secil_send_supportPackageData(const char *supportPackageData) 
{
    secil_message message = {
        .which_payload = secil_message_supportPackageData_tag,
    };
    strncpy(message.payload.supportPackageData.supportPackageData, supportPackageData, sizeof(message.payload.supportPackageData.supportPackageData) - 1);
    return secil_send(&message);
}
