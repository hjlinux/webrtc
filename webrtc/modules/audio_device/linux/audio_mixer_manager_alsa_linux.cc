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

#include "webrtc/modules/audio_device/linux/audio_mixer_manager_alsa_linux.h"
#include "webrtc/system_wrappers/include/trace.h"


namespace webrtc
{

AudioMixerManagerLinuxALSA::AudioMixerManagerLinuxALSA(const int32_t id) :
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id)//,
    //_outputMixerHandle(NULL),
    //_inputMixerHandle(NULL),
    //_outputMixerElement(NULL),
    //_inputMixerElement(NULL)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s constructed", __FUNCTION__);

    memset(_outputMixerStr, 0, kAdmMaxDeviceNameSize);
    memset(_inputMixerStr, 0, kAdmMaxDeviceNameSize);
}

AudioMixerManagerLinuxALSA::~AudioMixerManagerLinuxALSA()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s destructed", __FUNCTION__);

    Close();

    delete &_critSect;
}

// ============================================================================
//                                    PUBLIC METHODS
// ============================================================================

int32_t AudioMixerManagerLinuxALSA::Close()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s",
                 __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    CloseSpeaker();
    CloseMicrophone();

    return 0;

}

int32_t AudioMixerManagerLinuxALSA::CloseSpeaker()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s",
                 __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

#if 0
    if (_outputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing playout mixer");

 
        _outputMixerHandle = NULL;
        _outputMixerElement = NULL;
    }
    memset(_outputMixerStr, 0, kAdmMaxDeviceNameSize);
#endif
    return 0;
}

int32_t AudioMixerManagerLinuxALSA::CloseMicrophone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    //int errVal = 0;
#if 0
    if (_inputMixerHandle != NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Closing record mixer");

        _inputMixerHandle = NULL;
        _inputMixerElement = NULL;
    }
#endif
    memset(_inputMixerStr, 0, kAdmMaxDeviceNameSize);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::OpenSpeaker(char* deviceName)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::OpenSpeaker(name=%s)", deviceName);

    CriticalSectionScoped lock(&_critSect);

    //int errVal = 0;

    // Close any existing output mixer handle
    //


    char controlName[kAdmMaxDeviceNameSize] = { 0 };
    GetControlName(controlName, deviceName);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     snd_mixer_attach(_outputMixerHandle, %s)", controlName);

 
    strcpy(_outputMixerStr, controlName);


    // Load and find the proper mixer element
    if (LoadSpeakerMixerElement() < 0)
    {
        return -1;
    }

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::OpenMicrophone(char *deviceName)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::OpenMicrophone(name=%s)",
                 deviceName);

    CriticalSectionScoped lock(&_critSect);

    //int errVal = 0;

    // Close any existing input mixer handle


    char controlName[kAdmMaxDeviceNameSize] = { 0 };
    GetControlName(controlName, deviceName);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     snd_mixer_attach(_inputMixerHandle, %s)", controlName);

    strcpy(_inputMixerStr, controlName);


    // Load and find the proper mixer element
    if (LoadMicMixerElement() < 0)
    {
        return -1;
    }

    return 0;
}

bool AudioMixerManagerLinuxALSA::SpeakerIsInitialized() const
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    return 1;
}

bool AudioMixerManagerLinuxALSA::MicrophoneIsInitialized() const
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s",
                 __FUNCTION__);

    return 1;
}

int32_t AudioMixerManagerLinuxALSA::SetSpeakerVolume(
    uint32_t volume)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::SetSpeakerVolume(volume=%u)",
                 volume);

    CriticalSectionScoped lock(&_critSect);



	return -1;

    //return (0);
}

int32_t AudioMixerManagerLinuxALSA::SpeakerVolume(
    uint32_t& volume) const
{

    long int vol(0);


    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     AudioMixerManagerLinuxALSA::SpeakerVolume() => vol=%i",
                 vol);

    volume = static_cast<uint32_t> (vol);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MaxSpeakerVolume(
    uint32_t& maxVolume) const
{


    //long int minVol(0);
    long int maxVol(0);

 
    maxVolume = static_cast<uint32_t> (maxVol);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MinSpeakerVolume(
    uint32_t& minVolume) const
{



    long int minVol(0);
    //long int maxVol(0);



    minVolume = static_cast<uint32_t> (minVol);

    return 0;
}

// TL: Have done testnig with these but they don't seem reliable and
// they were therefore not added
/*
 // ----------------------------------------------------------------------------
 //    SetMaxSpeakerVolume
 // ----------------------------------------------------------------------------

 int32_t AudioMixerManagerLinuxALSA::SetMaxSpeakerVolume(
     uint32_t maxVolume)
 {

 if (_outputMixerElement == NULL)
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
 "  no avaliable output mixer element exists");
 return -1;
 }

 long int minVol(0);
 long int maxVol(0);

 int errVal = snd_mixer_selem_get_playback_volume_range(
 _outputMixerElement, &minVol, &maxVol);
 if ((maxVol <= minVol) || (errVal != 0))
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "     Error getting playback volume range: %s", snd_strerror(errVal));
 }

 maxVol = maxVolume;
 errVal = snd_mixer_selem_set_playback_volume_range(
 _outputMixerElement, minVol, maxVol);
 WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
  "     Playout hardware volume range, min: %d, max: %d", minVol, maxVol);
 if (errVal != 0)
 {
 WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
  "     Error setting playback volume range: %s", snd_strerror(errVal));
 return -1;
 }

 return 0;
 }

 // ----------------------------------------------------------------------------
 //    SetMinSpeakerVolume
 // ----------------------------------------------------------------------------

 int32_t AudioMixerManagerLinuxALSA::SetMinSpeakerVolume(
     uint32_t minVolume)
 {

 if (_outputMixerElement == NULL)
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
 "  no avaliable output mixer element exists");
 return -1;
 }

 long int minVol(0);
 long int maxVol(0);

 int errVal = snd_mixer_selem_get_playback_volume_range(
 _outputMixerElement, &minVol, &maxVol);
 if ((maxVol <= minVol) || (errVal != 0))
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "     Error getting playback volume range: %s", snd_strerror(errVal));
 }

 minVol = minVolume;
 errVal = snd_mixer_selem_set_playback_volume_range(
 _outputMixerElement, minVol, maxVol);
 WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
 "     Playout hardware volume range, min: %d, max: %d", minVol, maxVol);
 if (errVal != 0)
 {
 WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
 "     Error setting playback volume range: %s", snd_strerror(errVal));
 return -1;
 }

 return 0;
 }
 */

int32_t AudioMixerManagerLinuxALSA::SpeakerVolumeStepSize(
    uint16_t& stepSize) const
{


    // The step size is always 1 for ALSA
    stepSize = 1;

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::SpeakerVolumeIsAvailable(
    bool& available)
{

    return -1;
}

int32_t AudioMixerManagerLinuxALSA::SpeakerMuteIsAvailable(
    bool& available)
{


    return -1;
}

int32_t AudioMixerManagerLinuxALSA::SetSpeakerMute(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::SetSpeakerMute(enable=%u)",
                 enable);

   // CriticalSectionScoped lock(&_critSect);


    return -1;
}

int32_t AudioMixerManagerLinuxALSA::SpeakerMute(bool& enabled) const
{


    return -1;


    int value(false);


    // Note value = 0 (off) means muted
    enabled = (bool) !value;

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneMuteIsAvailable(
    bool& available)
{

	return -1;

}

int32_t AudioMixerManagerLinuxALSA::SetMicrophoneMute(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::SetMicrophoneMute(enable=%u)",
                 enable);

    CriticalSectionScoped lock(&_critSect);


    // Ensure that the selected microphone destination has a valid mute control.
    bool available(false);
    MicrophoneMuteIsAvailable(available);
    if (!available)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  it is not possible to mute the microphone");
        return -1;
    }

    return (0);
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneMute(bool& enabled) const
{

    int value(false);

    // Note value = 0 (off) means muted
    enabled = (bool) !value;

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneBoostIsAvailable(
    bool& available)
{

    // Microphone boost cannot be enabled through ALSA Simple Mixer Interface
    available = false;

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::SetMicrophoneBoost(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioMixerManagerLinuxALSA::SetMicrophoneBoost(enable=%u)",
                 enable);

    CriticalSectionScoped lock(&_critSect);


    // Ensure that the selected microphone destination has a valid mute control.
    bool available(false);
    MicrophoneMuteIsAvailable(available);
    if (!available)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  it is not possible to enable microphone boost");
        return -1;
    }

    // It is assumed that the call above fails!

    return (0);
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneBoost(bool& enabled) const
{

    // Microphone boost cannot be enabled on this platform!
    enabled = false;

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneVolumeIsAvailable(
    bool& available)
{

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::SetMicrophoneVolume(
    uint32_t volume)
{

    CriticalSectionScoped lock(&_critSect);

    return (0);
}

// TL: Have done testnig with these but they don't seem reliable and
// they were therefore not added
/*
 // ----------------------------------------------------------------------------
 //    SetMaxMicrophoneVolume
 // ----------------------------------------------------------------------------

 int32_t AudioMixerManagerLinuxALSA::SetMaxMicrophoneVolume(
     uint32_t maxVolume)
 {

 if (_inputMixerElement == NULL)
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "  no avaliable output mixer element exists");
 return -1;
 }

 long int minVol(0);
 long int maxVol(0);

 int errVal = snd_mixer_selem_get_capture_volume_range(_inputMixerElement,
  &minVol, &maxVol);
 if ((maxVol <= minVol) || (errVal != 0))
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "     Error getting capture volume range: %s", snd_strerror(errVal));
 }

 maxVol = (long int)maxVolume;
 printf("min %d max %d", minVol, maxVol);
 errVal = snd_mixer_selem_set_capture_volume_range(_inputMixerElement, minVol, maxVol);
 WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
 "     Capture hardware volume range, min: %d, max: %d", minVol, maxVol);
 if (errVal != 0)
 {
 WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
  "     Error setting capture volume range: %s", snd_strerror(errVal));
 return -1;
 }

 return 0;
 }

 // ----------------------------------------------------------------------------
 //    SetMinMicrophoneVolume
 // ----------------------------------------------------------------------------

 int32_t AudioMixerManagerLinuxALSA::SetMinMicrophoneVolume(
 uint32_t minVolume)
 {

 if (_inputMixerElement == NULL)
 {
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "  no avaliable output mixer element exists");
 return -1;
 }

 long int minVol(0);
 long int maxVol(0);

 int errVal = snd_mixer_selem_get_capture_volume_range(
 _inputMixerElement, &minVol, &maxVol);
 if (maxVol <= minVol)
 {
 //maxVol = 255;
 WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
  "     Error getting capture volume range: %s", snd_strerror(errVal));
 }

 printf("min %d max %d", minVol, maxVol);
 minVol = (long int)minVolume;
 errVal = snd_mixer_selem_set_capture_volume_range(
 _inputMixerElement, minVol, maxVol);
 WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
  "     Capture hardware volume range, min: %d, max: %d", minVol, maxVol);
 if (errVal != 0)
 {
 WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
  "     Error setting capture volume range: %s", snd_strerror(errVal));
 return -1;
 }

 return 0;
 }
 */

int32_t AudioMixerManagerLinuxALSA::MicrophoneVolume(
    uint32_t& volume) const
{

//    volume = static_cast<uint32_t> (vol);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MaxMicrophoneVolume(
    uint32_t& maxVolume) const
{

    //long int minVol(0);
    long int maxVol(0);


    maxVolume = static_cast<uint32_t> (maxVol);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MinMicrophoneVolume(
    uint32_t& minVolume) const
{

    long int minVol(0);
    //long int maxVol(0);

    minVolume = static_cast<uint32_t> (minVol);

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::MicrophoneVolumeStepSize(
    uint16_t& stepSize) const
{

    // The step size is always 1 for ALSA
    stepSize = 1;

    return 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

int32_t AudioMixerManagerLinuxALSA::LoadMicMixerElement() const
{

    return 0;
}

int32_t AudioMixerManagerLinuxALSA::LoadSpeakerMixerElement() const
{

    return 0;
}

void AudioMixerManagerLinuxALSA::GetControlName(char* controlName,
                                                char* deviceName) const
{
    // Example
    // deviceName: "front:CARD=Intel,DEV=0"
    // controlName: "hw:CARD=Intel"
    char* pos1 = strchr(deviceName, ':');
    char* pos2 = strchr(deviceName, ',');
    if (!pos2)
    {
        // Can also be default:CARD=Intel
        pos2 = &deviceName[strlen(deviceName)];
    }
    if (pos1 && pos2)
    {
        strcpy(controlName, "hw");
        int nChar = (int) (pos2 - pos1);
        strncpy(&controlName[2], pos1, nChar);
        controlName[2 + nChar] = '\0';
    } else
    {
        strcpy(controlName, deviceName);
    }

}

}
