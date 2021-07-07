//
// Created by Cyril on 28/06/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_COM_H
#define WORLDCOIN_FIRMWARE_ORB_APP_COM_H

#include <stdint.h>
#include <assert.h>

/// Tell com TX that new data is ready to be sent
/// This will unblock the TX task using a notification
void
com_new_data(void);

void
com_init(void);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_COM_H
