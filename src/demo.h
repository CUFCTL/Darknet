#ifndef DEMO
#define DEMO

#include "image.h"

void demo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names, int classes, int frame_skip, char *prefix, float hier_thresh);
double get_wall_time();
void test_stream(int cam_index, const char *filename, int frame_skip, char *prefix);

#endif
