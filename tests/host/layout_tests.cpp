#include <mk7/host/layout.hpp>
#include <cassert>
int main(){auto r=mk7::host::compute_screen_layout(1280,720);assert(r[0].width*240==r[0].height*400);assert(r[1].width==r[1].height*320/240);assert(r[0].x+r[0].width/2==640);assert(r[1].x+r[1].width/2==640);assert(r[0].y+r[0].height<r[1].y);}
