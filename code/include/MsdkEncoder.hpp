/*
 * ScreenCapture.hpp
 *
 *  Created on: JUNE 11, 2013
 *      Author: farzon
 */
 
#ifndef _MSDK_ENCODER_HPP_
#define _MSDK_ENCODER_HPP_

#include "common_utils.h"
#include "HW_Cpy.hpp"
#include <memory>
#include <string>

//#define BENCHMARK_COPY
#define OGL
#ifdef BENCHMARK_COPY
LARGE_INTEGER	tStart, tEnd;
double			freq;
double			duration;
double			durationTotal;
double			durationAvg;
int				countFrame;
#endif

#define YUVON  0
#define H264ON 0

class FileHandler
{
	std::string fname;
	FILE*		fSink;
	public:
		FileHandler(std::string fileName="buffer_capture.264") : fname(fileName)
		{
			// Create output elementary stream (ES) H.264 file
			init();
		}
		~FileHandler()
		{
			fclose(fSink);
		}
		mfxStatus init() 
		{
			fopen_s(&fSink, fname.c_str(), "wb");
			MSDK_CHECK_POINTER(fSink, MFX_ERR_NULL_PTR);
			return MFX_ERR_NONE;
		}

		friend class MsdkEncoder;
};

class MsdkEncoder {

protected:
	//TODO replace all pointers with smart pointers
	//FILE*					                fSink;
	FileHandler                            fHandle;
	std::shared_ptr <MFXVideoSession>		pmfxSession;
	std::unique_ptr <MFXVideoENCODE>		pmfxENC;
	std::unique_ptr <MFXVideoVPP>			pmfxVPP;
	mfxFrameAllocator		                mfxAllocator;
	mfxFrameAllocRequest	                EncRequest;
	mfxFrameAllocRequest	                VPPRequest[2];// [0] - in, [1] - out
	mfxVideoParam			                mfxEncParams;
	mfxVideoParam			                VPPParams;
	mfxExtCodingOption		                extendedCodingOptions;
	mfxFrameAllocResponse	                mfxResponseVPPIn;
	mfxFrameAllocResponse	                mfxResponseVPPOutEnc;
	mfxFrameSurface1**						pmfxSurfacesVPPIn;
	mfxFrameSurface1**						pVPPSurfacesVPPOutEnc;
	//TODO figure out where surf nums get set: Ans: numSurfaceSet()
	mfxU16					                nSurfNumVPPIn;
	mfxU16					                nSurfNumVPPOutEnc;
	mfxBitstream			                mfxBS; 
	mfxExtVPPDoNotUse		                extDoNotUse;
	mfxExtBuffer*                           extendedBuffers[1];
	mfxIMPL                                 impl;
	#if YUVON
	FILE*                                   yuv_fSink;
	#endif

public:
	MsdkEncoder(mfxIMPL impl,FileHandler &fh) : impl(impl),fHandle(fh) 
	{}
    virtual ~MsdkEncoder()
	{
		for (int i = 0; i < nSurfNumVPPIn; i++)
			delete pmfxSurfacesVPPIn[i];

		MSDK_SAFE_DELETE_ARRAY(pmfxSurfacesVPPIn);

		for (int i = 0; i < nSurfNumVPPOutEnc; i++)
			delete pVPPSurfacesVPPOutEnc[i];

		MSDK_SAFE_DELETE_ARRAY(pVPPSurfacesVPPOutEnc);
		
		MSDK_SAFE_DELETE_ARRAY(mfxBS.Data);
		MSDK_SAFE_DELETE_ARRAY(extDoNotUse.AlgList);

		#if YUVON
		fclose(yuv_fSink);
		#endif
		
	}

	bool isHW()
	{
		mfxIMPL impl;
		pmfxSession->QueryIMPL(&impl);
		if(MFX_IMPL_HARDWARE & impl) return true;
		return false;
	}

	mfxStatus mediaSdkEndPipline(mfxU16 videoWidth, mfxU16 videoHeight);
	mfxStatus processFrameStart(int *nVPPSurfIdx);
	mfxStatus processFrameEnd(int nVPPSurfIdx, int nEncSurfIdx, mfxSyncPoint *syncpVPP, mfxSyncPoint *syncpEnc);
	mfxVideoParam const& getVPPParams() const    {return VPPParams;}   //  Getter
	mfxBitstream  const& getBitStream() const     {return mfxBS;}   //  Getter
	virtual mfxStatus processFrame() = 0;
private:
	// Get free raw frame surface
	int GetFreeSurfaceIndex(mfxFrameSurface1** pSurfacesPool, mfxU16 nPoolSize);
	inline void createVPPandEncoder();
	inline mfxStatus queryVPP();
	inline mfxStatus queryEncoder();
	inline void numSurfaceSet();
	inline void vppEncodeTypeSet();
	inline mfxStatus encoderInit();
	inline mfxStatus vppInit();
	inline mfxStatus setupBitStream();


protected:
	mfxStatus msdkSessionInit();
	void msdkh264EncodingInit(mfxU16 videoWidth, mfxU16 videoHeight);
	void latencyConfig();
	void VppInit(mfxU16 videoWidth, mfxU16 videoHeight);
	inline mfxStatus queryPipline();
	mfxStatus allocateSurfaceHeaders();
	mfxStatus disableDefaultVPPOpp();
	mfxStatus intializeStream();

	#ifdef BENCHMARK_COPY
	void benchMarkInit()
	{
		QueryPerformanceFrequency(&tStart);
		freq = (double)tStart.QuadPart;
		durationTotal = 0.0;
		countFrame = 0;
	}
	#endif
};
 
#endif /* _MSDK_ENCODER_HPP_ */