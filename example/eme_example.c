#include <stdio.h>
#include "common.h"
#include "secil.h"


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

   secil_error_t startup_result = secil_startup(secil_operating_mode_t_SERVER);
   if (startup_result != SECIL_OK)
   {
      fprintf(stderr, "Failed to start up as server: %s\n", secil_error_string(startup_result));
      return 1;
   }

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
      printf(" 10. Loopback Test\n");
      printf(" 11. Exit\n");
      printf("Please select an option (1-11):\n");
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
         test_uart_loopback();
         break;
      case 11:
         printf("Exiting...\n");
         break;
      }
   } while (option != 11);

   // Deinitialize the library
   secil_deinit();

   return 0;
}