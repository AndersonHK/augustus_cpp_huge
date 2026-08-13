#ifdef STARTUP_PARSER_TEST

#include "city/view.h"
#include "figure/figure.h"
#include "figure/image.h"

int city_view_orientation(void)
{
    return 0;
}

unsigned int Figure::id() const
{
    return 0;
}

int figure_image_normalize_direction(int direction)
{
    direction %= 8;
    return direction < 0 ? direction + 8 : direction;
}

#endif // STARTUP_PARSER_TEST
