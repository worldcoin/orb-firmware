#include <zephyr/kernel.h>

typedef struct callback_info {
    void (*callback)(void *context);
    void *context;
} callback_info_t;

/**
 * @brief Initialize the timer2 interrupt
 */
void
timer2_init(void);

/**
 * @brief Register a callback to be called when the timer2 interrupt occurs
 *
 * @param channel The channel to register the callback for
 * @param callback The callback to be called
 * @param context The context to be passed to the callback
 */
void
timer2_register_callback(uint8_t channel, void (*callback)(void *context),
                         void *context);
