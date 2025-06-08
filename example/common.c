#include "secil.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <string.h>
#include <errno.h>  

static struct
{
    int uart_fd; // File descriptor for UART
} g_secil_context;

// This function will fork and launch the socat command below:
// socat pty,link=dev_uart1,raw,echo=0 pty,link=dev_uart2,raw,echo=0
static bool create_psuedo_uarts_via_socat(const char *dev_uart1, const char *dev_uart2)
{
    // If paths exists, then we assume that the pseudo UARTs are already created.
    if (access(dev_uart1, F_OK) == 0 && access(dev_uart2, F_OK) == 0)
    {
        printf("Pseudo UARTs already exist: %s and %s\n", dev_uart1, dev_uart2);
        return true;
    }

    printf("Creating pseudo uarts via socat: %s <-> %s\n", dev_uart1, dev_uart2);

    pid_t child_pid;

    // Fork a child process
    if ((child_pid = fork()) < 0)
    {
        // Fork failed
        perror("fork");
        return false;
    }

    if (child_pid == 0)
    {
        char arg1[256], arg2[256];
        snprintf(arg1, sizeof(arg1), "pty,link=%s,raw,echo=0", dev_uart1);
        snprintf(arg2, sizeof(arg2), "pty,link=%s,raw,echo=0", dev_uart2);
        execlp("socat", "socat", arg1, arg2, NULL);

        // If execlp returns, there was an error
        perror("execlp");
        exit(1);
    }

    // Parent process continues here
    printf("Started socat process with PID: %d\n", child_pid);

    int iterations = 0;
    // wait for up to 5 seconds the files to be created
    while (access(dev_uart1, F_OK) != 0 && access(dev_uart2, F_OK) != 0)
    {
        sleep(1);
        iterations++;
        if (iterations >= 5)
        {
            fprintf(stderr, "Timeout waiting for pseudo UARTs to be created.\n");
            return false;
        }
    }
    if (access(dev_uart1, F_OK) != 0 || access(dev_uart2, F_OK) != 0)
    {
        fprintf(stderr, "Failed to create pseudo UARTs: %s and %s\n", dev_uart1, dev_uart2);
        return false;
    }

    return true;
}

static void initialise_uart(const char *uart_device)
{
    // Open UART 1 for reading and writing
    g_secil_context.uart_fd = open(uart_device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (g_secil_context.uart_fd == -1)
    {
        perror("Failed to open UART 1");
        exit(EXIT_FAILURE);
    }
}

static void log_fn(void *user_data, secil_log_severity_t severity, const char *message)
{
    switch (severity)
    {
    default:
    case secil_LOG_DEBUG:   printf("[DEBUG]: "); break;
    case secil_LOG_INFO:    printf("[INFO ]: "); break;
    case secil_LOG_WARNING: printf("[WARN ]: "); break;
    case secil_LOG_ERROR:   printf("[ERROR]: "); break;
    }
    printf("%s", message);
}

static bool read_uart(void *user_data, unsigned char *buf, size_t required_count)
{
    // keep reading from global uart until we have the required number of bytes
    // NOTE: The uart is non-blocking, so we need to wait for data to be available with the select() function.
    ssize_t bytes_read = 0;
    size_t total_bytes_read = 0;
    fd_set g_fds;
    struct timeval timeout; 
    timeout.tv_sec = 120; // 2 minutes timeout
    timeout.tv_usec = 0;
    while (total_bytes_read < required_count)
    {
        FD_ZERO(&g_fds);
        FD_SET(g_secil_context.uart_fd, &g_fds);

        // Wait for data to be available on the UART
        int select_result = select(g_secil_context.uart_fd + 1, &g_fds, NULL, NULL, &timeout);
        if (select_result < 0)
        {
            perror("select failed");
            return false;
        }
        else if (select_result == 0)
        {
            printf("Timeout waiting for data on UART\n");
            return false; // Timeout
        }

        // Read from the UART
        bytes_read = read(g_secil_context.uart_fd, buf + total_bytes_read, required_count - total_bytes_read);
        if (bytes_read < 0)
        {
            perror("Failed to read from UART");
            return false;
        }
        
        total_bytes_read += bytes_read;
    }
    if (total_bytes_read != required_count)
    {
        fprintf(stderr, "Expected %zu bytes, but read %zu bytes\n", required_count, total_bytes_read);
        return false;
    }
    return true; // Successfully read the required number of bytes
}

static bool write_uart(void *user_data, const unsigned char *buf, size_t count)
{
    ssize_t bytes_written = write(g_secil_context.uart_fd, buf, count);
    if (bytes_written < 0)
    {
        perror("Failed to write to UART");
        return false;
    }
    return bytes_written == count;
}

/// @brief Log the message received.
/// @param type - The type of the message.
/// @param payload - The payload of the message.
void log_message_received(secil_message_type_t type, secil_message_payload *payload)
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

bool initialise_comms_library(const char *uart_local, const char *uart_remote)
{
    // Create pseudo UARTs using socat
    if (!create_psuedo_uarts_via_socat(uart_local, uart_remote))
    {
        return false;
    }

    // Initialize a connection to the local UART
    initialise_uart(uart_local);

    // Initialize the secil library with our read and write functions
    if (!secil_init(
            read_uart,
            write_uart,
            log_fn,
            NULL))
    {
        perror("Failed to initialize secil library");
        return false;
    }

    return true;
}
