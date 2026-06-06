#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#define SCENARIO_CUSTOM_EMPIRE 99

int scenario_empire_id(void);

int scenario_empire_is_expanded(void);

void scenario_empire_process_expansion(void);

#ifdef __cplusplus
}
#endif
