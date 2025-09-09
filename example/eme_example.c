#include <stdio.h>
#include <pthread.h>
#include "common.h"
#include "secil.h"


void receive_thread()
{
   while (1)
   {
      secil_message payload;

      if (secil_receive(&payload))
      {
         log_message_received(&payload);
      }
      else
      {
         printf("Failed to receive message.\n");
         break;
      }
   }
}

void launch_receive_thread()
{
   printf("Launching receive thread...\n");

   pthread_t thread_id;
   if (pthread_create(&thread_id, NULL, (void*(*)(void*))receive_thread, NULL) != 0)
   {
      perror("Failed to create receive thread");
   }
   else
   {
      pthread_detach(thread_id); // Detach the thread to allow it to run independently
      printf("Receive thread launched successfully.\n");
   }
}  

int main()
{
   printf("This program is pretending to be the EME chip using the comms library.\n");

   // Initialize the communication library with pseudo UARTs
   // if (!initialise_comms_library("/dev/ttyUSB0"))
   if (!initialise_comms_library_with_psuedo_uarts("/tmp/ttyEME", "/tmp/ttySE"))
   {
      fprintf(stderr, "Failed to initialize communication library.\n");
      return 1;
   }

   printf("SE Comms Library initialized successfully.\n");
   
   printf("Starting up as server...\n");
   char client_version[32];
   memset(client_version, 0, sizeof(client_version));
   // Start up as server
   secil_error_t startup_result = secil_startup_as_server("EME_Example_v1.0", client_version, sizeof(client_version));
   if (startup_result != SECIL_OK)
   {
      fprintf(stderr, "Failed to start up as server: %s\n", secil_error_string(startup_result));
      return 1;
   }

   printf("Started up as server successfully. Client version: %s\n", client_version);

   launch_receive_thread();

   int option;

   do
   {
      // We want to test all the following functions:
      // bool secil_send_currentTemperature(int8_t currentTemperature);
      // bool secil_send_heatingSetpoint(int8_t heatingSetpoint);
      // bool secil_send_awayHeatingSetpoint(int8_t awayHeatingSetpoint);
      // bool secil_send_coolingSetpoint(int8_t coolingSetpoint);
      // bool secil_send_awayCoolingSetpoint(int8_t awayCoolingSetpoint);
      // bool secil_send_hvacMode(int8_t hvacMode);
      // bool secil_send_relativeHumidity(bool relativeHumidity);
      // bool secil_send_accessoryState(bool accessoryState);
      // bool secil_send_supportPackageData(const char *supportPackageData);

      printf("Options: \n");
      printf(" 1. Send Current Temperature\n");
      printf(" 2. Send Heating Setpoint\n");
      printf(" 3. Send Away Heating Setpoint\n");
      printf(" 4. Send Cooling Setpoint\n");
      printf(" 5. Send Away Cooling Setpoint\n");
      printf(" 6. Send HVAC Mode\n");
      printf(" 7. Send Relative Humidity\n");
      printf(" 8. Send Accessory State\n");
      printf(" 9. Send Support Package Data\n");
      printf(" 10. Receive Messages\n");
      printf(" 11. Loopback Test\n");
      printf(" 12. Exit\n");
      printf("Please select an option (1-12):\n");
      scanf("%d", &option); // Note the space before %c to consume any newline character

      switch (option)
      {
      case 1:
      {
         int8_t currentTemperature;
         printf("Enter Current Temperature: ");
         scanf("%hhd", &currentTemperature);
         secil_send_currentTemperature(currentTemperature);
         printf("Current Temperature sent: %d\n", currentTemperature);
         break;
      }
      case 2:
      {
         int8_t heatingSetpoint;
         printf("Enter Heating Setpoint: ");
         scanf("%hhd", &heatingSetpoint);
         secil_send_heatingSetpoint(heatingSetpoint);
         printf("Heating Setpoint sent: %d\n", heatingSetpoint);
         break;
      }
      case 3:
      {
         int8_t awayHeatingSetpoint;
         printf("Enter Away Heating Setpoint: ");
         scanf("%hhd", &awayHeatingSetpoint);
         secil_send_awayHeatingSetpoint(awayHeatingSetpoint);
         printf("Away Heating Setpoint sent: %d\n", awayHeatingSetpoint);
         break;
      }
      case 4:
      {
         int8_t coolingSetpoint;
         printf("Enter Cooling Setpoint: ");
         scanf("%hhd", &coolingSetpoint);
         secil_send_coolingSetpoint(coolingSetpoint);
         printf("Cooling Setpoint sent: %d\n", coolingSetpoint);
         break;
      }
      case 5:
      {
         int8_t awayCoolingSetpoint;
         printf("Enter Away Cooling Setpoint: ");
         scanf("%hhd", &awayCoolingSetpoint);
         secil_send_awayCoolingSetpoint(awayCoolingSetpoint);
         printf("Away Cooling Setpoint sent: %d\n", awayCoolingSetpoint);
         break;
      }
      case 6:
      {
         int8_t hvacMode;
         printf("Enter HVAC Mode (0-3): ");
         scanf("%hhd", &hvacMode);
         secil_send_hvacMode(hvacMode);
         printf("HVAC Mode sent: %d\n", hvacMode);
         break;
      }
      case 7:
      {
         int relativeHumidity;
         printf("Enter Relative Humidity (0 for false, 1 for true): ");
         scanf("%d", &relativeHumidity);
         secil_send_relativeHumidity(relativeHumidity);
         printf("Relative Humidity sent: %d\n", relativeHumidity);
         break;
      }
      case 8:
      {
         int accessoryState;
         printf("Enter Accessory State (0 for false, 1 for true): ");
         scanf("%d", &accessoryState);
         secil_send_accessoryState(accessoryState);
         printf("Accessory State sent: %d\n", accessoryState);
         break;
      }
      case 9:
      {
         char supportPackageData[256];
         printf("Enter Support Package Data (max 255 characters): ");
         scanf(" %[^\n]", supportPackageData); // Read string with spaces
         secil_send_supportPackageData(supportPackageData);
         printf("Support Package Data sent: %s\n", supportPackageData);
         break;
      }
      case 10:
      {
         while (1)
         {
            secil_message payload;

            if (secil_receive(&payload))
            {
               log_message_received(&payload);
            }
            // else
            // {
            //    printf("Failed to receive message.\n");
            //    break;
            // }
         }
         break;
      }
      case 11:
         test_uart_loopback();
         break;
      case 12:
         printf("Exiting...\n");
         break;
      }
   } while (option != 12);

   // Deinitialize the library
   secil_deinit();

   return 0;
}