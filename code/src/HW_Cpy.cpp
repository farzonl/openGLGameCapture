#include "HW_Cpy.hpp"
#include <cstring>
__m128i     cache[CACHED_BUFFER_SIZE/16];

void CopyFrame( void * pSrc, void * pDest, UINT width, UINT height, UINT pitch )
{
	__m128i		x0, x1, x2, x3;
	__m128i		*pLoad;
	__m128i		*pStore;
	__m128i		*pCache;
	UINT		x, y, yLoad, yStore;
	UINT		rowsPerBlock;
	UINT		width64;
	UINT		extraPitch;

	rowsPerBlock = CACHED_BUFFER_SIZE / pitch;
	if(!rowsPerBlock) rowsPerBlock = (CACHED_BUFFER_SIZE*2) / pitch;
	width64 = (width + 63) & ~0x03f;
	extraPitch = (pitch - width64) / 16;

	pLoad  = (__m128i *)pSrc;
	pStore = (__m128i *)pDest;

	//  COPY THROUGH 4KB CACHED BUFFER
	for( y = 0; y < height; y += rowsPerBlock  )
	{
		//  ROWS LEFT TO COPY AT END
		if( y + rowsPerBlock > height )
			rowsPerBlock = height - y;

		pCache = (__m128i *)cache;

		_mm_mfence();				
        
		// LOAD ROWS OF PITCH WIDTH INTO CACHED BLOCK
		for( yLoad = 0; yLoad < rowsPerBlock; yLoad++ )
		{
			// COPY A ROW, CACHE LINE AT A TIME
			for( x = 0; x < pitch; x +=64 )
			{
				x0 = _mm_stream_load_si128( pLoad +0 );
				x1 = _mm_stream_load_si128( pLoad +1 );
				x2 = _mm_stream_load_si128( pLoad +2 );
				x3 = _mm_stream_load_si128( pLoad +3 );

				_mm_store_si128( pCache +0,	x0 );
				_mm_store_si128( pCache +1, x1 );
				_mm_store_si128( pCache +2, x2 );
				_mm_store_si128( pCache +3, x3 );

				pCache += 4;
				pLoad += 4;
			}
		}

		_mm_mfence();

		pCache = (__m128i *)cache;

		// STORE ROWS OF FRAME WIDTH FROM CACHED BLOCK
		for( yStore = 0; yStore < rowsPerBlock; yStore++ )
		{
			// copy a row, cache line at a time
			for( x = 0; x < width64; x +=64 )
			{
				x0 = _mm_load_si128( pCache );
				x1 = _mm_load_si128( pCache +1 );
				x2 = _mm_load_si128( pCache +2 );
				x3 = _mm_load_si128( pCache +3 );

				_mm_stream_si128( pStore,	x0 );
				_mm_stream_si128( pStore +1, x1 );
				_mm_stream_si128( pStore +2, x2 );
				_mm_stream_si128( pStore +3, x3 );

				pCache += 4;
				pStore += 4;
			}

			pCache += extraPitch;
			pStore += extraPitch;
		}
	}
}

//#include <intrin.h> need to eddit this to not use asm

int CopyMemSSE4(int* pDst, int* pSrc, unsigned long SizeInBytes)
{

// Initialize pointers to start of the USWC memory
	#ifdef _M_IX86
	_asm
	{
		mov esi, pSrc
		mov edx, pSrc
 
		// Initialize pointer to end of the USWC memory
		add edx, SizeInBytes
 
		// Initialize pointer to start of the cacheable WB buffer
		mov edi, pDst
 
		// Start of Bulk Load loop
		inner_start:
		
		// Load data from USWC Memory using Streaming Load
		MOVNTDQA xmm0, xmmword ptr [esi]
		MOVNTDQA xmm1, xmmword ptr [esi+16]
		MOVNTDQA xmm2, xmmword ptr [esi+32]
		MOVNTDQA xmm3, xmmword ptr [esi+48]
 
		// Copy data to buffer
		MOVDQA xmmword ptr [edi], xmm0
		MOVDQA xmmword ptr [edi+16], xmm1
		MOVDQA xmmword ptr [edi+32], xmm2
		MOVDQA xmmword ptr [edi+48], xmm3
 
		// Increment pointers by cache line size and test for end of loop
		add esi, 040h
		add edi, 040h
		cmp esi, edx
		jne inner_start
	}
	#else
	char * pcSrc  = (char*)pSrc;
	char * pcDst  = (char*)pDst;
	char *dst_end = pcDst+SizeInBytes;
    while(pcDst!=dst_end)
	{
    	__m128i res = _mm_stream_load_si128((__m128i *)pcSrc);
    	*((__m128i *)pcDst)=res;
    	pcSrc+=16;
    	pcDst+=16;
    }
    #endif
	// End of Bulk Load loop
	return 0;
}