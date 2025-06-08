#include <stdio.h>

#include "common.h"
#include "secil.h"

int main()
{
   printf("This program is pretending to be the SE chip using the comms library.\n");

   // Initialize the communication library with pseudo UARTs
   if (!initialise_comms_library("/tmp/ttySE", "/tmp/ttyEME"))
   {
      fprintf(stderr, "Failed to initialize communication library.\n");
      return 1;
   }

   printf("SE Comms Library initialized successfully.\n");

   int option;
   
   do
   {
      printf("Options: \n");
      printf(" 1. Send Accessory State\n");
      printf(" 2. Send Auto Wake\n");
      printf(" 3. Send Away Mode\n");
      printf(" 4. Receive Messages\n");
      printf(" 5. Exit\n");
      printf("Please select an option (1-5):\n");

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
               secil_message_type_t type;
               secil_message_payload payload;

               if (secil_receive(&type, &payload))
               {
                  log_message_received(type, &payload);
               }
               else
               {
                  printf("Failed to receive message.\n");
                  break;
               }
            }
            break;
         case 5:
            printf("Exiting...\n");
            break;
         default:
            printf("Invalid option. Please try again.\n");
      }
   }
   while(option != 5);

   // Deinitialize the library
   secil_deinit();

   return 0;
}