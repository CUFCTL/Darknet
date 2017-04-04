#ifndef PERCEPTION
#define PERCEPTION

void pdemo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names, int classes, int frame_skip, char *prefix, float hier_thresh);
void stereo_stream(int cam_index, const char *filename, int frame_skip, char *prefix);

#endif
