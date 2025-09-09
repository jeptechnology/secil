#include <stdio.h>

#include "common.h"
#include "secil.h"

int main()
{
   printf("This program is pretending to be the SE chip using the comms library.\n");

   // Initialize the communication library with pseudo UARTs
   // if (!initialise_comms_library("/dev/ttyUSB0"))
   if (!initialise_comms_library_with_psuedo_uarts("/tmp/ttySE", "/tmp/ttyEME"))
   {
      fprintf(stderr, "Failed to initialize communication library.\n");
      return 1;
   }

   printf("SE Comms Library initialized successfully.\n");

   printf("Starting up as client...\n");
   char server_version[32];
   memset(server_version, 0, sizeof(server_version));
   // Start up as client
   secil_error_t startup_result = secil_startup_as_client("SE_Example_v1.0", server_version, sizeof(server_version));
   if (startup_result != SECIL_OK)
   {
       fprintf(stderr, "Failed to start up as client: %s\n", secil_error_string(startup_result));
       return 1;
   }

   printf("Started up as client successfully. Talking to server version: %s\n", server_version);

   int option;
   
   do
   {
      printf("Options: \n");
      printf(" 1. Send Accessory State\n");
      printf(" 2. Send Auto Wake\n");
      printf(" 3. Send Away Mode\n");
      printf(" 4. Receive Messages\n");
      printf(" 5. UART loopback test\n");
      printf(" 6. Exit\n");
      printf("Please select an option (1-6):\n");

      scanf("%d", &option);
      switch (option)
      {
         case 1:
            {
               int accessoryState;
               printf("Enter Accessory State (0 for false, or 1 for true): ");
               scanf("%d", &accessoryState);
               secil_send_accessoryState(accessoryState > 0);
               printf("Accessory State sent: %d\n", accessoryState);
            }
            break;
         case 2:
            {
               int autoWake;
               printf("Enter Auto Wake (0 or 1): ");
               scanf("%d", &autoWake);
               secil_send_autoWake(autoWake);
               printf("Auto Wake sent: %d\n", autoWake);
            }
            break;
         case 3:
            {
               int awayMode;
               printf("Enter Away Mode (0 or 1): ");
               scanf("%d", &awayMode);
               secil_send_awayMode(awayMode);
               printf("Away Mode sent: %d\n", awayMode);
            }
            break;
         case 4:
           while(1) 
           {
               secil_message payload;

               if (secil_receive(&payload))
               {
                  log_message_received(&payload);
               }
            }
            break;
         case 5:
            test_uart_loopback();
            break;
         case 6:
            printf("Exiting...\n");
            break;
         default:
            printf("Invalid option. Please try again.\n");
      }
   }
   while(option != 6);

   // Deinitialize the library
   secil_deinit();

   return 0;
}