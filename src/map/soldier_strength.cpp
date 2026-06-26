#include "soldier_strength.h"

#include "figure/figure.h"
#include "map/figure.h"
#include "map/grid.h"

static grid_u8 strength;

void map_soldier_strength_clear(void)
{
    map_grid_clear_u8(strength.items);
}

void map_soldier_strength_add(int x, int y, int radius, int amount)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            strength.items[grid_offset] += amount;
            if (map_has_figure_at(grid_offset) && Figure::get(map_figure_at(grid_offset))->is_legion()) {
                strength.items[grid_offset] += 2;
            }
        }
    }
}

int map_soldier_strength_get(int grid_offset)
{
    return strength.items[grid_offset];
}
