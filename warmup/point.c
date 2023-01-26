#include <assert.h>
#include "common.h"
#include "point.h"
#include <math.h>

void
point_translate(struct point *p, double x, double y)
{
    p->x = p->x + x;
    p->y = p->y + y;
}

double
point_distance(const struct point *p1, const struct point *p2)
{
    double xDistance = p1->x - p2->x;
    double yDistance = p1->y - p2->y;
    double distance = sqrt(pow(xDistance,2)+pow(yDistance,2));
    return distance;
}

int
point_compare(const struct point *p1, const struct point *p2)
{
    double p1_distance = sqrt(pow(p1->x,2)+pow(p1->y,2));
    double p2_distance = sqrt(pow(p2->x,2)+pow(p2->y,2));
    if(p1_distance < p2_distance) {
        return -1;
    } else if (p1_distance == p2_distance) {
        return 0;
    } else {
        return 1;
    }
}
