struct arg_struct {
    char *leftImg;
    char *rightImg;
	CvMat *out;
};

struct zed_struct {
    image zed;
    CvMat *cvleft;
	CvMat *cvright;
	CvMat *disparity;
};

void compute_stereo(char *leftImg, char *rightImg, CvMat *out);
void compute_stereo_mat(CvMat *leftImg, CvMat *rightImg, CvMat *out);
