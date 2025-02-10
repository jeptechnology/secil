// This test file opens up a linux uart device and sends a message to the device.

#include <secil.h>
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

static struct 
{
   int rx_fd;
   int tx_fd;
} state;

static void log_fn(void *user_data, secil_log_severity_t severity, const char *message)
{
   printf("%s\n", message);
}

static bool read_fn(void *user_data, unsigned char *buf, size_t required_count)
{
   // Read from the fifo
   return read(state.rx_fd, buf, required_count) == required_count;
}

static bool write_fn(void *user_data, const unsigned char *buf, size_t count)
{
   // Write to the fifo
   return write(state.tx_fd, buf, count) == count;
}

void inject_error(size_t bytes)
{
   // Inject random bytes
   for (size_t i = 0; i < bytes; i++)
   {
      unsigned char randomChar = rand() % 256;
      write_fn(NULL, &randomChar, 1);
   }
}

int main(int argc, char **argv)
{
   signal(SIGPIPE, SIG_IGN);

   const char *FIFO_SENDER_PATH   = "/tmp/secil_fifo_rx";
   const char *FIFO_RECEIVER_PATH = "/tmp/secil_fifo_tx";

   // if either fifo does not exist, create it
   if (access(FIFO_SENDER_PATH, F_OK) == -1)
   {
      mkfifo(FIFO_SENDER_PATH, 0777);
   }
   
   if (access(FIFO_RECEIVER_PATH, F_OK) == -1)
   {
      mkfifo(FIFO_RECEIVER_PATH, 0777);
   }

   // if the program name contains "linux_eme" we are the EME side
   if (argc < 1)
   {
      printf("Error - Unable to determine side\n");
      return 1;
   }

   bool isEME = strstr(argv[0], "linux_eme") != NULL;
   const char *rx_path = isEME ? FIFO_SENDER_PATH : FIFO_RECEIVER_PATH;
   const char *tx_path = isEME ? FIFO_RECEIVER_PATH : FIFO_SENDER_PATH;

   state.tx_fd = open(tx_path, O_RDONLY | O_NONBLOCK);
   if (state.tx_fd == -1)
   {
      printf("Error - Unable to open transmit FIFO %s: %s\n", tx_path, strerror(errno));
      return 1;
   }
   state.rx_fd = open(rx_path, O_WRONLY | O_NONBLOCK);
   if (state.rx_fd == -1)
   {
      printf("Error - Unable to open receive FIFO %s: %s\n", rx_path, strerror(errno));
      return 1;
   }

   // Initialize the library using our common example code that uses a ram based buffer
   if (!secil_init(read_fn, write_fn, log_fn, &state))
   {
      printf("Error - Unable to initialize the library\n");
      goto exit;
   }

   printf("Library initialized\n");
   printf("Options: \n");
   printf("  0 - Listen (blocking)\n");
   printf("  1 - Send currentTemperature\n");
   printf("  2 - Send heatingSetpoint\n");
   printf("  3 - Send awayHeatingSetpoint\n");
   printf("  4 - Send coolingSetpoint\n");
   printf("  5 - Send awayCoolingSetpoint\n");
   printf("  6 - Send hvacMode\n");
   printf("  7 - Send relativeHumidity\n");
   printf("  8 - Send accessoryState\n");
   printf("  9 - Send supportPackageData\n");
   printf("  a - Send demandResponse\n");
   printf("  b - Send awayMode\n");
   printf("  c - Send autoWake\n");
   printf("  e - Send localUiState\n");
   printf("  f - Inject error\n");
   printf("  q - Quit\n");

   char option;
   while (1)
   {
      printf("Enter option: ");
      scanf(" %c", &option);

      switch (option)
      {
      case '0':
      {
         secil_message_type_t type;
         secil_message_payload payload;

         while(secil_receive(&type, &payload))
         {
            log_message_received(type, &payload);
         }
         break;
      }
      case '1':
         secil_send_currentTemperature('2');
         break;
      case '2':
         secil_send_heatingSetpoint('3');
         break;
      case '3':
         secil_send_awayHeatingSetpoint('4');
         break;
      case '4':
         secil_send_coolingSetpoint('5');
         break;
      case '5':
         secil_send_awayCoolingSetpoint('6');
         break;
      case '6':
         secil_send_hvacMode('7');
         break;
      case '7':
         secil_send_relativeHumidity(50);
         break;
      case '8':
         secil_send_accessoryState(true);
         break;
      case '9':
         secil_send_supportPackageData("Hello, world!");
         break;
      case 'a':
         secil_send_demandResponse(true);
         break;
      case 'b':
         secil_send_awayMode(false);
         break;
      case 'c':
         secil_send_autoWake(1);
         break;
      case 'e':
         secil_send_localUiState(3);
         break;
      case 'f':
         inject_error(1);
         break;
      case 'q':
         goto exit;
      default:
         printf("Unknown option\n");
         break;
      }
   }

exit:
   close(state.rx_fd);
   close(state.tx_fd);

   return 0;
}
