#ifndef __AUDIO_CAPTURE_HPP__
#define __AUDIO_CAPTURE_HPP__

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>    // HMMIO
#include <mmdeviceapi.h> //CoInitialize & CoUninitialize
#include <audioclient.h> //IAudioClient
#include <avrt.h>        // AvSetMmThreadCharacteristics
#include <iostream> //for io
//#include "msdkAudioEncoder.hpp" //TODO FIX THIS

#define WRITEWAVE 0

namespace userAudio
{
	
	class inAudioCapture
	{
		WAVEFORMATEX pwfx;
		WAVEHDR audioInHead;
		HWAVEIN hWaveIn; // handle to wave in device
		int timeElapsed;
		int* dataIn;
		int data_size;
		int bufferSize;
		int samplingRate;
		HMMIO handle;
		MMCKINFO ChunkInfo;
		MMCKINFO FormatChunkInfo;
		MMCKINFO DataChunkInfo;
		bool byteReady;
	    public:
			MMRESULT sts;
			inAudioCapture(WAVEFORMATEX *pwfx=NULL,int samplingRate=44100) : 
				timeElapsed(1),samplingRate(samplingRate),byteReady(false)
			{
				if(!pwfx)
				{
					waveFormatInit();
				}
				else
				{
					this->pwfx = *pwfx; 
				}
				data_size = this->samplingRate*timeElapsed;
				dataIn = new int[data_size];

				initializeBuffer();
				#if WRITEWAVE
				handle = mmioOpen("test.wav", 0, MMIO_CREATE | MMIO_WRITE);
				#endif
			}
			~inAudioCapture()
			{
				#if WRITEWAVE
				finishWaveFile();
				#endif
				waveInStop(hWaveIn);
				std::cout << "Stop Recording.\n" << std::endl;
				waveInClose(hWaveIn);
				if(!dataIn) delete [] dataIn;
				
			}
			int  writeProcCheck();
			void writeProcEnd();
			//WAVEHDR const& getVoiceData() {if(byteReady) {byteReady = false; return audioInHead;}}   //  Getter
			char* const& getVoiceData() const {return audioInHead.lpData;   }   //  Getter
			int  const getDataSize () const {return audioInHead.dwBytesRecorded;}   //  Getter
			WAVEFORMATEX const& getWAVFormat() const {return pwfx;}
			void startRecording();
			void writeProc();
		private:
			void initializeBuffer()
			{
				sts = waveInOpen(&hWaveIn, WAVE_MAPPER,&pwfx,0L, 0L, WAVE_FORMAT_DIRECT);
				if(sts)
					std::cout << "Failed to open waveform input device." << std::endl;

				WaveInHeaderinit();
			}
			void WaveInHeaderinit();
			void waveFormatInit();
			void SetupWaveFile();
			void writeLoop();
			void finishWaveFile();
	};
}

namespace systemAudio
{
	extern char* filename;
	DWORD WINAPI LoopbackCaptureThreadFunction(LPVOID pContext);
	int threadInitialize();
	void threadcleanUp();
	
	class audioCapture
	{
		
		bool bInt16;
		HANDLE hStartedEvent;
		HANDLE hStopEvent;
		UINT32 nFrames;
		HANDLE hWakeUp;
		HANDLE hTask;
		HMMIO hFile;
		IAudioClient *pAudioClient;
		IMMDevice *pMMDevice;
		IAudioCaptureClient *pAudioCaptureClient;
		WAVEFORMATEX *pwfx;
		REFERENCE_TIME hnsDefaultDevicePeriod;
		UINT32 nBlockAlign;
		//msdkAudioEncoder *mAudioEncoder;
		userAudio::inAudioCapture *mic;
		public:
			HRESULT hr;
			audioCapture( 
				bool bInt16, 
				HANDLE hStartedEvent,
				HANDLE hStopEvent,
				UINT32 nFrames);
			HRESULT LoopbackCapture();
			~audioCapture();
			void writeToFile();// FIXME
		private:
			HRESULT WriteWaveHeader(HMMIO hFile, LPCWAVEFORMATEX pwfx, MMCKINFO *pckRIFF, MMCKINFO *pckData);
			HRESULT FinishWaveFile(HMMIO hFile, MMCKINFO *pckRIFF, MMCKINFO *pckData);
			HRESULT DefineWaveFormat();
			HRESULT captureLoop();
			HRESULT activateAudioClient();
			HRESULT activateAudioCaptureClient(); //init pAudioCaptureClient
			void cleanup();
			HRESULT definePeriod();
			int descendToWAVEChunk(MMCKINFO *ckRIFF);
			int descendToFACTChunk(MMCKINFO *ckRIFF,MMCKINFO *ckFact);
			int openhFile();
			int closehFile();
			int writeToChunk();
			int ascendChunk(MMCKINFO *ckFact);
	};
}

namespace threadArgs
{
	extern HANDLE hStartedEvent;
	extern HANDLE hStopEvent;
	extern HANDLE hThread;
	extern systemAudio::audioCapture *systemSound;
	 
}

#endif //__AUDIO_CAPTURE_HPP__