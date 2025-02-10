#include <secil.h>
#include "common.h"

#include <stdio.h>

int main(int argc, char **argv)
{ 
    // Initialize the library using our loopback example code that uses a ram based buffer
    init_secil_loopback();
    
    // inject an error of 10 random bytes to the stream
    inject_loopback_error(10);

    // Send some valid messages
    secil_send_accessoryState(true);
    secil_send_autoWake(1);
    secil_send_awayMode(false);
    secil_send_currentTemperature('2');

    // inject an error of 1 random byte to the stream
    inject_loopback_error(1);

    // Send some more valid messages
    secil_send_demandResponse(true);
    secil_send_localUiState(3);
    secil_send_relativeHumidity(50);
    secil_send_supportPackageData("Hello, world!");

    int failures = 0;
    int attempts = 0;
    int messages = 0;
    while (failures < 3 && messages < 8)
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

    printf("Total sent messages: 8\n");
    printf("Total read attempts: %d\n", attempts);
    printf("Total read messages: %d\n", messages);
    printf("Total read failures: %d\n", failures);
    printf("Success rate: %d.%02d%%\n", (int)((messages / 8.0) * 100), (int)((messages / 8.0) * 10000) % 100);

    return 0;
}