#include "secil.h"

#include <stdarg.h>
#include <stdio.h>
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
            if (log) secil_log(secil_LOG_DEBUG, log); \
            return err; \
        } \
    } while (0)

#define HEADER_SIZE 4
#define FOOTER_SIZE 4
#define HEADROOM 8
#define MAX_MESSAGE_SIZE (HEADER_SIZE + secil_message_size + FOOTER_SIZE + HEADROOM)

static struct
{
    secil_read_fn read_callback;
    secil_write_fn write_callback;
    secil_on_connect_fn on_connect;
    secil_log_fn logger;
    secil_operating_mode_t mode;
    char remote_version[32]; // Version string of the remote end
    void *user_data; // User data pointer passed to callbacks

    char log_buffer[128]; // Buffer for logging messages
    uint8_t outgoingMessage[MAX_MESSAGE_SIZE]; // Buffer for encoding messages
    uint8_t incomingMessage[MAX_MESSAGE_SIZE]; // Buffer for decoding messages

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

static void secil_log(secil_log_severity_t severity, const char *format, ...)
{
    // Call the user-provided logger if available
    if (state.logger)
    {
        va_list args;
        va_start(args, format);
        vsnprintf(state.log_buffer, sizeof(state.log_buffer) - 1, format, args);
        va_end(args);
        state.logger(state.user_data, severity, state.log_buffer);
    }
}

/// @brief Callback function for reading from the stream.
/// @param buf - The buffer.
/// @param count - The count.
/// @return true if the read was successful, false otherwise.
static bool secil_read(pb_byte_t *buf, size_t count)
{
    return state.read_callback(state.user_data, buf, count);
}

/// @brief Callback function for writing to the stream.
/// @param buf - The buffer.
/// @param count - The count.
static bool secil_write(const pb_byte_t *buf, size_t count)
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

/// @brief Create a CRC check for the given memory block.
/// @param crc Initial CRC value.
/// @param mem Pointer to the memory block.
/// @param len Length of the memory block.
/// @return The computed CRC value.
static uint16_t crc16arc_bit(uint16_t crc, void const *mem, size_t len) 
{
    const uint8_t *data = (const uint8_t*)(mem);
    if (data == NULL)
        return 0;
        
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (unsigned k = 0; k < 8; k++) {
            crc = crc & 1 ? (crc >> 1) ^ 0xa001 : crc >> 1;
        }
    }
    return crc;
}

extern pb_istream_t pb_istream_from_buffer(const pb_byte_t *buf, size_t msglen);
extern pb_ostream_t pb_ostream_from_buffer(pb_byte_t *buf, size_t bufsize);

/// @brief Creates an pb input stream from the given state.
/// @return An instance of a pb_istream_t structure.
static pb_istream_t secil_create_istream(uint16_t msglen)
{
    return pb_istream_from_buffer(state.incomingMessage + 4, msglen);
}

/// @brief Creates an pb output stream from the given state.
/// @return An instance of a pb_ostream_t structure.
static pb_ostream_t secil_create_ostream()
{
    return pb_ostream_from_buffer(state.outgoingMessage + 4, sizeof(state.outgoingMessage) - 8); // Leave space for header and footer
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
    memset(state.log_buffer, 0, sizeof(state.log_buffer));
    memset(state.outgoingMessage, 0, sizeof(state.outgoingMessage));
    memset(state.incomingMessage, 0, sizeof(state.incomingMessage));
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
    case SECIL_ERROR_READ_TIMEOUT:
        return "Read operation timed out";
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

static secil_error_t secil_read_next_header()
{
    // First try reading the full header at once
    if (!secil_read(state.incomingMessage, 4))
    {
        return SECIL_ERROR_READ_TIMEOUT;
    }

    while (true)
    {
        // Check for header magic bytes
        if (   state.incomingMessage[0] == 0xCA 
            && state.incomingMessage[1] == 0xFE)
        {
            // Valid header found
            return SECIL_OK;
        }

        // Shift the buffer left by one byte and continue reading
        state.incomingMessage[0] = state.incomingMessage[1];
        state.incomingMessage[1] = state.incomingMessage[2];
        state.incomingMessage[2] = state.incomingMessage[3];
        if (!secil_read(&state.incomingMessage[3], 1))
        {
            return SECIL_ERROR_READ_TIMEOUT;
        }
    }
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

    RETURN_IF_ERROR(secil_read_next_header(), NULL);

    // Read message length from header
    uint16_t message_length = (uint16_t)state.incomingMessage[2] | ((uint16_t)state.incomingMessage[3] << 8);
    if (message_length > secil_message_size)
    {
        secil_log(secil_LOG_ERROR, "Incoming message too large.");
        return SECIL_ERROR_MESSAGE_TOO_LARGE;
    }

    // Read the message body
    if (!secil_read(state.incomingMessage + HEADER_SIZE, message_length + FOOTER_SIZE))
    {
        secil_log(secil_LOG_ERROR, "Failed to read message body.");
        return SECIL_ERROR_READ_TIMEOUT;
    }
    // Verify footer magic bytes
    if (state.incomingMessage[message_length + 6] != 0xFA || state.incomingMessage[message_length + 7] != 0xDE)
    {
        secil_log(secil_LOG_ERROR, "Invalid footer magic bytes.");
        return SECIL_ERROR_DECODE_FAILED;
    }

    // Verify the CRC
    uint16_t received_crc = (uint16_t)state.incomingMessage[message_length + 4] | ((uint16_t)state.incomingMessage[message_length + 5] << 8);
    uint16_t computed_crc = crc16arc_bit(0, state.incomingMessage, message_length + 4);
    if (received_crc != computed_crc)
    {
        secil_log(secil_LOG_ERROR, "Invalid message CRC: expected 0x%04X, got 0x%04X", computed_crc, received_crc);
        return SECIL_ERROR_DECODE_FAILED;
    }

    message->which_payload = 0;

    // Decode the message from
    pb_istream_t stream = secil_create_istream(message_length);
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


static void secil_write_header(uint16_t msglen)
{
    // Write the header, which is two "magic" bytes, followed by the message length as two bytes (little-endian)
    uint8_t *header = state.outgoingMessage;
    header[0] = 0xCA;
    header[1] = 0xFE;
    header[2] = (uint8_t)(msglen & 0xFF);
    header[3] = (uint8_t)((msglen >> 8) & 0xFF);
}

static void secil_write_footer(uint16_t msglen)
{
    // Calculate CRC of header + message
    uint16_t crc = crc16arc_bit(0, state.outgoingMessage, HEADER_SIZE + msglen);
    uint8_t *footer = state.outgoingMessage + HEADER_SIZE + msglen;
    footer[0] = (uint8_t)(crc & 0xFF);
    footer[1] = (uint8_t)((crc >> 8) & 0xFF);
    // Footer magic bytes (0xFADE)
    footer[2] = 0xFA;
    footer[3] = 0xDE;
}

/// @brief Send a secil message
/// @param message The message to send
/// @note The message is sent with a header consisting of two null bytes followed by the message length as two bytes (little-endian).
///       The message itself is encoded using nanopb with a varint length prefix.
///       A footer is then added consisting of a CRC16-ARC checksum of the header and message.
/// @return SECIL_OK if the message was sent successfully, otherwise an error code.
static secil_error_t secil_send(const secil_message *message)
{
    RETURN_IF_ERROR(secil_io_callbacks_valid(), "I/O callbacks not set.");
    
    if (!message)
    {
        secil_log(secil_LOG_ERROR, "Cannot send message - message is NULL.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    pb_ostream_t stream = secil_create_ostream();

    if (!pb_encode_ex(&stream, secil_message_fields, message, PB_ENCODE_DELIMITED))
    {
        return SECIL_ERROR_ENCODE_FAILED;
    }

    uint16_t encoded_message_size = (uint16_t)stream.bytes_written;

    if (encoded_message_size > secil_message_size)
    {
        secil_log(secil_LOG_ERROR, "Cannot send message - encoded message too large.");
        return SECIL_ERROR_MESSAGE_TOO_LARGE;
    }

    // Write the header to the outgoing message buffer
    secil_write_header(encoded_message_size);

    // Write the footer (CRC + magic bytes) to the end of the outgoing message buffer
    secil_write_footer(encoded_message_size);

    // Finally, write the entire message (header + message + footer) to the stream
    if (!secil_write(state.outgoingMessage,  HEADER_SIZE + encoded_message_size + FOOTER_SIZE))
    {
        secil_log(secil_LOG_ERROR, "Failed to write message.");
        return SECIL_ERROR_WRITE_FAILED;
    }

    return SECIL_OK;
}

#define SECIL_SEND_MSG(MSG, FIELD, VALUE) \
    secil_message message = { \
        .which_payload = secil_message_##MSG##_tag, \
        .payload = { .MSG = { .FIELD = VALUE } } \
    }; \
    return secil_send(&message)

// Use this macro when the name of the message is equal to the one and only msg field it contains
#define SECIL_SEND(FIELD, VALUE) SECIL_SEND_MSG(FIELD, FIELD, VALUE)

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
secil_error_t secil_send_pairingState(secil_pairing_state_t state)       { SECIL_SEND_MSG(pairingState, state, state);           }
secil_error_t secil_send_wifiStatus(secil_system_status_t status)        { SECIL_SEND_MSG(wifiStatus, state, status);           }
secil_error_t secil_send_matterStatus(secil_system_status_t status)      { SECIL_SEND_MSG(matterStatus, state, status);         }
secil_error_t secil_send_factoryReset(secil_reset_state_t state)         { SECIL_SEND_MSG(factoryReset, state, state);         }

secil_error_t secil_send_otaStatus(secil_ota_state_t state, uint8_t progress, const char *version) 
{
    if (!version)
    {
        version = "";
    }
    if (progress > 100)
    {
        progress = 100;
    }

    secil_message message = {
        .which_payload = secil_message_otaStatus_tag,
        .payload = { .otaStatus = { 
            .state = state,
            .progress = progress
        } }
    };

    strncpy(message.payload.otaStatus.version, version, sizeof(message.payload.otaStatus.version) - 1);

    return secil_send(&message);
}

secil_error_t secil_send_warning(secil_warning_type_t type, const char *message) 
{
    if (!message)
    {
        secil_log(secil_LOG_ERROR, "Cannot send warning - message is NULL.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    secil_message msg = {
        .which_payload = secil_message_warning_tag,
        .payload = { .warning = { .type = type } }
    };
    strncpy(msg.payload.warning.message, message, sizeof(msg.payload.warning.message) - 1);
    return secil_send(&msg);
}

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

static secil_error_t secil_receive_handshake(secil_operating_mode_t our_mode)
{
    secil_operating_mode_t expected_mode = (our_mode == secil_operating_mode_t_CLIENT) ? secil_operating_mode_t_SERVER : secil_operating_mode_t_CLIENT;

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

    // If the handshake message had needs_ack set, we must respond with our own handshake message
    if (response_message.payload.handshake.needs_ack)
    {
        RETURN_IF_ERROR(secil_send_startup_message(our_mode, false), "Failed to send handshake ack to remote end.");
    }

    return SECIL_OK;
}

static secil_error_t secil_startup_internal(secil_operating_mode_t mode, bool fail_on_version_mismatch)
{
    if (mode == secil_operating_mode_t_UNINITIALIZED)
    {
        secil_log(secil_LOG_ERROR, "Cannot invoke startup - Invalid mode.");
        return SECIL_ERROR_INVALID_PARAMETER;
    }

    // When starting up, we always send an initial startup message
    // If we are a client, we then wait for the server's response
    // If we are a server, we wait for the client's startup message and then respond
    // NOTE: When we are a server and we are restarting, the client will receive a startup message from us and
    //       should respond with its own startup message again. It allows for the client to detect that the server has restarted.
    RETURN_IF_ERROR(secil_send_startup_message(mode, true), "Failed to send handshake message to remote end.");
    RETURN_IF_ERROR(secil_receive_handshake(mode), "Failed to receive handshake message from remote end.");

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

    // Now confirm that we are fully initialized
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
