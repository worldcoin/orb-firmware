#include "version.h"
#include "mcu_messaging.pb.h"
#include <can_messaging.h>
#include <dfu.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(version);

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
        .message.m_message.payload.versions.primary_app.commit_hash =
            version.iv_build_num,
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
        versions.message.m_message.payload.versions.secondary_app.commit_hash =
            version.iv_build_num;
    }

    return can_messaging_push_tx(&versions);
}
