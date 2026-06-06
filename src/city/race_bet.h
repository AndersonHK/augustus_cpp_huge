#pragma once
#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    NO_BET,
    BLUE_HORSE,
    RED_HORSE,
    WHITE_HORSE,
    GREEN_HORSE
} bet_horse;

int has_bet_in_progress(void);
void race_result_process(void);

#ifdef __cplusplus
}
#endif
