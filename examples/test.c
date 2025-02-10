#include <secil.h>
#include <stdio.h>

extern void common_init();
extern void inject_error(size_t bytes);

void log_message_received(void *user_data, secil_message_type_t type, secil_message_payload* payload)
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

int main(int argc, char **argv)
{ 
    // Initialize the library using our common example code that uses a ram based buffer
    common_init();
    
    // inject an error of 10 random bytes to the stream
    inject_error(10);

    // Send some valid messages
    secil_send_accessoryState(true);
    secil_send_autoWake(1);
    secil_send_awayMode(false);
    secil_send_currentTemperature('2');

    // inject an error of 1 random byte to the stream
    inject_error(1);

    // Send some more valid messages
    secil_send_demandResponse(true);
    secil_send_localUiState(3);
    secil_send_relativeHumidity(50);
    secil_send_supportPackageData("Hello, world!");

    int failures = 0;
    int attempts = 0;
    while (failures < 3)
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
            log_message_received(NULL, type, &payload);
        }
    }

    printf("Failed to receive a message %d times\n", failures);
    printf("Total attempts: %d\n", attempts);
}