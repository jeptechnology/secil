#include "secil.h"

#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "secil.pb.h"

#define RETURN_IF_ERROR(operation, log) \
    do                  \
    {                   \
        secil_error_t err = (operation); \
        if (err != SECIL_OK) \
        { \
            if (log) secil_log(secil_LOG_ERROR, log); \
            return err; \
        } \
    } while (0)

static struct
{
    secil_read_fn read_callback;
    secil_write_fn write_callback;
    secil_on_connect_fn on_connect;
    secil_log_fn logger;
    secil_operating_mode_t mode;
    char remote_version[32]; // Version string of the remote end
    void *user_data; // User data pointer passed to callbacks
} state;

static secil_error_t secil_send(const secil_message *message);
static secil_error_t secil_send_startup_message(secil_operating_mode_t mode, bool needs_ack);


/// @brief Check if the current state is valid.
/// @return True if the current state is valid, false otherwise.
static secil_error_t secil_io_callbacks_valid()
{
    if (!state.read_callback || !state.write_callback)
    {
        return SECIL_ERROR_NOT_INITIALIZED;
    }
    return SECIL_OK;
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

static void secil_notify_on_connect()
{
    if (state.on_connect)
    {
        state.on_connect(state.user_data, 
                         state.mode == secil_operating_mode_t_CLIENT ? secil_operating_mode_t_SERVER : secil_operating_mode_t_CLIENT,
                         state.remote_version);
    }
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

static secil_error_t secil_skip_to_next_null(pb_istream_t *stream)
{
    RETURN_IF_ERROR(secil_io_callbacks_valid(), "I/O callbacks not set.");
    pb_byte_t byte;
    while (state.read_callback(state.user_data, &byte, 1) && byte != 0);
    if (byte != 0)
    {
        secil_log(secil_LOG_ERROR, "Failed to find null terminator in stream.");
        return SECIL_ERROR_READ_FAILED;
    }
    return SECIL_OK;
}

secil_error_t secil_init(secil_read_fn read_callback,
                         secil_write_fn write_callback,
                         secil_on_connect_fn on_connect,
                         secil_log_fn logger,
                         void *user_data)
{
    if (secil_io_callbacks_valid() == SECIL_OK)
    {
        return SECIL_ERROR_ALREADY_INITIALIZED;
    }

    state.read_callback = read_callback;
    state.write_callback = write_callback;
    state.on_connect = on_connect;
    state.logger = logger;
    state.user_data = user_data;
    memset(state.remote_version, 0, sizeof(state.remote_version));
    state.mode = secil_operating_mode_t_UNINITIALIZED;

    return secil_io_callbacks_valid();
}

void secil_deinit()
{
    state.read_callback = NULL;
    state.write_callback = NULL;
    state.logger = NULL;
    state.user_data = NULL;
    memset(state.remote_version, 0, sizeof(state.remote_version));
    state.mode = secil_operating_mode_t_UNINITIALIZED;
}

const char *secil_error_string(secil_error_t error_code)
{
    switch (error_code)
    {
    case SECIL_OK:
        return "No error";
    case SECIL_ERROR_INIT_FAILED:
        return "Initialization failed";
    case SECIL_ERROR_INVALID_STATE:
        return "Invalid state";
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

static secil_error_t secil_handle_remote_restarted(secil_message *handshake_message)
{
    // Handshake messages may be received at any time, if the remote end has restarted
    secil_log(secil_LOG_INFO, "Remote end has restarted.");

    if (state.mode == secil_operating_mode_t_UNINITIALIZED)
    {
        secil_log(secil_LOG_ERROR, "Cannot handle remote restart - local end not started up.");
        return SECIL_ERROR_INVALID_STATE;
    }
    
    // We can't have both local and remote end in the same mode
    if (state.mode == handshake_message->payload.handshake.mode)
    {
        secil_log(secil_LOG_ERROR, "Remote end has restarted in unexpected mode.");
        return SECIL_ERROR_INVALID_STATE;
    }

    // Always make a note of the new version string of the remote connection
    strncpy(state.remote_version, handshake_message->payload.handshake.version, sizeof(state.remote_version) - 1);
    state.remote_version[sizeof(state.remote_version) - 1] = '\0'; // Ensure null termination

    if (handshake_message->payload.handshake.needs_ack)
    {
        // Send an ack back to the remote end
        RETURN_IF_ERROR(secil_send_startup_message(state.mode, false), "Failed to send handshake ack to remote end.");

        // Notify the application of the new connection
        secil_notify_on_connect();
    }   

    return SECIL_OK;
}

/// @brief Internal implementation of secil_receive
/// @param message 
/// @return 
static secil_error_t secil_receive_internal(secil_message *message)
{
    RETURN_IF_ERROR(secil_io_callbacks_valid(), "I/O callbacks not set.");

    if (!message)
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke loop - message buffer is NULL.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    pb_istream_t stream = secil_create_istream();

    message->which_payload = 0;

    secil_skip_to_next_null(&stream);

    // Decode a message
    if (!pb_decode_ex(&stream, secil_message_fields, message, PB_DECODE_NOINIT | PB_DECODE_DELIMITED))
    {
        secil_log(secil_LOG_WARNING, "Cannot decode message");
        secil_log(secil_LOG_WARNING, stream.errmsg ? stream.errmsg : "Unknown error");

        return SECIL_ERROR_DECODE_FAILED;
    }

    return SECIL_OK;
}

secil_error_t secil_receive(secil_message *message)
{
    while (true)
    {
        RETURN_IF_ERROR(secil_receive_internal(message), "Could not receive message");

        // Some messages need to be handled internally, such as loopback test messages and handshake messages
        switch (message->which_payload)
        {
        case secil_message_loopbackTest_tag:
            // Just echo the message back
            RETURN_IF_ERROR(secil_send(message), "Failed to send loopback test message.");
            break;

        case secil_message_handshake_tag:
            RETURN_IF_ERROR(secil_handle_remote_restarted(message), "Failed to handle remote restart handshake.");
            break;

        default:
            // Normal message, return it to the caller
            return SECIL_OK;
        }
    }
}

static secil_error_t secil_send(const secil_message *message)
{
    RETURN_IF_ERROR(secil_io_callbacks_valid(), "I/O callbacks not set.");
    
    if (!message)
    {
        secil_log(secil_LOG_ERROR, "Cannot send message - message is NULL.");
        return SECIL_ERROR_INVALID_PARAMETER;
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
    RETURN_IF_ERROR(secil_io_callbacks_valid(), "I/O callbacks not set.");

    if (!test_data)
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke loopback test - Invalid parameters.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    size_t test_data_size = strlen(test_data);
    if (test_data_size == 0 || test_data_size >= sizeof(((secil_message*)0)->payload.loopbackTest.data))
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke loopback test - Test data is empty or too large. Must be non-empty and less than 256 characters.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    secil_message message = { .which_payload = secil_message_loopbackTest_tag };
    strncpy(message.payload.loopbackTest.data, test_data, sizeof(message.payload.loopbackTest.data) - 1);
    
    RETURN_IF_ERROR(secil_send(&message), "Failed to send loopback test message.");

    memset(&message, 0, sizeof(message));

    RETURN_IF_ERROR(secil_receive_internal(&message), "Failed to receive loopback test message.");

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

/// @brief Sends a startup message to the remote end.
/// @param mode The operating mode of this end (client or server).
/// @param needs_ack True if this is the first handshake message and an ack is expected.
/// @return SECIL_OK if the message was sent successfully, otherwise an error code.
static secil_error_t secil_send_startup_message(secil_operating_mode_t mode, bool needs_ack)
{
    RETURN_IF_ERROR(secil_io_callbacks_valid(), "I/O callbacks not set.");

    secil_message message = {
        .which_payload = secil_message_handshake_tag,
        .payload = {
            .handshake = {
                .mode = mode, 
                .needs_ack = needs_ack
            }
        }
    };
    strncpy(message.payload.handshake.version, SECIL_VERSION, sizeof(message.payload.handshake.version) - 1);

    return secil_send(&message);
}

static secil_error_t secil_receive_handshake(secil_operating_mode_t expected_mode)
{
    // Wait for the server's startup message
    secil_message response_message;
    RETURN_IF_ERROR(secil_receive_internal(&response_message), NULL);
    if( response_message.which_payload != secil_message_handshake_tag)
    {
        secil_log(secil_LOG_ERROR, "Expected handshake message from server MCU.");
        return SECIL_ERROR_UNKNOWN_MESSAGE_TYPE;
    }

    if (response_message.payload.handshake.mode != expected_mode)
    {
        if (expected_mode == secil_operating_mode_t_SERVER)
        {
            secil_log(secil_LOG_ERROR, "Received handshake message from remote end but expected a 'server' and got a 'client'.");
        }
        else
        {
            secil_log(secil_LOG_ERROR, "Received handshake message from remote end but expected a 'client' and got a 'server'.");
        }        
        return SECIL_ERROR_STARTUP_FAILED;
    }

    // Copy the server version string to the provided buffer
    strncpy(state.remote_version, response_message.payload.handshake.version, sizeof(state.remote_version) - 1);
    state.remote_version[sizeof(state.remote_version) - 1] = '\0'; // Ensure null termination

    return SECIL_OK;
}

static secil_error_t secil_startup_internal(secil_operating_mode_t mode, bool fail_on_version_mismatch)
{
    // When starting up, we always send an initial startup message
    // If we are a client, we then wait for the server's response
    // If we are a server, we wait for the client's startup message and then respond
    // NOTE: When we are a server and we are restarting, the client will receive a startup message from us and
    //       should respond with its own startup message again. It allows for the client to detect that the server has restarted.
    RETURN_IF_ERROR(secil_send_startup_message(mode, true), "Failed to send startup message.");

    if (mode == secil_operating_mode_t_CLIENT)
    {
        // In client mode, after we send our startup message, we expect a server's handshake response
        RETURN_IF_ERROR(secil_receive_handshake(secil_operating_mode_t_SERVER), "Failed to receive server handshake response.");
    }
    else if (mode == secil_operating_mode_t_SERVER)
    {
        // In server mode, after we send our startup message, we expect a client's handshake response
        RETURN_IF_ERROR(secil_receive_handshake(secil_operating_mode_t_CLIENT), "Failed to receive client handshake response.");
        // But we also must send our startup message again to the client, so it knows we are ready
        RETURN_IF_ERROR(secil_send_startup_message(mode, false), "Failed to send server startup message after client handshake.");
    }
    else
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke startup - Invalid mode.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    if (fail_on_version_mismatch)
    {
        // Check version string matches
        if (strncmp(state.remote_version, SECIL_VERSION, sizeof(state.remote_version)) != 0)
        {
            secil_log(secil_LOG_ERROR, "Version mismatch between client and server:");
            secil_log(secil_LOG_ERROR, " Local version: ");
            secil_log(secil_LOG_ERROR, SECIL_VERSION);
            secil_log(secil_LOG_ERROR, " Remote version: ");
            secil_log(secil_LOG_ERROR, state.remote_version);
            return SECIL_ERROR_VERSION_MISMATCH;
        }
    }

    state.mode = mode;

    // Notify the application of the new connection
    secil_notify_on_connect();

    return SECIL_OK;
}

secil_error_t secil_startup(secil_operating_mode_t mode)
{
    return secil_startup_internal(mode, true);
}

secil_error_t secil_startup_ignore_mismatch(secil_operating_mode_t mode)
{
    return secil_startup_internal(mode, false);
}

secil_error_t secil_get_remote_version(char *version, size_t version_size)
{
    if (state.remote_version[0] == '\0')
    {
        secil_log(secil_LOG_ERROR, "Cannot get remote version - Remote version is not set.");
        return SECIL_ERROR_NOT_INITIALIZED;
    }
    
    if (!version || version_size == 0)
    {
        secil_log(secil_LOG_ERROR, "Cannot get remote version - Invalid parameters.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    strncpy(version, state.remote_version, version_size - 1);
    version[version_size - 1] = '\0'; // Ensure null termination

    return SECIL_OK;
}
