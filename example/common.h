#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <secil.h>

extern bool initialise_comms_library_with_psuedo_uarts(const char *uart_local, const char *uart_remote);
extern bool initialise_comms_library(const char *uart_device);
extern void log_message_received(secil_message* payload);
extern void test_uart_loopback();

#endif // COMMON_H