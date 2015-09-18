#include "audioCapture.hpp"


using namespace systemAudio;

HANDLE threadArgs::hStartedEvent = NULL;
HANDLE threadArgs::hStopEvent = NULL;
HANDLE threadArgs::hThread = NULL;
audioCapture *threadArgs::systemSound = NULL;
char* systemAudio::filename = "capture.wav";

HRESULT get_default_device(IMMDevice **ppMMDevice);
HRESULT open_file(char* szFileName, HMMIO *phFile);


audioCapture::audioCapture(bool bInt16, HANDLE hStartedEvent,HANDLE hStopEvent,UINT32 nFrames) :
	bInt16(bInt16),
	hStartedEvent(hStartedEvent),
	hStopEvent(hStopEvent),
	nFrames(nFrames),
	pMMDevice(NULL),
	hFile(NULL),
	hWakeUp(NULL),
	hTask(NULL),
	pAudioClient(NULL),
	pAudioCaptureClient(NULL),
	pwfx(NULL),
	hnsDefaultDevicePeriod(NULL)
	{
		get_default_device(&pMMDevice);
		open_file(filename, &hFile);

		//msdk audioEncoder
		//mAudioEncoder = new msdkAudioEncoder("result_raw.aac");
		//mAudioEncoder->initializeMsdkAudioPipline();

		//audio recorder
		mic = new userAudio::inAudioCapture();
	}

audioCapture::~audioCapture()
{
	closehFile();
	//delete mAudioEncoder;
	delete mic;
}

DWORD WINAPI systemAudio::LoopbackCaptureThreadFunction(LPVOID pContext) 
{
    //audioCapture *pArgs = (audioCapture*)pContext;

    threadArgs::systemSound->hr = CoInitialize(NULL);
    if (FAILED(threadArgs::systemSound->hr))
	{
        //printf("CoInitialize failed: hr = 0x%08x\n", pArgs->hr);
		std::cout << "CoInitialize failed: hr = " 
			<< std::hex << threadArgs::systemSound->hr  << std::endl;
        return 0;
    }

	threadArgs::systemSound->hr = threadArgs::systemSound->LoopbackCapture();

    CoUninitialize();
    return 0;
}

void systemAudio::threadcleanUp()
{
	CloseHandle(threadArgs::hThread);
    CloseHandle(threadArgs::hStopEvent);
	delete threadArgs::systemSound;
	CoUninitialize();
}

int audioCapture::closehFile()
{
	MMRESULT result = mmioClose(hFile, 0);
    hFile = NULL;
    if (MMSYSERR_NOERROR != result) 
	{
        std::cout << "mmioClose failed: MMSYSERR = " <<result << std::endl;
		return -1;
	}
	return 0;
}

int audioCapture::openhFile()
{
	MMIOINFO mi = {0};
    hFile = mmioOpen(const_cast<char*>(filename), &mi, MMIO_READWRITE);
    if (NULL == hFile) {
        printf("mmioOpen(\"%ls\", ...) failed. wErrorRet == %u\n", filename, mi.wErrorRet);
        return -1;
    }
	return 0;
}
int audioCapture::descendToWAVEChunk(MMCKINFO *ckRIFF)
{
	
    ckRIFF->ckid = MAKEFOURCC('W', 'A', 'V', 'E'); // this is right for mmioDescend
    MMRESULT result = mmioDescend(hFile, ckRIFF, NULL, MMIO_FINDRIFF);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioDescend(\"WAVE\") failed: MMSYSERR = %u\n", result);
        return -1;
    }
	return 0;
}
int audioCapture::descendToFACTChunk(MMCKINFO *ckRIFF,MMCKINFO *ckFact)
{
	
    ckFact->ckid = MAKEFOURCC('f', 'a', 'c', 't');
    MMRESULT result = mmioDescend(hFile, ckFact, ckRIFF, MMIO_FINDCHUNK);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioDescend(\"fact\") failed: MMSYSERR = %u\n", result);
        return -1;
    }
	return 0;
}
int audioCapture::writeToChunk()
{
	LONG lBytesWritten = mmioWrite(hFile,
        reinterpret_cast<PCHAR>(&nFrames),
        sizeof(nFrames)
    );
    if (lBytesWritten != sizeof(nFrames)) {
        printf("Updating the fact chunk wrote %u bytes; expected %u\n", lBytesWritten, (UINT32)sizeof(nFrames));
        return -1;
    }
	return 0;
}
int audioCapture::ascendChunk(MMCKINFO *ckFact)
{
	MMRESULT result = mmioAscend(hFile, ckFact, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioAscend(\"fact\") failed: MMSYSERR = %u\n", result);
        return -1;
    }
	return 0;
}

void audioCapture::writeToFile()
{
	MMCKINFO ckRIFF = {0};
	MMCKINFO ckFact = {0};
	int results = 0;
	results = closehFile();
	results = openhFile();
	results = descendToWAVEChunk(&ckRIFF);
	results = descendToFACTChunk(&ckRIFF,&ckFact);
	results = writeToChunk();
	results = ascendChunk(&ckFact);

}

HRESULT audioCapture::DefineWaveFormat()
{
	if (bInt16) 
	{
        // coerce int-16 wave format
        // can do this in-place since we're not changing the size of the format
        // also, the engine will auto-convert from float to int for us
        switch (pwfx->wFormatTag) 
		{
            case WAVE_FORMAT_IEEE_FLOAT:
                pwfx->wFormatTag = WAVE_FORMAT_PCM;
                pwfx->wBitsPerSample = 16;
                pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
                pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
                break;

            case WAVE_FORMAT_EXTENSIBLE:
                {
                    // naked scope for case-local variable
                    PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
                    if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat))
					{
                        pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
                        pEx->Samples.wValidBitsPerSample = 16;
                        pwfx->wBitsPerSample = 16;
                        pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
                        pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
                    }
					
					else 
					{
                        printf("Don't know how to coerce mix format to int-16\n");
                        CoTaskMemFree(pwfx);
                        pAudioClient->Release();
                        return E_UNEXPECTED;
                    }
                }
                break;

            default:
                printf("Don't know how to coerce WAVEFORMATEX with wFormatTag = 0x%08x to int-16\n", pwfx->wFormatTag);
                CoTaskMemFree(pwfx);
                pAudioClient->Release();
                return E_UNEXPECTED;
        }
    }
	return S_OK;
}

HRESULT audioCapture::captureLoop()
{
	    // loopback capture loop
    HANDLE waitArray[2] = { hStopEvent, hWakeUp };
    DWORD dwWaitResult;

    bool bDone = false;
    bool bFirstPacket = true;
    for (UINT32 nPasses = 0; !bDone; nPasses++) 
	{
		//write mic to file
		mic->writeProc();

        dwWaitResult = WaitForMultipleObjects(ARRAYSIZE(waitArray), waitArray,
            FALSE, INFINITE);

        if (WAIT_OBJECT_0 == dwWaitResult) {
            printf("Received stop event after %u passes and %u frames\n", nPasses, nFrames);
            bDone = true;
            continue; // exits loop
        }

        if (WAIT_OBJECT_0 + 1 != dwWaitResult) {
            printf("Unexpected WaitForMultipleObjects return value %u on pass %u after %u frames\n", dwWaitResult, nPasses, nFrames);
            cleanup();
            return E_UNEXPECTED;
        }

        // got a "wake up" event - see if there's data
        UINT32 nNextPacketSize;
        hr = pAudioCaptureClient->GetNextPacketSize(&nNextPacketSize);
        if (FAILED(hr)) {
            printf("IAudioCaptureClient::GetNextPacketSize failed on pass %u after %u frames: hr = 0x%08x\n", nPasses, nFrames, hr);
            cleanup();            
            return hr;
        }

        if (0 == nNextPacketSize) {
            // no data yet
            continue;
        }

        // get the captured data
        BYTE *pData;
        UINT32 nNumFramesToRead;
        DWORD dwFlags;

        hr = pAudioCaptureClient->GetBuffer(
            &pData,
            &nNumFramesToRead,
            &dwFlags,
            NULL,
            NULL
        );

		//mAudioEncoder->ProcessByte(pData,nNumFramesToRead);
		

        if (FAILED(hr)) {
            printf("IAudioCaptureClient::GetBuffer failed on pass %u after %u frames: hr = 0x%08x\n", nPasses, nFrames, hr);
            cleanup();           
            return hr;            
        }

        if (bFirstPacket && AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags) {
            printf("Probably spurious glitch reported on first packet\n");
        } else if (0 != dwFlags) {
            printf("IAudioCaptureClient::GetBuffer set flags to 0x%08x on pass %u after %u frames\n", dwFlags, nPasses, nFrames);
            //cleanup();            
            //return E_UNEXPECTED;
        }

        if (0 == nNumFramesToRead) {
            printf("IAudioCaptureClient::GetBuffer said to read 0 frames on pass %u after %u frames\n", nPasses, nFrames);
            cleanup();            
            return E_UNEXPECTED;            
        }

        LONG lBytesToWrite = nNumFramesToRead * nBlockAlign;
#pragma prefast(suppress: __WARNING_INCORRECT_ANNOTATION, "IAudioCaptureClient::GetBuffer SAL annotation implies a 1-byte buffer")
        LONG lBytesWritten = mmioWrite(hFile, reinterpret_cast<PCHAR>(pData), lBytesToWrite);
        if (lBytesToWrite != lBytesWritten) {
            printf("mmioWrite wrote %u bytes on pass %u after %u frames: expected %u bytes\n", lBytesWritten, nPasses, nFrames, lBytesToWrite);
            cleanup();           
            return E_UNEXPECTED;
        }
        nFrames += nNumFramesToRead;
        
        hr = pAudioCaptureClient->ReleaseBuffer(nNumFramesToRead);
        if (FAILED(hr)) {
            printf("IAudioCaptureClient::ReleaseBuffer failed on pass %u after %u frames: hr = 0x%08x\n", nPasses, nFrames, hr);
            cleanup();           
            return hr;            
        }
        
        bFirstPacket = false;
    } // capture loop
	return hr;
}

HRESULT audioCapture::activateAudioClient()
{
	HRESULT hr;
	// activate an IAudioClient

	hr = pMMDevice->Activate(__uuidof(IAudioClient),CLSCTX_ALL, NULL,
		(void**)&pAudioClient);
	if (FAILED(hr)) 
	{
        printf("IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
    }
	return hr;
}

HRESULT audioCapture::activateAudioCaptureClient()
{
	HRESULT hr;
	// activate an IAudioCaptureClient
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient),(void**)&pAudioCaptureClient);
    if (FAILED(hr)) 
	{
        printf("IAudioClient::GetService(IAudioCaptureClient) failed: hr 0x%08x\n", hr);
        CloseHandle(hWakeUp);
        pAudioClient->Release();
    }
	return hr;
}

void audioCapture::cleanup()
{
	pAudioClient->Stop();
    CancelWaitableTimer(hWakeUp);
    AvRevertMmThreadCharacteristics(hTask);
    pAudioCaptureClient->Release();
    CloseHandle(hWakeUp);
    pAudioClient->Release();
}

HRESULT audioCapture::definePeriod()
{
	HRESULT hr;
	// get the default device periodicity
    hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, NULL);
    if (FAILED(hr)) 
	{
        printf("IAudioClient::GetDevicePeriod failed: hr = 0x%08x\n", hr);
        pAudioClient->Release();
    }
	return hr;
}

HRESULT audioCapture::LoopbackCapture() 
{
	//TODO handle failure reuslts
    HRESULT hr = activateAudioClient();
	hr = definePeriod();

	 // get the default device format
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) 
	{
        printf("IAudioClient::GetMixFormat failed: hr = 0x%08x\n", hr);
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        return hr;
    }

	hr = DefineWaveFormat();

   
	MMCKINFO ckRIFF = {0};
    MMCKINFO ckData = {0};
    hr = WriteWaveHeader(hFile, pwfx, &ckRIFF, &ckData);
    if (FAILED(hr)) 
	{
        // WriteWaveHeader does its own logging
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        return hr;
    }
    

    // create a periodic waitable timer
    hWakeUp = CreateWaitableTimer(NULL, FALSE, NULL);
    if (NULL == hWakeUp) 
	{
        DWORD dwErr = GetLastError();
        printf("CreateWaitableTimer failed: last error = %u\n", dwErr);
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        return HRESULT_FROM_WIN32(dwErr);
    }
	nBlockAlign = pwfx->nBlockAlign;
    nFrames = 0;
    
    // call IAudioClient::Initialize
    // note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK
    // do not work together...
    // the "data ready" event never gets set
    // so we're going to do a timer-driven loop
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,
        0, 0, pwfx, 0);
    if (FAILED(hr))
	{
        printf("IAudioClient::Initialize failed: hr = 0x%08x\n", hr);
        CloseHandle(hWakeUp);
        pAudioClient->Release();
        return hr;
    }
    CoTaskMemFree(pwfx);

	hr = activateAudioCaptureClient();

	// register with MMCSS
    DWORD nTaskIndex = 0;
    hTask = AvSetMmThreadCharacteristics("Capture", &nTaskIndex);
    if (NULL == hTask) {
        DWORD dwErr = GetLastError();
        printf("AvSetMmThreadCharacteristics failed: last error = %u\n", dwErr);
        pAudioCaptureClient->Release();
        CloseHandle(hWakeUp);
        pAudioClient->Release();
        return HRESULT_FROM_WIN32(dwErr);
    }    

    // set the waitable timer
    LARGE_INTEGER liFirstFire;
    liFirstFire.QuadPart = -hnsDefaultDevicePeriod / 2; // negative means relative time
    LONG lTimeBetweenFires = (LONG)hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds
    BOOL bOK = SetWaitableTimer(
        hWakeUp,
        &liFirstFire,
        lTimeBetweenFires,
        NULL, NULL, FALSE
    );
    if (!bOK) 
	{
        DWORD dwErr = GetLastError();
        printf("SetWaitableTimer failed: last error = %u\n", dwErr);
        AvRevertMmThreadCharacteristics(hTask);
        pAudioCaptureClient->Release();
        CloseHandle(hWakeUp);
        pAudioClient->Release();
        return HRESULT_FROM_WIN32(dwErr);
    }
    
    // call IAudioClient::Start
    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        printf("IAudioClient::Start failed: hr = 0x%08x\n", hr);
        AvRevertMmThreadCharacteristics(hTask);
        pAudioCaptureClient->Release();
        CloseHandle(hWakeUp);
        pAudioClient->Release();
        return hr;
    }
    SetEvent(hStartedEvent);
	
	mic->startRecording();
	hr = captureLoop();

	if(S_OK != hr) return hr;
	hr = FinishWaveFile(hFile, &ckData, &ckRIFF);

	cleanup();
	return hr;
}

HRESULT audioCapture::FinishWaveFile(HMMIO hFile, MMCKINFO *pckRIFF, MMCKINFO *pckData) {
    MMRESULT result;

    result = mmioAscend(hFile, pckData, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioAscend(\"data\" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    result = mmioAscend(hFile, pckRIFF, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioAscend(\"RIFF/WAVE\" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    return S_OK;    
}

HRESULT get_default_device(IMMDevice **ppMMDevice) {
    HRESULT hr = S_OK;
    IMMDeviceEnumerator *pMMDeviceEnumerator;

    // activate a device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
        __uuidof(IMMDeviceEnumerator),
        (void**)&pMMDeviceEnumerator
    );
    if (FAILED(hr)) {
        printf("CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x\n", hr);
        return hr;
    }

    // get the default render endpoint
    hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, ppMMDevice);
    pMMDeviceEnumerator->Release();
    if (FAILED(hr)) {
        printf("IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x\n", hr);
        return hr;
    }

    return S_OK;
}

HRESULT open_file(char* szFileName, HMMIO *phFile) {
    MMIOINFO mi = {0};

    *phFile = mmioOpen(szFileName,&mi,MMIO_WRITE | MMIO_CREATE);

    if (NULL == *phFile) {
        printf("mmioOpen(\"%ls\", ...) failed. wErrorRet == %u\n", szFileName, mi.wErrorRet);
        return E_FAIL;
    }

    return S_OK;
}

int systemAudio::threadInitialize()
{
	HRESULT hr = S_OK;
	hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        printf("CoInitialize failed: hr = 0x%08x", hr);
        return -1;
    }

	threadArgs::hStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	threadArgs::hStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	int retType = 0;
    if (NULL == threadArgs::hStartedEvent) 
	{
        printf("CreateEvent failed: last error is %u\n", GetLastError());
		retType = -1;
	}
	if (NULL == threadArgs::hStopEvent) 
	{
        printf("CreateEvent failed: last error is %u\n", GetLastError());
		CloseHandle(threadArgs::hStartedEvent);
		retType = -1;
	}
	
	threadArgs::systemSound = new audioCapture(true, threadArgs::hStartedEvent,threadArgs::hStopEvent,0);
	
	threadArgs::hThread = CreateThread(NULL, 0,
		systemAudio::LoopbackCaptureThreadFunction, NULL,0, NULL);

	if (NULL == threadArgs::hThread) {
        printf("CreateThread failed: last error is %u\n", GetLastError());
        CloseHandle(threadArgs::hStopEvent);
        CloseHandle(threadArgs::hStartedEvent);
        retType = -1;
    }
	
	return retType;
}
//TODO FIX THIS
HRESULT audioCapture::WriteWaveHeader(HMMIO hFile, LPCWAVEFORMATEX pwfx, MMCKINFO *pckRIFF, MMCKINFO *pckData) {
    MMRESULT result;

    // make a RIFF/WAVE chunk
    pckRIFF->ckid = MAKEFOURCC('R', 'I', 'F', 'F');
    pckRIFF->fccType = MAKEFOURCC('W', 'A', 'V', 'E');

    result = mmioCreateChunk(hFile, pckRIFF, MMIO_CREATERIFF);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioCreateChunk(\"RIFF/WAVE\") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }
    
    // make a 'fmt ' chunk (within the RIFF/WAVE chunk)
    MMCKINFO chunk;
    chunk.ckid = MAKEFOURCC('f', 'm', 't', ' ');
    result = mmioCreateChunk(hFile, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioCreateChunk(\"fmt \") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // write the WAVEFORMATEX data to it
    LONG lBytesInWfx = sizeof(WAVEFORMATEX) + pwfx->cbSize;
    LONG lBytesWritten =
        mmioWrite(
            hFile,
            reinterpret_cast<PCHAR>(const_cast<LPWAVEFORMATEX>(pwfx)),
            lBytesInWfx
        );
    if (lBytesWritten != lBytesInWfx) {
        printf("mmioWrite(fmt data) wrote %u bytes; expected %u bytes\n", lBytesWritten, lBytesInWfx);
        return E_FAIL;
    }

    // ascend from the 'fmt ' chunk
    result = mmioAscend(hFile, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioAscend(\"fmt \" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }
    
    // make a 'fact' chunk whose data is (DWORD)0
    chunk.ckid = MAKEFOURCC('f', 'a', 'c', 't');
    result = mmioCreateChunk(hFile, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioCreateChunk(\"fmt \") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // write (DWORD)0 to it
    // this is cleaned up later
    DWORD frames = 0;
    lBytesWritten = mmioWrite(hFile, reinterpret_cast<PCHAR>(&frames), sizeof(frames));
    if (lBytesWritten != sizeof(frames)) {
        printf("mmioWrite(fact data) wrote %u bytes; expected %u bytes\n", lBytesWritten, (UINT32)sizeof(frames));
        return E_FAIL;
    }

    // ascend from the 'fact' chunk
    result = mmioAscend(hFile, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioAscend(\"fact\" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // make a 'data' chunk and leave the data pointer there
    pckData->ckid = MAKEFOURCC('d', 'a', 't', 'a');
    result = mmioCreateChunk(hFile, pckData, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioCreateChunk(\"data\") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    return S_OK;
}

void userAudio::inAudioCapture::waveFormatInit()
{
	pwfx.wFormatTag = WAVE_FORMAT_PCM; // simple, uncompressed format
	pwfx.nChannels = 2; // 1=mono, 2=stereo
	pwfx.nSamplesPerSec = samplingRate; // 44100
	pwfx.wBitsPerSample = 16; // 16 for high quality, 8 for telephone-grade
	pwfx.nBlockAlign = pwfx.nChannels*pwfx.wBitsPerSample/8; 
	pwfx.nAvgBytesPerSec = pwfx.nBlockAlign*pwfx.nSamplesPerSec; 
}

void userAudio::inAudioCapture::WaveInHeaderinit()
{
	audioInHead.lpData = (LPSTR)dataIn;
	audioInHead.dwBufferLength = samplingRate*timeElapsed*pwfx.nBlockAlign;
	audioInHead.dwBytesRecorded=0;
	audioInHead.dwUser = 0L;
	audioInHead.dwFlags = 0L;
	audioInHead.dwLoops = 0L;
	waveInPrepareHeader(hWaveIn, &audioInHead, sizeof(WAVEHDR));
}

void userAudio::inAudioCapture::startRecording()
{
	sts = waveInAddBuffer(hWaveIn, &audioInHead, sizeof(WAVEHDR));
	if(sts)
		std::cout << "Failed to read block from device" << std::endl;

	sts = waveInStart(hWaveIn);
	if(sts)
		std::cout << "Failed to start recording" << std::endl;
	#if WRITEWAVE
	SetupWaveFile();
	#endif
}


void userAudio::inAudioCapture::writeProc()
{
	if (waveInUnprepareHeader(hWaveIn, &audioInHead, sizeof(WAVEHDR))== WAVERR_STILLPLAYING)
	{}
	else
	{
		mmioWrite(handle, (char*)audioInHead.lpData, audioInHead.dwBytesRecorded);
		waveInReset(hWaveIn);
		waveInUnprepareHeader(hWaveIn, &audioInHead, sizeof(WAVEHDR));
		waveInPrepareHeader(hWaveIn, &audioInHead, sizeof(WAVEHDR));
		sts = waveInAddBuffer(hWaveIn, &audioInHead, sizeof(WAVEHDR));
		sts = waveInStart(hWaveIn);
		//k++;
		byteReady = true;
	}
		
}

int userAudio::inAudioCapture::writeProcCheck()
{
	if (waveInUnprepareHeader(hWaveIn, &audioInHead, sizeof(WAVEHDR))== WAVERR_STILLPLAYING)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

void userAudio::inAudioCapture::writeProcEnd()
{
	#if WRITEWAVE
	mmioWrite(handle, (char*)audioInHead.lpData, audioInHead.dwBytesRecorded);
	#endif
	waveInReset(hWaveIn);
	waveInUnprepareHeader(hWaveIn, &audioInHead, sizeof(WAVEHDR));
	waveInPrepareHeader(hWaveIn, &audioInHead, sizeof(WAVEHDR));
	sts = waveInAddBuffer(hWaveIn, &audioInHead, sizeof(WAVEHDR));
	sts = waveInStart(hWaveIn);
	//k++;
	byteReady = true;
}


void userAudio::inAudioCapture::writeLoop()
{
	while(!(GetKeyState(VK_RETURN)&0x80))
{
	if (waveInUnprepareHeader(hWaveIn, &audioInHead, sizeof(WAVEHDR))== WAVERR_STILLPLAYING)
	{continue;}
	else
	{
		mmioWrite(handle, (char*)audioInHead.lpData, audioInHead.dwBytesRecorded);
		

		waveInReset(hWaveIn);
		waveInUnprepareHeader(hWaveIn, &audioInHead, sizeof(WAVEHDR));
		waveInPrepareHeader(hWaveIn, &audioInHead, sizeof(WAVEHDR));
		sts = waveInAddBuffer(hWaveIn, &audioInHead, sizeof(WAVEHDR));
		sts = waveInStart(hWaveIn);
		//k++;
		
	}
}

}

void userAudio::inAudioCapture::SetupWaveFile()
{
memset(&ChunkInfo, 0, sizeof(MMCKINFO));
memset(&FormatChunkInfo, 0, sizeof(MMCKINFO));
memset(&DataChunkInfo, 0, sizeof(MMCKINFO));

ChunkInfo.ckid = mmioStringToFOURCC("RIFF", 0);
ChunkInfo.fccType = mmioStringToFOURCC("WAVE", 0);
DWORD Res = mmioCreateChunk(handle, &ChunkInfo, MMIO_CREATERIFF);
if(Res != 0) std::cout << "MMIO Error. Error Code: " << Res << std::endl;

FormatChunkInfo.ckid = mmioStringToFOURCC("fmt ", 0);
FormatChunkInfo.cksize = sizeof(WAVEFORMATEX);
Res = mmioCreateChunk(handle, &FormatChunkInfo, 0);
if(Res != 0) std::cout << "MMIO Error. Error Code: " << Res << std::endl;
// Write the wave format data.
LONG lBytesInWfx = sizeof(WAVEFORMATEX) + pwfx.cbSize;
mmioWrite(handle, (char*)&pwfx, sizeof(pwfx)+lBytesInWfx);

Res = mmioAscend(handle, &FormatChunkInfo, 0);
if(Res != 0) std::cout << "MMIO Error. Error Code: " << Res << std::endl;

FormatChunkInfo.ckid = mmioStringToFOURCC("fact",0);
Res = mmioCreateChunk(handle, &FormatChunkInfo, 0);
Res = mmioAscend(handle, &FormatChunkInfo, 0);

DataChunkInfo.ckid = mmioStringToFOURCC("data", 0);
DWORD DataSize = audioInHead.dwBytesRecorded;
DataChunkInfo.cksize = DataSize;
Res = mmioCreateChunk(handle, &DataChunkInfo, 0);
if(Res != 0) std::cout << "MMIO Error. Error Code: " << Res << std::endl;

//mmioWrite(handle, (char*)audioInHead.lpData, audioInHead.dwBytesRecorded);
}
void userAudio::inAudioCapture::finishWaveFile()
{
	// Ascend out of the data chunk.
	mmioAscend(handle, &DataChunkInfo, 0);
	// Ascend out of the RIFF chunk (the main chunk). Failure to do 
	// this will result in a file that is unreadable by Windows
	// Sound Recorder.
	mmioAscend(handle, &ChunkInfo, 0);
	mmioClose(handle, 0);
}