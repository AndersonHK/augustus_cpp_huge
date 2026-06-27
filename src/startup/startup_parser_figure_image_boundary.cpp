#ifdef STARTUP_PARSER_TEST

#include "figure/figure.h"
#include "figure/image.h"

unsigned int Figure::id() const
{
    return 0;
}

int figure_image_normalize_direction(int direction)
{
    direction %= 8;
    return direction < 0 ? direction + 8 : direction;
}

int figure_image_corpse_offset(Figure *f)
{
    (void) f;
    return 0;
}

#endif // STARTUP_PARSER_TEST
