/*
 *  Created on: Sept 28, 2016
 *      Author: yankai
 */
#include "_DetectNet.h"

namespace kai
{
_DetectNet::_DetectNet()
{
	num_channels_ = 0;
	m_pRGBA = NULL;
	m_pRGBAf = NULL;
	m_minCofidence = 0.0;
	m_minSize = 0.0;
	m_maxSize = 1.0;
	m_overlapMin = 1.0;
	m_area.init();
	m_area.m_z = 1.0;
	m_area.m_w = 1.0;


#ifdef USE_TENSORRT
	m_pDN = NULL;
#endif
	m_nBox = 0;
	m_nBoxMax = 0;
	m_nClass = 0;

	m_bbCPU = NULL;
	m_bbCUDA = NULL;
	m_confCPU = NULL;
	m_confCUDA = NULL;

	m_className = "";
}

_DetectNet::~_DetectNet()
{
	DEL(m_pRGBA);
	DEL(m_pRGBAf);
}

bool _DetectNet::init(void* pKiss)
{
	IF_F(!this->_AIbase::init(pKiss));
	Kiss* pK = (Kiss*) pKiss;
	pK->m_pInst = this;

	F_INFO(pK->v("minConfidence", &m_minCofidence));
	F_INFO(pK->v("className", &m_className));
	F_INFO(pK->v("minSize", &m_minSize));
	F_INFO(pK->v("maxSize", &m_maxSize));
	F_INFO(pK->v("overlapMin", &m_overlapMin));

	F_INFO(pK->v("left", &m_area.m_x));
	F_INFO(pK->v("top", &m_area.m_y));
	F_INFO(pK->v("right", &m_area.m_z));
	F_INFO(pK->v("bottom", &m_area.m_w));

	m_pRGBA = new Frame();
	m_pRGBAf = new Frame();

	return true;
}

bool _DetectNet::link(void)
{
	IF_F(!this->_AIbase::link());
	Kiss* pK = (Kiss*) m_pKiss;

	return true;
}

bool _DetectNet::start(void)
{
	m_bThreadON = true;
	int retCode = pthread_create(&m_threadID, 0, getUpdateThread, this);
	if (retCode != 0)
	{
		LOG_E(retCode);
		m_bThreadON = false;
		return false;
	}

	return true;
}

void _DetectNet::update(void)
{
#ifdef USE_TENSORRT
	m_pDN = detectNet::Create(m_fileModel.c_str(), m_fileTrained.c_str(),
			m_fileMean.c_str(), m_minCofidence);
	NULL_(m_pDN);

	m_nBoxMax = m_pDN->GetMaxBoundingBoxes();
	m_nClass = m_pDN->GetNumClasses();

	IF_(	!cudaAllocMapped((void** )&m_bbCPU, (void** )&m_bbCUDA,
					m_nBoxMax * sizeof(float4)));
	IF_(	!cudaAllocMapped((void** )&m_confCPU, (void** )&m_confCUDA,
					m_nBoxMax * m_nClass * sizeof(float)));
#endif

	while (m_bThreadON)
	{
		this->autoFPSfrom();

		detect();

		this->autoFPSto();
	}

}

void _DetectNet::detect(void)
{
	NULL_(m_pStream);
#ifdef USE_TENSORRT
	NULL_(m_pDN);
#endif

	Frame* pBGR = m_pStream->bgr();
	NULL_(pBGR);
	IF_(pBGR->empty());
	IF_(m_pRGBA->isNewerThan(pBGR));

	m_pRGBA->getRGBAOf(pBGR);
	GpuMat* pGMat = m_pRGBA->getGMat();
	IF_(pGMat->empty());

	GpuMat fGMat;
	pGMat->convertTo(fGMat, CV_32FC4);

	m_nBox = m_nBoxMax;

#ifdef USE_TENSORRT
	IF_(
			!m_pDN->Detect((float* )fGMat.data, fGMat.cols, fGMat.rows, m_bbCPU,
					&m_nBox, m_confCPU));

	LOG_I("Detected BBox: "<<m_nBox);

	int camArea = fGMat.cols * fGMat.rows;
	int minSize = camArea * m_minSize;
	int maxSize = camArea * m_maxSize;
//	int bLeft = fGMat.cols * m_area.m_x;
//	int bRight = fGMat.cols * m_area.m_z;
//	int bTop = fGMat.rows * m_area.m_y;
//	int bBottom = fGMat.rows * m_area.m_w;

	uint64_t tNow = get_time_usec();

	OBJECT obj;
	for (int n = 0; n < m_nBox; n++)
	{
		float* bb = m_bbCPU + (n * 4);
		obj.m_bbox.m_x = (int) bb[0];
		obj.m_bbox.m_y = (int) bb[1];
		obj.m_bbox.m_z = (int) bb[2];
		obj.m_bbox.m_w = (int) bb[3];
		obj.m_camSize.m_x = fGMat.cols;
		obj.m_camSize.m_y = fGMat.rows;
		obj.i2fBBox();

		obj.m_iClass = m_confCPU[n*2+1];
		obj.m_dist = 0.0;
		obj.m_prob = (double)m_confCPU[n*2];
		obj.m_name = m_className;
		obj.m_frameID = tNow;

		int oSize = obj.m_bbox.area();
		IF_CONT(oSize < minSize);
		IF_CONT(oSize > maxSize);

//		IF_CONT(obj.m_bbox.m_x < bLeft);
//		IF_CONT(obj.m_bbox.m_z > bRight);
//		IF_CONT(obj.m_bbox.m_y < bTop);
//		IF_CONT(obj.m_bbox.m_w > bBottom);

		addOrUpdate(&obj);
	}
#endif

}

void _DetectNet::addOrUpdate(OBJECT* pNewObj)
{
	NULL_(pNewObj);

	for (int i = 0; i < m_nObj; i++)
	{
		OBJECT* pObj = get(i, 0);
		IF_CONT(!pObj);
		IF_CONT(pObj->m_frameID <= 0);
		IF_CONT(overlapRatio(&pObj->m_bbox, &pNewObj->m_bbox) < m_overlapMin);

		*pObj = *pNewObj;
		return;
	}

	add(pNewObj);
}

bool _DetectNet::draw(void)
{
	IF_F(!this->_AIbase::draw());

	return true;
}

}
