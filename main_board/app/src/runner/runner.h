#ifndef RUNNER_H
#define RUNNER_H

#include <can_messaging.h>

/// Get number of successful run jobs
uint32_t
runner_successful_jobs_count(void);

/// Queue new message handling
/// ⚠️ Do not run from ISR
void
runner_handle_new(can_message_t *msg);

#endif // RUNNER_H
