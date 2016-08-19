/*
 * _SSD.h
 *
 *  Created on: Aug 17, 2015
 *      Author: yankai
 */

#ifndef AI__SSD_H_
#define AI__SSD_H_

#include <cuda_runtime.h>
#include <caffe/caffe.hpp>
#include <caffe/blob.hpp>
#include <caffe/common.hpp>
#include <caffe/util/io.hpp>
#include <caffe/proto/caffe.pb.h>

#include <algorithm>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../Base/common.h"
#include "../Base/cvplatform.h"
#include "../Base/_ThreadBase.h"
#include "../Navigation/_Universe.h"
#include "../Camera/_Stream.h"


namespace kai
{

using namespace caffe;
using caffe::Blob;
using caffe::Caffe;
using caffe::Net;
using caffe::shared_ptr;
using caffe::vector;
using std::string;
using namespace std;
using namespace cv;

class _SSD: public _ThreadBase
{
public:
	_SSD();
	~_SSD();

	bool init(JSON* pJson, string ssdName);
	void setup(const string& model_file, const string& trained_file,
			const string& mean_file, const string& label_file);

	std::vector<vector<float> > detect(const cv::Mat img);
	bool start(void);

private:
	void detectFrame(void);
	void SetMean(const string& mean_file);
	void WrapInputLayer(std::vector<cv::Mat>* input_channels);
	void Preprocess(const cv::Mat& img, std::vector<cv::Mat>* input_channels);

	void update(void);
	static void* getUpdateThread(void* This) {
		((_SSD*) This)->update();
		return NULL;
	}


private:
	uint64_t m_frameID;

	shared_ptr<Net<float> > net_;
	cv::Size input_geometry_;
	int num_channels_;
	cv::Mat mean_;

	vector<string> labels_;

public:
	_Universe* m_pClassifier;
	_Stream*	m_pCamStream;

};

}

#endif