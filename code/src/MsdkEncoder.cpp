#include "MsdkEncoder.hpp"

mfxStatus MsdkEncoder::msdkSessionInit()
{
	mfxStatus sts = MFX_ERR_NONE;

	// =====================================================================
	// Media SDK VPP/encode pipeline setup
	//

	// Create output elementary stream (ES) H.264 file
	//fopen_s(&fSink, "buffer_capture.264", "wb"); 
	#if YUVON
	fopen_s(&yuv_fSink, "dectest.yuv", "wb");
	MSDK_CHECK_POINTER(yuv_fSink, MFX_ERR_NULL_PTR);
	#endif

	// Initialize Media SDK session
	// - MFX_IMPL_AUTO_ANY selects HW accelaration if available (on any adapter)
	// - Version 1.0 is selected for greatest backwards compatibility.
	//   If more recent API features are needed, change the version accordingly
	pmfxSession = std::shared_ptr<MFXVideoSession>(new MFXVideoSession());
    //pmfxSession = new MFXVideoSession();
	
	mfxVersion ver = {0, 1};
    
	sts = pmfxSession->Init(impl, &ver);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	return sts;
}

mfxStatus MsdkEncoder::mediaSdkEndPipline(mfxU16 videoWidth, mfxU16 videoHeight)
{
	mfxStatus sts = MFX_ERR_NONE;
	msdkh264EncodingInit(videoWidth,videoHeight);
	latencyConfig();
	VppInit(videoWidth,videoHeight);
	sts = queryPipline();
	sts = allocateSurfaceHeaders();
	sts = disableDefaultVPPOpp();
	sts = intializeStream();

	return sts;
}

mfxStatus MsdkEncoder::processFrameStart(int *nVPPSurfIdx)
{
	mfxBS.DataLength = 0;
	mfxStatus sts = MFX_ERR_NONE;

	*nVPPSurfIdx = GetFreeSurfaceIndex(pmfxSurfacesVPPIn, nSurfNumVPPIn); // Find free input frame surface
	if (MFX_ERR_NOT_FOUND == *nVPPSurfIdx)
		return MFX_ERR_MEMORY_ALLOC;


	#ifdef BENCHMARK_COPY
	QueryPerformanceCounter(&tStart);
	#endif

	return sts;

}

mfxStatus MsdkEncoder::processFrameEnd(int nVPPSurfIdx, int nEncSurfIdx, mfxSyncPoint *syncpVPP, mfxSyncPoint *syncpEnc)
{
	mfxStatus sts = MFX_ERR_NONE;
	nEncSurfIdx = GetFreeSurfaceIndex(pVPPSurfacesVPPOutEnc, nSurfNumVPPOutEnc); // Find free output frame surface
	if (MFX_ERR_NOT_FOUND == nEncSurfIdx)
		return MFX_ERR_MEMORY_ALLOC;
		
	// Process a frame asychronously (returns immediately)
	sts = pmfxVPP->RunFrameVPPAsync(pmfxSurfacesVPPIn[nVPPSurfIdx], pVPPSurfacesVPPOutEnc[nEncSurfIdx], NULL, syncpVPP);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	for (;;)
	{    
		// Encode a frame asychronously (returns immediately)
		sts = pmfxENC->EncodeFrameAsync(NULL, pVPPSurfacesVPPOutEnc[nEncSurfIdx], &mfxBS, syncpEnc); 
           
		if (MFX_ERR_NONE < sts && !*syncpEnc) // Repeat the call if warning and no output
		{
			if (MFX_WRN_DEVICE_BUSY == sts)                
				Sleep(1); // Wait if device is busy, then repeat the same call            
		}
		else if (MFX_ERR_NONE < sts && *syncpEnc)                 
		{
			sts = MFX_ERR_NONE; // Ignore warnings if output is available  
			break;
		}
		else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
		{
			// Allocate more bitstream buffer memory here if needed...
			break;                
		}
		else
			break;
	}  

	if(MFX_ERR_NONE == sts)
	{
		sts = pmfxSession->SyncOperation(*syncpEnc, 60000); // Synchronize. Wait until encoded frame is ready
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		#if YUVON
		sts = mfxAllocator.Lock(mfxAllocator.pthis, pVPPSurfacesVPPOutEnc[nEncSurfIdx]->Data.MemId, &(pVPPSurfacesVPPOutEnc[nEncSurfIdx]->Data));
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		sts = WriteRawFrame(pVPPSurfacesVPPOutEnc[nEncSurfIdx], yuv_fSink);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		sts = mfxAllocator.Unlock(mfxAllocator.pthis, pVPPSurfacesVPPOutEnc[nEncSurfIdx]->Data.MemId, &(pVPPSurfacesVPPOutEnc[nEncSurfIdx]->Data));
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		#endif
		#if H264ON
		sts = WriteBitStreamFrame(&mfxBS, fHandle.fSink);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		#endif
	}
 
	//
	// In this example we are ignoring draining of the encoder buffer. A real application should handle this!
	//

	return sts;
}

// Get free raw frame surface
int MsdkEncoder::GetFreeSurfaceIndex(mfxFrameSurface1** pSurfacesPool, mfxU16 nPoolSize)
{    
if (pSurfacesPool)
    for (mfxU16 i = 0; i < nPoolSize; i++)
        if (0 == pSurfacesPool[i]->Data.Locked)
            return i;
return MFX_ERR_NOT_FOUND;
}

inline void MsdkEncoder::createVPPandEncoder()
	{
		// Create Media SDK encoder & VPP
		pmfxENC = std::unique_ptr<MFXVideoENCODE>(new MFXVideoENCODE(*pmfxSession));
		pmfxVPP = std::unique_ptr<MFXVideoVPP>(new MFXVideoVPP(*pmfxSession));

		//pmfxENC = new MFXVideoENCODE(*pmfxSession);
		//pmfxVPP = new MFXVideoVPP(*pmfxSession);
	}
inline mfxStatus  MsdkEncoder::queryVPP()
{
	mfxStatus sts = MFX_ERR_NONE;
	// Query number of required surfaces for VPP
	memset(&VPPRequest, 0, sizeof(mfxFrameAllocRequest)*2);
	sts = pmfxVPP->QueryIOSurf(&VPPParams, VPPRequest);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 
	return sts;
}
inline mfxStatus  MsdkEncoder::queryEncoder()
{
	mfxStatus sts = MFX_ERR_NONE;
	// Query number of required surfaces for encoder
	memset(&EncRequest, 0, sizeof(EncRequest));
	sts = pmfxENC->QueryIOSurf(&mfxEncParams, &EncRequest);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);   
	return sts; 
}

inline void  MsdkEncoder::numSurfaceSet()
{
	// Determine the required number of surfaces for VPP input and for VPP output (encoder input)
	nSurfNumVPPIn = VPPRequest[0].NumFrameSuggested;
	nSurfNumVPPOutEnc = EncRequest.NumFrameSuggested + VPPRequest[1].NumFrameSuggested;
	EncRequest.NumFrameSuggested = nSurfNumVPPOutEnc;
}

inline void  MsdkEncoder::vppEncodeTypeSet()
{
	//TODO figure out where to include the next line for dx11
	//VPPRequest[0].Type |= WILL_WRITE; // Hint to DX memory handler that application will write data to VPP input surfaces
	EncRequest.Type |= MFX_MEMTYPE_FROM_VPPOUT; // surfaces are shared between vpp output and encode input 
}

inline mfxStatus  MsdkEncoder::encoderInit()
{
	mfxStatus sts = MFX_ERR_NONE;
	// Initialize the Media SDK encoder
	sts = pmfxENC->Init(&mfxEncParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = pmfxENC->GetVideoParam(&mfxEncParams);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	return sts;
}

inline mfxStatus  MsdkEncoder::vppInit()
{
	mfxStatus sts = MFX_ERR_NONE;
	// Initialize Media SDK VPP
	sts = pmfxVPP->Init(&VPPParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);    
	return sts;
}

inline mfxStatus  MsdkEncoder::setupBitStream()
{
	mfxStatus sts = MFX_ERR_NONE;    

	// Retrieve video parameters selected by encoder.
	// - BufferSizeInKB parameter is required to set bit stream buffer size
	mfxVideoParam par;
	memset(&par, 0, sizeof(par));
	sts = pmfxENC->GetVideoParam(&par);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 

	// Prepare Media SDK bit stream buffer
	memset(&mfxBS, 0, sizeof(mfxBS));
	mfxBS.MaxLength = par.mfx.BufferSizeInKB * 1000;
	mfxBS.Data = new mfxU8[mfxBS.MaxLength];
	MSDK_CHECK_POINTER(mfxBS.Data, MFX_ERR_MEMORY_ALLOC);

	return sts;
}

void MsdkEncoder::latencyConfig()
{
	// Configuration for low latency
	mfxEncParams.AsyncDepth		= 1;
	mfxEncParams.mfx.GopRefDist = 1;
    
	memset(&extendedCodingOptions, 0, sizeof(extendedCodingOptions));
	extendedCodingOptions.Header.BufferId		= MFX_EXTBUFF_CODING_OPTION;
	extendedCodingOptions.Header.BufferSz		= sizeof(extendedCodingOptions);
	extendedCodingOptions.MaxDecFrameBuffering	= 1;
	
	extendedBuffers[0]			= (mfxExtBuffer*)&extendedCodingOptions;
	mfxEncParams.ExtParam		= extendedBuffers;
	mfxEncParams.NumExtParam	= 1;
}

void MsdkEncoder::VppInit(mfxU16 videoWidth, mfxU16 videoHeight)
{
	// Initialize VPP parameters
	memset(&VPPParams, 0, sizeof(VPPParams));
	// Input data
	VPPParams.vpp.In.FourCC         = MFX_FOURCC_RGB4;
	VPPParams.vpp.In.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;  
	VPPParams.vpp.In.CropX          = 0;
	VPPParams.vpp.In.CropY          = 0; 
	VPPParams.vpp.In.CropW          = videoWidth;
	VPPParams.vpp.In.CropH          = videoHeight;
	VPPParams.vpp.In.PicStruct      = MFX_PICSTRUCT_PROGRESSIVE;
	VPPParams.vpp.In.FrameRateExtN  = 60;
	VPPParams.vpp.In.FrameRateExtD  = 1;
	// width must be a multiple of 16 
	// height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture  
	VPPParams.vpp.In.Width  = MSDK_ALIGN16(videoWidth);
	VPPParams.vpp.In.Height = (MFX_PICSTRUCT_PROGRESSIVE == VPPParams.vpp.In.PicStruct)?
									MSDK_ALIGN16(videoHeight) : MSDK_ALIGN32(videoHeight);
	// Output data
	VPPParams.vpp.Out.FourCC        = MFX_FOURCC_NV12;     
	VPPParams.vpp.Out.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;             
	VPPParams.vpp.Out.CropX         = 0;
	VPPParams.vpp.Out.CropY         = 0; 
	VPPParams.vpp.Out.CropW         = videoWidth;
	VPPParams.vpp.Out.CropH         = videoHeight;
	VPPParams.vpp.Out.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
	VPPParams.vpp.Out.FrameRateExtN = 60;
	VPPParams.vpp.Out.FrameRateExtD = 1;
	// width must be a multiple of 16 
	// height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture  
	VPPParams.vpp.Out.Width  = MSDK_ALIGN16(VPPParams.vpp.Out.CropW); 
	VPPParams.vpp.Out.Height = (MFX_PICSTRUCT_PROGRESSIVE == VPPParams.vpp.Out.PicStruct)?
									MSDK_ALIGN16(VPPParams.vpp.Out.CropH) : MSDK_ALIGN32(VPPParams.vpp.Out.CropH);

	VPPParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

	// Configuration for low latency
	VPPParams.AsyncDepth		= 1;
}

inline mfxStatus MsdkEncoder::queryPipline()
{
	mfxStatus sts = MFX_ERR_NONE;
	createVPPandEncoder();
	sts = queryEncoder();
	sts = queryVPP();
	vppEncodeTypeSet();
	numSurfaceSet();

	return sts;
}

mfxStatus MsdkEncoder::allocateSurfaceHeaders()
{
	mfxStatus sts = MFX_ERR_NONE;
	// Allocate required surfaces
	
	sts = mfxAllocator.Alloc(mfxAllocator.pthis, &EncRequest, &mfxResponseVPPOutEnc);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	sts = mfxAllocator.Alloc(mfxAllocator.pthis, &VPPRequest[0], &mfxResponseVPPIn);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


	// Allocate surface headers (mfxFrameSurface1) for VPPIn
	pmfxSurfacesVPPIn = new mfxFrameSurface1*[nSurfNumVPPIn];
	MSDK_CHECK_POINTER(pmfxSurfacesVPPIn, MFX_ERR_MEMORY_ALLOC);       
	for (int i = 0; i < nSurfNumVPPIn; i++)
	{
		pmfxSurfacesVPPIn[i] = new mfxFrameSurface1;
		memset(pmfxSurfacesVPPIn[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pmfxSurfacesVPPIn[i]->Info), &(VPPParams.vpp.In), sizeof(mfxFrameInfo));
		
		pmfxSurfacesVPPIn[i]->Data.MemId = mfxResponseVPPIn.mids[i]; 
		
	}  

	pVPPSurfacesVPPOutEnc = new mfxFrameSurface1*[nSurfNumVPPOutEnc];
	MSDK_CHECK_POINTER(pVPPSurfacesVPPOutEnc, MFX_ERR_MEMORY_ALLOC);       
	for (int i = 0; i < nSurfNumVPPOutEnc; i++)
	{       
		pVPPSurfacesVPPOutEnc[i] = new mfxFrameSurface1;
		memset(pVPPSurfacesVPPOutEnc[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pVPPSurfacesVPPOutEnc[i]->Info), &(VPPParams.vpp.Out), sizeof(mfxFrameInfo));
		
		pVPPSurfacesVPPOutEnc[i]->Data.MemId = mfxResponseVPPOutEnc.mids[i];
		
	}  

	return sts;
}
mfxStatus MsdkEncoder::disableDefaultVPPOpp()
{
	mfxStatus sts = MFX_ERR_NONE;

	memset(&extDoNotUse, 0, sizeof(mfxExtVPPDoNotUse));
	extDoNotUse.Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
	extDoNotUse.Header.BufferSz = sizeof(mfxExtVPPDoNotUse);
	extDoNotUse.NumAlg  = 4;
	extDoNotUse.AlgList = new mfxU32 [extDoNotUse.NumAlg];    
	MSDK_CHECK_POINTER(extDoNotUse.AlgList,  MFX_ERR_MEMORY_ALLOC);
	extDoNotUse.AlgList[0] = MFX_EXTBUFF_VPP_DENOISE; // turn off denoising (on by default)
	extDoNotUse.AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS; // turn off scene analysis (on by default)
	extDoNotUse.AlgList[2] = MFX_EXTBUFF_VPP_DETAIL; // turn off detail enhancement (on by default)
	extDoNotUse.AlgList[3] = MFX_EXTBUFF_VPP_PROCAMP; // turn off processing amplified (on by default)

	return sts;
}

mfxStatus MsdkEncoder::intializeStream()
{
	mfxStatus sts = encoderInit();
	sts =vppInit();
	sts=setupBitStream();
	return sts;
}

void MsdkEncoder::msdkh264EncodingInit(mfxU16 videoWidth, mfxU16 videoHeight)
{
	// Initialize encoder parameters
	// - In this example we are encoding an AVC (H.264) stream
	memset(&mfxEncParams, 0, sizeof(mfxEncParams));
	mfxEncParams.mfx.CodecId                    = MFX_CODEC_AVC;
	mfxEncParams.mfx.TargetUsage                = MFX_TARGETUSAGE_BALANCED;//MFX_TARGETUSAGE_BEST_SPEED;
	mfxEncParams.mfx.TargetKbps                 = 2000;
	mfxEncParams.mfx.RateControlMethod          = MFX_RATECONTROL_CBR; 
	mfxEncParams.mfx.FrameInfo.FrameRateExtN    = 60;
	mfxEncParams.mfx.FrameInfo.FrameRateExtD    = 1;
	mfxEncParams.mfx.FrameInfo.FourCC           = MFX_FOURCC_NV12;
	mfxEncParams.mfx.FrameInfo.ChromaFormat     = MFX_CHROMAFORMAT_YUV420;
	mfxEncParams.mfx.FrameInfo.PicStruct        = MFX_PICSTRUCT_PROGRESSIVE;
	mfxEncParams.mfx.FrameInfo.CropX            = 0; 
	mfxEncParams.mfx.FrameInfo.CropY            = 0;
	mfxEncParams.mfx.FrameInfo.CropW            = videoWidth;
	mfxEncParams.mfx.FrameInfo.CropH            = videoHeight;
	// Width must be a multiple of 16 
	// Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	mfxEncParams.mfx.FrameInfo.Width  = MSDK_ALIGN16(videoWidth);
	mfxEncParams.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == mfxEncParams.mfx.FrameInfo.PicStruct)?
		MSDK_ALIGN16(videoHeight) : MSDK_ALIGN32(videoHeight);
    
	mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

	mfxEncParams.mfx.GopPicSize = 100;
	//mfxEncParams.mfx.GopRefDist = 1;
}

