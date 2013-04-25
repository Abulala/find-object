
#include <stdio.h>
#include <stdlib.h>

// OpenCV stuff
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/nonfree/features2d.hpp>
#include <opencv2/calib3d/calib3d.hpp> // for homography

void showUsage()
{
	printf(
	"\n"
	"Return similarity between two images.\n"
	"Usage :\n"
	"  ./find_object-console [option] object.png scene.png\n"
	"Options: \n"
	"   -total              return total matches (default total)\n"
	"   -inliers			return inliers percentage : inliers / (inliers + outliers)\n"
	"   -quiet				don't show messages\n");

	exit(-1);
}

enum {mTotal, mInliers};

int main(int argc, char * argv[])
{
	bool quiet = false;
	int method = mTotal; //total matches
	if(argc<3)
	{
		printf("Two images required!\n");
		showUsage();
	}
	else if(argc>3)
	{
		for(int i=1; i<argc-2; ++i)
		{
			if(std::string(argv[1]).compare("-total") == 0)
			{
				method = mTotal;
			}
			else if(std::string(argv[1]).compare("-inliers") == 0)
			{
				method = mInliers;
			}
			else if(std::string(argv[1]).compare("-quiet") == 0)
			{
				quiet = true;
			}
			else
			{
				printf("Option %s not recognized!", argv[1]);
				showUsage();
			}
		}
	}


	//Load as grayscale
	cv::Mat objectImg = cv::imread(argv[argc-2], cv::IMREAD_GRAYSCALE);
	cv::Mat sceneImg = cv::imread(argv[argc-1], cv::IMREAD_GRAYSCALE);

	int value = 0;
	if(!objectImg.empty() && !sceneImg.empty())
	{
		std::vector<cv::KeyPoint> objectKeypoints;
		std::vector<cv::KeyPoint> sceneKeypoints;
		cv::Mat objectDescriptors;
		cv::Mat sceneDescriptors;

		////////////////////////////
		// EXTRACT KEYPOINTS
		////////////////////////////
		cv::SIFT sift;
		sift.detect(objectImg, objectKeypoints);
		sift.detect(sceneImg, sceneKeypoints);

		////////////////////////////
		// EXTRACT DESCRIPTORS
		////////////////////////////
		sift.compute(objectImg, objectKeypoints, objectDescriptors);
		sift.compute(sceneImg, sceneKeypoints, sceneDescriptors);

		////////////////////////////
		// NEAREST NEIGHBOR MATCHING USING FLANN LIBRARY (included in OpenCV)
		////////////////////////////
		cv::Mat results;
		cv::Mat dists;
		std::vector<std::vector<cv::DMatch> > matches;
		int k=2; // find the 2 nearest neighbors

		// Create Flann KDTree index
		cv::flann::Index flannIndex(sceneDescriptors, cv::flann::KDTreeIndexParams(), cvflann::FLANN_DIST_EUCLIDEAN);
		results = cv::Mat(objectDescriptors.rows, k, CV_32SC1); // Results index
		dists = cv::Mat(objectDescriptors.rows, k, CV_32FC1); // Distance results are CV_32FC1

		// search (nearest neighbor)
		flannIndex.knnSearch(objectDescriptors, results, dists, k, cv::flann::SearchParams() );

		////////////////////////////
		// PROCESS NEAREST NEIGHBOR RESULTS
		////////////////////////////

		// Find correspondences by NNDR (Nearest Neighbor Distance Ratio)
		float nndrRatio = 0.6;
		std::vector<cv::Point2f> mpts_1, mpts_2; // Used for homography
		std::vector<int> indexes_1, indexes_2; // Used for homography
		std::vector<uchar> outlier_mask;  // Used for homography
		// Check if this descriptor matches with those of the objects

		for(int i=0; i<objectDescriptors.rows; ++i)
		{
			// Apply NNDR
			if(dists.at<float>(i,0) <= nndrRatio * dists.at<float>(i,1))
			{
				mpts_1.push_back(objectKeypoints.at(i).pt);
				indexes_1.push_back(i);

				mpts_2.push_back(sceneKeypoints.at(results.at<int>(i,0)).pt);
				indexes_2.push_back(results.at<int>(i,0));
			}
		}

		if(method == mInliers)
		{
			// FIND HOMOGRAPHY
			unsigned int minInliers = 8;
			if(mpts_1.size() >= minInliers)
			{
				cv::Mat H = findHomography(mpts_1,
						mpts_2,
						cv::RANSAC,
						1.0,
						outlier_mask);
				int inliers=0, outliers=0;
				for(unsigned int k=0; k<mpts_1.size();++k)
				{
					if(outlier_mask.at(k))
					{
						++inliers;
					}
					else
					{
						++outliers;
					}
				}
				if(!quiet)
					printf("Total=%d Inliers=%d Outliers=%d\n", (int)mpts_1.size(), inliers, outliers);
				value = (inliers*100) / (inliers+outliers);
			}
		}
		else
		{
			value = mpts_1.size();
		}
	}
	else
	{
		printf("Images are not valid!\n");
		showUsage();
	}
	if(!quiet)
		printf("Similarity = %d\n", value);
	return value;
}