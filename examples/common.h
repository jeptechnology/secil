#pragma once

#include <secil.h>

/// @brief Prints to the stdout the message received.
/// @param type    The type of the message. 
/// @param payload The payload of the message.
void log_message_received(secil_message_type_t type, secil_message_payload* payload);

/// @brief Inject an error into the loopback stream by appending random bytes to the buffer.
/// @param bytes Number of bytes to inject.
void inject_loopback_error(size_t bytes);

/// @brief Initialize the loopback example code that uses a ram based buffer.
bool init_secil_loopback();