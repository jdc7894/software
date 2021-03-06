#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include <cv.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <cmath>
#include <vector>
#include <iostream>

using namespace std;
using cv::Mat;

ros::Subscriber sub_input;

void cb_image (const sensor_msgs::Image::ConstPtr msg_image) 
{
	cout << endl << "Height:  " << msg_image->height << "   Width:  " << msg_image->width << endl;
	
	Mat imageCV(640, 480, 1);

//	CvMat* imageCV = cvCreateMat(msg_image->height, msg_image->width, CV_8U);
	for (int i = 0; i < msg_image->height; i++)
	{
		for (int j = 0; j < msg_image->width; j++)
		{
			//imageCV.at<float>(i,j) = msg_image->data[msg_image->width*i + j];
		}
	}

	//cv::imshow("Image", imageCV);
	//cv::waitKey(1);

}

int main (int argc, char** argv) {
	ros::init(argc, argv, "opencv_display");
	ros::NodeHandle nh;
	sub_input = nh.subscribe("/camera/image_raw", 10, cb_image);

	ros::Rate loop_rate(1);
	while(ros::ok()) {
    	ros::spinOnce();
		loop_rate.sleep();
	}
}
