#include "network.h"
#include "detection_layer.h"
#include "region_layer.h"
#include "cost_layer.h"
#include "utils.h"
#include "parser.h"
#include "box.h"
#include "image.h"
#include "demo.h"
#include "stereo.h"
#include "perception.h"
#include <sys/time.h>
#define FRAMES 3

static char **demo_names;
static image **demo_alphabet;
static int demo_classes;

static float **probs;
static box *boxes;
static network net;
static image in   ;
static image in_s ;
static image det  ;
static image det_s;
static image disp = {0};
static CvCapture * cap;
static float fps = 0;
static float demo_thresh = 0;
static float demo_hier_thresh = .5;

static float *predictions[FRAMES];
static int demo_index = 0;
static image images[FRAMES];
static float *avg;

static image zed;

#ifdef OPENCV
static void *stereo_in_thread(void *arguments)
{
	struct zed_struct *args = arguments;
	printf("Computing stereo...\n");
	compute_stereo_mat(args->cvleft, args->cvright, args->disparity);
    return 0;
}

static void *zed_in_thread(void *ptr)
{
	// printf("Grabbing image...\n");
    zed = get_image_from_stream(cap);
	// printf("Cropping image...\n");
	in = crop_image(zed, 0, 0, zed.w/2, zed.h);
	// in = get_image_from_stream(cap);

    if(!in.data){
        error("Stream closed.");
    }
	in_s = resize_image(in, net.w, net.h);
    return 0;
}

static void *detect_in_thread(void *ptr)
{
    float nms = .4;

    layer l = net.layers[net.n-1];
    float *X = det_s.data;
    float *prediction = network_predict(net, X);

    memcpy(predictions[demo_index], prediction, l.outputs*sizeof(float));
    mean_arrays(predictions, FRAMES, l.outputs, avg);
    l.output = avg;

    free_image(det_s);
    if(l.type == DETECTION){
        get_detection_boxes(l, 1, 1, demo_thresh, probs, boxes, 0);
    } else if (l.type == REGION){
        get_region_boxes(l, 1, 1, demo_thresh, probs, boxes, 0, 0, demo_hier_thresh);
    } else {
        error("Last layer must produce detections\n");
    }
    if (nms > 0) do_nms(boxes, probs, l.w*l.h*l.n, l.classes, nms);
    printf("\033[2J");
    printf("\033[1;1H");
    printf("\nFPS:%.1f\n",fps);
    printf("Objects:\n\n");

    images[demo_index] = det;
    det = images[(demo_index + FRAMES/2 + 1)%FRAMES];
    demo_index = (demo_index + 1)%FRAMES;

    draw_detections(det, l.w*l.h*l.n, demo_thresh, boxes, probs, demo_names, demo_alphabet, demo_classes);

    return 0;
}

void stereo_stream(int cam_index, const char *filename, int frame_skip, char *prefix)
{
	printf("Stereo stream\n");

    int j, count;
    int delay = frame_skip;
	int p[3];
	p[0] = CV_IMWRITE_PNG_COMPRESSION;
    p[1] = 100;
    p[2] = 0;

	// printf("Opening camera...\n");
    if(filename){
        printf("video file: %s\n", filename);
        cap = cvCaptureFromFile(filename);
    }else{
        cap = cvCaptureFromCAM(cam_index);
    }
    if(!cap) error("Couldn't connect to webcam.\n");

	// printf("Initial thread calls...\n");
    pthread_t zed_thread;
	pthread_t stereo_thread;
    zed_in_thread(0);
    zed_in_thread(0);
    for(j = 0; j < FRAMES/2; ++j){
        zed_in_thread(0);
    }

	// printf("Creating Zed struct...\n");
	struct zed_struct zed_args;
	zed_args.zed = zed;
	zed_args.cvleft = cvCreateMat(zed.h, zed.w/2, CV_8UC3);
	zed_args.cvright = cvCreateMat(zed.h, zed.w/2, CV_8UC3);
	zed_args.disparity = cvCreateMat(zed.h, zed.w/2, CV_8UC1);
	CvMat *stereo_disp = cvCreateMat(zed.h, zed.w/2, CV_8UC1);
	
	// printf("Save initial images...\n");
	image_to_CvMat_zed(&zed_args);
	stereo_in_thread(&zed_args);
	save_image(zed_args.zed, "zed");
	cvSaveImage("cvleft.png", zed_args.cvleft, p);
	cvSaveImage("cvright.png", zed_args.cvright, p);
	cvSaveImage("disparity.png", zed_args.disparity, p);

	// printf("Creating cvWindows...\n");
    count = 0;
    if(!prefix){
        cvNamedWindow("Stereo", CV_WINDOW_NORMAL); 
        cvMoveWindow("Stereo", 0, 0);
        cvResizeWindow("Stereo", 600, 400);
    }

	// printf("Start processing...\n");
    double before = get_wall_time();
    while(1){
        ++count;
        if(1){
            if(pthread_create(&zed_thread, 0, zed_in_thread, &zed_args)) 
				error("Thread creation failed");
			if(pthread_create(&stereo_thread, 0, stereo_in_thread, &zed_args)) 
				error("Thread creation failed");

            if(!prefix){
				cvShowImage("Stereo", stereo_disp);
                int c = cvWaitKey(1);
                if (c == 10){
                    if(frame_skip == 0) frame_skip = 60;
                    else if(frame_skip == 4) frame_skip = 0;
                    else if(frame_skip == 60) frame_skip = 4;   
                    else frame_skip = 0;
                }
            }else{
                char buff[256];
                sprintf(buff, "%s_%08d", prefix, count);
                save_image(disp, buff);
            }

            pthread_join(zed_thread, 0);
			pthread_join(stereo_thread, 0);
			zed_args.zed = zed;
			image_to_CvMat_zed(&zed_args);

            if(delay == 0){
				free_image(zed);
				stereo_disp = zed_args.disparity;
            }
        }else {
            zed_in_thread(0);
            if(delay == 0) {
                free_image(zed);
				stereo_disp = zed_args.disparity;
            }
            show_image(disp, "Stereo");
            cvWaitKey(1);
        }
        --delay;
        if(delay < 0){
            delay = frame_skip;

            double after = get_wall_time();
            float curr = 1./(after - before);
            fps = curr;
            before = after;
        }
    }
}

void pdemo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names, int classes, int frame_skip, char *prefix, float hier_thresh)
{
	printf("Perception Demo\n");

    int j, count;
	int p[3];
	p[0] = CV_IMWRITE_PNG_COMPRESSION;
    p[1] = 100;
    p[2] = 0;

    image **alphabet = load_alphabet();
    int delay = frame_skip;
    demo_names = names;
    demo_alphabet = alphabet;
    demo_classes = classes;
    demo_thresh = thresh;
    demo_hier_thresh = hier_thresh;

	// printf("Loading network...\n");
    net = parse_network_cfg(cfgfile);
    if(weightfile){
        load_weights(&net, weightfile);
    }
    set_batch_network(&net, 1);
    srand(2222222);

	// printf("Opening camera...\n");
    if(filename){
        printf("video file: %s\n", filename);
        cap = cvCaptureFromFile(filename);
    }else{
        cap = cvCaptureFromCAM(cam_index);
    }
    if(!cap) error("Couldn't connect to webcam.\n");

	// printf("Creating memory objects...\n");
    layer l = net.layers[net.n-1];
    avg = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) predictions[j] = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) images[j] = make_image(1,1,3);
    boxes = (box *)calloc(l.w*l.h*l.n, sizeof(box));
    probs = (float **)calloc(l.w*l.h*l.n, sizeof(float *));
    for(j = 0; j < l.w*l.h*l.n; ++j) probs[j] = (float *)calloc(l.classes, sizeof(float));

	// printf("Initial thread calls...\n");
    pthread_t zed_thread;
    pthread_t detect_thread;
	pthread_t stereo_thread;
    zed_in_thread(0);
    det = in;
    det_s = in_s;
    zed_in_thread(0);
    detect_in_thread(0);
    disp = det;
    det = in;
    det_s = in_s;
    for(j = 0; j < FRAMES/2; ++j){
        zed_in_thread(0);
        detect_in_thread(0);
        disp = det;
        det = in;
        det_s = in_s;
    }
	
	// printf("Creating Zed struct...\n");
	struct zed_struct zed_args;
	zed_args.zed = zed;
	zed_args.cvleft = cvCreateMat(zed.h, zed.w/2, CV_8UC3);
	zed_args.cvright = cvCreateMat(zed.h, zed.w/2, CV_8UC3);
	zed_args.disparity = cvCreateMat(zed.h, zed.w/2, CV_8UC1);
	CvMat *stereo_disp = cvCreateMat(zed.h, zed.w/2, CV_8UC1);
	
	// printf("Save initial images...\n");
	image_to_CvMat_zed(&zed_args);
	stereo_in_thread(&zed_args);
	save_image(zed_args.zed, "zed");
	cvSaveImage("cvleft.png", zed_args.cvleft, p);
	cvSaveImage("cvright.png", zed_args.cvright, p);
	cvSaveImage("disparity.png", zed_args.disparity, p);

	// printf("Creating cvWindows...\n");
    count = 0;
    if(!prefix){
        cvNamedWindow("Demo", CV_WINDOW_NORMAL); 
        cvMoveWindow("Demo", 0, 0);
        cvResizeWindow("Demo", 600, 400);
		cvNamedWindow("Stereo", CV_WINDOW_NORMAL); 
        cvMoveWindow("Stereo", 600, 0);
        cvResizeWindow("Stereo", 600, 400);
    }

	// printf("Start processing...\n");
    double before = get_wall_time();
    while(1){
        ++count;
        if(1){
            if(pthread_create(&zed_thread, 0, zed_in_thread, &zed_args)) 
				error("Thread creation failed");
            if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) 
				error("Thread creation failed");
			if(pthread_create(&stereo_thread, 0, stereo_in_thread, &zed_args)) 
				error("Thread creation failed");

            if(!prefix){
                show_image(disp, "Demo");
				cvShowImage("Stereo", stereo_disp);
                int c = cvWaitKey(1);
                if (c == 10){
                    if(frame_skip == 0) frame_skip = 60;
                    else if(frame_skip == 4) frame_skip = 0;
                    else if(frame_skip == 60) frame_skip = 4;   
                    else frame_skip = 0;
                }
            }else{
                char buff[256];
                sprintf(buff, "%s_%08d", prefix, count);
                save_image(disp, buff);
            }

            pthread_join(zed_thread, 0);
            pthread_join(detect_thread, 0);
			pthread_join(stereo_thread, 0);
			zed_args.zed = zed;
			image_to_CvMat_zed(&zed_args);

            if(delay == 0){
				free_image(zed);
                free_image(disp);
				stereo_disp = zed_args.disparity;
                disp  = det;
            }
            det   = in;
            det_s = in_s;
        }else {
            zed_in_thread(0);
            det   = in;
            det_s = in_s;
            detect_in_thread(0);
            if(delay == 0) {
                free_image(zed);
                free_image(disp);
				stereo_disp = zed_args.disparity;
                disp  = det;
            }
            show_image(disp, "Demo");
            cvWaitKey(1);
        }
        --delay;
        if(delay < 0){
            delay = frame_skip;

            double after = get_wall_time();
            float curr = 1./(after - before);
            fps = curr;
            before = after;
        }
    }
}

#else // NO OPENCV
void pdemo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names, int classes, int frame_skip, char *prefix, float hier_thresh)
{
    fprintf(stderr, "Perception Demo needs Jetson VisionWorks\n");
}
void stereo_stream(int cam_index, const char *filename, int frame_skip, char *prefix)
{
	fprintf(stderr, "Stereo stream needs Jetson VisionWorks\n");
}
void test_stream(int cam_index, const char *filename, int frame_skip, char *prefix)
{
	fprintf(stderr, "Stream needs OpenCV for webcam images.\n");
}
#endif
