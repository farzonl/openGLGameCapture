/*
 * ScreenCapture.hpp
 *
 *  Created on: JUNE 11, 2013
 *      Author: farzon
 */
 
#ifndef _MSDK_OGL_ENCODER_HPP_
#define _MSDK_OGL_ENCODER_HPP_

#define BACKBUFFER_COPY_METHOD_FAST_COPY      // FAST!
#include "MsdkEncoder.hpp"
#include "common_directx.h"
#include <gl\GL.h>

class MsdkOpenGLEncoder : public MsdkEncoder {
public:
    MsdkOpenGLEncoder(FileHandler &fh, mfxU16 videoWidth, mfxU16 videoHeight,
		mfxIMPL impl = MFX_IMPL_AUTO_ANY) : MsdkEncoder(impl,fh)
	{
		msdkPipelineInit( NULL,  videoWidth,  videoHeight);
}
    virtual ~MsdkOpenGLEncoder()
	{
		CleanupHWDevice();
	}
	
	mfxStatus processFrame(GLubyte* buffData);
	
private:
	mfxStatus processFrame() {return MFX_ERR_NONE;}
	mfxStatus msdkPipelineInit(HWND hWnd, mfxU16 videoWidth, mfxU16 videoHeight);
	mfxStatus msdkAllocatorInit()
	{
		mfxStatus sts = MFX_ERR_NONE;

		mfxAllocator.Alloc	= simple_alloc;
		mfxAllocator.Free	= simple_free;
		mfxAllocator.Lock	= simple_lock;
		mfxAllocator.Unlock = simple_unlock;
		mfxAllocator.GetHDL = simple_gethdl;

		// In case of video memory we must provide Media SDK with an external allocator 
		sts = pmfxSession->SetFrameAllocator(&mfxAllocator);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		return sts;
	}
};
 
#endif /* _MSDK_OGL_ENCODER_HPP_ */