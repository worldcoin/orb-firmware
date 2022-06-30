#ifndef INCOMING_MESSAGE_HANDLING_H
#define INCOMING_MESSAGE_HANDLING_H

#include <can_messaging.h>

uint32_t
runner_successful_jobs_count(void);

void
runner_handle_new(can_message_t *msg);

#endif // INCOMING_MESSAGE_HANDLING_H
