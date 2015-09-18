
#ifndef _HW_CPY_HPP_
#define _HW_CPY_HPP_

// CPU assisted Fast copy
// 
#include <emmintrin.h>
#include <smmintrin.h>
typedef		unsigned int		UINT;
#define		CACHED_BUFFER_SIZE	4096
extern __m128i cache[];

// See white paper at: http://software.intel.com/en-us/articles/copying-accelerated-video-decode-frame-buffers
void CopyFrame( void * pSrc, void * pDest, UINT width, UINT height, UINT pitch );

// See white paper at: http://software.intel.com/en-us/articles/increasing-memory-throughput-with-intel-streaming-simd-extensions-4-intel-sse4-streaming-load/
int CopyMemSSE4(int* piDst, int* piSrc, unsigned long SizeInBytes);

#endif// _HW_CPY_HPP_

