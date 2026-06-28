#include "mission.h"

#include <cstdlib>
#include <vector>

static struct {
    std::vector<campaign_mission> missions;
    std::vector<campaign_scenario> scenarios;
} data;

campaign_mission *campaign_mission_new(void)
{
    data.missions.emplace_back();
    campaign_mission &mission = data.missions.back();
    mission = {};
    mission.id = static_cast<unsigned int>(data.missions.size() - 1);
    mission.first_scenario = static_cast<int>(data.scenarios.size());
    mission.last_scenario = mission.first_scenario - 1;
    return &mission;
}

campaign_mission *campaign_mission_current(int index)
{
    for (campaign_mission &mission : data.missions) {
        if (mission.first_scenario <= index && mission.last_scenario >= index) {
            return &mission;
        }
    }
    return nullptr;
}

campaign_mission *campaign_mission_next(int last_index)
{
    for (campaign_mission &mission : data.missions) {
        if (mission.first_scenario > last_index) {
            return &mission;
        }
    }
    return nullptr;
}

campaign_scenario *campaign_mission_new_scenario(void)
{
    data.scenarios.emplace_back();
    campaign_scenario &new_scenario = data.scenarios.back();
    new_scenario = {};
    new_scenario.id = static_cast<unsigned int>(data.scenarios.size() - 1);
    return &new_scenario;
}

campaign_scenario *campaign_mission_get_scenario(unsigned int scenario_id)
{
    return scenario_id < data.scenarios.size() ? &data.scenarios[scenario_id] : nullptr;
}

int campaign_mission_init(void)
{
    campaign_mission_clear();
    return 1;
}

void campaign_mission_clear(void)
{
    if (game_campaign_is_custom()) {
        for (campaign_mission &mission : data.missions) {
            free((uint8_t *) mission.title);
            free((char *) mission.background_image.path);
            free((char *) mission.intro_video);
        }
    }
    data.missions.clear();
    for (campaign_scenario &campaign_entry : data.scenarios) {
        free((uint8_t *) campaign_entry.name);
        if (game_campaign_is_custom()) {
            free((uint8_t *) campaign_entry.description);
            free((char *) campaign_entry.fanfare);
            free((char *) campaign_entry.path);
        }
    }
    data.scenarios.clear();
}
