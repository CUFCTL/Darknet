struct arg_struct {
    char *leftImg;
    char *rightImg;
	CvMat *out;
};

void compute_stereo(char *leftImg, char *rightImg, CvMat *out);
void compute_stereo_mat(CvMat *leftImg, CvMat *rightImg, CvMat *out);
