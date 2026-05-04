#include "properties.h"

#include "assets/assets.h"
#include "core/image_group.h"
#include "sound/city.h"
#include "translation/translation.h"
#include "type.h"

#include <stddef.h>
#include <stdlib.h>

static model_building buildings[BUILDING_TYPE_MAX];

// PROPERTIES
// BuildingType XML owns the migrated building rows. The static table below is
// only for legacy-only types, tools, terrain, houses, and internal records.

static building_properties properties[BUILDING_TYPE_MAX] = {
    [BUILDING_ANY] = {
        .event_data.attr = "any",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_ANY
    },
    [BUILDING_MENU_FARMS] = {
        .event_data.attr = "farms|all_farms",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_FARMS
    },
    [BUILDING_MENU_RAW_MATERIALS] = {
        .event_data.attr = "raw_materials|all_raw_materials",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_RAW_MATERIALS
    },
    [BUILDING_MENU_WORKSHOPS] = {
        .event_data.attr = "workshops|all_workshops",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_WORKSHOPS
    },
    [BUILDING_CLEAR_LAND] = {
        .event_data.attr = "clear_land",
        .event_data.cannot_count = 1,
        .building_model_data = {.cost = 2, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
    },
    [BUILDING_REPAIR_LAND] = {
        .event_data.attr = "repair_land",
        .event_data.cannot_count = 1,
        .building_model_data = {.cost = 3, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
    },
    [BUILDING_CLEAR_TREES] = {
        .event_data.attr = "clear_trees",
        .event_data.cannot_count = 1,
        .building_model_data = {.cost = 2, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
    },
    [BUILDING_ROAD] = {
        .size = 1,
        .image_group = 112,
        .event_data.attr = "road",
        .building_model_data = {.cost = 4, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
    },
    [BUILDING_WALL] = {
        .size = 1,
        .image_group = 24,
        .image_offset = 26,
        .fire_proof = 1,
        .draw_desirability_range = 1,
        .event_data.attr = "wall",
        .building_model_data = {.cost = 12, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
    },
    [BUILDING_DRAGGABLE_RESERVOIR] = {
        .sound_id = SOUND_CITY_RESERVOIR,
        .event_data.attr = "reservoir",
    },
    [BUILDING_AQUEDUCT] = {
        .size = 1,
        .image_group = 19,
        .image_offset = 2,
        .fire_proof = 1,
        .event_data.attr = "aqueduct",
        .building_model_data = {.cost = 8, .desirability_value = -2, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 2, .laborers = 0}
    },
    [BUILDING_HOUSE_SMALL_TENT] = {
        .size = 1,
        .sound_id = SOUND_CITY_HOUSE_SLUM,
        .event_data.attr = "vacant_lot|house_small_tent|housing",
        .building_model_data = {.cost = 10, .desirability_value = -3, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 3, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = -101,
            .evolve_desirability = -10,
            .entertainment = 0,
            .water = 0,
            .religion = 0,
            .education = 0,
            .barber = 0,
            .bathhouse = 0,
            .health = 0,
            .food_types = 0,
            .pottery = 0,
            .oil = 0,
            .furniture = 0,
            .wine = 0,
            .prosperity = 5,
            .max_people = 5,
            .tax_multiplier = 1}
    },
    [BUILDING_HOUSE_LARGE_TENT] = {
        .size = 1,
        .sound_id = SOUND_CITY_HOUSE_SLUM,
        .event_data.attr = "house_large_tent",
        .building_model_data = {.cost = 0, .desirability_value = -3, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 3, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = -12,
            .evolve_desirability = -5,
            .entertainment = 0,
            .water = 1,
            .religion = 0,
            .education = 0,
            .barber = 0,
            .bathhouse = 0,
            .health = 0,
            .food_types = 0,
            .pottery = 0,
            .oil = 0,
            .furniture = 0,
            .wine = 0,
            .prosperity = 10,
            .max_people = 7,
            .tax_multiplier = 1}
    },
    [BUILDING_HOUSE_SMALL_SHACK] = {
        .size = 1,
        .sound_id = SOUND_CITY_HOUSE_SLUM,
        .event_data.attr = "house_small_shack",
        .building_model_data = {.cost = 0, .desirability_value = -2, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 2, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = -7,
            .evolve_desirability = 0,
            .entertainment = 0,
            .water = 1,
            .religion = 0,
            .education = 0,
            .barber = 0,
            .bathhouse = 0,
            .health = 0,
            .food_types = 1,
            .pottery = 0,
            .oil = 0,
            .furniture = 0,
            .wine = 0,
            .prosperity = 15,
            .max_people = 9,
            .tax_multiplier = 1}
    },
    [BUILDING_HOUSE_LARGE_SHACK] = {
        .size = 1,
        .sound_id = SOUND_CITY_HOUSE_SLUM,
        .event_data.attr = "house_large_shack",
        .building_model_data = {.cost = 0, .desirability_value = -2, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 2, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = -2,
            .evolve_desirability = 4,
            .entertainment = 0,
            .water = 1,
            .religion = 1,
            .education = 0,
            .barber = 0,
            .bathhouse = 0,
            .health = 0,
            .food_types = 1,
            .pottery = 0,
            .oil = 0,
            .furniture = 0,
            .wine = 0,
            .prosperity = 20,
            .max_people = 11,
            .tax_multiplier = 1}
    },
    [BUILDING_HOUSE_SMALL_HOVEL] = {
        .size = 1,
        .sound_id = SOUND_CITY_HOUSE_POOR,
        .event_data.attr = "house_small_hovel",
        .building_model_data = {.cost = 0, .desirability_value = -2, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 2, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 2,
            .evolve_desirability = 8,
            .entertainment = 0,
            .water = 2,
            .religion = 1,
            .education = 0,
            .barber = 0,
            .bathhouse = 0,
            .health = 0,
            .food_types = 1,
            .pottery = 0,
            .oil = 0,
            .furniture = 0,
            .wine = 0,
            .prosperity = 25,
            .max_people = 13,
            .tax_multiplier = 2}
    },
    [BUILDING_HOUSE_LARGE_HOVEL] = {
        .size = 1,
        .sound_id = SOUND_CITY_HOUSE_POOR,
        .event_data.attr = "house_large_hovel",
        .building_model_data = {.cost = 0, .desirability_value = -2, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 2, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 6,
            .evolve_desirability = 12,
            .entertainment = 10,
            .water = 2,
            .religion = 1,
            .education = 0,
            .barber = 0,
            .bathhouse = 0,
            .health = 0,
            .food_types = 1,
            .pottery = 0,
            .oil = 0,
            .furniture = 0,
            .wine = 0,
            .prosperity = 30,
            .max_people = 15,
            .tax_multiplier = 2}
    },
    [BUILDING_HOUSE_SMALL_CASA] = {
        .size = 1,
        .sound_id = SOUND_CITY_HOUSE_POOR,
        .event_data.attr = "house_small_casa",
        .building_model_data = {.cost = 0, .desirability_value = -1, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 1, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 10,
            .evolve_desirability = 16,
            .entertainment = 10,
            .water = 2,
            .religion = 1,
            .education = 1,
            .barber = 0,
            .bathhouse = 0,
            .health = 0,
            .food_types = 1,
            .pottery = 0,
            .oil = 0,
            .furniture = 0,
            .wine = 0,
            .prosperity = 35,
            .max_people = 17,
            .tax_multiplier = 2}
    },
    [BUILDING_HOUSE_LARGE_CASA] = {
        .size = 1,
        .sound_id = SOUND_CITY_HOUSE_POOR,
        .event_data.attr = "house_large_casa",
        .building_model_data = {.cost = 0, .desirability_value = -1, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 1, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 14,
            .evolve_desirability = 20,
            .entertainment = 10,
            .water = 2,
            .religion = 1,
            .education = 1,
            .barber = 0,
            .bathhouse = 1,
            .health = 0,
            .food_types = 1,
            .pottery = 1,
            .oil = 0,
            .furniture = 0,
            .wine = 0,
            .prosperity = 45,
            .max_people = 19,
            .tax_multiplier = 2}
    },
    [BUILDING_HOUSE_SMALL_INSULA] = {
        .size = 1,
        .sound_id = SOUND_CITY_HOUSE_MEDIUM,
        .event_data.attr = "house_small_insula",
        .building_model_data = {.cost = 0, .desirability_value = 0, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 1, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 18,
            .evolve_desirability = 25,
            .entertainment = 25,
            .water = 2,
            .religion = 1,
            .education = 1,
            .barber = 0,
            .bathhouse = 1,
            .health = 0,
            .food_types = 1,
            .pottery = 1,
            .oil = 0,
            .furniture = 0,
            .wine = 0,
            .prosperity = 50,
            .max_people = 19,
            .tax_multiplier = 3}
    },
    [BUILDING_HOUSE_MEDIUM_INSULA] = {
        .size = 1,
        .sound_id = SOUND_CITY_HOUSE_MEDIUM,
        .event_data.attr = "house_medium_insula",
        .building_model_data = {.cost = 0, .desirability_value = 0, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 1, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 22,
            .evolve_desirability = 32,
            .entertainment = 25,
            .water = 2,
            .religion = 1,
            .education = 1,
            .barber = 0,
            .bathhouse = 1,
            .health = 1,
            .food_types = 1,
            .pottery = 1,
            .oil = 0,
            .furniture = 1,
            .wine = 0,
            .prosperity = 58,
            .max_people = 20,
            .tax_multiplier = 3}
    },
    [BUILDING_HOUSE_LARGE_INSULA] = {
        .size = 2,
        .sound_id = SOUND_CITY_HOUSE_MEDIUM,
        .event_data.attr = "house_large_insula",
        .building_model_data = {.cost = 0, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 29,
            .evolve_desirability = 40,
            .entertainment = 25,
            .water = 2,
            .religion = 1,
            .education = 2,
            .barber = 1,
            .bathhouse = 1,
            .health = 1,
            .food_types = 1,
            .pottery = 1,
            .oil = 1,
            .furniture = 1,
            .wine = 0,
            .prosperity = 65,
            .max_people = 84,
            .tax_multiplier = 3}
    },
    [BUILDING_HOUSE_GRAND_INSULA] = {
        .size = 2,
        .sound_id = SOUND_CITY_HOUSE_MEDIUM,
        .event_data.attr = "house_grand_insula",
        .building_model_data = {.cost = 0, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 37,
            .evolve_desirability = 48,
            .entertainment = 35,
            .water = 2,
            .religion = 1,
            .education = 2,
            .barber = 1,
            .bathhouse = 1,
            .health = 1,
            .food_types = 2,
            .pottery = 1,
            .oil = 1,
            .furniture = 1,
            .wine = 0,
            .prosperity = 80,
            .max_people = 84,
            .tax_multiplier = 4}
    },
    [BUILDING_HOUSE_SMALL_VILLA] = {
        .size = 2,
        .sound_id = SOUND_CITY_HOUSE_GOOD,
        .event_data.attr = "house_small_villa",
        .building_model_data = {.cost = 0, .desirability_value = 1, .desirability_step = 2,
            .desirability_step_size = -1, .desirability_range = 2, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 45,
            .evolve_desirability = 53,
            .entertainment = 35,
            .water = 2,
            .religion = 2,
            .education = 2,
            .barber = 1,
            .bathhouse = 1,
            .health = 1,
            .food_types = 2,
            .pottery = 1,
            .oil = 1,
            .furniture = 1,
            .wine = 1,
            .prosperity = 150,
            .max_people = 40,
            .tax_multiplier = 9}
    },
    [BUILDING_HOUSE_MEDIUM_VILLA] = {
        .size = 2,
        .sound_id = SOUND_CITY_HOUSE_GOOD,
        .event_data.attr = "house_medium_villa",
        .building_model_data = {.cost = 0, .desirability_value = 1, .desirability_step = 2,
            .desirability_step_size = -1, .desirability_range = 2, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 50,
            .evolve_desirability = 58,
            .entertainment = 40,
            .water = 2,
            .religion = 2,
            .education = 2,
            .barber = 1,
            .bathhouse = 1,
            .health = 2,
            .food_types = 2,
            .pottery = 1,
            .oil = 1,
            .furniture = 1,
            .wine = 1,
            .prosperity = 180,
            .max_people = 42,
            .tax_multiplier = 10}
    },
    [BUILDING_HOUSE_LARGE_VILLA] = {
        .size = 3,
        .sound_id = SOUND_CITY_HOUSE_GOOD,
        .event_data.attr = "house_large_villa",
        .building_model_data = {.cost = 0, .desirability_value = 2, .desirability_step = 2,
            .desirability_step_size = -2, .desirability_range = 2, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 55,
            .evolve_desirability = 63,
            .entertainment = 45,
            .water = 2,
            .religion = 2,
            .education = 3,
            .barber = 1,
            .bathhouse = 1,
            .health = 2,
            .food_types = 2,
            .pottery = 1,
            .oil = 1,
            .furniture = 1,
            .wine = 1,
            .prosperity = 400,
            .max_people = 90,
            .tax_multiplier = 11}
    },
    [BUILDING_HOUSE_GRAND_VILLA] = {
        .size = 3,
        .sound_id = SOUND_CITY_HOUSE_GOOD,
        .event_data.attr = "house_grand_villa",
        .building_model_data = {.cost = 0, .desirability_value = 2, .desirability_step = 2,
            .desirability_step_size = -2, .desirability_range = 2, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 60,
            .evolve_desirability = 68,
            .entertainment = 50,
            .water = 2,
            .religion = 3,
            .education = 3,
            .barber = 1,
            .bathhouse = 1,
            .health = 2,
            .food_types = 3,
            .pottery = 1,
            .oil = 1,
            .furniture = 1,
            .wine = 1,
            .prosperity = 600,
            .max_people = 100,
            .tax_multiplier = 11}
    },
    [BUILDING_HOUSE_SMALL_PALACE] = {
        .size = 3,
        .sound_id = SOUND_CITY_HOUSE_POSH,
        .event_data.attr = "house_small_palace",
        .building_model_data = {.cost = 0, .desirability_value = 3, .desirability_step = 2,
            .desirability_step_size = -1, .desirability_range = 6, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 65,
            .evolve_desirability = 74,
            .entertainment = 55,
            .water = 2,
            .religion = 3,
            .education = 3,
            .barber = 1,
            .bathhouse = 1,
            .health = 2,
            .food_types = 3,
            .pottery = 1,
            .oil = 1,
            .furniture = 1,
            .wine = 2,
            .prosperity = 700,
            .max_people = 106,
            .tax_multiplier = 12}
    },
    [BUILDING_HOUSE_MEDIUM_PALACE] = {
        .size = 3,
        .sound_id = SOUND_CITY_HOUSE_POSH,
        .event_data.attr = "house_medium_palace",
        .building_model_data = {.cost = 0, .desirability_value = 3, .desirability_step = 2,
            .desirability_step_size = -1, .desirability_range = 6, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 70,
            .evolve_desirability = 80,
            .entertainment = 60,
            .water = 2,
            .religion = 4,
            .education = 3,
            .barber = 1,
            .bathhouse = 1,
            .health = 2,
            .food_types = 3,
            .pottery = 1,
            .oil = 1,
            .furniture = 1,
            .wine = 2,
            .prosperity = 900,
            .max_people = 112,
            .tax_multiplier = 12}
    },
    [BUILDING_HOUSE_LARGE_PALACE] = {
        .size = 4,
        .sound_id = SOUND_CITY_HOUSE_POSH,
        .event_data.attr = "house_large_palace",
        .building_model_data = {.cost = 0, .desirability_value = 4, .desirability_step = 2,
            .desirability_step_size = -1, .desirability_range = 6, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 76,
            .evolve_desirability = 90,
            .entertainment = 70,
            .water = 2,
            .religion = 4,
            .education = 3,
            .barber = 1,
            .bathhouse = 1,
            .health = 2,
            .food_types = 3,
            .pottery = 1,
            .oil = 1,
            .furniture = 1,
            .wine = 2,
            .prosperity = 1500,
            .max_people = 190,
            .tax_multiplier = 15}
    },
    [BUILDING_HOUSE_LUXURY_PALACE] = {
        .size = 4,
        .sound_id = SOUND_CITY_HOUSE_POSH,
        .event_data.attr = "house_luxury_palace",
        .building_model_data = {.cost = 0, .desirability_value = 4, .desirability_step = 2,
            .desirability_step_size = -1, .desirability_range = 6, .laborers = 0},
        .house_model_data = {
            .devolve_desirability = 85,
            .evolve_desirability = 100,
            .entertainment = 80,
            .water = 2,
            .religion = 4,
            .education = 3,
            .barber = 1,
            .bathhouse = 1,
            .health = 2,
            .food_types = 3,
            .pottery = 1,
            .oil = 1,
            .furniture = 1,
            .wine = 2,
            .prosperity = 1750,
            .max_people = 200,
            .tax_multiplier = 16}
    },
    [BUILDING_HIPPODROME] = {
        .size = 5,
        .fire_proof = 1,
        .image_group = 213,
        .sound_id = SOUND_CITY_HIPPODROME,
        .event_data.attr = "hippodrome",
        .building_model_data = {.cost = 3500, .desirability_value = -3, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 3, .laborers = 150} // Differs from model.txt
    },
    [BUILDING_COLOSSEUM] = {
        .size = 5,
        .fire_proof = 1,
        .image_group = 48,
        .sound_id = SOUND_CITY_COLOSSEUM,
        .event_data.attr = "colosseum",
        .building_model_data = {.cost = 1500, .desirability_value = -3, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 3, .laborers = 100} // Differs from model.txt
    },
    [BUILDING_CHARIOT_MAKER] = {
        .size = 3,
        .image_group = 52,
        .sound_id = SOUND_CITY_CHARIOT_MAKER,
        .event_data.attr = "chariot_maker",
        .building_model_data = {.cost = 75, .desirability_value = -3, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 3, .laborers = 10}
    },
    [BUILDING_PLAZA] = {
        .size = 1,
        .fire_proof = 1,
        .image_group = 58,
        .event_data.attr = "plaza",
        .building_model_data = {.cost = 15, .desirability_value = 4, .desirability_step = 1,
            .desirability_step_size = -2, .desirability_range = 2, .laborers = 0}
    },
    [BUILDING_GARDENS] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .image_group = 59,
        .image_offset = 1,
        .sound_id = SOUND_CITY_GARDEN,
        .event_data.attr = "gardens",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
    },
    [BUILDING_MENU_FORT] = {
        .event_data.attr = "fort|all_forts",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_FORTS
    },
    [BUILDING_FORT_GROUND] = {
        .size = 4,
        .fire_proof = 1,
        .image_group = 66,
        .image_offset = 1
    },
    [BUILDING_FORT_LEGIONARIES] = {
        .size = 3,
        .fire_proof = 1,
        .image_group = 66,
        .draw_desirability_range = 1,
        .sound_id = SOUND_CITY_FORT,
        .event_data.attr = "fort_legionaries",
        .building_model_data = {.cost = 1000, .desirability_value = -20, .desirability_step = 2,
            .desirability_step_size = 2, .desirability_range = 8, .laborers = 0}
    },
    [BUILDING_FORT_JAVELIN] = {
        .size = 3,
        .fire_proof = 1,
        .image_group = 66,
        .draw_desirability_range = 1,
        .sound_id = SOUND_CITY_FORT,
        .event_data.attr = "fort_javelin",
        .building_model_data = {.cost = 1000, .desirability_value = -20, .desirability_step = 2,
            .desirability_step_size = 2, .desirability_range = 8, .laborers = 0}
    },
    [BUILDING_FORT_MOUNTED] = {
        .size = 3,
        .fire_proof = 1,
        .image_group = 66,
        .draw_desirability_range = 1,
        .sound_id = SOUND_CITY_FORT,
        .event_data.attr = "fort_mounted",
        .building_model_data = {.cost = 1000, .desirability_value = -20, .desirability_step = 2,
            .desirability_step_size = 2, .desirability_range = 8, .laborers = 0}
    },
    [BUILDING_FORT_ARCHERS] = {
        .size = 3,
        .fire_proof = 1,
        .image_group = 66,
        .draw_desirability_range = 1,
        .sound_id = SOUND_CITY_FORT,
        .event_data.attr = "fort_archers",
        .building_model_data = {.cost = 1000, .desirability_value = -20, .desirability_step = 2,
            .desirability_step_size = 2, .desirability_range = 8, .laborers = 0}
    },
    [BUILDING_FORT_AUXILIA_INFANTRY] = {
        .size = 3,
        .fire_proof = 1,
        .image_group = 66,
        .draw_desirability_range = 1,
        .sound_id = SOUND_CITY_FORT,
        .event_data.attr = "fort_swords",
        .building_model_data = {.cost = 1000, .desirability_value = -20, .desirability_step = 2,
            .desirability_step_size = 2, .desirability_range = 8, .laborers = 0}
    },
    [BUILDING_SMALL_STATUE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .rotation_offset = -12,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "V Small Statue",
        .event_data.attr = "small_statue",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
    },
    [BUILDING_MEDIUM_STATUE] = {
        .venus_gt_bonus = 1,
        .size = 2,
        .fire_proof = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Med_Statue_R",
        .event_data.attr = "medium_statue",
        .building_model_data = {.cost = 60, .desirability_value = 10, .desirability_step = 1,
            .desirability_step_size = -2, .desirability_range = 4, .laborers = 0}
    },
    [BUILDING_DISTRIBUTION_CENTER_UNUSED] = {
        .size = 3,
        .image_group = 66
    },
    [BUILDING_TRIUMPHAL_ARCH] = {
        .size = 3,
        .fire_proof = 1,
        .image_group = 205,
        .event_data.attr = "triumphal_arch",
        .building_model_data = {.cost = 0, .desirability_value = 18, .desirability_step = 2,
            .desirability_step_size = 3, .desirability_range = 5, .laborers = 0}
    },
    [BUILDING_GATEHOUSE] = {
        .size = 2,
        .fire_proof = 1,
        .image_group = 17,
        .image_offset = 1,
        .sound_id = SOUND_CITY_TOWER,
        .draw_desirability_range = 1,
        .event_data.attr = "gatehouse",
        .building_model_data = {.cost = 100, .desirability_value = -4, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 3, .laborers = 0}
    },
    [BUILDING_TOWER] = {
        .size = 2,
        .fire_proof = 1,
        .image_group = 17,
        .sound_id = SOUND_CITY_TOWER,
        .draw_desirability_range = 1,
        .event_data.attr = "tower",
        .building_model_data = {.cost = 150, .desirability_value = -8, .desirability_step = 1,
            .desirability_step_size = 2, .desirability_range = 3, .laborers = 6}
    },
    [BUILDING_GRANARY] = {
        .size = 3,
        .image_group = 99,
        .sound_id = SOUND_CITY_GRANARY,
        .event_data.attr = "granary",
        .building_model_data = {.cost = 100, .desirability_value = -4, .desirability_step = 1,
            .desirability_step_size = 2, .desirability_range = 2, .laborers = 6}
    },
    [BUILDING_WAREHOUSE] = {
        .size = 1,
        .fire_proof = 1,
        .image_group = 82,
        .sound_id = SOUND_CITY_WAREHOUSE,
        .event_data.attr = "warehouse",
        .building_model_data = {.cost = 70, .desirability_value = -5, .desirability_step = 2,
            .desirability_step_size = 2, .desirability_range = 3, .laborers = 6}
    },
    [BUILDING_WAREHOUSE_SPACE] = {
        .size = 1,
        .fire_proof = 1,
        .image_group = 82
    },
    [BUILDING_SHIPYARD] = {
        .size = 2,
        .image_group = 77,
        .sound_id = SOUND_CITY_SHIPYARD,
        .event_data.attr = "shipyard",
        .building_model_data = {.cost = 100, .desirability_value = -8, .desirability_step = 2,
            .desirability_step_size = 2, .desirability_range = 3, .laborers = 10}
    },
    [BUILDING_DOCK] = {
        .size = 3,
        .image_group = 78,
        .sound_id = SOUND_CITY_DOCK,
        .event_data.attr = "dock",
        .building_model_data = {.cost = 100, .desirability_value = -8, .desirability_step = 2,
            .desirability_step_size = 2, .desirability_range = 3, .laborers = 12}
    },
    [BUILDING_WHARF] = {
        .size = 2,
        .image_group = 79,
        .sound_id = SOUND_CITY_WHARF,
        .event_data.attr = "wharf",
        .building_model_data = {.cost = 60, .desirability_value = -8, .desirability_step = 2,
            .desirability_step_size = 2, .desirability_range = 3, .laborers = 6}
    },
    [BUILDING_GOVERNORS_HOUSE] = {
        .size = 3,
        .image_group = 85,
        .sound_id = SOUND_CITY_PALACE,
        .event_data.attr = "governors_house",
        .building_model_data = {.cost = 150, .desirability_value = 12, .desirability_step = 2,
            .desirability_step_size = -2, .desirability_range = 3, .laborers = 0}
    },
    [BUILDING_GOVERNORS_VILLA] = {
        .size = 4,
        .image_group = 86,
        .sound_id = SOUND_CITY_PALACE,
        .event_data.attr = "governors_villa",
        .building_model_data = {.cost = 400, .desirability_value = 20, .desirability_step = 2,
            .desirability_step_size = -3, .desirability_range = 4, .laborers = 0}
    },
    [BUILDING_GOVERNORS_PALACE] = {
        .size = 5,
        .image_group = 87,
        .sound_id = SOUND_CITY_PALACE,
        .event_data.attr = "governors_palace",
        .building_model_data = {.cost = 750, .desirability_value = 28, .desirability_step = 2,
            .desirability_step_size = -4, .desirability_range = 5, .laborers = 0}
    },
    [BUILDING_MISSION_POST] = {
        .size = 2,
        .fire_proof = 1,
        .image_group = 184,
        .sound_id = SOUND_CITY_MISSION_POST,
        .event_data.attr = "mission_post",
        .building_model_data = {.cost = 100, .desirability_value = -3, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 2, .laborers = 20}
    },
    [BUILDING_LOW_BRIDGE] = {
        .size = 1,
        .fire_proof = 1,
        .event_data.attr = "low_bridge",
        .building_model_data = {.cost = 40, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
    },
    [BUILDING_SHIP_BRIDGE] = {
        .size = 1,
        .fire_proof = 1,
        .event_data.attr = "ship_bridge",
        .building_model_data = {.cost = 100, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
    },
    [BUILDING_NATIVE_HUT] = {
        .size = 1,
        .fire_proof = 1,
        .image_group = 183,
        .sound_id = SOUND_CITY_NATIVE_HUT,
        .event_data.attr = "native_hut",
        .building_model_data = {.cost = 50, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
    },
    [BUILDING_NATIVE_MEETING] = {
        .size = 2,
        .fire_proof = 1,
        .image_group = 183,
        .image_offset = 2,
        .sound_id = SOUND_CITY_NATIVE_HUT,
        .event_data.attr = "native_meeting",
        .building_model_data = {.cost = 50, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
    },
     [BUILDING_NATIVE_CROPS] = {
        .size = 1,
        .fire_proof = 1,
        .image_group = 100,
        .event_data.attr = "native_crops",
        .sound_id = SOUND_CITY_WHEAT_FARM,
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_NATIVE_CROPS,
        .building_model_data = {.cost = 0, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
     },
     [BUILDING_MILITARY_ACADEMY] = {
        .size = 3,
        .image_group = 201,
        .sound_id = SOUND_CITY_MILITARY_ACADEMY,
        .draw_desirability_range = 1,
        .event_data.attr = "military_academy",
        .building_model_data = {.cost = 1000, .desirability_value = -3, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 3, .laborers = 20}
     },
     [BUILDING_BARRACKS] = {
        .size = 3,
        .image_group = 166,
        .sound_id = SOUND_CITY_BARRACKS,
        .draw_desirability_range = 1,
        .event_data.attr = "barracks",
        .building_model_data = {.cost = 150, .desirability_value = -6, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 3, .laborers = 10}
     },
     [BUILDING_MENU_SMALL_TEMPLES] = {
        .event_data.attr = "small_temples|all_small_temples",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_SMALL_TEMPLES
     },
     [BUILDING_MENU_LARGE_TEMPLES] = {
        .event_data.attr = "large_temples|all_large_temples",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_LARGE_TEMPLES
     },
     [BUILDING_ORACLE] = {
        .venus_gt_bonus = 1,
        .size = 2,
        .fire_proof = 1,
        .image_group = 76,
        .sound_id = SOUND_CITY_ORACLE,
        .draw_desirability_range = 1,
        .event_data.attr = "oracle",
        .building_model_data = {.cost = 200, .desirability_value = 8, .desirability_step = 2,
            .desirability_step_size = -1, .desirability_range = 6, .laborers = 0}
     },
     [BUILDING_BURNING_RUIN] = {
        .size = 1,
        .fire_proof = 1,
        .sound_id = SOUND_CITY_BURNING_RUIN,
        .building_model_data = {.cost = 0, .desirability_value = -1, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 2, .laborers = 0}
     },
     [BUILDING_ROADBLOCK] = {
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Admin_Logistics",
        .event_data.attr = "roadblock",
        .building_model_data = {.cost = 12, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
     },
     [BUILDING_MENU_GRAND_TEMPLES] = {
        .event_data.attr = "grand_temples|all_grand_temples",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_GRAND_TEMPLES
     },
     [BUILDING_MENU_TREES] = {
        .event_data.attr = "trees|all_trees",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_TREES
     },
     [BUILDING_MENU_PATHS] = {
        .event_data.attr = "paths|all_paths",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_PATHS
     },
     [BUILDING_MENU_PARKS] = {
        .event_data.attr = "parks|all_parks",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_PARKS
     },
     [BUILDING_PINE_TREE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental pine",
        .event_data.attr = "pine_tree",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_FIR_TREE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental fir",
        .event_data.attr = "fir_tree",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_OAK_TREE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental oak",
        .event_data.attr = "oak_tree",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_ELM_TREE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental elm",
        .event_data.attr = "elm_tree",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_FIG_TREE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental fig",
        .event_data.attr = "fig_tree",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_PLUM_TREE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental plum",
        .event_data.attr = "plum_tree",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_PALM_TREE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental palm",
        .event_data.attr = "palm_tree",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_DATE_TREE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental date",
        .event_data.attr = "date_tree",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_PINE_PATH] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn pine",
        .event_data.attr = "pine_tree",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_FIR_PATH] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn fir",
        .event_data.attr = "fir_path",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_OAK_PATH] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn oak",
        .event_data.attr = "oak_path",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_ELM_PATH] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn elm",
        .event_data.attr = "elm_path",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_FIG_PATH] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn fig",
        .event_data.attr = "fig_path",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_PLUM_PATH] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn plum",
        .event_data.attr = "plum_path",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_PALM_PATH] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn palm",
        .event_data.attr = "palm_path",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_DATE_PATH] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn date",
        .event_data.attr = "date_path",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_PAVILION_BLUE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "pavilion blue",
        .event_data.attr = "pavilion_blue",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_PAVILION_RED] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "pavilion red",
        .event_data.attr = "pavilion_red",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_PAVILION_ORANGE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "pavilion orange",
        .event_data.attr = "pavilion_orange",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_PAVILION_YELLOW] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "pavilion yellow",
        .event_data.attr = "pavilion_yellow",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_PAVILION_GREEN] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "pavilion green",
        .event_data.attr = "pavilion_green",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_GODDESS_STATUE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .rotation_offset = 13,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "sml statue 2",
        .event_data.attr = "goddess_statue|small_statue_alt",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_SENATOR_STATUE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .rotation_offset = 13,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "sml statue 3",
        .event_data.attr = "senator_statue|small_statue_alt_b",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_OBELISK] = {
        .venus_gt_bonus = 1,
        .size = 2,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "obelisk",
        .event_data.attr = "obelisk",
        .building_model_data = {.cost = 60, .desirability_value = 10, .desirability_step = 1,
            .desirability_step_size = -2, .desirability_range = 4, .laborers = 0}
     },
     [BUILDING_MESS_HALL] = {
        .size = 3,
        .sound_id = SOUND_CITY_MESS_HALL,
        .draw_desirability_range = 1,
        .custom_asset.group = "Military",
        .custom_asset.id = "Mess OFF Central",
        .event_data.attr = "mess_hall",
        .building_model_data = {.cost = 100, .desirability_value = -8, .desirability_step = 1,
            .desirability_step_size = 2, .desirability_range = 4, .laborers = 10}
     },
     [BUILDING_MENU_STATUES] = {
        .event_data.attr = "statues|all_statues",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_STATUES
     },
     [BUILDING_MENU_GOV_RES] = {
        .event_data.attr = "governor_home|all_governors_houses",
        .event_data.key = TR_PARAMETER_VALUE_BUILDING_MENU_GOV_RES
     },
     [BUILDING_GRAND_GARDEN] = {
        .venus_gt_bonus = 1,
        .size = 2,
        .fire_proof = 1,
        .event_data.attr = "grand_garden",
        .event_data.cannot_count = 1,
        .building_model_data = {.cost = 400, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0} // Unused
     },
     [BUILDING_HORSE_STATUE] = {
        .venus_gt_bonus = 1,
        .size = 3,
        .fire_proof = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Eque Statue",
        .event_data.attr = "horse_statue",
        .building_model_data = {.cost = 150, .desirability_value = 14, .desirability_step = 2,
            .desirability_step_size = -2, .desirability_range = 5, .laborers = 0}
     },
     [BUILDING_DOLPHIN_FOUNTAIN] = {
        .venus_gt_bonus = 1,
        .size = 2,
        .fire_proof = 1,
        .event_data.attr = "dolphin_fountain",
        .event_data.cannot_count = 1,
        .building_model_data = {.cost = 60, .desirability_value = 10, .desirability_step = 1,
            .desirability_step_size = -2, .desirability_range = 4, .laborers = 0} // Unused
     },
     [BUILDING_HEDGE_DARK] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "D Hedge 01",
        .event_data.attr = "hedge_dark",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_HEDGE_LIGHT] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "L Hedge 01",
        .event_data.attr = "hedge_light",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_LOOPED_GARDEN_WALL] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "C Garden Wall 01",
        .event_data.attr = "looped_garden_wall",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_LEGION_STATUE] = {
        .venus_gt_bonus = 1,
        .size = 2,
        .fire_proof = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "legio statue",
        .event_data.attr = "legion_statue",
        .building_model_data = {.cost = 60, .desirability_value = 10, .desirability_step = 1,
            .desirability_step_size = -2, .desirability_range = 4, .laborers = 0}
     },
     [BUILDING_DECORATIVE_COLUMN] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "sml col B",
        .event_data.attr = "decorative_column",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_COLONNADE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "G Colonnade 01",
        .event_data.attr = "colonnade",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_GARDEN_PATH] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Garden Path 01",
        .event_data.attr = "garden_path",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_LARARIUM] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .draw_desirability_range = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Lararium 01",
        .event_data.attr = "lararium",
        .building_model_data = {.cost = 45, .desirability_value = 4, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_NYMPHAEUM] = {
        .venus_gt_bonus = 1,
        .size = 3,
        .fire_proof = 1,
        .sound_id = SOUND_CITY_ORACLE,
        .draw_desirability_range = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Nymphaeum OFF",
        .event_data.attr = "nymphaeum",
        .building_model_data = {.cost = 400, .desirability_value = 12, .desirability_step = 2,
            .desirability_step_size = -1, .desirability_range = 6, .laborers = 0}
     },
     [BUILDING_SMALL_MAUSOLEUM] = {
        .venus_gt_bonus = 1,
        .size = 2,
        .fire_proof = 1,
        .sound_id = SOUND_CITY_ORACLE,
        .draw_desirability_range = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Mausoleum S",
        .event_data.attr = "small_mausoleum",
        .building_model_data = {.cost = 250, .desirability_value = -8, .desirability_step = 1,
            .desirability_step_size = 3, .desirability_range = 5, .laborers = 0}
     },
     [BUILDING_LARGE_MAUSOLEUM] = {
        .venus_gt_bonus = 1,
        .size = 3,
        .fire_proof = 1,
        .sound_id = SOUND_CITY_ORACLE,
        .draw_desirability_range = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Mausoleum L",
        .event_data.attr = "large_mausoleum",
        .building_model_data = {.cost = 500, .desirability_value = -10, .desirability_step = 1,
            .desirability_step_size = 3, .desirability_range = 6, .laborers = 0}
     },
     [BUILDING_ROOFED_GARDEN_WALL] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "R Garden Wall 01",
        .event_data.attr = "roofed_garden_wall",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_ROOFED_GARDEN_WALL_GATE] = {
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Garden_Gate_B",
        .event_data.attr = "garden_wall_gate",
        .building_model_data = {.cost = 12, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
     },
     [BUILDING_PALISADE] = {
        .size = 1,
        .fire_proof = 1,
        .draw_desirability_range = 1,
        .custom_asset.group = "Military",
        .custom_asset.id = "Pal Wall C 01",
        .event_data.attr = "palisade",
        .building_model_data = {.cost = 6, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
     },
     [BUILDING_HEDGE_GATE_DARK] = {
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "D Hedge Gate",
        .event_data.attr = "hedge_gate_dark",
        .building_model_data = {.cost = 12, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
     },
     [BUILDING_HEDGE_GATE_LIGHT] = {
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "L Hedge Gate",
        .event_data.attr = "hedge_gate_light",
        .building_model_data = {.cost = 12, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
     },
     [BUILDING_PALISADE_GATE] = {
        .size = 1,
        .fire_proof = 1,
        .draw_desirability_range = 1,
        .custom_asset.group = "Military",
        .custom_asset.id = "Palisade_Gate",
        .event_data.attr = "palisade_gate",
        .building_model_data = {.cost = 12, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
     },
     [BUILDING_GLADIATOR_STATUE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Aesthetics",
        .event_data.attr = "gladiator_statue",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_HIGHWAY] = {
        .size = 2,
        .fire_proof = 1,
        .custom_asset.group = "Admin_Logistics",
        .custom_asset.id = "Highway_Placement",
        .event_data.attr = "highway",
        .building_model_data = {.cost = 100, .desirability_value = -4, .desirability_step = 1,
            .desirability_step_size = 2, .desirability_range = 2, .laborers = 0}
     },
     [BUILDING_CITY_MINT] = {
        .size = 3,
        .fire_proof = 1,
        .sound_id = SOUND_CITY_CITY_MINT,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "City_Mint_ON",
        .event_data.attr = "city_mint",
        .building_model_data = {.cost = 250, .desirability_value = -3, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 3, .laborers = 40}
     },
     [BUILDING_DEPOT] = {
        .size = 2,
        .sound_id = SOUND_CITY_DEPOT,
        .custom_asset.group = "Admin_Logistics",
        .custom_asset.id = "Cart Depot N OFF",
        .event_data.attr = "cart_depot",
        .building_model_data = {.cost = 100, .desirability_value = -3, .desirability_step = 1,
            .desirability_step_size = 1, .desirability_range = 2, .laborers = 15}
     },
     [BUILDING_LOOPED_GARDEN_GATE] = {
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Garden_Gate",
        .event_data.attr = "looped_garden_gate",
        .building_model_data = {.cost = 12, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
     },
     [BUILDING_PANELLED_GARDEN_GATE] = {
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Garden_Gate_C",
        .event_data.attr = "panelled_garden_gate",
        .building_model_data = {.cost = 12, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
     },
     [BUILDING_PANELLED_GARDEN_WALL] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Garden_Wall_C",
        .event_data.attr = "panelled_garden_wall",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_SHRINE_CERES] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .rotation_offset = 1,
        .draw_desirability_range = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Altar_Ceres",
        .event_data.attr = "shrine_ceres",
        .building_model_data = {.cost = 45, .desirability_value = 4, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_SHRINE_MARS] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .rotation_offset = 1,
        .draw_desirability_range = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Altar_Mars",
        .event_data.attr = "shrine_mars",
        .building_model_data = {.cost = 45, .desirability_value = 4, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_SHRINE_MERCURY] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .rotation_offset = 1,
        .draw_desirability_range = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Altar_Mercury",
        .event_data.attr = "shrine_mercury",
        .building_model_data = {.cost = 45, .desirability_value = 4, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_SHRINE_NEPTUNE] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .rotation_offset = 1,
        .draw_desirability_range = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Altar_Neptune",
        .event_data.attr = "shrine_neptune",
        .building_model_data = {.cost = 45, .desirability_value = 4, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_SHRINE_VENUS] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .rotation_offset = 1,
        .draw_desirability_range = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Altar_Venus",
        .event_data.attr = "shrine_venus",
        .building_model_data = {.cost = 45, .desirability_value = 4, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_OVERGROWN_GARDENS] = {
        .venus_gt_bonus = 1,
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Overgrown_Garden_01",
        .event_data.attr = "overgrown_gardens",
        .building_model_data = {.cost = 12, .desirability_value = 3, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 3, .laborers = 0}
     },
     [BUILDING_LATRINES] = {
        .size = 1,
        .fire_proof = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Latrine_N",
        .event_data.attr = "latrines",
        .building_model_data = {.cost = 15, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 2}
     },
     [BUILDING_NATIVE_HUT_ALT] = {
        .size = 1,
        .sound_id = SOUND_CITY_NATIVE_HUT,
        .fire_proof = 1,
        .custom_asset.group = "Terrain_Maps",
        .custom_asset.id = "Native_Hut_Central_01",
        .event_data.attr = "native_hut_alt",
        .building_model_data = {.cost = 50, .desirability_value = 0, .desirability_step = 0,
            .desirability_step_size = 0, .desirability_range = 0, .laborers = 0}
     },
     [BUILDING_NATIVE_DECORATION] = {
        .size = 1,
        .sound_id = SOUND_CITY_NATIVE_DECORATION,
        .fire_proof = 1,
        .custom_asset.group = "Terrain_Maps",
        .custom_asset.id = "Native_Decoration_Central_01",
        .event_data.attr = "native_decor",
        .building_model_data = {.cost = 0, .desirability_value = 6, .desirability_step = 1,
            .desirability_step_size = -1, .desirability_range = 4, .laborers = 0}
     },
     [BUILDING_NATIVE_MONUMENT] = {
        .size = 4,
        .sound_id = SOUND_CITY_NATIVE_DECORATION,
        .fire_proof = 1,
        .custom_asset.group = "Terrain_Maps",
        .custom_asset.id = "Native_Monument_Central_01",
        .event_data.attr = "native_monument",
        .building_model_data = {.cost = 0, .desirability_value = 15, .desirability_step = 2,
            .desirability_step_size = -2, .desirability_range = 8, .laborers = 0}
     },
     [BUILDING_NATIVE_WATCHTOWER] = {
        .size = 1,
        .sound_id = SOUND_CITY_WATCHTOWER,
        .fire_proof = 1,
        .custom_asset.group = "Terrain_Maps",
        .custom_asset.id = "Native_Watchtower_Central_01",
        .event_data.attr = "native_watchtower",
        .building_model_data = {.cost = 0, .desirability_value = -10, .desirability_step = 1,
            .desirability_step_size = 2, .desirability_range = 4, .laborers = 0}
     },
};

void building_properties_init(void)
{
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type++) {
        if (!properties[type].custom_asset.group) {
            continue;
        }
        if (properties[type].custom_asset.id) {
            properties[type].image_group = assets_get_image_id(properties[type].custom_asset.group,
                properties[type].custom_asset.id);
        } else {
            properties[type].image_group = assets_get_group_id(properties[type].custom_asset.group);
        }
    }
}

void building_properties_clear_xml_runtime_fields(building_type type)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return;
    }
    properties[type].size = 0;
    properties[type].fire_proof = 0;
    properties[type].sound_id = 0;
    properties[type].draw_desirability_range = 0;
    properties[type].venus_gt_bonus = 0;
    properties[type].event_data.attr = 0;
    properties[type].event_data.key = 0;
    properties[type].event_data.cannot_count = 0;
}

void building_properties_apply_xml_model_size(building_type type, int size)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return;
    }
    properties[type].size = size;
}

void building_properties_apply_xml_event_attr(building_type type, const char *attr)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return;
    }
    properties[type].event_data.attr = attr;
}

void building_properties_apply_xml_sound_id(building_type type, int sound_id)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return;
    }
    properties[type].sound_id = sound_id;
}

void building_properties_apply_xml_fire_proof(building_type type, int fire_proof)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return;
    }
    properties[type].fire_proof = fire_proof;
}

void building_properties_apply_xml_draw_desirability_range(building_type type, int draw_desirability_range)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return;
    }
    properties[type].draw_desirability_range = draw_desirability_range;
}

void building_properties_apply_xml_venus_gt_bonus(building_type type, int venus_gt_bonus)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return;
    }
    properties[type].venus_gt_bonus = venus_gt_bonus;
}

const building_properties *building_properties_for_type(building_type type)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return &properties[0];
    }
    return &properties[type];
}

// MODEL DATA

static model_building NOTHING = { .cost = 0, .desirability_value = 0, .desirability_step = 0,
 .desirability_step_size = 0, .desirability_range = 0, .laborers = 0 };

void model_reset(void)
{
    for (building_type type = BUILDING_ANY; type < BUILDING_TYPE_MAX; type++) {
        const building_properties *props = &properties[type];
        if (((props->size && props->event_data.attr) &&
            (type != BUILDING_GRAND_GARDEN && type != BUILDING_DOLPHIN_FOUNTAIN)) ||
            type == BUILDING_CLEAR_LAND || type == BUILDING_REPAIR_LAND || type == BUILDING_CLEAR_TREES) {
            buildings[type] = props->building_model_data;
        } else {
            buildings[type] = NOTHING;
        }
    }
}

void model_save_model_data(buffer *buf)
{
    int buf_size = sizeof(model_building) * BUILDING_TYPE_MAX;
    uint8_t *buf_data = malloc(buf_size);

    buffer_init(buf, buf_data, buf_size);

    buffer_write_raw(buf, buildings, buf_size);
}
void model_load_model_data(buffer *buf)
{
    int buf_size = sizeof(model_building) * BUILDING_TYPE_MAX;

    buffer_read_raw(buf, buildings, buf_size);
}

const model_house *model_get_house(house_level level)
{
    return &properties[level + 10].house_model_data;
}

model_building *model_get_building(building_type type)
{
    if (type > BUILDING_TYPE_MAX) {
        return &NOTHING;
    }
    return &buildings[type];
}

int model_house_uses_inventory(house_level level, resource_type inventory)
{
    const model_house *house = model_get_house(level);
    switch (inventory) {
        case RESOURCE_WINE:
            return house->wine;
        case RESOURCE_OIL:
            return house->oil;
        case RESOURCE_FURNITURE:
            return house->furniture;
        case RESOURCE_POTTERY:
            return house->pottery;
        default:
            return 0;
    }
}
