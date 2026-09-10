#pragma once

// Legacy atlas layout knowledge belongs to extraction, never to simulation or rendering.
// Group numbers and exceptional frame orders describe the Caesar III source format.
#include <map>
#include <string>
#include <vector>

namespace vespasian::graphics::extraction {

struct LegacySequence {
    std::string name;
    int base;
    std::vector<int> frames;
    bool source_offsets = true;
    int x = 0;
    int y = 0;
};

inline std::vector<int> sequence_indices(int base, int count, int stride)
{
    std::vector<int> result;
    for (int frame = 0; frame < count; ++frame) result.push_back(base + frame * stride);
    return result;
}

// The shared SG2 walker layout interleaves eight directions in each movement row.
inline std::vector<LegacySequence> legacy_walker_sequences(int corpse_base, int corpse_count = 8, std::vector<LegacySequence> actions = {})
{
    static constexpr const char *directions[] = { "ne", "e", "se", "s", "sw", "w", "nw", "n" };
    std::vector<LegacySequence> sequences;
    for (int direction = 0; direction < 8; ++direction) {
        sequences.push_back({ std::string("move_") + directions[direction], direction, sequence_indices(direction, 12, 8) });
    }
    if (corpse_count > 0) sequences.push_back({ "corpse", corpse_base, sequence_indices(corpse_base, corpse_count, 1) });
    sequences.insert(sequences.end(), actions.begin(), actions.end());
    return sequences;
}

inline const std::vector<LegacySequence> &legacy_animation_sequences(int group)
{
    static const std::map<int, std::vector<LegacySequence>> layouts = {
        { 57, legacy_walker_sequences(96, 8) },
        { 88, legacy_walker_sequences(96, 8) },
        { 91, legacy_walker_sequences(96, 8) },
        { 98, legacy_walker_sequences(96, 8) },
        { 101, legacy_walker_sequences(96, 8) },
        { 102, {
            { "cloud", 0, sequence_indices(0, 8, 1), false, 0, 0 },
        } },
        { 104, legacy_walker_sequences(96, 8) },
        { 105, legacy_walker_sequences(96, 8) },
        { 106, legacy_walker_sequences(96, 8) },
        { 107, legacy_walker_sequences(96, 8) },
        { 108, legacy_walker_sequences(96, 8) },
        { 109, legacy_walker_sequences(0, 0) },
        { 110, legacy_walker_sequences(96, 8) },
        { 111, legacy_walker_sequences(96, 8, {
            { "attack_ne", 96, { 96, 96, 110, 110, 118, 118, 126, 126, 134, 134, 142, 142, 150, 150, 158, 158 }, true, 0, 0 },
            { "attack_e", 97, { 97, 97, 111, 111, 119, 119, 127, 127, 135, 135, 143, 143, 151, 151, 159, 159 }, true, 0, 0 },
            { "attack_se", 104, { 104, 104, 112, 112, 120, 120, 128, 128, 136, 136, 144, 144, 152, 152, 160, 160 }, true, 0, 0 },
            { "attack_s", 105, { 105, 105, 113, 113, 121, 121, 129, 129, 137, 137, 145, 145, 153, 153, 161, 161 }, true, 0, 0 },
            { "attack_sw", 106, { 106, 106, 114, 114, 122, 122, 130, 130, 138, 138, 146, 146, 154, 154, 162, 162 }, true, 0, 0 },
            { "attack_w", 107, { 107, 107, 115, 115, 123, 123, 131, 131, 139, 139, 147, 147, 155, 155, 163, 163 }, true, 0, 0 },
            { "attack_nw", 108, { 108, 108, 116, 116, 124, 124, 132, 132, 140, 140, 148, 148, 156, 156, 164, 164 }, true, 0, 0 },
            { "attack_n", 109, { 109, 109, 117, 117, 125, 125, 133, 133, 141, 141, 149, 149, 157, 157, 165, 165 }, true, 0, 0 },
        }) },
        { 115, {
            { "move_ne", 0, sequence_indices(0, 12, 8), false, 0, 0 },
            { "move_e", 1, sequence_indices(1, 12, 8), false, 0, 0 },
            { "move_se", 2, sequence_indices(2, 12, 8), false, 0, 0 },
            { "move_s", 3, sequence_indices(3, 12, 8), false, 0, 0 },
            { "move_sw", 4, sequence_indices(4, 12, 8), false, 0, 0 },
            { "move_w", 5, sequence_indices(5, 12, 8), false, 0, 0 },
            { "move_nw", 6, sequence_indices(6, 12, 8), false, 0, 0 },
            { "move_n", 7, sequence_indices(7, 12, 8), false, 0, 0 },
            { "corpse", 96, sequence_indices(96, 8, 1), false, 0, 0 },
            { "gesture", 104, { 104, 104, 105, 106, 107, 108, 109, 110, 111, 111, 110, 109, 108, 107, 106, 105 }, false, 0, 0 },
        } },
        { 116, legacy_walker_sequences(96, 8) },
        { 117, legacy_walker_sequences(96, 8) },
        { 118, legacy_walker_sequences(96, 8, {
            { "beggar", 104, sequence_indices(104, 8, 1), true, 0, 0 },
        }) },
        { 153, {
            { "wood", 0, { 0, 1, 2, 3, 4, 4, 4, 3, 2, 1, 0, 0 }, false, 0, 0 },
        } },
        { 154, {
            { "cargo_a", 0, { 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 3, 2, 1, 0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 0 }, false, 0, 0 },
        } },
        { 155, {
            { "cargo_b", 0, { 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 3, 2, 1, 0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 0 }, false, 0, 0 },
        } },
        { 156, {
            { "debris", 0, { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4 }, false, 0, 0 },
        } },
        { 194, legacy_walker_sequences(136, 8, {
            { "attack_ne", 96, sequence_indices(96, 5, 8), true, 0, 0 },
            { "attack_e", 97, sequence_indices(97, 5, 8), true, 0, 0 },
            { "attack_se", 98, sequence_indices(98, 5, 8), true, 0, 0 },
            { "attack_s", 99, sequence_indices(99, 5, 8), true, 0, 0 },
            { "attack_sw", 100, sequence_indices(100, 5, 8), true, 0, 0 },
            { "attack_w", 101, sequence_indices(101, 5, 8), true, 0, 0 },
            { "attack_nw", 102, sequence_indices(102, 5, 8), true, 0, 0 },
            { "attack_n", 103, sequence_indices(103, 5, 8), true, 0, 0 },
        }) },
        { 206, {
            { "variant_a", 0, sequence_indices(0, 18, 1), false, 0, 0 },
            { "variant_b", 18, sequence_indices(18, 24, 1), false, 0, 0 },
        } },
        { 209, legacy_walker_sequences(96, 8) },
        { 215, legacy_walker_sequences(0, 1) },
        { 217, {
            { "horse_blue_ne", 0, sequence_indices(0, 8, 8), false, 12, 16 },
            { "horse_blue_e", 1, sequence_indices(1, 8, 8), false, 12, 16 },
            { "horse_blue_se", 2, sequence_indices(2, 8, 8), false, 13, 16 },
            { "horse_blue_s", 3, sequence_indices(3, 8, 8), false, 12, 16 },
            { "horse_blue_sw", 4, sequence_indices(4, 8, 8), false, 12, 16 },
            { "horse_blue_w", 5, sequence_indices(5, 8, 8), false, 13, 16 },
            { "horse_blue_nw", 6, sequence_indices(6, 8, 8), false, 11, 16 },
            { "horse_blue_n", 7, sequence_indices(7, 8, 8), false, 12, 16 },
        } },
        { 218, {
            { "horse_red_ne", 0, sequence_indices(0, 8, 8), false, 12, 16 },
            { "horse_red_e", 1, sequence_indices(1, 8, 8), false, 12, 16 },
            { "horse_red_se", 2, sequence_indices(2, 8, 8), false, 13, 16 },
            { "horse_red_s", 3, sequence_indices(3, 8, 8), false, 12, 16 },
            { "horse_red_sw", 4, sequence_indices(4, 8, 8), false, 12, 16 },
            { "horse_red_w", 5, sequence_indices(5, 8, 8), false, 13, 16 },
            { "horse_red_nw", 6, sequence_indices(6, 8, 8), false, 11, 16 },
            { "horse_red_n", 7, sequence_indices(7, 8, 8), false, 12, 16 },
        } },
        { 219, {
            { "cart_blue_ne", 0, {}, true, 0, 0 },
            { "cart_blue_e", 1, {}, true, 0, 0 },
            { "cart_blue_se", 2, {}, true, 0, 0 },
            { "cart_blue_s", 3, {}, true, 0, 0 },
            { "cart_blue_sw", 4, {}, true, 0, 0 },
            { "cart_blue_w", 5, {}, true, 0, 0 },
            { "cart_blue_nw", 6, {}, true, 0, 0 },
            { "cart_blue_n", 7, {}, true, 0, 0 },
        } },
        { 220, {
            { "cart_red_ne", 0, {}, true, 0, 0 },
            { "cart_red_e", 1, {}, true, 0, 0 },
            { "cart_red_se", 2, {}, true, 0, 0 },
            { "cart_red_s", 3, {}, true, 0, 0 },
            { "cart_red_sw", 4, {}, true, 0, 0 },
            { "cart_red_w", 5, {}, true, 0, 0 },
            { "cart_red_nw", 6, {}, true, 0, 0 },
            { "cart_red_n", 7, {}, true, 0, 0 },
        } },
        { 228, legacy_walker_sequences(96, 8) },
        { 229, legacy_walker_sequences(96, 8) },
        { 230, legacy_walker_sequences(96, 8) },
        { 231, legacy_walker_sequences(96, 8) },
        { 233, {
            { "move_ne", 0, sequence_indices(0, 6, 8), false, 0, 0 },
            { "rest_ne", 48, { 48, 48, 56, 56, 64, 64, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 80, 80, 88, 88, 40, 40, 40, 40, 40, 40, 40, 40 }, false, 0, 0 },
            { "alternate_rest_ne", 96, {}, true, 0, 0 },
            { "move_e", 1, sequence_indices(1, 6, 8), false, 0, 0 },
            { "rest_e", 49, { 49, 49, 57, 57, 65, 65, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 81, 81, 89, 89, 41, 41, 41, 41, 41, 41, 41, 41 }, false, 0, 0 },
            { "alternate_rest_e", 97, {}, true, 0, 0 },
            { "move_se", 2, sequence_indices(2, 6, 8), false, 0, 0 },
            { "rest_se", 50, { 50, 50, 58, 58, 66, 66, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 82, 82, 90, 90, 42, 42, 42, 42, 42, 42, 42, 42 }, false, 0, 0 },
            { "alternate_rest_se", 98, {}, true, 0, 0 },
            { "move_s", 3, sequence_indices(3, 6, 8), false, 0, 0 },
            { "rest_s", 51, { 51, 51, 59, 59, 67, 67, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 83, 83, 91, 91, 43, 43, 43, 43, 43, 43, 43, 43 }, false, 0, 0 },
            { "alternate_rest_s", 99, {}, true, 0, 0 },
            { "move_sw", 4, sequence_indices(4, 6, 8), false, 0, 0 },
            { "rest_sw", 52, { 52, 52, 60, 60, 68, 68, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 84, 84, 92, 92, 44, 44, 44, 44, 44, 44, 44, 44 }, false, 0, 0 },
            { "alternate_rest_sw", 100, {}, true, 0, 0 },
            { "move_w", 5, sequence_indices(5, 6, 8), false, 0, 0 },
            { "rest_w", 53, { 53, 53, 61, 61, 69, 69, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 85, 85, 93, 93, 45, 45, 45, 45, 45, 45, 45, 45 }, false, 0, 0 },
            { "alternate_rest_w", 101, {}, true, 0, 0 },
            { "move_nw", 6, sequence_indices(6, 6, 8), false, 0, 0 },
            { "rest_nw", 54, { 54, 54, 62, 62, 70, 70, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 86, 86, 94, 94, 46, 46, 46, 46, 46, 46, 46, 46 }, false, 0, 0 },
            { "alternate_rest_nw", 102, {}, true, 0, 0 },
            { "move_n", 7, sequence_indices(7, 6, 8), false, 0, 0 },
            { "rest_n", 55, { 55, 55, 63, 63, 71, 71, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 87, 87, 95, 95, 47, 47, 47, 47, 47, 47, 47, 47 }, false, 0, 0 },
            { "alternate_rest_n", 103, {}, true, 0, 0 },
            { "corpse", 104, sequence_indices(104, 8, 1), false, 0, 0 },
        } },
        { 234, {
            { "move_ne", 0, sequence_indices(0, 12, 8), false, 0, 0 },
            { "attack_ne", 104, sequence_indices(104, 6, 8), false, 0, 0 },
            { "rest_ne", 152, {}, true, 0, 0 },
            { "move_e", 1, sequence_indices(1, 12, 8), false, 0, 0 },
            { "attack_e", 105, sequence_indices(105, 6, 8), false, 0, 0 },
            { "rest_e", 153, {}, true, 0, 0 },
            { "move_se", 2, sequence_indices(2, 12, 8), false, 0, 0 },
            { "attack_se", 106, sequence_indices(106, 6, 8), false, 0, 0 },
            { "rest_se", 154, {}, true, 0, 0 },
            { "move_s", 3, sequence_indices(3, 12, 8), false, 0, 0 },
            { "attack_s", 107, sequence_indices(107, 6, 8), false, 0, 0 },
            { "rest_s", 155, {}, true, 0, 0 },
            { "move_sw", 4, sequence_indices(4, 12, 8), false, 0, 0 },
            { "attack_sw", 108, sequence_indices(108, 6, 8), false, 0, 0 },
            { "rest_sw", 156, {}, true, 0, 0 },
            { "move_w", 5, sequence_indices(5, 12, 8), false, 0, 0 },
            { "attack_w", 109, sequence_indices(109, 6, 8), false, 0, 0 },
            { "rest_w", 157, {}, true, 0, 0 },
            { "move_nw", 6, sequence_indices(6, 12, 8), false, 0, 0 },
            { "attack_nw", 110, sequence_indices(110, 6, 8), false, 0, 0 },
            { "rest_nw", 158, {}, true, 0, 0 },
            { "move_n", 7, sequence_indices(7, 12, 8), false, 0, 0 },
            { "attack_n", 111, sequence_indices(111, 6, 8), false, 0, 0 },
            { "rest_n", 159, {}, true, 0, 0 },
            { "corpse", 96, sequence_indices(96, 8, 1), false, 0, 0 },
        } },
        { 235, {
            { "move_ne", 0, sequence_indices(0, 12, 8), false, 0, 0 },
            { "rest_ne", 104, { 104, 104, 112, 112, 120, 120, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 136, 136, 136, 136, 0, 0, 0, 0, 0, 0, 0, 0 }, false, 0, 0 },
            { "alternate_rest_ne", 0, {}, true, 0, 0 },
            { "move_e", 1, sequence_indices(1, 12, 8), false, 0, 0 },
            { "rest_e", 105, { 105, 105, 113, 113, 121, 121, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 137, 137, 137, 137, 1, 1, 1, 1, 1, 1, 1, 1 }, false, 0, 0 },
            { "alternate_rest_e", 1, {}, true, 0, 0 },
            { "move_se", 2, sequence_indices(2, 12, 8), false, 0, 0 },
            { "rest_se", 106, { 106, 106, 114, 114, 122, 122, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 138, 138, 138, 138, 2, 2, 2, 2, 2, 2, 2, 2 }, false, 0, 0 },
            { "alternate_rest_se", 2, {}, true, 0, 0 },
            { "move_s", 3, sequence_indices(3, 12, 8), false, 0, 0 },
            { "rest_s", 107, { 107, 107, 115, 115, 123, 123, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 139, 139, 139, 139, 3, 3, 3, 3, 3, 3, 3, 3 }, false, 0, 0 },
            { "alternate_rest_s", 3, {}, true, 0, 0 },
            { "move_sw", 4, sequence_indices(4, 12, 8), false, 0, 0 },
            { "rest_sw", 108, { 108, 108, 116, 116, 124, 124, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 140, 140, 140, 140, 4, 4, 4, 4, 4, 4, 4, 4 }, false, 0, 0 },
            { "alternate_rest_sw", 4, {}, true, 0, 0 },
            { "move_w", 5, sequence_indices(5, 12, 8), false, 0, 0 },
            { "rest_w", 109, { 109, 109, 117, 117, 125, 125, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 141, 141, 141, 141, 5, 5, 5, 5, 5, 5, 5, 5 }, false, 0, 0 },
            { "alternate_rest_w", 5, {}, true, 0, 0 },
            { "move_nw", 6, sequence_indices(6, 12, 8), false, 0, 0 },
            { "rest_nw", 110, { 110, 110, 118, 118, 126, 126, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 142, 142, 142, 142, 6, 6, 6, 6, 6, 6, 6, 6 }, false, 0, 0 },
            { "alternate_rest_nw", 6, {}, true, 0, 0 },
            { "move_n", 7, sequence_indices(7, 12, 8), false, 0, 0 },
            { "rest_n", 111, { 111, 111, 119, 119, 127, 127, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 143, 143, 143, 143, 7, 7, 7, 7, 7, 7, 7, 7 }, false, 0, 0 },
            { "alternate_rest_n", 7, {}, true, 0, 0 },
            { "corpse", 96, sequence_indices(96, 8, 1), false, 0, 0 },
        } },
        { 242, {
            { "neptune_sheep", 0, { 0, 1, 2, 3, 4, 4, 4, 3, 2, 1, 0, 0 }, false, 0, 0 },
        } },
    };
    static const std::vector<LegacySequence> empty;
    const auto found = layouts.find(group);
    return found == layouts.end() ? empty : found->second;
}

} // namespace vespasian::graphics::extraction
