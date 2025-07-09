/// @file secil.h
/// @brief The Schneider Electric Comms Interface Library (SECIL)
///        A library for communicating between "Cloud" Processor and "Thermostat" Processor.

#if !defined(SECIL_H)
#define SECIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <secil.pb.h>

#if defined(__cplusplus)
extern "C"
{
#endif

    /// @brief Signative for a callback function that reads required_count from a stream and writes to the given buffer.
    /// @param user_data The user data.
    /// @param buf The buffer to write to.
    /// @param required_count The number of bytes to read.
    /// @return True if we read required_count bytes successfully, false otherwise.
    /// @note The read function should **block** until the required number of bytes is available, or return false if it cannot read the required number of bytes.
    typedef bool (*secil_read_fn)(void *user_data, unsigned char *buf, size_t required_count);

    /// @brief Signature for a callback function that writes count bytes from the given buffer to a stream.
    /// @param user_data The user data.
    /// @param buf The buffer to read from.
    /// @param count The number of bytes to write.
    /// @return True if we wrote count bytes successfully, false otherwise.
    typedef bool (*secil_write_fn)(void *user_data, const unsigned char *buf, size_t count);

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
    /// @param message A pointer to a valid instance of message that will be filled with the received message.
    /// @return True if we successfully received a message, false otherwise.
    /// @warning This function will **block** until a message is received.
    /// @note If there was a problem receiving a message, the function will attempt to log the error internally using the logger callback function.
    bool secil_receive(secil_message *message);

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
    bool secil_send_dateTime(uint64_t dataTime);

#if defined(__cplusplus)
}
#endif

#endif // SECIL_H