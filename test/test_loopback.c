#include <secil.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"

typedef struct
{
    char buffer[2*1024*1024]; // 2 MB buffer for the memory stream
    size_t read_index;
    size_t write_index;
} memory_buffer_t;

/// @brief A user defined logging function passed into the library at initialisation.
/// @param user_data - The user data, which is a pointer to the memory buffer (unused in this example).
/// @param severity - The severity of the log message.
/// @param message - The log message.
static void log_fn(void *user_data, secil_log_severity_t severity, const char *message)
{
    printf("%s\n", message);
}

/// @brief This is the user defined callback function to read data from the stream.
/// @param user_data - The user data, which is a pointer to the memory buffer.
/// @param buf - The buffer to read the data into.
/// @param required_count - The number of bytes to read.
/// @return true if the read was successful, false otherwise.
static bool read_fn(void *user_data, unsigned char *buf, size_t required_count)
{
    // The user data is a pointer to the memory buffer
    memory_buffer_t *memory_buffer = (memory_buffer_t *)user_data;

    // Check if there is enough data in the memory buffer
    if (memory_buffer->read_index + required_count > memory_buffer->write_index)
    {
        return false;
    }

    // Copy the data from the memory buffer to the read buffer and update the read index
    memcpy(buf, memory_buffer->buffer + memory_buffer->read_index, required_count);
    memory_buffer->read_index += required_count;

    return true;
}

/// @brief This is the user defined callback function to write data to the stream.
/// @param user_data - The user data, which is a pointer to the memory buffer.
/// @param buf - The data to write.
/// @param count - The number of bytes to write.
/// @return true if the write was successful, false otherwise.
static bool write_fn(void *user_data, const unsigned char *buf, size_t count)
{
    // The user data is a pointer to the memory buffer
    memory_buffer_t *memory_buffer = (memory_buffer_t *)user_data;

    // Check if there is enough space in the memory buffer
    if (memory_buffer->write_index + count > sizeof(memory_buffer->buffer))
    {
        return false;
    }

    // Copy the data from the write buffer to the memory buffer and update the write index
    memcpy(memory_buffer->buffer + memory_buffer->write_index, buf, count);
    memory_buffer->write_index += count;

    return true;
}

/// @brief Inject an error into the loopback stream by appending random bytes to the buffer.
/// @param bytes Number of bytes to inject.
/// @param memory_buffer - The memory buffer to inject the error into.
/// @note This function is used to simulate a communication error by injecting random bytes into the stream.
void inject_loopback_error(size_t bytes, memory_buffer_t *memory_buffer)
{
    // Inject random bytes
    for (size_t i = 0; i < bytes; i++)
    {
        unsigned char randomChar = rand() % 256;
        write_fn(memory_buffer, &randomChar, 1);
    }
}

int main(int argc, char **argv)
{
    memory_buffer_t memory_buffer = {0}; // Initialize the memory buffer

    const int total_test_iterations = 10000; // Total number of test iterations

    // Initialize the library using our loopback example code above that uses a ram based buffer
    secil_init(
        read_fn,
        write_fn,
        log_fn,
        &memory_buffer); // Pass the memory buffer as the user data

    for (int i = 0; i < total_test_iterations; i++)
    {
        // inject an error of 10 random bytes to the stream
        inject_loopback_error(10, &memory_buffer);

        // Send some valid messages - note: we expect to read these back later.
        secil_send_currentTemperature(100);
        secil_send_heatingSetpoint(89);
        secil_send_awayHeatingSetpoint(75);
        secil_send_coolingSetpoint(22);
        secil_send_awayCoolingSetpoint(18);
        secil_send_hvacMode(2); // Example HVAC mode
        secil_send_relativeHumidity(true);

        // inject an error of 1 random byte to the stream
        inject_loopback_error(1, &memory_buffer);

        // Send some more valid messages
        secil_send_accessoryState(false);
        secil_send_supportPackageData("Support Package Data Example");
        secil_send_demandResponse(true);
        secil_send_awayMode(true);
        secil_send_autoWake(false);
        secil_send_localUiState(1);
        secil_send_dateTime(1633036800); // Example date time (Unix timestamp for 2021-10-01 00:00:00 UTC)

    }

    // Receive some messages - we should expect to receive the same as the ones we sent
    int failures = 0;
    int attempts = 0;
    int messages = 0;
    bool last_was_error = false;
    int recoveries = 0;
    int current_error_sequence = 0;
    int longest_error_sequence = 0;

    while (memory_buffer.read_index < memory_buffer.write_index)
    {
        attempts++;

        secil_message message;

        if (!secil_receive(&message))
        {
            failures++;
            if (last_was_error)
            {
                current_error_sequence++;
                if (current_error_sequence > 10)
                {
                    // If we have more than 10 consecutive errors, inject a larger error
                    printf("Long error sequence detected here\n");
                }
            }
            else
            {
                current_error_sequence = 1; // Reset the sequence count
            }
            last_was_error = true;

            if (current_error_sequence > longest_error_sequence)
            {
                longest_error_sequence = current_error_sequence; // Update the longest error sequence
            }
        }
        else
        {
            messages++;
            if (last_was_error)
            {
                recoveries++;
                last_was_error = false;
            }
            log_message_received(&message);
        }
    }

    printf("Total sent messages: %d\n", total_test_iterations * 14); // 14 messages per iteration
    printf("Total read attempts: %d\n", attempts);
    printf("Total read messages: %d\n", messages);
    printf("Total read failures: %d\n", failures);
    printf("Total injected errors: %d\n", (total_test_iterations * 2)); // 10 + 1 bytes per iteration
    printf("Total recoveries: %d\n", recoveries);
    printf("Longest sequence of errors: %d\n", longest_error_sequence);
    printf("Success rate: %d.%02d%%\n", (int)((messages / (float)attempts) * 100), (int)((messages / (float)attempts) * 10000) % 100);

    return 0;
}