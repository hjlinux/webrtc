/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/video_coding/codecs/h264/h264_decoder_impl.h"

#include <algorithm>
#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/keep_ref_until_done.h"
#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/include/metrics.h"

extern "C" {
#include "resource_note.h"
#include "h264_dec.h"
#include "class_interface.h"
}  // extern "C"


static int ClientChn = 0;

namespace webrtc {

namespace {

const size_t kYPlaneIndex = 0;
const size_t kUPlaneIndex = 1;
const size_t kVPlaneIndex = 2;

// Used by histograms. Values of entries should not be changed.
enum H264DecoderImplEvent {
  kH264DecoderEventInit = 0,
  kH264DecoderEventError = 1,
  kH264DecoderEventMax = 16,
};

void* Createframe(int width, int height, unsigned char *py, 
	                    unsigned char *pu, unsigned char *pv)
{
	VideoFrame* video_frame = new VideoFrame();
	video_frame->CreateFrame(py, pu, pv, width, height, width, 
	                         (width+1)/2, (width+1)/2, kVideoRotation_0);
	return (void *)video_frame;
}


}  // namespace



H264DecoderImpl::H264DecoderImpl() : pool_(true),
                                     decoded_image_callback_(nullptr),
                                     has_reported_init_(false),
                                     has_reported_error_(false) {
    vdec_ch_ = -1;
}

H264DecoderImpl::~H264DecoderImpl() {
  Release();
}

int32_t H264DecoderImpl::InitDecode(const VideoCodec* codec_settings,
                                    int32_t number_of_cores) {
  int32_t ret;
  
  ReportInit();
  if (codec_settings &&
      codec_settings->codecType != kVideoCodecH264) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // Release necessary in case of re-initializing.
  ret = Release();
  if (ret != WEBRTC_VIDEO_CODEC_OK) {
    ReportError();
    return ret;
  }

  if (codec_settings) {
    width_ = codec_settings->width;
    height_ = codec_settings->height;
  }
#if 1
  //vdec_ch_ = GetClientChn();//channel 0
  vdec_ch_ = ClientChn;//channel 0
  printf("H264DecoderImpl::InitDecode:%d\n",vdec_ch_);
  ClientChn ++;
  //vdec_fd_ = InitVdec(vdec_ch_);
#if 0
  if (vdec_fd_ < 0) {
    LOG(LS_ERROR) << "arm h264 init vdec failed.";
    Release();
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
#endif
#endif
  has_init_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264DecoderImpl::Release() {
    printf("H264DecoderImpl::Release1:%d\n",vdec_ch_);
  if(has_init_ && (vdec_ch_ >= 0))
  {
    vdec_ch_ = -1;
    has_init_ = false;
    printf("H264DecoderImpl::Release2,%d\n",vdec_ch_);
    ClientChn = 0;
    //close(vdec_fd_);
    vdec_fd_ = -1;
    ReleaseVdec(vdec_ch_);
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264DecoderImpl::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  decoded_image_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264DecoderImpl::Decode(const EncodedImage& input_image,
                                bool /*missing_frames*/,
                                const RTPFragmentationHeader* /*fragmentation*/,
                                const CodecSpecificInfo* codec_specific_info,
                                int64_t /*render_time_ms*/) {
    //printf("H264DecoderImpl::Decode\n");
  if (!IsInitialized()) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (!decoded_image_callback_) {
    LOG(LS_WARNING) << "InitDecode() has been called, but a callback function "
        "has not been set with RegisterDecodeCompleteCallback()";
    ReportError();
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (!input_image._buffer || !input_image._length) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_specific_info &&
      codec_specific_info->codecType != kVideoCodecH264) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
#if 0
  // FFmpeg requires padding due to some optimized bitstream readers reading 32
  // or 64 bits at once and could read over the end. See avcodec_decode_video2.
  RTC_CHECK_GE(input_image._size, input_image._length +
                   EncodedImage::GetBufferPaddingBytes(kVideoCodecH264));
  // "If the first 23 bits of the additional bytes are not 0, then damaged MPEG
  // bitstreams could cause overread and segfault." See
  // AV_INPUT_BUFFER_PADDING_SIZE. We'll zero the entire padding just in case.
  memset(input_image._buffer + input_image._length,
         0,
         EncodedImage::GetBufferPaddingBytes(kVideoCodecH264));
#endif

  if (input_image._length >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  VDecStream stream;
  int result;
  /*
  int fd;

  if(vdec_fd_ <= 0) {
      fd = getRemoteFd(vdec_ch_);
      if(fd <= 0) {
            return WEBRTC_VIDEO_CODEC_OK;    
      } else {
          vdec_fd_ = fd;
      }
  }
  */
  stream.fd = vdec_fd_;
  stream.ch = vdec_ch_;
  stream.pbuf_264 = input_image._buffer;
  stream.len_264 = static_cast<int>(input_image._length);
  stream.ntp = input_image.ntp_time_ms_ * 1000;  // ms -> μs
  stream.Createframe = Createframe;

  result = DecodeVideoStream(&stream);
  //printf("decode return %d\n", result);
  if (result < 0) {
    //LOG(LS_ERROR) << "decode_video failed: " << result;
    //ReportError();
    //return WEBRTC_VIDEO_CODEC_ERROR;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  VideoFrame* video_frame = (VideoFrame*)stream.pframe;//static_cast<VideoFrame*>()
  video_frame->set_timestamp(input_image._timeStamp);

  //printf("decode width %d, height %d \n", video_frame->width(), video_frame->height());

  // Return decoded frame.
  int32_t ret = decoded_image_callback_->Decoded(*video_frame);
  // Stop referencing it, possibly freeing |video_frame|.
  delete video_frame;
#if 1
  if (ret) {
    LOG(LS_WARNING) << "DecodedImageCallback::Decoded returned " << ret;
    return ret;
  }
#endif
  return WEBRTC_VIDEO_CODEC_OK;
}

bool H264DecoderImpl::IsInitialized() const {
  return has_init_;//av_context_ != nullptr;
}

void H264DecoderImpl::ReportInit() {
  if (has_reported_init_)
    return;
  RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264DecoderImpl.Event",
                            kH264DecoderEventInit,
                            kH264DecoderEventMax);
  has_reported_init_ = true;
}

void H264DecoderImpl::ReportError() {
  if (has_reported_error_)
    return;
  RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264DecoderImpl.Event",
                            kH264DecoderEventError,
                            kH264DecoderEventMax);
  has_reported_error_ = true;
}

}  // namespace webrtc
