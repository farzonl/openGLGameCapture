#include "Intel_MediaSdk_FBOtoMediaSdk.h" //FBOtoMediaSdk.h"
#include "FBOReadBack.hpp"
#include "MsdkOpenGlEncoder.hpp"
#include <iostream>
#include "ffmpeg_container.hpp"

using std::cout;
using std::endl;
using namespace userAudio;

static int                g_w,g_h;
static inAudioCapture    *g_iac_mic           = nullptr;
static FFMPEG_Muxer      *g_muxer             = nullptr;
static MsdkOpenGLEncoder *g_msdkOGLEncoder    = nullptr; 
static MsdkOpenGLEncoder *g_msdkOGLEncoder_sw = nullptr; 
static MsdkOpenGLEncoder *g_msdkOGLEncoder_hw = nullptr; 
static FBOReadBack       *g_frb               = nullptr;
GLubyte                  *image               = nullptr;
//FileHandler              g_fhandle("buffer.264");
FileHandler              g_fhandle;

JNIEXPORT void JNICALL Java_Intel_MediaSdk_FBOtoMediaSdk_FBOInit
  (JNIEnv *, jobject, jint width, jint height)
{
	cout << "fbo init with width: " << width <<" and height: " << height <<endl;
	int error = glewInit();
	cout << " cout error code is: " << error << endl;
	g_frb = new FBOReadBack(width,height);
}

JNIEXPORT void JNICALL Java_Intel_MediaSdk_FBOtoMediaSdk_MsdkInit
  (JNIEnv *, jobject, jint width, jint height)
{
	g_msdkOGLEncoder = new MsdkOpenGLEncoder(g_fhandle,width,height);
	g_iac_mic        = new inAudioCapture();
	g_w = width;
	g_h = height;
	g_muxer          = new FFMPEG_Muxer(*g_iac_mic,g_msdkOGLEncoder);
	cout << "msdk init with width: " << width <<" and height: " << height <<endl;
}

JNIEXPORT void JNICALL Java_Intel_MediaSdk_FBOtoMediaSdk_FBOsetup
  (JNIEnv *, jobject)
{
	g_frb->fboSetup();
}

JNIEXPORT void JNICALL Java_Intel_MediaSdk_FBOtoMediaSdk_cleanUp
  (JNIEnv *, jobject)
{
	delete g_msdkOGLEncoder;
	g_msdkOGLEncoder = NULL;
	delete g_frb;
	delete g_iac_mic;
	delete g_muxer;
	if(g_msdkOGLEncoder_sw) delete g_msdkOGLEncoder_sw;
}

JNIEXPORT void JNICALL Java_Intel_MediaSdk_FBOtoMediaSdk_processFrame
  (JNIEnv *, jobject)
{
	bool b_hw = g_msdkOGLEncoder->isHW();
	image = g_frb->capture();
	g_msdkOGLEncoder->processFrame(image);
	g_muxer->test2();
}

JNIEXPORT void JNICALL Java_Intel_MediaSdk_FBOtoMediaSdk_hw_1swSwap
  (JNIEnv *, jobject)
{
	if(g_msdkOGLEncoder->isHW())
	{
		g_msdkOGLEncoder_hw = g_msdkOGLEncoder;
		if(!g_msdkOGLEncoder_sw) g_msdkOGLEncoder_sw = new  MsdkOpenGLEncoder(g_fhandle,g_w,g_h,MFX_IMPL_SOFTWARE);
		g_msdkOGLEncoder = g_msdkOGLEncoder_sw;
	}
	else
	{
		if(g_msdkOGLEncoder_hw)
		{
			g_msdkOGLEncoder = g_msdkOGLEncoder_hw;
		}
	}
}
