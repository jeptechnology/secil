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

    typedef enum{
        SECIL_OK = 0,
        SECIL_ERROR_INIT_FAILED,
        SECIL_ERROR_ALREADY_INITIALIZED,
        SECIL_ERROR_NOT_INITIALIZED,
        SECIL_ERROR_INVALID_PARAMETER,
        SECIL_ERROR_READ_FAILED,
        SECIL_ERROR_WRITE_FAILED,
        SECIL_ERROR_ENCODE_FAILED,
        SECIL_ERROR_DECODE_FAILED,
        SECIL_ERROR_MESSAGE_TOO_LARGE,
        SECIL_ERROR_UNKNOWN_MESSAGE_TYPE,
        SECIL_ERROR_SEND_FAILED,
        SECIL_ERROR_RECEIVE_FAILED,
        SECIL_ERROR_STARTUP_FAILED,
        SECIL_ERROR_VERSION_MISMATCH

    } secil_error_t;

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
    /// @return SECIL_OK if the library was initialized successfully, otherwise an error code.
    /// @note This function must be called before any other functions in the library.
    secil_error_t secil_init(secil_read_fn read_callback,
                             secil_write_fn write_callback,
                             secil_log_fn logger,
                             void *user_data);

    /// @brief Deinitializes the eme_se_comms library.
    /// @param handle The handle to the eme_se_comms library.
    void secil_deinit();

    /// @brief Get a string representation of the given error code.
    /// @param error_code 
    /// @return 
    const char *secil_error_string(secil_error_t error_code);

    /// @brief Sends data to the remote end and expects to receive the same data back.
    /// @param test_data The data to send - must be a null-terminated printable string.
    /// @return SECIL_OK if the test was successful, otherwise an error code.
    secil_error_t secil_loopback_test(const char *test_data);

    /// @brief Start up the SECIL library as either a client or server.
    /// @param myversion The version string of this side (max 31 characters + null terminator).
    /// @param serverversion A buffer to receive the version string of the other side (max 31 characters + null terminator).
    /// @param serverversion_size The size of the serverversion buffer.
    /// @return SECIL_OK if the startup was successful, otherwise an error code.
    secil_error_t secil_startup_as_client(const char *myversion, char *serverversion, size_t serverversion_size);

    /// @brief Start up the SECIL library as either a client or server.
    /// @param myversion The version string of this side (max 31 characters + null terminator).
    /// @param clientversion A buffer to receive the version string of the other side (max 31 characters + null terminator).
    /// @param clientversion_size The size of the clientversion buffer.
    /// @return SECIL_OK if the startup was successful, otherwise an error code.
    secil_error_t secil_startup_as_server(const char *myversion, char *clientversion, size_t clientversion_size);

    /// @brief The main loop of the eme_se_comms library - this function should be called repeatedly in a loop.
    /// @param message A pointer to a valid instance of message that will be filled with the received message.
    /// @return SECIL_OK if a message was received successfully, otherwise an error code.
    /// @warning This function will **block** until a message is received.
    /// @note If there was a problem receiving a message, the function will attempt to log the error internally using the logger callback function.
    secil_error_t secil_receive(secil_message *message);

    /// @brief Send messages to the eme_se_comms library.
    /// @param <various> parameters depending on the message type.
    /// @return SECIL_OK if the message was sent successfully, otherwise an error code.
    secil_error_t secil_send_currentTemperature(int8_t currentTemperature);
    secil_error_t secil_send_heatingSetpoint(int8_t heatingSetpoint);
    secil_error_t secil_send_awayHeatingSetpoint(int8_t awayHeatingSetpoint);
    secil_error_t secil_send_coolingSetpoint(int8_t coolingSetpoint);
    secil_error_t secil_send_awayCoolingSetpoint(int8_t awayCoolingSetpoint);
    secil_error_t secil_send_hvacMode(int8_t hvacMode);
    secil_error_t secil_send_relativeHumidity(bool relativeHumidity);
    secil_error_t secil_send_accessoryState(bool accessoryState);
    secil_error_t secil_send_supportPackageData(const char *supportPackageData);
    secil_error_t secil_send_demandResponse(bool demandResponse);
    secil_error_t secil_send_awayMode(bool awayMode);
    secil_error_t secil_send_autoWake(bool autoWake);
    secil_error_t secil_send_localUiState(int8_t localUiState);
    secil_error_t secil_send_dateTime(uint64_t dataTime);

#if defined(__cplusplus)
}
#endif

#endif // SECIL_H