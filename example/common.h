#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <secil.h>

extern bool initialise_comms_library(const char *uart_local, const char *uart_remote);
extern void log_message_received(secil_message* payload);

#endif // COMMON_H