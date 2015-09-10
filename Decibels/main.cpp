#include <iostream>
#include <cmath>

// OpenAL header files
#include <AL/al.h>    
#include <AL/alc.h>

using namespace std;

#ifdef _WIN32
	#include <windows.h>
#else
	#include <unistd.h>
#endif

void sleep(int ms)
{
#ifdef _WIN32
	Sleep(ms);
#else
	usleep(ms * 1000);   // usleep takes sleep time in us (1 millionth of a second)
#endif
}

#define SAMPLE_RATE 22050
#define BUFFERS		2
#define MAX_AMPLITUDE (1 << 15)


int main()
{
	ALCdevice *dev[2];
	ALCcontext *ctx;
	ALuint source;
	ALuint buffers[BUFFERS];
	short data[2500];
	ALuint buf;
	ALint val;

	dev[0] = alcOpenDevice(NULL);
	ctx = alcCreateContext(dev[0], NULL);
	alcMakeContextCurrent(ctx);

	// Create buffers
	alGenSources(1, &source);
	alGenBuffers(BUFFERS, buffers);

	// Init buffers to
	for (auto i = 0; i < BUFFERS; i++)
		alBufferData(buffers[i], AL_FORMAT_MONO16, data, sizeof(data), SAMPLE_RATE);
	
	alSourceQueueBuffers(source, BUFFERS, buffers);
	
	// Disable 3D spatialization for speed up
	alDistanceModel(AL_NONE);

	// Prepare Microphone
	dev[1] = alcCaptureOpenDevice(nullptr, SAMPLE_RATE, AL_FORMAT_MONO16, sizeof(data) / 2);

	alSourcePlay(source);

	// Start Microphone capture
	alcCaptureStart(dev[1]);

	// Smoothed decibels and alpha as transition quotient
	double rmsSmooth = 0;
	double alpha = 0.4;

	while (true)
	{
		sleep(100);

		// Check if buffer was already played, val is true or false
		alGetSourcei(source, AL_BUFFERS_PROCESSED, &val);

		if (val <= 0)
			continue;

		// Get number of frames captuered
		alcGetIntegerv(dev[1], ALC_CAPTURE_SAMPLES, 1, &val);
		alcCaptureSamples(dev[1], data, val);

		//
		// Calculate dB
		//
		double rms = 0;
		for (int i = 0; i < val; i++)
			rms += data[i] * data[i];

		rms = sqrt(rms / val);

		// Compute a smoothed version for less flickering of the display.
		// Take previous and current into account
		rmsSmooth = rmsSmooth * alpha + (1 - alpha) * rms;
		auto rmsdB = 20.0 * log10(rmsSmooth / MAX_AMPLITUDE);
		cout << rmsdB << "dB" << endl;

		// Manage Buffer
		// Pop the oldest finished buffer, fill it with the new capture data, 
		// then re-queue it to play on the source
		alSourceUnqueueBuffers(source, 1, &buf);
		alBufferData(buf, AL_FORMAT_MONO16, data, val * 2, SAMPLE_RATE);
		alSourceQueueBuffers(source, 1, &buf);

		// Make sure the source is still playing
		alGetSourcei(source, AL_SOURCE_STATE, &val);

		if (val != AL_PLAYING)
			alSourcePlay(source);
	}

	// Shutdown and cleanup
	alcCaptureStop(dev[1]);
	alcCaptureCloseDevice(dev[1]);

	alSourceStop(source);
	alDeleteSources(1, &source);
	alDeleteBuffers(BUFFERS, buffers);
	alDeleteBuffers(1, &buf);

	alcMakeContextCurrent(NULL);
	alcDestroyContext(ctx);
	alcCloseDevice(dev[0]);

	return 0;
}