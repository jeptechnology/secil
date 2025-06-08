# Secil
Schneider Electric to EME Comms Interface Library

## Overview
Secil is a library designed to wrap the complexity of the local UART comms between the two microcontrollers.
It is custom made for the specific data interface in our thermostat application.

## Features
- Pure C99 implementation of the comms interface
- No dynamic memory
- Tiny RAM and FLASH footprints
- Platform agnostic
   - Supply your own UART functions
   - No threads
- Consistent C API for all messages
- All serialisation / deserialisation of messaegs is taken care of.
- Error handling in synchronisation
- Easy integration to embedded systems
  - Examples use CMake but there are only a few C files to include.

NOTE: Under the hood, Secil uses the nanopb library (https://github.com/nanopb/nanopb) which enables us to delegate the serialisation to a well trusted 3rd party library. We can expand on this any time new requirements arise and new functions added to the libary.

## Installation

```bash
# Clone the repository and install locally
git clone https://github.com/jeptechnology/secil.git
cd secil
```

## Using CMake on Linux based machines for testing

```bash
mkdir build && cd build
cmake ..
make
```

## Running the loopback tests - inside the build folder

```bash
./loopback_test
```

## Running the examples 

We have two example applications that will talk to each other locally on the Linux system.
They will attempt to create two psuedo TTYs via the `socat` command (this is done by whichever first launches)
Each one will connect to either end of these TTYs and use them as if they are a UART.

You should open two terminals, go to the build folder and launch both of these applications:

```bash
./se_example
./eme_example
```

Each application will give you several options.
Once you begin the "receive messages" loop, the application will continue to receive messages until 

## Integrate the comms linbrary in your project

To use this libray in your own project, enure you have linked the library and have access to its header files.

Then proceed as follows:

```C
#include "secil.h"

// Setup your UARTs (platform specific)

// Implement some functions to read and write from your uarts (we assume you have created these)
bool my_uart_read_fn(void *user_data, unsigned char *buf, size_t required_count);
bool my_uart_write_fn(void *user_data, const unsigned char *buf, size_t count);

// Optionally implement a log function
void my_log_fn(void *user_data, secil_log_severity_t severity, const char *message);

// Optionally, point to your own user_data which will be passed to all the callback functions above
// e.g. you may wish to point to a structure holding information about your UART config
void *user_data = NULL; 

// Initialise the library, passing in an optional log function:
bool secil_init_ok = secil_init(my_uart_read_fn, 
                                my_uart_read_fn, 
                                my_log_fn, 
                                user_data);
```

Start a background task / thread to process new messages as they arrive:

```C
// An example main loop for processing received messages
void my_main_processing_loop()
{
   while (!end)
   {
      secil_message_type_t type; // This will be written with the type of message received.
      secil_message_payload payload; // This will be written with the payload - which is a union of well typed message data.
                                    // NOTE: up to 256 bytes in size, keep an eye on stack usage if tight.

      // Receive the next message
      if (secil_receive(&type, &payload))
      {
         // Do something with this message
      }
      else
      {
         printf("Failed to receive message.\n");
      }
   }
}
```

Elsewhere in your project, you can send messages whenever you need to:

```C
// NOTE: Sending messages from multiple threads is not supported.
//       However, it is safe to send from a different thread to the main processing thread.
secil_send_currentTemperature(200);
secil_send_heatingSetpoint(250);
secil_send_hvacMode(1);
```
