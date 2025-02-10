#include "secil.h"

#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "secil.pb.h"

static struct {
    secil_read_fn read_callback;
    secil_write_fn write_callback;
    secil_on_message_received_fn on_message;
    secil_log_fn logger;
    void *user_data;
} state;

/// @brief Check if the current state is valid.
/// @return True if the current state is valid, false otherwise.
static bool msmt_is_state_valid()
{
    return state.read_callback && state.write_callback && state.on_message;
}

static void msmt_log(secil_log_severity_t severity, const char *message)
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
static bool msmt_read_callback(pb_istream_t *stream, pb_byte_t *buf, size_t count)
{
    return state.read_callback(state.user_data, buf, count);
}

/// @brief Callback function for writing to the stream.
/// @param stream - The stream.
/// @param buf - The buffer.
/// @param count - The count.
static bool msmt_write_callback(pb_ostream_t *stream, const pb_byte_t *buf, size_t count)
{
    return state.write_callback(state.user_data, buf, count);
}

/// @brief Creates an pb input stream from the given state.
/// @return An instance of a pb_istream_t structure.
static pb_istream_t msmt_create_istream()
{
    pb_istream_t stream;
    stream.callback = &msmt_read_callback;
    stream.state = state.user_data;
    stream.bytes_left = 4096; // 4KB max message size: adjust as needed
#ifndef PB_NO_ERRMSG
    stream.errmsg = NULL;
#endif
    return stream;
}

/// @brief Creates an pb output stream from the given state.
/// @return An instance of a pb_ostream_t structure.
static pb_ostream_t msmt_create_ostream()
{
    pb_ostream_t stream;
    stream.callback = &msmt_write_callback;
    stream.state = state.user_data;
    stream.max_size = 4096; // 4KB max message size: adjust as needed
    stream.bytes_written = 0;
    return stream;
}

static void msmt_skip_to_next_null(pb_istream_t* stream)
{
    if (!msmt_is_state_valid())
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
                     secil_on_message_received_fn on_message,
                     secil_log_fn logger,
                     void *user_data)
{
    state.read_callback = read_callback;
    state.write_callback = write_callback;
    state.on_message = on_message;
    state.logger = logger;
    state.user_data = user_data;

    return msmt_is_state_valid();
}

/// @brief Deinitializes the eme_se_comms library.
/// @param handle The handle to the eme_se_comms library.
void secil_deinit()
{
    state.read_callback = NULL;
    state.write_callback = NULL;
    state.on_message = NULL;
    state.logger = NULL;
    state.user_data = NULL;
}

/// @brief The main loop of the eme_se_comms library - this function should be called repeatedly in a loop.
/// @param handle The handle to the eme_se_comms library.
/// @return True if the main loop ran successfully, false otherwise.
bool secil_loop(unsigned int max_iterations)
{
    if (!msmt_is_state_valid())
    {
        msmt_log(secil_LOG_ERROR, "Cannot invoke loop - Invalid state. Have you initialised the library?");
        return false;
    }

    pb_istream_t stream = msmt_create_istream();

    for (unsigned int i = 0; i < max_iterations; i++)
    {
        msmt_skip_to_next_null(&stream);
        
        // Decode a message
        SecilMessage message = SecilMessage_init_zero;
        if (!pb_decode_ex(&stream, SecilMessage_fields, &message, PB_DECODE_NOINIT | PB_DECODE_DELIMITED))
        {
            msmt_log(secil_LOG_WARNING, "Cannot decode message");
            msmt_log(secil_LOG_WARNING, stream.errmsg);

            return false;
        }

        // Handle the message
        state.on_message(state.user_data, (secil_message_type_t)message.which_payload, (const secil_message_payload*)&message.payload);
    }

    return true;
}

static bool secil_send(const SecilMessage* message)
{
    if (!msmt_is_state_valid())
    {
        return false;
    }

    pb_ostream_t stream = msmt_create_ostream();
    
    // Write null terminator, followed by the message to the stream
    pb_byte_t null_byte = 0;
    return pb_write(&stream, &null_byte, 1)
        && pb_encode_ex(&stream, SecilMessage_fields, message, PB_ENCODE_DELIMITED);
}

bool secil_send_currentTemperature(int8_t currentTemperature)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_currentTemperature_tag;
    message.payload.currentTemperature.currentTemperature = currentTemperature;
    return secil_send(&message);
}

bool secil_send_heatingSetpoint(int8_t heatingSetpoint)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_heatingSetpoint_tag;
    message.payload.heatingSetpoint.heatingSetpoint = heatingSetpoint;
    return secil_send(&message);
}

bool secil_send_awayHeatingSetpoint(int8_t awayHeatingSetpoint)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_awayHeatingSetpoint_tag;
    message.payload.awayHeatingSetpoint.awayHeatingSetpoint = awayHeatingSetpoint;
    return secil_send(&message);
}

bool secil_send_coolingSetpoint(int8_t coolingSetpoint)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_coolingSetpoint_tag;
    message.payload.coolingSetpoint.coolingSetpoint = coolingSetpoint;
    return secil_send(&message);
}

bool secil_send_awayCoolingSetpoint(int8_t awayCoolingSetpoint)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_awayCoolingSetpoint_tag;
    message.payload.awayCoolingSetpoint.awayCoolingSetpoint = awayCoolingSetpoint;
    return secil_send(&message);
}

bool secil_send_hvacMode(int8_t hvacMode)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_hvacMode_tag;
    message.payload.hvacMode.hvacMode = hvacMode;
    return secil_send(&message);
}

bool secil_send_relativeHumidity(bool relativeHumidity)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_relativeHumidity_tag;
    message.payload.relativeHumidity.relativeHumidity = relativeHumidity;
    return secil_send(&message);
}

bool secil_send_accessoryState(bool accessoryState)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_accessoryState_tag;
    message.payload.accessoryState.accessoryState = accessoryState;
    return secil_send(&message);
}

bool secil_send_supportPackageData(const char *supportPackageData)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_supportPackageData_tag;
    strncpy(message.payload.supportPackageData.supportPackageData, supportPackageData, sizeof(message.payload.supportPackageData));
    return secil_send(&message);
}

bool secil_send_demandResponse(bool demandResponse)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_demandResponse_tag;
    message.payload.demandResponse.demandResponse = demandResponse;
    return secil_send(&message);
}

bool secil_send_awayMode(bool awayMode)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_awayMode_tag;
    message.payload.awayMode.awayMode = awayMode;
    return secil_send(&message);
}

bool secil_send_autoWake(bool autoWake)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_autoWake_tag;
    message.payload.autoWake.autoWake = autoWake;
    return secil_send(&message);
}

bool secil_send_localUiState(int8_t localUiState)
{
    SecilMessage message = SecilMessage_init_zero;
    message.which_payload = SecilMessage_localUiState_tag;
    message.payload.localUiState.localUiState = localUiState;
    return secil_send(&message);
}


