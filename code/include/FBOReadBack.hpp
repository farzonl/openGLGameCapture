#ifndef __FBO_READ_BACK_HPP__
#define __FBO_READ_BACK_HPP__

#include <GL/glew.h>
#include <string.h>
#include <iostream>
//#include "ipp.h"
//#include <ippi.h>
#include "HW_Cpy.hpp"
#include <algorithm>

#define colorScheme GL_RGBA
//#define colorScheme GL_BGRA

union Color
{
   unsigned int i;
   unsigned char rgba[4];

};

class FBOReadBack
{
	GLuint pbos[2]; // for double buffering
	//GLuint c_tex;
	GLuint fbo,colorRenderBuf,depthRenderBuf;
	int curr_pbo_read,curr_pbo_cpy;
	int width, height;
	int dataSize;
	GLubyte* m_buff_dat;
	int stepSize;
	public:
		void ReverseBytes( void *start, int size )
		{
			int *istart = (int *)start, *iend = istart + size/4;
			//std::rotate(istart,istart+size/8,iend);
			std::reverse(istart, iend);
			
			//http://software.intel.com/sites/products/documentation/hpc/ipp/ippi/ippi_ch12/functn_Mirror.html
			//
			//IppiSize srcRoi = { width*4,height };
			//ippiMirror_32s_C4R
			//ippiMirror_8u_C4R((GLubyte*)start,width*4, (GLubyte*)m_buff_dat,height, srcRoi, ippAxsBoth);
		}

		int RGBA_to_BGRA(int rgba)
		{
			Color color;
			color.i = rgba;
			unsigned char c = color.rgba[3];
			color.rgba[3]   = color.rgba[1];
			color.rgba[1]   = c;

			return color.i;
		}

		void custom_reverse_algo(void *src,void *dst)
		{
			int *pstart = (int*)src+dataSize/stepSize - width;
			int *pend   = (int*)src;
			int *pdst   = (int*)dst;

			while(true)
			{
				//memcpy(pdst,pstart,sizeof(int)*width);
				CopyMemSSE4(pdst,pstart,sizeof(int)*width);

				if(pstart == pend ) return;

				pstart -=width;
				pdst   +=width;
			}

		}
		FBOReadBack(int width,int height, int stepSize = 4) : 
			fbo(0), curr_pbo_read(0),width(width), height(height) ,stepSize(stepSize)
		{
			std::cout << "it allocated" << std::endl;

			
			dataSize = width*height* stepSize; // 4 b/c I assume we are using RGBA
			m_buff_dat = new GLubyte[dataSize];
			//memset( m_buff_dat,0, dataSize );
			pboInit(0);
			pboInit(1);
			

			glGenFramebuffers( 1, &fbo );
			glGenRenderbuffers( 1, &colorRenderBuf );
			glGenRenderbuffers( 1, &depthRenderBuf );
			
			// Initialize the color render-buffer for usage as a color buffer.
			glBindRenderbuffer( GL_RENDERBUFFER, colorRenderBuf );
			glRenderbufferStorage( GL_RENDERBUFFER, colorScheme, width, height );

			// Initialize the render-buffer for usage as a depth buffer.
			// We don't really need this to render things into the frame-buffer object,
			// but without it the geometry will not be sorted properly.
			glBindRenderbuffer( GL_RENDERBUFFER, depthRenderBuf );
			glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 
				width, height );

			//GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
		}
		~FBOReadBack()
		{
			glDeleteBuffers( 1, &pbos[0] );
			glDeleteBuffers( 1, &pbos[1] );
			glDeleteRenderbuffers( 1, &colorRenderBuf );
			glDeleteRenderbuffers( 1, &depthRenderBuf );
			glDeleteFramebuffers( 1, &fbo );
			//glDeleteTextures( 1, &c_tex );
			delete [] m_buff_dat;
		}
		void fboSetup()
		{
			glBindTexture(GL_TEXTURE_2D, 0);
			glBindFramebuffer( GL_FRAMEBUFFER, fbo );
			glBindRenderbuffer( GL_RENDERBUFFER, colorRenderBuf );
			glBindRenderbuffer( GL_RENDERBUFFER, depthRenderBuf );
			glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRenderBuf );
			glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderBuf );
			

			
			
			
		}
		GLubyte* capture()
		{
			bufferSwap();


			//step 4.0 set the read buffer to the fbo
			//glReadBuffer(fbo); //TODO see if needed

			// step 4.1 rebind the pbo for writeback
			glBindBuffer( GL_PIXEL_PACK_BUFFER, pbos[curr_pbo_read] );
			//step 4.2  initiates a dma transfer
			//glPixelStorei(GL_PACK_ALIGNMENT,4);

			glReadPixels(0, 0, width, height, colorScheme, GL_UNSIGNED_BYTE, 0);
			// While the other PBO is asynchronously reading pixels - map the other PBO and copy out its pixels
			glBindBuffer( GL_PIXEL_PACK_BUFFER, pbos[curr_pbo_cpy] );
			GLubyte* bufferData = (GLubyte*)glMapBuffer( GL_PIXEL_PACK_BUFFER, GL_READ_ONLY );

			//memcpy(m_buff_dat,bufferData,dataSize);
			//ReverseBytes(m_buff_dat,dataSize);
			
			glEnable(GL_TEXTURE_2D);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			glBindBuffer( GL_PIXEL_PACK_BUFFER,  0 );
			glBindFramebuffer( GL_FRAMEBUFFER,   0 );
			glBindRenderbuffer( GL_RENDERBUFFER, 0 );
			
			
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);  
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GL_BACK);  
			glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,  GL_COLOR_BUFFER_BIT, GL_NEAREST); 
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);  
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);  
			custom_reverse_algo(bufferData,m_buff_dat);
		
			return m_buff_dat;
		}
	private:
		void pboInit(int id)
		{
			//step 1. create a pbo
			glGenBuffers( 1, &pbos[id] );

			//step 2. bind it and define data to be stored
			glBindBuffer( GL_PIXEL_PACK_BUFFER, pbos[id] );
			glBufferData( GL_PIXEL_PACK_BUFFER, dataSize, 0, GL_STREAM_READ );
			
			glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 ); //step 3. unbind the pbo
		}

		void bufferSwap()
		{
			if (curr_pbo_read == 0)
			{
				curr_pbo_read = 1;
				curr_pbo_cpy  = 0;
			}
			else
			{
				curr_pbo_read = 0;
				curr_pbo_cpy  = 1;
			}
		}
};


#endif //__FBO_READ_BACK_HPP__