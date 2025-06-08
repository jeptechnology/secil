/// @file secil.h
/// @brief The Schneider-EME Comms Interface Library (SECIL)
///        A library for communicating between Schneider Electric's Application Processor and EME's Thermostat Application.

#if !defined(SECIL_H)
#define SECIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C"
{
#endif

    typedef enum
    {
        secil_SUCCESS,
        secil_ERROR,
        secil_ERROR_INVALID_ARGUMENT,
        secil_ERROR_INVALID_STATE,
        secil_ERROR_INVALID_MESSAGE,
        secil_ERROR_INVALID_PAYLOAD
    } secil_error_t;

    /// @brief Signative for a callback function that reads required_count from a stream and writes to the given buffer.
    /// @param user_data The user data.
    /// @param buf The buffer to write to.
    /// @param required_count The number of bytes to read.
    /// @return True if we read required_count bytes successfully, false otherwise.
    typedef bool (*secil_read_fn)(void *user_data, unsigned char *buf, size_t required_count);

    /// @brief Signature for a callback function that writes count bytes from the given buffer to a stream.
    /// @param user_data The user data.
    /// @param buf The buffer to read from.
    /// @param count The number of bytes to write.
    /// @return True if we wrote count bytes successfully, false otherwise.
    typedef bool (*secil_write_fn)(void *user_data, const unsigned char *buf, size_t count);

    /* Struct definitions */
    typedef struct
    {
        int8_t currentTemperature;
    } secil_currentTemperature;
    typedef struct
    {
        int8_t heatingSetpoint;
    } secil_heatingSetpoint;
    typedef struct
    {
        int8_t awayHeatingSetpoint;
    } secil_awayHeatingSetpoint;
    typedef struct
    {
        int8_t coolingSetpoint;
    } secil_coolingSetpoint;
    typedef struct
    {
        int8_t awayCoolingSetpoint;
    } secil_awayCoolingSetpoint;
    typedef struct
    {
        int8_t hvacMode;
    } secil_hvacMode;
    typedef struct
    {
        bool relativeHumidity;
    } secil_relativeHumidity;
    typedef struct
    {
        bool accessoryState;
    } secil_accessoryState;
    typedef struct
    {
        char supportPackageData[256];
    } secil_supportPackageData;
    typedef struct
    {
        bool demandResponse;
    } secil_demandResponse;
    typedef struct
    {
        bool awayMode;
    } secil_awayMode;
    typedef struct
    {
        bool autoWake;
    } secil_autoWake;
    typedef struct
    {
        int8_t localUiState;
    } secil_localUiState;
    typedef struct
    {
        uint64_t dataTime; // Unix timestamp in seconds
    } secil_dateTime;

    typedef enum
    {
        secil_message_type_currentTemperature = 2,
        secil_message_type_heatingSetpoint = 3,
        secil_message_type_awayHeatingSetpoint = 4,
        secil_message_type_coolingSetpoint = 5,
        secil_message_type_awayCoolingSetpoint = 6,
        secil_message_type_hvacMode = 7,
        secil_message_type_relativeHumidity = 8,
        secil_message_type_accessoryState = 9,
        secil_message_type_supportPackageData = 10,
        secil_message_type_demandResponse = 11,
        secil_message_type_awayMode = 12,
        secil_message_type_autoWake = 13,
        secil_message_type_localUiState = 14,
        secil_message_type_dateTime = 15
    } secil_message_type_t;

    /// @brief A union of all possible message types.
    typedef union
    {
        secil_currentTemperature currentTemperature;
        secil_heatingSetpoint heatingSetpoint;
        secil_awayHeatingSetpoint awayHeatingSetpoint;
        secil_coolingSetpoint coolingSetpoint;
        secil_awayCoolingSetpoint awayCoolingSetpoint;
        secil_hvacMode hvacMode;
        secil_relativeHumidity relativeHumidity;
        secil_accessoryState accessoryState;
        secil_supportPackageData supportPackageData;
        secil_demandResponse demandResponse;
        secil_awayMode awayMode;
        secil_autoWake autoWake;
        secil_localUiState localUiState;
        secil_dateTime dateTime;
    } secil_message_payload;

    /// @brief The severity of a log message.
    typedef enum
    {
        secil_LOG_DEBUG,
        secil_LOG_INFO,
        secil_LOG_WARNING,
        secil_LOG_ERROR
    } secil_log_severity_t;

    /// @brief A callback function that is called when a log message is received.
    /// @param user_data The user data.
    /// @param message The log message.
    typedef void (*secil_log_fn)(void *user_data, secil_log_severity_t severity, const char *message);

    /// @brief A handle to the eme_se_comms library.
    typedef struct secil_handle_s secil_handle_t;

    /// @brief Initializes the eme_se_comms library.
    /// @param read_callback The read callback function (required).
    /// @param write_callback The write callback function (required).
    /// @param on_message The on message callback function (required).
    /// @param logger The logger callback function (optional - can be null).
    /// @param user_data Pointer to any user-defined data (optional - can be null).
    /// @return True if the comms interface library was initialized successfully, false otherwise.
    bool secil_init(secil_read_fn read_callback,
                    secil_write_fn write_callback,
                    secil_log_fn logger,
                    void *user_data);

    /// @brief Deinitializes the eme_se_comms library.
    /// @param handle The handle to the eme_se_comms library.
    void secil_deinit();

    /// @brief The main loop of the eme_se_comms library - this function should be called repeatedly in a loop.
    /// @param type The type of message that was received.
    /// @param message The message that was received.
    /// @return True if we successfully received a message, false otherwise.
    bool secil_receive(secil_message_type_t *type, secil_message_payload *message);

    /// @brief Send messages to the eme_se_comms library.
    /// @param <various> parameters depending on the message type.
    /// @return true if the message was sent successfully, false otherwise.
    bool secil_send_currentTemperature(int8_t currentTemperature);
    bool secil_send_heatingSetpoint(int8_t heatingSetpoint);
    bool secil_send_awayHeatingSetpoint(int8_t awayHeatingSetpoint);
    bool secil_send_coolingSetpoint(int8_t coolingSetpoint);
    bool secil_send_awayCoolingSetpoint(int8_t awayCoolingSetpoint);
    bool secil_send_hvacMode(int8_t hvacMode);
    bool secil_send_relativeHumidity(bool relativeHumidity);
    bool secil_send_accessoryState(bool accessoryState);
    bool secil_send_supportPackageData(const char *supportPackageData);
    bool secil_send_demandResponse(bool demandResponse);
    bool secil_send_awayMode(bool awayMode);
    bool secil_send_autoWake(bool autoWake);
    bool secil_send_localUiState(int8_t localUiState);

#if defined(__cplusplus)
}
#endif

#endif // SECIL_H