//
// Copyright (c) 2022 Tools for Humanity. All rights reserved.
//

#include "version.h"
#include <can_messaging.h>
#include <dfu.h>

int
version_send(void)
{
    struct image_version version = {0};

    dfu_version_primary_get(&version);

    McuMessage versions = {
        .which_message = McuMessage_m_message_tag,
        .message.m_message.which_payload = McuToJetson_versions_tag,
        .message.m_message.payload.versions.has_primary_app = true,
        .message.m_message.payload.versions.primary_app.major =
            version.iv_major,
        .message.m_message.payload.versions.primary_app.minor =
            version.iv_minor,
        .message.m_message.payload.versions.primary_app.patch =
            version.iv_revision,
#if defined(CONFIG_BOARD_MCU_MAIN_V30)
        .message.m_message.payload.versions.hardware_version = 30,
#elif defined(CONFIG_BOARD_MCU_MAIN_V31)
        .message.m_message.payload.versions.hardware_version = 31,
#endif
    };

    memset(&version, 0, sizeof version);
    if (dfu_version_secondary_get(&version) == 0) {
        versions.message.m_message.payload.versions.has_secondary_app = true;
        versions.message.m_message.payload.versions.secondary_app.major =
            version.iv_major;
        versions.message.m_message.payload.versions.secondary_app.minor =
            version.iv_minor;
        versions.message.m_message.payload.versions.secondary_app.patch =
            version.iv_revision;
    }

    return can_messaging_push_tx(&versions);
}
