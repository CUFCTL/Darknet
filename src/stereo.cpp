using namespace std;

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <memory>

#include <NVX/nvx.h>
#include <NVX/nvx_timer.hpp>

#include <NVXIO/Application.hpp>
#include <NVXIO/ConfigParser.hpp>
#include <NVXIO/FrameSource.hpp>
#include <NVXIO/Render.hpp>
#include <NVXIO/SyncTimer.hpp>
#include <NVXIO/Utility.hpp>

#include <NVX/nvx_opencv_interop.hpp>
#include <opencv2/opencv.hpp>

#include "stereo_matching.hpp"

extern "C" {
    void compute_stereo(char *leftImg, char *rightImg, CvMat *out);
	void compute_stereo_mat(CvMat *leftImg, CvMat *rightImg, CvMat *out);
}

static bool read(const std::string &nf, StereoMatching::StereoMatchingParams &config, std::string &message)
{
    std::unique_ptr<nvxio::ConfigParser> parser(nvxio::createConfigParser());
    parser->addParameter("min_disparity",
                         nvxio::OptionHandler::integer(
                             &config.min_disparity,
                             nvxio::ranges::atLeast(0) & nvxio::ranges::atMost(256)));
    parser->addParameter("max_disparity",
                         nvxio::OptionHandler::integer(
                             &config.max_disparity,
                             nvxio::ranges::atLeast(0) & nvxio::ranges::atMost(256)));
    parser->addParameter("P1",
                         nvxio::OptionHandler::integer(
                             &config.P1,
                             nvxio::ranges::atLeast(0) & nvxio::ranges::atMost(256)));
    parser->addParameter("P2",
                         nvxio::OptionHandler::integer(
                             &config.P2,
                             nvxio::ranges::atLeast(0) & nvxio::ranges::atMost(256)));
    parser->addParameter("sad",
                         nvxio::OptionHandler::integer(
                             &config.sad,
                             nvxio::ranges::atLeast(0) & nvxio::ranges::atMost(31)));
    parser->addParameter("bt_clip_value",
                         nvxio::OptionHandler::integer(
                             &config.bt_clip_value,
                             nvxio::ranges::atLeast(15) & nvxio::ranges::atMost(95)));
    parser->addParameter("max_diff",
                         nvxio::OptionHandler::integer(
                             &config.max_diff));
    parser->addParameter("uniqueness_ratio",
                         nvxio::OptionHandler::integer(
                             &config.uniqueness_ratio,
                             nvxio::ranges::atLeast(0) & nvxio::ranges::atMost(100)));
    parser->addParameter("scanlines_mask",
                         nvxio::OptionHandler::integer(
                             &config.scanlines_mask,
                             nvxio::ranges::atLeast(0) & nvxio::ranges::atMost(256)));
    parser->addParameter("flags",
                         nvxio::OptionHandler::integer(
                             &config.flags,
                             nvxio::ranges::atLeast(0) & nvxio::ranges::atMost(3)));
    parser->addParameter("ct_win_size",
                         nvxio::OptionHandler::integer(
                             &config.ct_win_size,
                             nvxio::ranges::atLeast(0) & nvxio::ranges::atMost(5)));
    parser->addParameter("hc_win_size",
                         nvxio::OptionHandler::integer(
                             &config.hc_win_size,
                             nvxio::ranges::atLeast(0) & nvxio::ranges::atMost(5)));

    message = parser->parse(nf);

    return message.empty();
}

void compute_stereo(char *leftImg, char *rightImg, CvMat *out)
{
    // create application
    nvxio::Application &app = nvxio::Application::get();

    // stereo configuration
    std::string configFile = "/home/ubuntu/Software/jtetrea-darknet/data/zed_config.ini";
    StereoMatching::StereoMatchingParams params;
    StereoMatching::ImplementationType implementationType = StereoMatching::HIGH_LEVEL_API;

    // initialize application
    app.setDescription("This demo demonstrates Stereo Matching algorithm");
    app.addOption('c', "config", "Config file path", nvxio::OptionHandler::string(&configFile));
    app.addOption('t', "type", "Implementation type",
                  nvxio::OptionHandler::oneOf(&implementationType,
                                              {
                                                  {"hl", StereoMatching::HIGH_LEVEL_API},
                                                  {"ll", StereoMatching::LOW_LEVEL_API},
                                                  {"pyr", StereoMatching::LOW_LEVEL_API_PYRAMIDAL}
                                              }));
    int argc = 0;
    char *argv[1];
    app.init(argc, argv);

    // read configuration file
    // printf("Read parameters...\n");
    std::string error;
    if (!read(configFile, params, error))
    {
        std::cerr << error;
        return;
    }

    // create OpenVX context and message handler
    nvxio::ContextGuard context;
    vxDirective(context, VX_DIRECTIVE_ENABLE_PERFORMANCE);
    vxRegisterLogCallback(context, &nvxio::stdoutLogCallback, vx_false_e);
    
    // load and resize input images
    // printf("Loading left/right images...\n");
    std::string sourceLeft = leftImg;
    std::string sourceRight = rightImg;
    vx_image left_in = nvxio::loadImageFromFile(context, sourceLeft, VX_DF_IMAGE_RGB);
    vx_image left = vxCreateImage(context, 1248, 384, VX_DF_IMAGE_RGB);
    vxuScaleImage(context, left_in, left, VX_INTERPOLATION_TYPE_AREA);
    NVXIO_CHECK_REFERENCE(left);
    vx_image right_in = nvxio::loadImageFromFile(context, sourceRight, VX_DF_IMAGE_RGB);
    vx_image right = vxCreateImage(context, 1248, 384, VX_DF_IMAGE_RGB);
    vxuScaleImage(context, right_in, right, VX_INTERPOLATION_TYPE_AREA);
    NVXIO_CHECK_REFERENCE(right);

    // query image
    // printf("Querying image...\n");
    vx_uint32 frameWidth, frameHeight;
    vxQueryImage(left, VX_IMAGE_WIDTH, &frameWidth, sizeof(frameWidth));
    vxQueryImage(right, VX_IMAGE_HEIGHT, &frameHeight, sizeof(frameHeight));
    // printf("frameWidth: %d, frameHeight: %d\n", frameWidth, frameHeight);

    // create disparity image
    // printf("Creating disparity image...\n");
    vx_image disparity = vxCreateImage(context, 1248, 384, VX_DF_IMAGE_U8);
    NVXIO_CHECK_REFERENCE(disparity);

    // build stereo engine
    // printf("Building stereo engine...\n");
    std::unique_ptr<StereoMatching> stereo(
            StereoMatching::createStereoMatching(
                context, params,
                implementationType,
                left, right, disparity));

    // compute stereo image
    // printf("Computing stereo image...\n");
    nvx::Timer procTimer;
    procTimer.tic();

    stereo->run();

    double proc_ms = procTimer.toc();
    printf("Stereo compute time: %.2fms\n", proc_ms);

	// copy over data
    vx_uint32 plane_index = 0;
    vx_rectangle_t rect = {
        0u, 0u,
        1248u, 384u
    };
    nvx_cv::VXImageToCVMatMapper map(disparity, plane_index, &rect, VX_READ_AND_WRITE, VX_MEMORY_TYPE_HOST);
    cv::Mat cv_img = map.getMat();
	CvMat temp = cv_img;
	cvCopy(&temp, out);
	printf("Data copied...\n");

	// free VX objects
	vxReleaseImage(&left_in);
    vxReleaseImage(&left);
	vxReleaseImage(&right_in);
    vxReleaseImage(&right);
    vxReleaseImage(&disparity);

	return;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void compute_stereo_mat(CvMat *leftImg, CvMat *rightImg, CvMat *out)
{
    // create application
    nvxio::Application &app = nvxio::Application::get();

    // stereo configuration
    std::string configFile = "/usr/share/visionworks/sources/data/stereo_matching_demo_config.ini";
    StereoMatching::StereoMatchingParams params;
    StereoMatching::ImplementationType implementationType = StereoMatching::LOW_LEVEL_API;

    // initialize application
    app.setDescription("This demo demonstrates Stereo Matching algorithm");
    app.addOption('c', "config", "Config file path", nvxio::OptionHandler::string(&configFile));
    app.addOption('t', "type", "Implementation type",
                  nvxio::OptionHandler::oneOf(&implementationType,
                                              {
                                                  {"hl", StereoMatching::HIGH_LEVEL_API},
                                                  {"ll", StereoMatching::LOW_LEVEL_API},
                                                  {"pyr", StereoMatching::LOW_LEVEL_API_PYRAMIDAL}
                                              }));
    int argc = 0;
    char *argv[1];
    app.init(argc, argv);

    // read configuration file
    // printf("Read parameters...\n");
    std::string error;
    if (!read(configFile, params, error))
    {
        std::cerr << error;
        return;
    }

    // create OpenVX context and message handler
    nvxio::ContextGuard context;
    vxDirective(context, VX_DIRECTIVE_ENABLE_PERFORMANCE);
    vxRegisterLogCallback(context, &nvxio::stdoutLogCallback, vx_false_e);
    
    // load and resize input images
    // printf("Loading left/right images...\n");
    vx_image left_in = nvx_cv::createVXImageFromCVMat(context, leftImg);
    vx_image left = vxCreateImage(context, 672, 376, VX_DF_IMAGE_RGB);
	// vx_image left_in_rgb = vxCreateImage(context, 1248, 384, VX_DF_IMAGE_RGB);
	// vxuConvertDepth(context, left_in, left_in_rgb, VX_DF_IMAGE_RGB, 0);
    vxuScaleImage(context, left_in, left, VX_INTERPOLATION_TYPE_AREA);
    NVXIO_CHECK_REFERENCE(left);

    vx_image right_in = nvx_cv::createVXImageFromCVMat(context, rightImg); 
    vx_image right = vxCreateImage(context, 672, 376, VX_DF_IMAGE_RGB);
    vxuScaleImage(context, right_in, right, VX_INTERPOLATION_TYPE_AREA);
    NVXIO_CHECK_REFERENCE(right);

    // query image
    // printf("Querying image...\n");
    vx_uint32 frameWidth, frameHeight;
    vxQueryImage(left, VX_IMAGE_WIDTH, &frameWidth, sizeof(frameWidth));
    vxQueryImage(right, VX_IMAGE_HEIGHT, &frameHeight, sizeof(frameHeight));
    // printf("frameWidth: %d, frameHeight: %d\n", frameWidth, frameHeight);

    // create disparity image
    // printf("Creating disparity image...\n");
    vx_image disparity = vxCreateImage(context, 672, 376, VX_DF_IMAGE_U8);
    NVXIO_CHECK_REFERENCE(disparity);

    // build stereo engine
    // printf("Building stereo engine...\n");
    std::unique_ptr<StereoMatching> stereo(
            StereoMatching::createStereoMatching(
                context, params,
                implementationType,
                left, right, disparity));

    // compute stereo image
    // printf("Computing stereo image...\n");
    nvx::Timer procTimer;
    procTimer.tic();

    stereo->run();

    double proc_ms = procTimer.toc();
    printf("Stereo compute time: %.2fms\n", proc_ms);

    // copy over data
    vx_uint32 plane_index = 0;
    vx_rectangle_t rect = {
        0u, 0u,
        672u, 376u
    };
	
    nvx_cv::VXImageToCVMatMapper map(disparity, plane_index, &rect, VX_READ_AND_WRITE, VX_MEMORY_TYPE_HOST);
    cv::Mat cv_img = map.getMat();
	CvMat temp = cv_img;
	cvCopy(&temp, out);
	// imwrite("disparity_stereo.png", cv_img);

	// free VX objects
	vxReleaseImage(&left_in);
    vxReleaseImage(&left);
	vxReleaseImage(&right_in);
    vxReleaseImage(&right);
    vxReleaseImage(&disparity);
	return;
}
