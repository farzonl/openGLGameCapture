#include "MsdkOpenGLEncoder.hpp"


mfxStatus MsdkOpenGLEncoder::msdkPipelineInit(HWND hWnd, mfxU16 videoWidth, mfxU16 videoHeight)
{
	mfxStatus sts = msdkSessionInit();
		
	// Create DirectX 9 device context
	mfxHDL deviceHandle;
	sts = CreateHWDevice(*pmfxSession, &deviceHandle, hWnd);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 
		
	// Provide device manager to Media SDK
	sts = pmfxSession->SetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, deviceHandle);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 
	
	sts = msdkAllocatorInit();
	sts = mediaSdkEndPipline(videoWidth,videoHeight);

	return sts;
}


mfxStatus MsdkOpenGLEncoder::processFrame(GLubyte* buffData)
{
	

	const size_t bytesPerPixel = 4;	// RGBA
	int nEncSurfIdx = 0;
	int nVPPSurfIdx = 0;
	mfxSyncPoint syncpVPP, syncpEnc;
		
	mfxStatus sts = processFrameStart(&nVPPSurfIdx);

	#ifdef BACKBUFFER_COPY_METHOD_FAST_COPY
	sts = mfxAllocator.Lock(mfxAllocator.pthis, pmfxSurfacesVPPIn[nVPPSurfIdx]->Data.MemId, &(pmfxSurfacesVPPIn[nVPPSurfIdx]->Data));
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	CopyFrame(buffData,
            pmfxSurfacesVPPIn[nVPPSurfIdx]->Data.B,
            pmfxSurfacesVPPIn[nVPPSurfIdx]->Info.CropW*bytesPerPixel,
            pmfxSurfacesVPPIn[nVPPSurfIdx]->Info.CropH,
            pmfxSurfacesVPPIn[nVPPSurfIdx]->Data.Pitch);
		

	sts = mfxAllocator.Unlock(mfxAllocator.pthis, pmfxSurfacesVPPIn[nVPPSurfIdx]->Data.MemId, &(pmfxSurfacesVPPIn[nVPPSurfIdx]->Data));
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    #endif
	#ifdef BENCHMARK_COPY
	QueryPerformanceCounter(&tEnd);
	duration = ((double)tEnd.QuadPart - (double)tStart.QuadPart)  / freq;
	durationTotal += duration;
	durationAvg = durationTotal/(++countFrame);
	//printf("D: %3.4f\n", duration);
	#endif

	processFrameEnd(nVPPSurfIdx,nEncSurfIdx,&syncpVPP,&syncpEnc);


}