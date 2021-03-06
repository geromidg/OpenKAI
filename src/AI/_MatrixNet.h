/*
 * _MatrixNet.h
 *
 *  Created on: Aug 17, 2016
 *      Author: yankai
 */

#ifndef AI__MatrixNet_H_
#define AI__MatrixNet_H_

#include "../Base/common.h"
#include "_AIbase.h"
#include "_ImageNet.h"

namespace kai
{

class _MatrixNet: public _AIbase
{
public:
	_MatrixNet();
	~_MatrixNet();

	bool init(void* pKiss);
	bool link(void);
	bool draw(void);

	OBJECT* get(int i, int64_t minFrameID);
	bool bFound(int iClass, double minProb, int64_t minFrameID);

	void bSetActive(bool bActive);

public:
	_ImageNet* m_pIN;

	double m_w;
	double m_h;
	double m_dW;
	double m_dH;
	vDouble4 m_area;
	OBJECT** m_ppObj;

};

}

#endif
