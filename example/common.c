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
#include <termios.h>

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
    printf("%s\n", message);
    fflush(stdout);
}

bool read_uart(void *user_data, unsigned char *buf, size_t required_count)
{
    // keep reading from global uart until we have the required number of bytes
    // NOTE: The uart is non-blocking, so we need to wait for data to be available with the select() function.
    ssize_t bytes_read = 0;
    size_t total_bytes_read = 0;
    fd_set g_fds;
    struct timeval timeout; 
    timeout.tv_sec = 300; // 5 minutes timeout
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
void log_message_received(secil_message *message)
{
    // What type of message did we receive?
    switch (message->which_payload)
    {
    case secil_message_currentTemperature_tag:
        printf("Received current temperature: %d\n", message->payload.currentTemperature.currentTemperature);
        break;
    case secil_message_heatingSetpoint_tag:
        printf("Received heating setpoint: %d\n", message->payload.heatingSetpoint.heatingSetpoint);
        break;
    case secil_message_awayHeatingSetpoint_tag:
        printf("Received away heating setpoint: %d\n", message->payload.awayHeatingSetpoint.awayHeatingSetpoint);
        break;
    case secil_message_coolingSetpoint_tag:
        printf("Received cooling setpoint: %d\n", message->payload.coolingSetpoint.coolingSetpoint);
        break;
    case secil_message_awayCoolingSetpoint_tag:
        printf("Received away cooling setpoint: %d\n", message->payload.awayCoolingSetpoint.awayCoolingSetpoint);
        break;
    case secil_message_hvacMode_tag:
        printf("Received HVAC mode: %d\n", message->payload.hvacMode.hvacMode);
        break;
    case secil_message_relativeHumidity_tag:
        printf("Received relative humidity: %s\n", message->payload.relativeHumidity.relativeHumidity ? "true" : "false");
        break;
    case secil_message_accessoryState_tag:
        printf("Received accessory state: %s\n", message->payload.accessoryState.accessoryState ? "true" : "false");
        break;
    case secil_message_supportPackageData_tag:
        printf("Received support package data: %s\n", message->payload.supportPackageData.supportPackageData);
        break;
    case secil_message_demandResponse_tag:
        printf("Received demand response: %s\n", message->payload.demandResponse.demandResponse ? "true" : "false");
        break;
    case secil_message_awayMode_tag:
        printf("Received away mode: %s\n", message->payload.awayMode.awayMode ? "true" : "false");
        break;
    case secil_message_autoWake_tag:
        printf("Received auto wake: %s\n", message->payload.autoWake.autoWake ? "true" : "false");
        break;
    case secil_message_localUiState_tag:
        printf("Received local UI state: %d\n", message->payload.localUiState.localUiState);
        break;
    case secil_message_dateAndTime_tag:
        printf("Received date time: %llu\n", (unsigned long long)message->payload.dateAndTime.dateAndTime);
        break;
    
    // Add cases for any other message types you expect to receive
    // ....
    // ....

    default:
        printf("Received unknown message type: %d\n", message->which_payload);
        break;
    }
}

bool initialise_comms_library_with_psuedo_uarts(const char *uart_local, const char *uart_remote)
{
    // If we are given two UARTs, create pseudo UARTs using socat
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

bool initialise_comms_library(const char *uart_device)
{
    // Initialize a connection to the local UART
    initialise_uart(uart_device);

    // Ensure we set the UART to blocking mode, 115200 baud, 8 data bits, no parity, 1 stop bit
    int result = fcntl(g_secil_context.uart_fd, F_SETFL, 0);
    if (result == -1)
    {
        perror("Failed to set UART to blocking mode");
        return false;
    }

    struct termios options;
    result = tcgetattr(g_secil_context.uart_fd, &options);
    if (result == -1)
    {
        perror("Failed to get UART attributes");
        return false;
    }

    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= (CLOCAL | CREAD); // Enable receiver, ignore modem control lines
    options.c_cflag &= ~PARENB;          // No parity
    options.c_cflag &= ~CSTOPB;          // 1 stop bit
    options.c_cflag &= ~CSIZE;           // Clear data bits setting
    options.c_cflag |= CS8;              // 8 data bits
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    tcsetattr(g_secil_context.uart_fd, TCSANOW, &options);

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

void test_uart_loopback()
{
    // Just read some chars from the UART until we get a newline
    char buffer[256];
    printf("Loopback test - Start typing characters to read from the UART. Automatically stops once we receive a newline.\n");

    // read into buffer until newline or buffer full
    size_t index = 0;
    char c = 0;
    while (c != '\n' && index < sizeof(buffer) - 1)
    {
        if (getchar() == EOF)
        {
            printf("Error reading from stdin\n");
            return;
        }
        c = getchar();
        buffer[index++] = c;
    }
    buffer[index] = '\0';    

    // Now do the loopback test
    secil_error_t result = secil_loopback_test(buffer);
    if (result == SECIL_OK)
    {
        printf("Loopback test successful. Sent and received: %s\n", buffer);
    }
    else
    {
        printf("Loopback test failed with error code: %d\n", result);
    }
}
