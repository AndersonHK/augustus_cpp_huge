#pragma once

typedef enum {
    SCENARIO_EVENTS_XML_CURRENT_VERSION = 2,

    SCENARIO_EVENTS_XML_VERSION_NONE = 0,
    SCENARIO_EVENTS_XML_VERSION_INITIAL = 1,
    SCENARIO_EVENTS_XML_CUSTOM_VARIABLES_ADDED = 2,
} custom_variables_version;

int scenario_events_export_to_xml(const char *filename);
