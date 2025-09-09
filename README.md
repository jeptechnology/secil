# Secil

Schneider Electric Comms Interface Library

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
- All serialisation / deserialisation of messaegs is taken care of
- Error handling in synchronisation
  - Bad/corrupt messages are ignored
  - Resynchronisation with incoming stream is performed automatically
- Easily integrated to embedded C projects
  - The examples use CMake but there are only a few C files to include

NOTE: Under the hood, Secil uses the nanopb library <https://github.com/nanopb/nanopb> which enables us to delegate the serialisation to a well trusted 3rd party library. We can add more messages to the interface as new requirements arise in future versions of this library.

## Integrating into your project

In order to use secil, you should download a release from <https://github.com/jeptechnology/secil/releases>

The zip file: secil_install.zip will have the following layout:

``` text
secil/
├── CMakeLists.txt
├── include
│   ├── pb.h
│   ├── secil.h
│   └── secil.pb.h
└── source
    ├── secil.pb.c
    ├── pb_common.h
    ├── pb_common.c
    ├── pb_decode.h
    ├── pb_decode.c
    ├── pb_encode.h
    ├── pb_encode.c
    └── secil.c
```

If your codebase is using CMake then you might be able to use the given CMakeLists.txt file inside your own project.
Otherwise you simply need to ensure that your project:

- Compiles all *.c source files in /source
- Gives access to the /include folder to your code that needs to make use of the library.

### Integrate access to your platform's UART

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

### Receiving messages

Start a background task / thread to process new messages as they arrive:

```C
// An example processing loop for received messages
void my_main_processing_loop()
{
   while (!end)
   {
      // Declare a message on the stack (currently 256 bytes long)
      secil_message message; 

      // Read next message into our declared message
      secil_error_t result = secil_receive(&message);
      if (result == SECIL_OK)
      {
         // Determine the message type received and do something appropriate. 
         switch (message->which_payload)
         {
         case secil_message_currentTemperature_tag:
            printf("Current temperature is %d\n", message->payload.currentTemperature.currentTemperature);
            break;
         case secil_message_heatingSetpoint_tag:
            printf("Heating setpoint is %d\n", message->payload.heatingSetpoint.heatingSetpoint);
            break;
         case secil_message_awayHeatingSetpoint_tag:
            printf("Away heating setpoint: %d\n", message->payload.awayHeatingSetpoint.awayHeatingSetpoint);
            break;
         //... Handle other message types ...
         }
      }
      else
      {
         // Failed to receive message
      }
   }
}
```

### Sending messages

Elsewhere in your project, you can send messages whenever you need to:

```C
// NOTE: Sending messages from multiple threads is not supported.
//       However, it is safe to send from a different thread to the main processing thread.
secil_send_currentTemperature(200);
secil_send_heatingSetpoint(250);
secil_send_hvacMode(1);
```

## Developing the library

If you need to add more messages to the library, these should be mutually agreed upon by all stakeholders. The following sections explain how to develop the libray further.

If the latest version of the library has all the message types you need, there is no need to perform any of these steps, just use the released library zip as explained in previous sections.

### Installation

```bash
# Clone the repository and install locally
git clone https://github.com/jeptechnology/secil.git
cd secil
```

### Using VS Code with Docker

If you have VS Code installed and the ability to launch docker containers, then the .devcontainer folder contains definitions on how to create a build environment that is suitable for development.

### Building on the command line

The entire project can be built, tested and an install zip created by the single command:

```bash
./build_project.sh
```

### Running the examples

We have two example applications that will talk to each other locally on any Linux OS that has socat installed.
Each example will check for the existence of psuedo TTYs at `/tmp/ttySE` and `tmp/ttyEME` and if they do not exist will use
the `socat` command to create them.

NOTE: This means they can be launched in any order - but we recommend delaying for a couple of seconds before launching the second binary.

Each example will connect to one end of these TTYs and use them as if they are a UART connection between them.

You should open two terminals, go to the build folder and launch both of these applications:

```bash
./se_example
./eme_example
```

Each application will give you several options to allow you to send messages over the UART.
The penultimate option to "receive messages" will enter a receive loop that will continue until the application is exited (Ctrl-C).

## Adding new message types

When a new message is required, we should perform all of the following steps in this section.

Let us assume we need a new attribute called `alarm` and it is a 16 bit unsigned value.

### 1. Add new message to secil.proto

``` proto
message alarm {
    required uint32 alarm = 1; [(nanopb).int_size = IS_16];
}
```

### 2. Add to the possible payloads of the main message

At the bottom of the file `secil.proto` there is a payload definition which defines all possible messages we could receive. We need to add our new message to this list:

``` proto
message message 
{
    oneof payload 
    {
        currentTemperature currentTemperature = 2;
        // other messages...
        autoWake autoWake = 13;
        localUiState localUiState = 14;
        dateAndTime dateAndTime = 15;

        // Add new message here - with unique tag number...
        alarm alarm = 16;
    }
}
```

NOTE: Generated code will containe a new macro `secil_message_alarm_tag` and this should be used when comparing the `message->which_payload` in your processing loop when receiving messages.

### 3. Add a new sender function

Add a new send function called `secil_send_alarm` to `include/secil.h`

``` cpp
bool secil_send_alarm(uint16_t alarm);
```

Implement this function in `source/secil.c` as follows:

```cpp
bool secil_send_alarm(uint16_t alarm) 
{ 
   SECIL_SEND(alarm, alarm); 
}
```

### 4. Update tests

It is highly recommended to update the file `/test/test_loopback.c` to make use of this new message type and check it is working as expecting.

Optionally, you could add some code using this new message to the two examples:

- `/example/eme_example.c`
- `/example/se_example.c`

### 5. Share the release by tagging the repo

Once this repo is tagged, there is a github action that will create a new release.

The link to this can then be shared with colleagues.

## TODO

- Capture failures
- Do we need acknowledgements for messages?
  - Would require correlation ID
  - Timeouts?
  - Retries?
  - Do we just "reset" the comms every time there is an error?
- Do we need to know when both sides are ready?
  - Initial handshake messages?
