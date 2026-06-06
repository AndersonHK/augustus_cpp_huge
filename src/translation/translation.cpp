#include "translation/translation.h"
#include "translation/localization.h"
#include "translation/localization_internal.h"

extern "C" {
#include "core/lang.h"
#include "core/log.h"
}

void translation_load(language_type language)
{
    if (!localization::rebuild_legacy_cache(language)) {
        log_error("Invalid translation selected", 0, 0);
        return;
    }
    lang_refresh_message_cache();
}

const uint8_t *translation_for_key(const char *key)
{
    const uint8_t *text = localization::legacy_named_project_string(key);
    if (text && text[0]) {
        return text;
    }

    localization::detail::report_missing_project_key(key);
    return reinterpret_cast<const uint8_t *>(key && *key ? key : "");
}
