/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/modules/audio_device/linux/audio_device_alsa_linux.h"

#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/system_wrappers/include/trace.h"

#include "audio_pcm.h"
#include "param_conf.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>"
#include <iostream>


#define STEREO_EN    1

namespace webrtc
{

#if STEREO_EN
static const unsigned int ALSA_PLAYOUT_CH = 2;
static const unsigned int ALSA_CAPTURE_CH = 2;
#else
static const unsigned int ALSA_PLAYOUT_CH = 1;
static const unsigned int ALSA_CAPTURE_CH = 1;
#endif

static const unsigned int ALSA_PLAYOUT_FREQ = 48000;

static const unsigned int ALSA_PLAYOUT_LATENCY = 40*1000; // in us
static const unsigned int ALSA_CAPTURE_FREQ = 48000;

static const unsigned int ALSA_CAPTURE_LATENCY = 40*1000; // in us
static const unsigned int ALSA_CAPTURE_WAIT_TIMEOUT = 5; // in ms

#define FUNC_GET_NUM_OF_DEVICE 0
#define FUNC_GET_DEVICE_NAME 1
#define FUNC_GET_DEVICE_NAME_FOR_AN_ENUM 2



AudioDeviceLinuxALSA::AudioDeviceLinuxALSA(const int32_t id) :
    _ptrAudioBuffer(NULL),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _mixerManager(id),
    _inputDeviceIndex(0),
    _outputDeviceIndex(0),
    _inputDeviceIsSpecified(false),
    _outputDeviceIsSpecified(false),
    _recordingBufferSizeIn10MS(0),
    _playoutBufferSizeIn10MS(0),
    _recordingFramesIn10MS(0),
    _playoutFramesIn10MS(0),
    _recordingFreq(ALSA_CAPTURE_FREQ),
    _playoutFreq(ALSA_PLAYOUT_FREQ),
    _recChannels(ALSA_CAPTURE_CH),
    _playChannels(ALSA_PLAYOUT_CH),
    _recordingBuffer(NULL),
    _playoutBuffer(NULL),
    _recordingFramesLeft(0),
    _playoutFramesLeft(0),
    _playBufType(AudioDeviceModule::kFixedBufferSize),
    _initialized(false),
    _recording(false),
    _playing(false),
    _recIsInitialized(false),
    _playIsInitialized(false),
    _AGC(false),
    _playWarning(0),
    _playError(0),
    _recWarning(0),
    _recError(0),
    _playBufDelay(80),
    _playBufDelayFixed(80)
{
    memset(_oldKeyState, 0, sizeof(_oldKeyState));
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id,
                 "%s created", __FUNCTION__);
}

// ----------------------------------------------------------------------------
//  AudioDeviceLinuxALSA - dtor
// ----------------------------------------------------------------------------

AudioDeviceLinuxALSA::~AudioDeviceLinuxALSA()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s destroyed", __FUNCTION__);

    Terminate();

    // Clean up the recording buffer and playout buffer.
    if (_recordingBuffer)
    {
        delete [] _recordingBuffer;
        _recordingBuffer = NULL;
    }
    if (_playoutBuffer)
    {
        delete [] _playoutBuffer;
        _playoutBuffer = NULL;
    }
    delete &_critSect;
}

void AudioDeviceLinuxALSA::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "@@##\n";
    CriticalSectionScoped lock(&_critSect);

    _ptrAudioBuffer = audioBuffer;

    // Inform the AudioBuffer about default settings for this implementation.
    // Set all values to zero here since the actual settings will be done by
    // InitPlayout and InitRecording later.
    _ptrAudioBuffer->SetRecordingSampleRate(0);
    _ptrAudioBuffer->SetPlayoutSampleRate(0);
    _ptrAudioBuffer->SetRecordingChannels(0);
    _ptrAudioBuffer->SetPlayoutChannels(0);
}

int32_t AudioDeviceLinuxALSA::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const
{
    audioLayer = AudioDeviceModule::kLinuxAlsaAudio;
    return 0;
}

int32_t AudioDeviceLinuxALSA::Init()
{

    CriticalSectionScoped lock(&_critSect);



    if (_initialized)
    {
        return 0;
    }


    _playWarning = 0;
    _playError = 0;
    _recWarning = 0;
    _recError = 0;

    _initialized = true;

    return 0;
}

int32_t AudioDeviceLinuxALSA::Terminate()
{
    if (!_initialized)
    {
        return 0;
    }

    std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "\n";

    CriticalSectionScoped lock(&_critSect);

    _mixerManager.Close();

    // RECORDING
    if (_ptrThreadRec)
    {
        rtc::PlatformThread* tmpThread = _ptrThreadRec.release();
        _critSect.Leave();

        tmpThread->Stop();
        delete tmpThread;

        _critSect.Enter();
    }

    // PLAYOUT
    if (_ptrThreadPlay)
    {
        rtc::PlatformThread* tmpThread = _ptrThreadPlay.release();
        _critSect.Leave();

        tmpThread->Stop();
        delete tmpThread;

        _critSect.Enter();
    }

    _initialized = false;
    _outputDeviceIsSpecified = false;
    _inputDeviceIsSpecified = false;

    return 0;
}

bool AudioDeviceLinuxALSA::Initialized() const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (_initialized);
}

int32_t AudioDeviceLinuxALSA::InitSpeaker()
{

    CriticalSectionScoped lock(&_critSect);

    std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";

    if (_playing)
    {
        return -1;
    }

    char devName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, true, _outputDeviceIndex, devName, kAdmMaxDeviceNameSize);
    //return _mixerManager.OpenSpeaker(devName);
    return 0;
}

int32_t AudioDeviceLinuxALSA::InitMicrophone()
{

    CriticalSectionScoped lock(&_critSect);

    std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";

    if (_recording)
    {
        return -1;
    }

    char devName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, false, _inputDeviceIndex, devName, kAdmMaxDeviceNameSize);
    //return _mixerManager.OpenMicrophone(devName);
    return 0;
}

bool AudioDeviceLinuxALSA::SpeakerIsInitialized() const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (_mixerManager.SpeakerIsInitialized());
}

bool AudioDeviceLinuxALSA::MicrophoneIsInitialized() const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (_mixerManager.MicrophoneIsInitialized());
}

int32_t AudioDeviceLinuxALSA::SpeakerVolumeIsAvailable(bool& available)
{

    bool wasInitialized = _mixerManager.SpeakerIsInitialized();

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    // Make an attempt to open up the
    // output mixer corresponding to the currently selected output device.
    if (!wasInitialized && InitSpeaker() == -1)
    {
        // If we end up here it means that the selected speaker has no volume
        // control.
        available = false;
        return 0;
    }

    // Given that InitSpeaker was successful, we know that a volume control
    // exists
    available = false;

    // Close the initialized output mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseSpeaker();
    }

    return 0;
}

int32_t AudioDeviceLinuxALSA::SetSpeakerVolume(uint32_t volume)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
	_speaker_volume = volume;
    return 0;//(_mixerManager.SetSpeakerVolume(volume));
}

int32_t AudioDeviceLinuxALSA::SpeakerVolume(uint32_t& volume) const
{
#if 0
    uint32_t level(0);
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_mixerManager.SpeakerVolume(level) == -1)
    {
        return -1;
    }
	level = 10;
#endif
    volume = _speaker_volume;

    return 0;
}


int32_t AudioDeviceLinuxALSA::SetWaveOutVolume(uint16_t volumeLeft,
                                               uint16_t volumeRight)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceLinuxALSA::WaveOutVolume(
    uint16_t& /*volumeLeft*/,
    uint16_t& /*volumeRight*/) const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceLinuxALSA::MaxSpeakerVolume(
    uint32_t& maxVolume) const
{
	
    uint32_t maxVol(0);
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";

    if (_mixerManager.MaxSpeakerVolume(maxVol) == -1)
    {
        return -1;
    }
	maxVol = 255;
    maxVolume = maxVol;

    return 0;
}

int32_t AudioDeviceLinuxALSA::MinSpeakerVolume(
    uint32_t& minVolume) const
{

    uint32_t minVol(0);

    std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";

    if (_mixerManager.MinSpeakerVolume(minVol) == -1)
    {
        return -1;
    }
	minVol = 1;
    minVolume = minVol;

    return 0;
}

int32_t AudioDeviceLinuxALSA::SpeakerVolumeStepSize(
    uint16_t& stepSize) const
{

    uint16_t delta(0);
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_mixerManager.SpeakerVolumeStepSize(delta) == -1)
    {
        return -1;
    }
	return -1;
    stepSize = delta;

    return 0;
}

int32_t AudioDeviceLinuxALSA::SpeakerMuteIsAvailable(bool& available)
{

    //bool isAvailable(false);
    bool wasInitialized = _mixerManager.SpeakerIsInitialized();
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    // Make an attempt to open up the
    // output mixer corresponding to the currently selected output device.
    //
    if (!wasInitialized && InitSpeaker() == -1)
    {
        // If we end up here it means that the selected speaker has no volume
        // control, hence it is safe to state that there is no mute control
        // already at this stage.
        available = false;
        return 0;
    }

	available = false;
    // Check if the selected speaker has a mute control
   // _mixerManager.SpeakerMuteIsAvailable(isAvailable);

    //available = isAvailable;

    // Close the initialized output mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseSpeaker();
    }

    return 0;
}

int32_t AudioDeviceLinuxALSA::SetSpeakerMute(bool enable)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (_mixerManager.SetSpeakerMute(enable));
}

int32_t AudioDeviceLinuxALSA::SpeakerMute(bool& enabled) const
{

    bool muted(0);
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_mixerManager.SpeakerMute(muted) == -1)
    {
        return -1;
    }

    enabled = muted;

    return 0;
}

int32_t AudioDeviceLinuxALSA::MicrophoneMuteIsAvailable(bool& available)
{

    bool isAvailable(false);
    bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    // Make an attempt to open up the
    // input mixer corresponding to the currently selected input device.
    //
    if (!wasInitialized && InitMicrophone() == -1)
    {
        // If we end up here it means that the selected microphone has no volume
        // control, hence it is safe to state that there is no mute control
        // already at this stage.
        available = false;
        return 0;
    }

    // Check if the selected microphone has a mute control
    //
    _mixerManager.MicrophoneMuteIsAvailable(isAvailable);
    available = isAvailable;

    // Close the initialized input mixer
    //
    if (!wasInitialized)
    {
        _mixerManager.CloseMicrophone();
    }

    available = false;

    return 0;
}

int32_t AudioDeviceLinuxALSA::SetMicrophoneMute(bool enable)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (_mixerManager.SetMicrophoneMute(enable));
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceLinuxALSA::MicrophoneMute(bool& enabled) const
{

    bool muted(0);
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_mixerManager.MicrophoneMute(muted) == -1)
    {
        return -1;
    }

    enabled = muted;
    return 0;
}

int32_t AudioDeviceLinuxALSA::MicrophoneBoostIsAvailable(bool& available)
{

    bool isAvailable(false);
    bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    // Enumerate all avaliable microphone and make an attempt to open up the
    // input mixer corresponding to the currently selected input device.
    //
    if (!wasInitialized && InitMicrophone() == -1)
    {
        // If we end up here it means that the selected microphone has no volume
        // control, hence it is safe to state that there is no boost control
        // already at this stage.
        available = false;
        return 0;
    }

    // Check if the selected microphone has a boost control
    _mixerManager.MicrophoneBoostIsAvailable(isAvailable);
    available = isAvailable;

    // Close the initialized input mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseMicrophone();
    }

    return 0;
}

int32_t AudioDeviceLinuxALSA::SetMicrophoneBoost(bool enable)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (_mixerManager.SetMicrophoneBoost(enable));
}

int32_t AudioDeviceLinuxALSA::MicrophoneBoost(bool& enabled) const
{

    bool onOff(0);
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_mixerManager.MicrophoneBoost(onOff) == -1)
    {
        return -1;
    }

    enabled = onOff;

    return 0;
}

int32_t AudioDeviceLinuxALSA::StereoRecordingIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";

#if !STEREO_EN
	available = false;
	return 0;
#endif
	
    // If we already have initialized in stereo it's obviously available
    if (_recIsInitialized && (2 == _recChannels))
    {
        available = true;
        return 0;
    }

    // Save rec states and the number of rec channels
    bool recIsInitialized = _recIsInitialized;
    bool recording = _recording;
    int recChannels = _recChannels;

    available = false;

    // Stop/uninitialize recording if initialized (and possibly started)
    if (_recIsInitialized)
    {
        StopRecording();
    }

    // Try init in stereo;
    _recChannels = 2;
    if (InitRecording() == 0)
    {
        available = true;
    }

    // Stop/uninitialize recording
    StopRecording();

    // Recover previous states
    _recChannels = recChannels;
    if (recIsInitialized)
    {
        InitRecording();
    }
    if (recording)
    {
        StartRecording();
    }

    return 0;
}

int32_t AudioDeviceLinuxALSA::SetStereoRecording(bool enable)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (enable)
    {
        _recChannels = 2;
        printf("##set stereo recording.\n");
    }
    else
    {
        _recChannels = 1;
        printf("##set mono recording.\n");
    }

    return 0;
}

int32_t AudioDeviceLinuxALSA::StereoRecording(bool& enabled) const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_recChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

int32_t AudioDeviceLinuxALSA::StereoPlayoutIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

#if !STEREO_EN
    available = false;
    return 0;
#endif

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    // If we already have initialized in stereo it's obviously available
    if (_playIsInitialized && (2 == _playChannels))
    {
        available = true;
        std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
        return 0;
    }

    // Save rec states and the number of rec channels
    bool playIsInitialized = _playIsInitialized;
    bool playing = _playing;
    int playChannels = _playChannels;

    available = false;

    // Stop/uninitialize recording if initialized (and possibly started)
    if (_playIsInitialized)
    {
        StopPlayout();
    }

    // Try init in stereo;
    _playChannels = 2;
    if (InitPlayout() == 0)
    {
        available = true;
    }

    // Stop/uninitialize recording
    StopPlayout();

    // Recover previous states
    _playChannels = playChannels;
    if (playIsInitialized)
    {
        InitPlayout();
    }
    if (playing)
    {
        StartPlayout();
    }

    return 0;
}

int32_t AudioDeviceLinuxALSA::SetStereoPlayout(bool enable)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (enable)
        _playChannels = 2;
    else
        _playChannels = 1;

    return 0;
}

int32_t AudioDeviceLinuxALSA::StereoPlayout(bool& enabled) const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_playChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

int32_t AudioDeviceLinuxALSA::SetAGC(bool enable)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    _AGC = enable;

    return 0;
}

bool AudioDeviceLinuxALSA::AGC() const
{
	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return _AGC;
}

int32_t AudioDeviceLinuxALSA::MicrophoneVolumeIsAvailable(bool& available)
{

    bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    // Make an attempt to open up the
    // input mixer corresponding to the currently selected output device.
    if (!wasInitialized && InitMicrophone() == -1)
    {
        // If we end up here it means that the selected microphone has no volume
        // control.
        available = false;
        return 0;
    }

    // Given that InitMicrophone was successful, we know that a volume control
    // exists
    available = true;

    available = false;

    // Close the initialized input mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseMicrophone();
    }

    return 0;
}

int32_t AudioDeviceLinuxALSA::SetMicrophoneVolume(uint32_t volume)
{

	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    //return (_mixerManager.SetMicrophoneVolume(volume));
	_mic_volume = volume;
    return 0;
}

int32_t AudioDeviceLinuxALSA::MicrophoneVolume(uint32_t& volume) const
{
#if 0
    uint32_t level(0);

	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_mixerManager.MicrophoneVolume(level) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  failed to retrive current microphone level");
        return -1;
    }
	level = 15;
#endif
    volume = _mic_volume;

    return 0;
}

int32_t AudioDeviceLinuxALSA::MaxMicrophoneVolume(
    uint32_t& maxVolume) const
{

    uint32_t maxVol(0);

	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_mixerManager.MaxMicrophoneVolume(maxVol) == -1)
    {
        return -1;
    }
	maxVol = 255;
    maxVolume = maxVol;

    return 0;
}

int32_t AudioDeviceLinuxALSA::MinMicrophoneVolume(
    uint32_t& minVolume) const
{

    uint32_t minVol(0);

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_mixerManager.MinMicrophoneVolume(minVol) == -1)
    {
        return -1;
    }
	minVol = 1;
    minVolume = minVol;

    return 0;
}

int32_t AudioDeviceLinuxALSA::MicrophoneVolumeStepSize(
    uint16_t& stepSize) const
{

    uint16_t delta(0);

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_mixerManager.MicrophoneVolumeStepSize(delta) == -1)
    {
        return -1;
    }

    stepSize = delta;

    return 0;
}

int16_t AudioDeviceLinuxALSA::PlayoutDevices()
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (int16_t)GetDevicesInfo(0, true);
}

int32_t AudioDeviceLinuxALSA::SetPlayoutDevice(uint16_t index)
{

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (_playIsInitialized)
    {
        return -1;
    }

    uint32_t nDevices = GetDevicesInfo(0, true);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  number of availiable audio output devices is %u", nDevices);

    printf("##audio set playout device, index %d, device %d\n", index, nDevices);

    if (index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    _outputDeviceIndex = index;
    _outputDeviceIsSpecified = true;

    return 0;
}

int32_t AudioDeviceLinuxALSA::SetPlayoutDevice(
    AudioDeviceModule::WindowsDeviceType /*device*/)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "WindowsDeviceType not supported");
    return -1;
}

int32_t AudioDeviceLinuxALSA::PlayoutDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    const uint16_t nDevices(PlayoutDevices());
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return GetDevicesInfo(1, true, index, name, kAdmMaxDeviceNameSize);
}

int32_t AudioDeviceLinuxALSA::RecordingDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    const uint16_t nDevices(RecordingDevices());

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return GetDevicesInfo(1, false, index, name, kAdmMaxDeviceNameSize);
}

int16_t AudioDeviceLinuxALSA::RecordingDevices()
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (int16_t)GetDevicesInfo(0, false);
}

int32_t AudioDeviceLinuxALSA::SetRecordingDevice(uint16_t index)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";

    if (_recIsInitialized)
    {
        return -1;
    }

    uint32_t nDevices = GetDevicesInfo(0, false);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  number of availiable audio input devices is %u", nDevices);

	printf("##audio setrecordingdevice index %d, ndevice %d\n", index, nDevices);
    if (index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    _inputDeviceIndex = index;
    _inputDeviceIsSpecified = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice II (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceLinuxALSA::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType /*device*/)
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "WindowsDeviceType not supported");
    return -1;
}

int32_t AudioDeviceLinuxALSA::PlayoutIsAvailable(bool& available)
{

    available = false;
	
    // Try to initialize the playout side with mono
    // Assumes that user set num channels after calling this function
    _playChannels = 1;
    int32_t res = InitPlayout();

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    // Cancel effect of initialization
    StopPlayout();

    if (res != -1)
    {
        available = true;
    }
    else
    {
        // It may be possible to play out in stereo
        res = StereoPlayoutIsAvailable(available);
        if (available)
        {
            // Then set channels to 2 so InitPlayout doesn't fail
            _playChannels = 2;
        }
    }

    return res;
}

int32_t AudioDeviceLinuxALSA::RecordingIsAvailable(bool& available)
{

    available = false;

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    // Try to initialize the recording side with mono
    // Assumes that user set num channels after calling this function
    _recChannels = 1;
    int32_t res = InitRecording();

    // Cancel effect of initialization
    StopRecording();

    if (res != -1)
    {
        available = true;
    }
    else
    {
        // It may be possible to record in stereo
        res = StereoRecordingIsAvailable(available);
        if (available)
        {
            // Then set channels to 2 so InitPlayout doesn't fail
            _recChannels = 2;
        }
    }

    return res;
}

int32_t AudioDeviceLinuxALSA::InitPlayout()
{

    //int errVal = 0;
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    CriticalSectionScoped lock(&_critSect);
    if (_playing)
    {
        return -1;
    }

    if (!_outputDeviceIsSpecified)
    {
        return -1;
    }

    if (_playIsInitialized)
    {
        return 0;
    }
    // Initialize the speaker (devices might have been added or removed)
    if (InitSpeaker() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  InitSpeaker() failed");
    }

    // Start by closing any existing wave-output devices
    //


    // Open PCM device for playout
    char deviceName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, true, _outputDeviceIndex, deviceName,
                   kAdmMaxDeviceNameSize);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  InitPlayout open (%s)", deviceName);

	_playIsInitialized = true;
	_playoutFramesIn10MS = _playoutFreq/100;

    if (_ptrAudioBuffer)
    {
        // Update webrtc audio buffer with the selected parameters
        _ptrAudioBuffer->SetPlayoutSampleRate(_playoutFreq);
        _ptrAudioBuffer->SetPlayoutChannels(_playChannels);
        printf("##$ _playoutFreq %d, _playChannels %d\n",_playoutFreq,_playChannels);
    }

#if STEREO_EN
    _playoutBufferSizeIn10MS = _playoutFramesIn10MS * 4;
#else
	_playoutBufferSizeIn10MS = _playoutFramesIn10MS * 2;
#endif

    // Set play buffer size
    //_playoutBufferSizeIn10MS = LATE(snd_pcm_frames_to_bytes)(
    //    _handlePlayout, _playoutFramesIn10MS);

    // Init varaibles used for play
    _playWarning = 0;
    _playError = 0;

    return 0;
}

int32_t AudioDeviceLinuxALSA::InitRecording()
{

    //int errVal = 0;
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    CriticalSectionScoped lock(&_critSect);

    if (_recording)
    {
        return -1;
    }

    if (!_inputDeviceIsSpecified)
    {
        return -1;
    }

    if (_recIsInitialized)
    {
        return 0;
    }

    // Initialize the microphone (devices might have been added or removed)
    if (InitMicrophone() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   "  InitMicrophone() failed");
    }

    // Start by closing any existing pcm-input devices
    //

    // Open PCM device for recording
    // The corresponding settings for playout are made after the record settings
    char deviceName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, false, _inputDeviceIndex, deviceName,
                   kAdmMaxDeviceNameSize);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "InitRecording open (%s)", deviceName);

    _recordingFramesIn10MS = _recordingFreq/100;

    if (_ptrAudioBuffer)
    {
        // Update webrtc audio buffer with the selected parameters
        _ptrAudioBuffer->SetRecordingSampleRate(_recordingFreq);
        _ptrAudioBuffer->SetRecordingChannels(_recChannels);
    }
#if STEREO_EN
	_recordingBufferSizeIn10MS = _recordingFramesIn10MS * 4;
#else
	_recordingBufferSizeIn10MS = _recordingFramesIn10MS * 2;
#endif
    // Set rec buffer size and create buffer
    //_recordingBufferSizeIn10MS = LATE(snd_pcm_frames_to_bytes)(
    //    _handleRecord, _recordingFramesIn10MS);

    _recIsInitialized = true;

	printf("##init recording end\n");
    return 0;
}

int32_t AudioDeviceLinuxALSA::StartRecording()
{

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (!_recIsInitialized)
    {
		std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##1\n";
        return -1;
    }

    if (_recording)
    {
		std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##2\n";
        return 0;
    }

    _recording = true;

//    int errVal = 0;
    _recordingFramesLeft = _recordingFramesIn10MS;

    // Make sure we only create the buffer once.
    if (!_recordingBuffer)
        _recordingBuffer = new int8_t[_recordingBufferSizeIn10MS];
    if (!_recordingBuffer)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "   failed to alloc recording buffer");
        _recording = false;
        std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##3\n";
        return -1;
    }

    
    // RECORDING
    _ptrThreadRec.reset(new rtc::PlatformThread(
        RecThreadFunc, this, "webrtc_audio_module_capture_thread"));

    _ptrThreadRec->Start();
    _ptrThreadRec->SetPriority(rtc::kRealtimePriority);


    return 0;
}

int32_t AudioDeviceLinuxALSA::StopRecording()
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    {
      CriticalSectionScoped lock(&_critSect);

      if (!_recIsInitialized)
      {
          return 0;
      }

      // Make sure we don't start recording (it's asynchronous).
      _recIsInitialized = false;
      _recording = false;
    }

    if (_ptrThreadRec)
    {
        _ptrThreadRec->Stop();
        _ptrThreadRec.reset();
    }

    CriticalSectionScoped lock(&_critSect);
    _recordingFramesLeft = 0;
    if (_recordingBuffer)
    {
        delete [] _recordingBuffer;
        _recordingBuffer = NULL;
    }

    // Stop and close pcm recording device.

    // Check if we have muted and unmute if so.
    bool muteEnabled = false;
    MicrophoneMute(muteEnabled);
    if (muteEnabled)
    {
        SetMicrophoneMute(false);
    }

    // set the pcm input handle to NULL
    return 0;
}

bool AudioDeviceLinuxALSA::RecordingIsInitialized() const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (_recIsInitialized);
}

bool AudioDeviceLinuxALSA::Recording() const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (_recording);
}

bool AudioDeviceLinuxALSA::PlayoutIsInitialized() const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##_playIsInitialized:"<< _playIsInitialized <<"\n";
    return (_playIsInitialized);
}

int32_t AudioDeviceLinuxALSA::StartPlayout()
{
    printf("StartPlayout,_playIsInitialized[%d],_playing[%d],_inputDeviceIndex[%d]\n",_playIsInitialized,_playing,_inputDeviceIndex);
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (!_playIsInitialized)
    {
        return -1;
    }

    if (_playing)
    {
        return 0;
    }

    _playing = true;

    _playoutFramesLeft = 0;
    if (!_playoutBuffer)
        _playoutBuffer = new int8_t[_playoutBufferSizeIn10MS];
    if (!_playoutBuffer)
    {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                   "    failed to alloc playout buf");
      _playing = false;
      return -1;
    }

    // PLAYOUT
    _ptrThreadPlay.reset(new rtc::PlatformThread(
        PlayThreadFunc, this, "webrtc_audio_module_play_thread"));
    _ptrThreadPlay->Start();
    _ptrThreadPlay->SetPriority(rtc::kRealtimePriority);
    
    startAi();

    return 0;
}

int32_t AudioDeviceLinuxALSA::StopPlayout()
{
    printf("StopPlayout,_playIsInitialized[%d],_playing[%d],_inputDeviceIndex[%d]\n",_playIsInitialized,_playing,_inputDeviceIndex);
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    {
        CriticalSectionScoped lock(&_critSect);

        if (!_playIsInitialized)
        {
            return 0;
        }

        _playing = false;
    }

    // stop playout thread first
    if (_ptrThreadPlay)
    {
        _ptrThreadPlay->Stop();
        _ptrThreadPlay.reset();
    }

    CriticalSectionScoped lock(&_critSect);

    _playoutFramesLeft = 0;
    delete [] _playoutBuffer;
    _playoutBuffer = NULL;

    // stop and close pcm playout device

     // set the pcm input handle to NULL
     _playIsInitialized = false;
     WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                  "  handle_playout is now set to NULL");

    stopAi();
     return 0;
}

int32_t AudioDeviceLinuxALSA::PlayoutDelay(uint16_t& delayMS) const
{
	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    delayMS = (uint16_t)10 * 1000 / _playoutFreq;
    return 0;
}

int32_t AudioDeviceLinuxALSA::RecordingDelay(uint16_t& delayMS) const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    // Adding 10ms adjusted value to the record delay due to 10ms buffering.
    delayMS = (uint16_t)(10 + 10 * 1000 / _recordingFreq);
    return 0;
}

bool AudioDeviceLinuxALSA::Playing() const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (_playing);
}
// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

int32_t AudioDeviceLinuxALSA::SetPlayoutBuffer(
    const AudioDeviceModule::BufferType type,
    uint16_t sizeMS)
{

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    _playBufType = type;
    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        _playBufDelayFixed = sizeMS;
    }
    return 0;
}

int32_t AudioDeviceLinuxALSA::PlayoutBuffer(
    AudioDeviceModule::BufferType& type,
    uint16_t& sizeMS) const
{

	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    type = _playBufType;
    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        sizeMS = _playBufDelayFixed;
    }
    else
    {
        sizeMS = _playBufDelay;
    }

    return 0;
}

int32_t AudioDeviceLinuxALSA::CPULoad(uint16_t& load) const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
    return -1;
}

bool AudioDeviceLinuxALSA::PlayoutWarning() const
{
	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    CriticalSectionScoped lock(&_critSect);
    return (_playWarning > 0);
}

bool AudioDeviceLinuxALSA::PlayoutError() const
{
	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    CriticalSectionScoped lock(&_critSect);
    return (_playError > 0);
}

bool AudioDeviceLinuxALSA::RecordingWarning() const
{
	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    CriticalSectionScoped lock(&_critSect);
    return (_recWarning > 0);
}

bool AudioDeviceLinuxALSA::RecordingError() const
{
	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    CriticalSectionScoped lock(&_critSect);
    return (_recError > 0);
}

void AudioDeviceLinuxALSA::ClearPlayoutWarning()
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    CriticalSectionScoped lock(&_critSect);
    _playWarning = 0;
}

void AudioDeviceLinuxALSA::ClearPlayoutError()
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    CriticalSectionScoped lock(&_critSect);
    _playError = 0;
}

void AudioDeviceLinuxALSA::ClearRecordingWarning()
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    CriticalSectionScoped lock(&_critSect);
    _recWarning = 0;
}

void AudioDeviceLinuxALSA::ClearRecordingError()
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    CriticalSectionScoped lock(&_critSect);
    _recError = 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

int32_t AudioDeviceLinuxALSA::GetDevicesInfo(
    const int32_t function,
    const bool playback,
    const int32_t enumDeviceNo,
    char* enumDeviceName,
    const int32_t ednLen) const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
   //const char *type = playback ? "Output" : "Input";

    int enumCount(0);

#if 1    

	printf("##audio GetDevicesInfo\n");

    enumCount++; // default is 0
    if ((function == FUNC_GET_DEVICE_NAME ||
        function == FUNC_GET_DEVICE_NAME_FOR_AN_ENUM) && enumDeviceNo == 0)
    {
        strcpy(enumDeviceName, "default");

        return 0;
    }

    char name[] = "default";
    char *desc = name;
	
    if ((FUNC_GET_DEVICE_NAME == function) &&
        (enumDeviceNo == enumCount))
    {
        // We have found the enum device, copy the name to buffer.
        strncpy(enumDeviceName, desc, ednLen);
        enumDeviceName[ednLen-1] = '\0';
    }
    if ((FUNC_GET_DEVICE_NAME_FOR_AN_ENUM == function) &&
        (enumDeviceNo == enumCount))
    {
        // We have found the enum device, copy the name to buffer.
        strncpy(enumDeviceName, name, ednLen);
        enumDeviceName[ednLen-1] = '\0';
    }

#endif
    if (FUNC_GET_NUM_OF_DEVICE == function)
    {
        //if (enumCount == 1) // only default?
        //    enumCount = 0;
        return enumCount; // Normal return point for function 0
    }

    return 0;
}

int32_t AudioDeviceLinuxALSA::InputSanityCheckAfterUnlockedPeriod() const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return 0;
}

int32_t AudioDeviceLinuxALSA::OutputSanityCheckAfterUnlockedPeriod() const
{
	std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return 0;
}


// ============================================================================
//                                  Thread Methods
// ============================================================================

bool AudioDeviceLinuxALSA::PlayThreadFunc(void* pThis)
{
	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (static_cast<AudioDeviceLinuxALSA*>(pThis)->PlayThreadProcess());
}

bool AudioDeviceLinuxALSA::RecThreadFunc(void* pThis)
{
	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    return (static_cast<AudioDeviceLinuxALSA*>(pThis)->RecThreadProcess());
}

static int tea_fd,stu_fd;
bool AudioDeviceLinuxALSA::PlayThreadProcess()
{
	unsigned char buf0[960] = {0}, buf1[960] = {0};
	int i = 0, j = 0;

	//return false;

	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if(!_playing)
        return false;
//#if 1
    if (_playoutFramesLeft <= 0)
    {
        //UnLock();
        _ptrAudioBuffer->RequestPlayoutData(_playoutFramesIn10MS);
        Lock();

        _playoutFramesLeft = _ptrAudioBuffer->GetPlayoutData(_playoutBuffer);
        assert(_playoutFramesLeft == _playoutFramesIn10MS);
    }
#if 0
#if STEREO_EN
#if 1
	for (i = 0; i < 1920; i+=4)
	{
		buf0[j] = _playoutBuffer[i];
		buf0[j+1] = _playoutBuffer[i+1];

		buf1[j] = _playoutBuffer[i+2];
		buf1[j+1] = _playoutBuffer[i+3];
		j += 2;
	}
#endif
    SendPcmFrame(buf0, buf1, 960);
    //SendPcmFrame((unsigned char *)_playoutBuffer, (unsigned char*)(_playoutBuffer+960), 960);
#else
	SendPcmFrame((unsigned char *)_playoutBuffer, (unsigned char*)_playoutBuffer, 960);
#endif
#else 
    SendPcmFrame((unsigned char *)_playoutBuffer);
#if 0
    if(!IsTeacher()) {
        if(stu_fd == 0) {
             stu_fd = open("/hejl/webrtc/stu.pcm",O_CREAT | O_TRUNC | O_WRONLY,0666);
        }
        write(stu_fd,_playoutBuffer,960*(STEREO_EN+1));
    }
#endif
#endif
    _playoutFramesLeft = 0;

    UnLock();
    return true;
}



bool AudioDeviceLinuxALSA::RecThreadProcess()
{
	//std::cout << "#" << __FUNCTION__ << " : " << __LINE__ << "##\n";
    if (!_recording)
        return false;

//    int err;
    //int avail_frames = -1;
#if 0
    int ret = 0;
    //int8_t buffer[_recordingBufferSizeIn10MS];
    unsigned char buf0[960] = {0}, buf1[960] = {0};//, buf2[960] = {0};
    int i,j=0;

    Lock();

	ret = GetPcmFrame(buf0, buf1, 960);
	if (ret < 0)
	{
		UnLock();
		return true;
	}

	//printf("#@@1\n");
#if STEREO_EN
#if 1
	j = 0;
    for (i = 0; i < 480; i++)
    {
		_recordingBuffer[j] = buf0[i*2];
		_recordingBuffer[j+1] = buf0[i*2+1];

		_recordingBuffer[j+2] = buf1[i*2];
		_recordingBuffer[j+3] = buf1[i*2+1];
		j+=4;
    }
#endif

#else
	memcpy(_recordingBuffer, buf0, 960);//_recordingFramesIn10MS);
#endif
#else
    Lock();
    unsigned char buf[960*(STEREO_EN+1)] = {};
	int ret = GetPcmFrame(buf);
	if (ret < 0)
	{
		UnLock();
		return true;
	}
    memcpy(_recordingBuffer,buf,960*(STEREO_EN+1));
#if 0
    if(IsTeacher()) {
        if(tea_fd == 0) {
             tea_fd = open("/hejl/webrtc/tea1.pcm",O_CREAT | O_TRUNC | O_WRONLY,0666);
        }
        write(tea_fd,buf,960*(STEREO_EN+1));
    }
#endif
#endif
    _ptrAudioBuffer->SetRecordedBuffer(_recordingBuffer,
                                       _recordingFramesIn10MS);

    uint32_t currentMicLevel = 0;
    uint32_t newMicLevel = 0;

    if (AGC())
    {
        // store current mic level in the audio buffer if AGC is enabled
        if (MicrophoneVolume(currentMicLevel) == 0)
        {
            if (currentMicLevel == 0xffffffff)
                currentMicLevel = 100;
            // this call does not affect the actual microphone volume
            _ptrAudioBuffer->SetCurrentMicLevel(currentMicLevel);
        }
    }

    // calculate delay
	// TODO(xians): Shall we add 10ms buffer delay to the record delay?
    _ptrAudioBuffer->SetVQEData(
        100,
        0, 0);

    _ptrAudioBuffer->SetTypingStatus(KeyPressed());

    // Deliver recorded samples at specified sample rate, mic level etc.
    // to the observer using callback.
    UnLock();
    _ptrAudioBuffer->DeliverRecordedData();
    Lock();

    if (AGC())
    {
        newMicLevel = _ptrAudioBuffer->NewMicLevel();
        if (newMicLevel != 0)
        {
            // The VQE will only deliver non-zero microphone levels when a
            // change is needed. Set this new mic level (received from the
            // observer as return value in the callback).
            if (SetMicrophoneVolume(newMicLevel) == -1)
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                             "  the required modification of the "
                             "microphone volume failed");
        }
    }

    UnLock();
    return true;
}


bool AudioDeviceLinuxALSA::KeyPressed() const{
  return false;
}
}  // namespace webrtc
