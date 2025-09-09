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

static secil_error_t secil_send(const secil_message *message);

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

secil_error_t secil_init(secil_read_fn read_callback,
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

void secil_deinit()
{
    state.read_callback = NULL;
    state.write_callback = NULL;
    state.logger = NULL;
    state.user_data = NULL;
}

const char *secil_error_string(secil_error_t error_code)
{
    switch (error_code)
    {
    case SECIL_OK:
        return "No error";
    case SECIL_ERROR_INIT_FAILED:
        return "Initialization failed";
    case SECIL_ERROR_ALREADY_INITIALIZED:
        return "Library already initialized";
    case SECIL_ERROR_NOT_INITIALIZED:
        return "Library not initialized";
    case SECIL_ERROR_INVALID_PARAMETER:
        return "Invalid parameter";
    case SECIL_ERROR_READ_FAILED:
        return "Read operation failed";
    case SECIL_ERROR_WRITE_FAILED:
        return "Write operation failed";
    case SECIL_ERROR_ENCODE_FAILED:
        return "Encoding failed";
    case SECIL_ERROR_DECODE_FAILED:
        return "Decoding failed";
    case SECIL_ERROR_MESSAGE_TOO_LARGE:
        return "Message too large";
    case SECIL_ERROR_UNKNOWN_MESSAGE_TYPE:
        return "Unknown message type";
    case SECIL_ERROR_SEND_FAILED:
        return "Send operation failed";
    case SECIL_ERROR_RECEIVE_FAILED:
        return "Receive operation failed";
    case SECIL_ERROR_STARTUP_FAILED:
        return "Startup failed";
    case SECIL_ERROR_VERSION_MISMATCH:
        return "Version mismatch";
    default:
        return "Unknown error code";
    }
}

secil_error_t secil_receive(secil_message *message)
{
    if (!secil_is_state_valid())
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke loop - Invalid state. Have you initialised the library?");
        return SECIL_ERROR_NOT_INITIALIZED;
    }

    if (!message)
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke loop - message buffer is NULL.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    pb_istream_t stream = secil_create_istream();

    while (true)
    {
        secil_skip_to_next_null(&stream);

        // Decode a message
        if (!pb_decode_ex(&stream, secil_message_fields, message, PB_DECODE_NOINIT | PB_DECODE_DELIMITED))
        {
            secil_log(secil_LOG_WARNING, "Cannot decode message");
            secil_log(secil_LOG_WARNING, stream.errmsg ? stream.errmsg : "Unknown error");

            return SECIL_ERROR_DECODE_FAILED;
        }

        // if this is a loopback test message, handle it here and continue the loop to wait for the next message
        if (message->which_payload == secil_message_loopbackTest_tag)
        {
            // Echo the message back
            if (!secil_send(message))
            {
                secil_log(secil_LOG_ERROR, "Failed to send loopback test message.");
                return SECIL_ERROR_SEND_FAILED;
            }
        }
        else
        {
            break; // exit the loop and return the received message
        }
    }

    return SECIL_OK;
}

static secil_error_t secil_send(const secil_message *message)
{
    if (!secil_is_state_valid())
    {
        return SECIL_ERROR_NOT_INITIALIZED;
    }

    pb_ostream_t stream = secil_create_ostream();

    // Write null terminator, followed by the message to the stream
    pb_byte_t null_byte = 0;
    if (pb_write(&stream, &null_byte, 1) && pb_encode_ex(&stream, secil_message_fields, message, PB_ENCODE_DELIMITED))
    {
        return SECIL_OK;
    }
    else
    {
        return SECIL_ERROR_SEND_FAILED;
    }
}

#define SECIL_SEND(FIELD, VALUE) \
    secil_message message = { \
        .which_payload = secil_message_##FIELD##_tag, \
        .payload = { .FIELD = { .FIELD = VALUE } } \
    }; \
    return secil_send(&message)

secil_error_t secil_send_currentTemperature(int8_t currentTemperature)   { SECIL_SEND(currentTemperature, currentTemperature);   }
secil_error_t secil_send_heatingSetpoint(int8_t heatingSetpoint)         { SECIL_SEND(heatingSetpoint, heatingSetpoint);         }
secil_error_t secil_send_awayHeatingSetpoint(int8_t awayHeatingSetpoint) { SECIL_SEND(awayHeatingSetpoint, awayHeatingSetpoint); }
secil_error_t secil_send_coolingSetpoint(int8_t coolingSetpoint)         { SECIL_SEND(coolingSetpoint, coolingSetpoint);         }
secil_error_t secil_send_awayCoolingSetpoint(int8_t awayCoolingSetpoint) { SECIL_SEND(awayCoolingSetpoint, awayCoolingSetpoint); }
secil_error_t secil_send_hvacMode(int8_t hvacMode)                       { SECIL_SEND(hvacMode, hvacMode);                       }
secil_error_t secil_send_relativeHumidity(bool relativeHumidity)         { SECIL_SEND(relativeHumidity, relativeHumidity);       }
secil_error_t secil_send_accessoryState(bool accessoryState)             { SECIL_SEND(accessoryState, accessoryState);           }
secil_error_t secil_send_demandResponse(bool demandResponse)             { SECIL_SEND(demandResponse, demandResponse);           }
secil_error_t secil_send_awayMode(bool awayMode)                         { SECIL_SEND(awayMode, awayMode);                       }
secil_error_t secil_send_autoWake(bool autoWake)                         { SECIL_SEND(autoWake, autoWake);                       }
secil_error_t secil_send_localUiState(int8_t localUiState)               { SECIL_SEND(localUiState, localUiState);               }
secil_error_t secil_send_dateTime(uint64_t dateTime)                     { SECIL_SEND(dateAndTime, dateTime);                    }

// NOTE: This message is different from the others, as it contains a string and cannot be directly assigned like the others.
secil_error_t secil_send_supportPackageData(const char *supportPackageData) 
{
    secil_message message = {
        .which_payload = secil_message_supportPackageData_tag,
    };
    strncpy(message.payload.supportPackageData.supportPackageData, supportPackageData, sizeof(message.payload.supportPackageData.supportPackageData) - 1);
    return secil_send(&message);
}

secil_error_t secil_loopback_test(const char *test_data)
{
    if (!secil_is_state_valid())
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke loop - Invalid state. Have you initialised the library?");
        return SECIL_ERROR_NOT_INITIALIZED;
    }

    if (!test_data)
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke loop - Invalid parameters.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    size_t test_data_size = strlen(test_data);
    if (test_data_size == 0 || test_data_size >= sizeof(((secil_message*)0)->payload.loopbackTest.data))
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke loop - Test data is empty or too large. Must be non-empty and less than 256 characters.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    secil_message message = { .which_payload = secil_message_loopbackTest_tag };
    strncpy(message.payload.loopbackTest.data, test_data, sizeof(message.payload.loopbackTest.data) - 1);
    
    if (!secil_send(&message))
    {
        secil_log(secil_LOG_ERROR, "Failed to send test data.");
        return SECIL_ERROR_SEND_FAILED;
    }

    memset(&message, 0, sizeof(message));

    if (!secil_receive(&message))
    {
        secil_log(secil_LOG_ERROR, "Failed to receive test data.");
        return SECIL_ERROR_RECEIVE_FAILED;
    }

    if (message.which_payload != secil_message_loopbackTest_tag)
    {
        secil_log(secil_LOG_ERROR, "Loopback test expected to receive a loopbackTest message.");
        return SECIL_ERROR_UNKNOWN_MESSAGE_TYPE;
    }

    if (strncmp(message.payload.loopbackTest.data, test_data, sizeof(message.payload.loopbackTest.data)) != 0)
    {
        secil_log(secil_LOG_ERROR, "Loopback test data does not match sent data: ");
        secil_log(secil_LOG_ERROR, message.payload.loopbackTest.data);
        secil_log(secil_LOG_ERROR, " != ");
        secil_log(secil_LOG_ERROR, test_data);
        return SECIL_ERROR_RECEIVE_FAILED;
    }

    return SECIL_OK;
}

secil_error_t secil_startup_as_client(const char *myversion, char *serverversion, size_t serverversion_size)
{
    if (!secil_is_state_valid())
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke startup - Invalid state. Have you initialised the library?");
        return SECIL_ERROR_NOT_INITIALIZED;
    }

    if (!myversion || !serverversion || serverversion_size == 0)
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke startup - Invalid parameters.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    // Send our startup message
    secil_message startup_message = {
        .which_payload = secil_message_startup_tag,
        .payload = { .startup = { .client = true, .server = false } }
    };
    strncpy(startup_message.payload.startup.version, myversion, sizeof(startup_message.payload.startup.version) - 1);

    secil_error_t send_result = secil_send(&startup_message);
    if (send_result != SECIL_OK)
    {
        secil_log(secil_LOG_ERROR, "Failed to send startup message.");
        return send_result;
    }

    // Wait for the server's startup message
    secil_message response_message;
    secil_receive(&response_message);
    if( response_message.which_payload != secil_message_startup_tag)
    {
        secil_log(secil_LOG_ERROR, "Expected startup message from server MCU.");
        return SECIL_ERROR_UNKNOWN_MESSAGE_TYPE;
    }

    if (!response_message.payload.startup.server)
    {
        secil_log(secil_LOG_ERROR, "Received startup message from server - but server is not ready.");
        return SECIL_ERROR_STARTUP_FAILED;
    }

    // Copy the server version string to the provided buffer
    strncpy(serverversion, response_message.payload.startup.version, serverversion_size - 1);
    serverversion[serverversion_size - 1] = '\0'; // Ensure null termination

    return SECIL_OK;
}

secil_error_t secil_startup_as_server(const char *myversion, char *serverversion, size_t serverversion_size)
{
    if (!secil_is_state_valid())
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke startup - Invalid state. Have you initialised the library?");
        return SECIL_ERROR_NOT_INITIALIZED;
    }

    if (!myversion || !serverversion || serverversion_size == 0)
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke startup - Invalid parameters.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    // Wait for the client's startup message
    secil_message request_message;
    secil_receive(&request_message);
    if( request_message.which_payload != secil_message_startup_tag)
    {
        secil_log(secil_LOG_ERROR, "Expected startup message from client MCU.");
        return SECIL_ERROR_UNKNOWN_MESSAGE_TYPE;
    }

    if (!request_message.payload.startup.client)
    {
        secil_log(secil_LOG_ERROR, "Received startup message from client - but client is not ready.");
        return SECIL_ERROR_STARTUP_FAILED;
    }

    // Copy the client version string to the provided buffer
    strncpy(serverversion, request_message.payload.startup.version, serverversion_size - 1);
    serverversion[serverversion_size - 1] = '\0'; // Ensure null termination

    // Send our startup message
    secil_message startup_message = {
        .which_payload = secil_message_startup_tag,
        .payload = { .startup = { .client = false, .server = true } }
    };
    strncpy(startup_message.payload.startup.version, myversion, sizeof(startup_message.payload.startup.version) - 1);

    secil_error_t send_result = secil_send(&startup_message);
    if (send_result != SECIL_OK)
    {
        secil_log(secil_LOG_ERROR, "Failed to send startup message.");
        return send_result;
    }

    return SECIL_OK;
}

