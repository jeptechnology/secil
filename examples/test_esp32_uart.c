#include "secil.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "soc/gpio_periph.h"

void initialise_uart_1()
{
    // Configure UART1
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 13, 14, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0));
}

static void log_fn(void *user_data, secil_log_severity_t severity, const char *message)
{
    ESP_LOGI("SECIL", "%s", message);
}

static bool read_esp32_uart(void *user_data, unsigned char *buf, size_t required_count)
{
    // We simply read from the UART on the ESP32
    return uart_read_bytes(UART_NUM_1, buf, required_count, portMAX_DELAY) != required_count;
}

static bool write_esp32_uart(void *user_data, const unsigned char *buf, size_t count)
{
    // We simply write to the UART on the ESP32
    return uart_write_bytes(UART_NUM_1, buf, count) == count;
}

/// @brief A small test function for the secil library on the ESP32.
/// @return 
void test_secil_esp32()
{
    // Initialize UART 1 ready for use with secil.
    initialise_uart_1();

    // Write some clear text to begin with - just to show you that it is working
    write_esp32_uart(NULL, (const unsigned char*)"Hello, world!\n", 14);

    // Initialize the library using our uart read and write code above that uses a ram based buffer
    secil_init(
        read_esp32_uart, 
        write_esp32_uart,
        log_fn,
        NULL);

    // Send some example messages
    secil_send_accessoryState(true);
    secil_send_autoWake(1);
    secil_send_awayMode(false);

    // Try to receive up to 3 messages
    int failures = 0;
    int attempts = 0;
    int messages = 0;

    while (failures < 3 && messages < 3)
    {
        attempts++;

        secil_message_type_t type;
        secil_message_payload payload;

        if (!secil_receive(&type, &payload))
        {
            failures++;
        }
        else
        {
            messages++;
            log_message_received(type, &payload);
        }
    }

    // output statistics
    ESP_LOGI("SECIL", "Attempts: %d, Failures: %d, Messages: %d", attempts, failures, messages);

    // Deinitialize the library
    secil_deinit();
}
