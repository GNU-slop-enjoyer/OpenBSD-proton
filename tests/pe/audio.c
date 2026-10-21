/*
 * WASAPI shared-mode playback test (test tier 9).
 *
 * Plays a 440 Hz tone on the default render endpoint for 3 seconds, first
 * by polling GetCurrentPadding (as many older engines do), then
 * event-driven (AUDCLNT_STREAMFLAGS_EVENTCALLBACK, as XAudio2/FAudio do),
 * and checks what matters for glitch-free audio on a host whose scheduler
 * clock ticks at 100 Hz (OpenBSD):
 *   - the audio clock (IAudioClock) advances at the real-time rate
 *   - the buffer never runs empty after the start (underruns)
 *   - in event mode, buffer events arrive about once per device period
 * With wine's winepulse.drv a PulseAudio server is needed; a null sink is
 * enough (pulseaudio -n --load=module-null-sink
 * --load=module-native-protocol-unix).  Output is TAP style; the exit code
 * is the number of failed checks.
 *
 *   x86_64-w64-mingw32-gcc -O2 -Wall -o audio.exe audio.c -lole32 -lm
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <math.h>
#include <stdio.h>

static int test_no, failures;

static void check(int cond, const char *name)
{
	printf("%s %d - %s\n", cond ? "ok" : "not ok", ++test_no, name);
	if (!cond)
		failures++;
	fflush(stdout);
}

static double now(void)
{
	static LARGE_INTEGER freq;
	LARGE_INTEGER t;

	if (!freq.QuadPart)
		QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&t);
	return (double)t.QuadPart / freq.QuadPart;
}

struct tone {
	double phase;
	double step;
	int channels;
	int is_float;
	int bits;
};

static void fill(struct tone *t, BYTE *data, UINT32 frames)
{
	UINT32 i;
	int c;

	for (i = 0; i < frames; i++) {
		double v = 0.2 * sin(t->phase);

		t->phase += t->step;
		for (c = 0; c < t->channels; c++) {
			if (t->is_float)
				((float *)data)[i * t->channels + c] = (float)v;
			else if (t->bits == 16)
				((short *)data)[i * t->channels + c] = (short)(v * 32767);
			else
				((int *)data)[i * t->channels + c] = (int)(v * 2147483647.0);
		}
	}
}

static void play(IMMDevice *dev, int event_mode)
{
	IAudioClient *client = NULL;
	IAudioRenderClient *render = NULL;
	IAudioClock *clock = NULL;
	WAVEFORMATEX *fmt = NULL;
	REFERENCE_TIME def_period, min_period;
	HANDLE event = NULL;
	UINT32 buffer_frames, padding, frames;
	UINT64 freq, pos;
	struct tone tone;
	double start, last_event = 0, elapsed, ratio, period_s;
	double max_gap = 0, sum_gap = 0;
	int underruns = 0, events = 0, timeouts = 0;
	char name[128];
	BYTE *data;
	HRESULT hr;

	const char *mode = event_mode ? "event" : "polled";

	hr = IMMDevice_Activate(dev, &IID_IAudioClient, CLSCTX_ALL, NULL,
	    (void **)&client);
	snprintf(name, sizeof(name), "%s: activate IAudioClient", mode);
	check(SUCCEEDED(hr), name);
	if (FAILED(hr))
		return;
	IAudioClient_GetMixFormat(client, &fmt);
	IAudioClient_GetDevicePeriod(client, &def_period, &min_period);
	printf("# %s: mix format %lu Hz, %u ch, %u bits, tag %#x; period "
	    "%.1f ms (min %.1f)\n", mode, fmt->nSamplesPerSec, fmt->nChannels,
	    fmt->wBitsPerSample, fmt->wFormatTag, def_period / 10000.0,
	    min_period / 10000.0);

	tone.phase = 0;
	tone.step = 2 * M_PI * 440 / fmt->nSamplesPerSec;
	tone.channels = fmt->nChannels;
	tone.bits = fmt->wBitsPerSample;
	tone.is_float = fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
	    (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
	    /* KSDATAFORMAT_SUBTYPE_* GUIDs carry the format tag in Data1 */
	    ((WAVEFORMATEXTENSIBLE *)fmt)->SubFormat.Data1 ==
	    WAVE_FORMAT_IEEE_FLOAT);

	hr = IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED,
	    event_mode ? AUDCLNT_STREAMFLAGS_EVENTCALLBACK : 0,
	    event_mode ? 0 : 1000000, 0, fmt, NULL);
	snprintf(name, sizeof(name), "%s: Initialize (hr %#lx)", mode, hr);
	check(SUCCEEDED(hr), name);
	if (FAILED(hr))
		goto done;
	if (event_mode) {
		event = CreateEventW(NULL, FALSE, FALSE, NULL);
		IAudioClient_SetEventHandle(client, event);
	}
	IAudioClient_GetBufferSize(client, &buffer_frames);
	IAudioClient_GetService(client, &IID_IAudioRenderClient,
	    (void **)&render);
	IAudioClient_GetService(client, &IID_IAudioClock, (void **)&clock);
	IAudioClock_GetFrequency(clock, &freq);
	period_s = def_period / 1e7;
	printf("# %s: buffer %u frames (%.1f ms)\n", mode, buffer_frames,
	    buffer_frames * 1000.0 / fmt->nSamplesPerSec);

	/* prefill */
	IAudioRenderClient_GetBuffer(render, buffer_frames, &data);
	fill(&tone, data, buffer_frames);
	IAudioRenderClient_ReleaseBuffer(render, buffer_frames, 0);

	hr = IAudioClient_Start(client);
	snprintf(name, sizeof(name), "%s: Start", mode);
	check(SUCCEEDED(hr), name);
	start = now();
	while (now() - start < 3.0) {
		if (event_mode) {
			if (WaitForSingleObject(event, 200) != WAIT_OBJECT_0) {
				timeouts++;
				continue;
			}
			if (last_event) {
				double gap = now() - last_event;

				sum_gap += gap;
				if (gap > max_gap)
					max_gap = gap;
			}
			last_event = now();
			events++;
		} else
			Sleep(10);
		IAudioClient_GetCurrentPadding(client, &padding);
		if (padding == 0)
			underruns++;
		frames = buffer_frames - padding;
		if (frames && SUCCEEDED(IAudioRenderClient_GetBuffer(render,
		    frames, &data))) {
			fill(&tone, data, frames);
			IAudioRenderClient_ReleaseBuffer(render, frames, 0);
		}
	}
	IAudioClock_GetPosition(clock, &pos, NULL);
	elapsed = now() - start;
	IAudioClient_Stop(client);

	ratio = ((double)pos / freq) / elapsed;
	printf("# %s: played %.3f s of audio in %.3f s (ratio %.3f), "
	    "underruns %d\n", mode, (double)pos / freq, elapsed, ratio,
	    underruns);
	snprintf(name, sizeof(name), "%s: audio clock runs at real time "
	    "(within 5%%)", mode);
	check(ratio > 0.95 && ratio < 1.05, name);
	snprintf(name, sizeof(name), "%s: no buffer underruns", mode);
	check(underruns == 0, name);
	if (event_mode) {
		double mean = events > 1 ? sum_gap / (events - 1) : 0;

		printf("# event: %d events, mean interval %.1f ms, max %.1f ms, "
		    "%d waits timed out (period %.1f ms)\n", events,
		    mean * 1000, max_gap * 1000, timeouts, period_s * 1000);
		check(timeouts == 0, "event: buffer events keep arriving");
		check(mean > period_s * 0.5 && mean < period_s * 2,
		    "event: mean event interval near the device period");
		check(max_gap < 4 * period_s + 0.02,
		    "event: no long gaps between events");
	}

done:
	if (clock)
		IAudioClock_Release(clock);
	if (render)
		IAudioRenderClient_Release(render);
	if (event)
		CloseHandle(event);
	CoTaskMemFree(fmt);
	IAudioClient_Release(client);
}

int main(void)
{
	IMMDeviceEnumerator *mme = NULL;
	IMMDevice *dev = NULL;
	HRESULT hr;

	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
	    &IID_IMMDeviceEnumerator, (void **)&mme);
	check(SUCCEEDED(hr), "create MMDeviceEnumerator");
	if (SUCCEEDED(hr)) {
		hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(mme, eRender,
		    eConsole, &dev);
		check(SUCCEEDED(hr), "default render endpoint");
	}
	if (dev) {
		play(dev, 0);
		play(dev, 1);
		IMMDevice_Release(dev);
	}
	if (mme)
		IMMDeviceEnumerator_Release(mme);
	CoUninitialize();
	printf("1..%d\n", test_no);
	return failures;
}
