#ifdef STARTUP_PARSER_TEST

#include "building/menu.h"
#include "building/monument.h"
#include "scenario/event/parameter_data.h"

void building_menu_invalidate_catalog(void)
{
}

void building_monument_reset_runtime_bridge(void)
{
}

// Event selectors are not linked into the standalone definition parser.
// Their cache lifecycle is exercised by the executable's editor/settings tests.
void scenario_events_parameter_data_reset_mappings(void)
{
}

#endif // STARTUP_PARSER_TEST
