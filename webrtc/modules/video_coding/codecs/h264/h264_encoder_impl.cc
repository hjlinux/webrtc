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

#include "webrtc/modules/video_coding/codecs/h264/h264_encoder_impl.h"

#include <limits>

#include "third_party/openh264/src/codec/api/svc/codec_api.h"
#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_def.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/include/metrics.h"



extern "C" {
#include "capture.h"
#include "h264_enc.h"

}  // extern "C"


namespace webrtc {

namespace {


// Used by histograms. Values of entries should not be changed.
enum H264EncoderImplEvent {
  kH264EncoderEventInit = 0,
  kH264EncoderEventError = 1,
  kH264EncoderEventMax = 16,
};

int NumberOfThreads(int width, int height, int number_of_cores) {
  // TODO(hbos): In Chromium, multiple threads do not work with sandbox on Mac,
  // see crbug.com/583348. Until further investigated, only use one thread.
//  if (width * height >= 1920 * 1080 && number_of_cores > 8) {
//    return 8;  // 8 threads for 1080p on high perf machines.
//  } else if (width * height > 1280 * 960 && number_of_cores >= 6) {
//    return 3;  // 3 threads for 1080p.
//  } else if (width * height > 640 * 480 && number_of_cores >= 3) {
//    return 2;  // 2 threads for qHD/HD.
//  } else {
//    return 1;  // 1 thread for VGA or less.
//  }
  return 2;
}

FrameType ConvertToVideoFrameType(EVideoFrameType type) {
  switch (type) {
    case videoFrameTypeIDR:
      return kVideoFrameKey;
    case videoFrameTypeSkip:
    case videoFrameTypeI:
    case videoFrameTypeP:
    case videoFrameTypeIPMixed:
      return kVideoFrameDelta;
    case videoFrameTypeInvalid:
      break;
  }
  RTC_NOTREACHED() << "Unexpected/invalid frame type: " << type;
  return kEmptyFrame;
}


typedef struct _RtpFagParam_
{
	EncodedImage* encoded_image;
	std::unique_ptr<uint8_t[]>* encoded_image_buffer;
	RTPFragmentationHeader* frag_header;
}RtpFagParam;


static void RtpFragmentize(void *param, nal_info *pinfo)
{
	RtpFagParam *p = (RtpFagParam *)param;

	EncodedImage* encoded_image = p->encoded_image;
	std::unique_ptr<uint8_t[]>* encoded_image_buffer = p->encoded_image_buffer;
	RTPFragmentationHeader* frag_header = p->frag_header;

	// Calculate minimum buffer size required to hold encoded data.
	size_t required_size = 0;
	size_t fragments_count = 0;

	for (int nal = 0; nal < pinfo->num_nals; ++nal, ++fragments_count) {
		// Ensure |required_size| will not overflow.
		required_size += pinfo->nals[nal].len;
	}

	if (encoded_image->_size < required_size) {
	// Increase buffer size. Allocate enough to hold an unencoded image, this
	// should be more than enough to hold any encoded data of future frames of
	// the same size (avoiding possible future reallocation due to variations in
	// required size).
		encoded_image->_size = CalcBufferSize(kI420, encoded_image->_encodedWidth, 
		                                      encoded_image->_encodedHeight);
		if (encoded_image->_size < required_size) {
			// Encoded data > unencoded data. Allocate required bytes.
			LOG(LS_WARNING) << "Encoding produced more bytes than the original image "
			              << "data! Original bytes: " << encoded_image->_size
			              << ", encoded bytes: " << required_size << ".";
			encoded_image->_size = required_size;
		}
		encoded_image->_buffer = new uint8_t[encoded_image->_size];
		encoded_image_buffer->reset(encoded_image->_buffer);
	}

  // Iterate layers and NAL units, note each NAL unit as a fragment and copy
  // the data to |encoded_image->_buffer|.
	const uint8_t start_code[4] = {0, 0, 0, 1};
	frag_header->VerifyAndAllocateFragmentationHeader(fragments_count);
	size_t frag = 0;
	encoded_image->_length = 0;
	int nal_len = 0;

	for (int nal = 0; nal < pinfo->num_nals; ++nal, ++frag) 
	{
		nal_len = pinfo->nals[nal].len - sizeof(start_code);
		frag_header->fragmentationOffset[frag] = encoded_image->_length + sizeof(start_code);
		frag_header->fragmentationLength[frag] = nal_len;
		
		// Copy the entire layer's data (including start codes).
		memcpy(encoded_image->_buffer + encoded_image->_length,
		       pinfo->nals[nal].buf,
		       pinfo->nals[nal].len);
		encoded_image->_length += pinfo->nals[nal].len;
	}
	if (pinfo->idr_flag == true)
	{
		encoded_image->_frameType = kVideoFrameKey;
	}
	else
	{
		encoded_image->_frameType = kVideoFrameDelta;
	}
}


}  // namespace

#if 0
// Helper method used by H264EncoderImpl::Encode.
// Copies the encoded bytes from |info| to |encoded_image| and updates the
// fragmentation information of |frag_header|. The |encoded_image->_buffer| may
// be deleted and reallocated if a bigger buffer is required.
//
// After OpenH264 encoding, the encoded bytes are stored in |info| spread out
// over a number of layers and "NAL units". Each NAL unit is a fragment starting
// with the four-byte start code {0,0,0,1}. All of this data (including the
// start codes) is copied to the |encoded_image->_buffer| and the |frag_header|
// is updated to point to each fragment, with offsets and lengths set as to
// exclude the start codes.
static void RtpFragmentize(EncodedImage* encoded_image,
                           std::unique_ptr<uint8_t[]>* encoded_image_buffer,
                           const VideoFrame& frame,
                           SFrameBSInfo* info,
                           RTPFragmentationHeader* frag_header) {
  // Calculate minimum buffer size required to hold encoded data.
  size_t required_size = 0;
  size_t fragments_count = 0;
  for (int layer = 0; layer < info->iLayerNum; ++layer) {
    const SLayerBSInfo& layerInfo = info->sLayerInfo[layer];
    for (int nal = 0; nal < layerInfo.iNalCount; ++nal, ++fragments_count) {
      RTC_CHECK_GE(layerInfo.pNalLengthInByte[nal], 0);
      // Ensure |required_size| will not overflow.
      RTC_CHECK_LE(static_cast<size_t>(layerInfo.pNalLengthInByte[nal]),
                   std::numeric_limits<size_t>::max() - required_size);
      required_size += layerInfo.pNalLengthInByte[nal];
    }
  }
  if (encoded_image->_size < required_size) {
    // Increase buffer size. Allocate enough to hold an unencoded image, this
    // should be more than enough to hold any encoded data of future frames of
    // the same size (avoiding possible future reallocation due to variations in
    // required size).
    encoded_image->_size = CalcBufferSize(kI420, frame.width(), frame.height());
    if (encoded_image->_size < required_size) {
      // Encoded data > unencoded data. Allocate required bytes.
      LOG(LS_WARNING) << "Encoding produced more bytes than the original image "
                      << "data! Original bytes: " << encoded_image->_size
                      << ", encoded bytes: " << required_size << ".";
      encoded_image->_size = required_size;
    }
    encoded_image->_buffer = new uint8_t[encoded_image->_size];
    encoded_image_buffer->reset(encoded_image->_buffer);
  }

  // Iterate layers and NAL units, note each NAL unit as a fragment and copy
  // the data to |encoded_image->_buffer|.
  const uint8_t start_code[4] = {0, 0, 0, 1};
  frag_header->VerifyAndAllocateFragmentationHeader(fragments_count);
  size_t frag = 0;
  encoded_image->_length = 0;
  for (int layer = 0; layer < info->iLayerNum; ++layer) {
    const SLayerBSInfo& layerInfo = info->sLayerInfo[layer];
    // Iterate NAL units making up this layer, noting fragments.
    size_t layer_len = 0;
    for (int nal = 0; nal < layerInfo.iNalCount; ++nal, ++frag) {
      // Because the sum of all layer lengths, |required_size|, fits in a
      // |size_t|, we know that any indices in-between will not overflow.
      RTC_DCHECK_GE(layerInfo.pNalLengthInByte[nal], 4);
      RTC_DCHECK_EQ(layerInfo.pBsBuf[layer_len+0], start_code[0]);
      RTC_DCHECK_EQ(layerInfo.pBsBuf[layer_len+1], start_code[1]);
      RTC_DCHECK_EQ(layerInfo.pBsBuf[layer_len+2], start_code[2]);
      RTC_DCHECK_EQ(layerInfo.pBsBuf[layer_len+3], start_code[3]);
      frag_header->fragmentationOffset[frag] =
          encoded_image->_length + layer_len + sizeof(start_code);
      frag_header->fragmentationLength[frag] =
          layerInfo.pNalLengthInByte[nal] - sizeof(start_code);
      layer_len += layerInfo.pNalLengthInByte[nal];
    }
    // Copy the entire layer's data (including start codes).
    memcpy(encoded_image->_buffer + encoded_image->_length,
           layerInfo.pBsBuf,
           layer_len);
    encoded_image->_length += layer_len;
  }
}
#endif
H264EncoderImpl::H264EncoderImpl()
    : encoded_image_callback_(nullptr),
      has_reported_init_(false),
      has_reported_error_(false) {
}

H264EncoderImpl::~H264EncoderImpl() {
  Release();
}


int32_t H264EncoderImpl::InitEncode(const VideoCodec* codec_settings,
                                    int32_t number_of_cores,
                                    size_t /*max_payload_size*/) {
  ReportInit();
  if (!codec_settings ||
      codec_settings->codecType != kVideoCodecH264) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings->maxFramerate == 0) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_settings->width < 1 || codec_settings->height < 1) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  int32_t release_ret = Release();
  if (release_ret != WEBRTC_VIDEO_CODEC_OK) {
    ReportError();
    return release_ret;
  }
  venc_ch_ = -1;
#if 1
  if (has_init_ == false)
  {
	  // Create encoder.
	  printf("InitVenc\n");
        /*
	  if (InitVenc(venc_ch_) != 0) {
	    // Failed to create encoder.
	    LOG(LS_ERROR) << "Failed to init H264 encoder";
	    ReportError();
	    return WEBRTC_VIDEO_CODEC_ERROR;
	  }
      */
	  has_init_ = true;
  }
#endif

  codec_settings_ = *codec_settings;
  if (codec_settings_.targetBitrate == 0)
    codec_settings_.targetBitrate = codec_settings_.startBitrate;
  //codec_settings_.startBitrate = 512;
  //codec_settings_.targetBitrate = 512;

  // Initialization parameters.
  // There are two ways to initialize. There is SEncParamBase (cleared with
  // memset(&p, 0, sizeof(SEncParamBase)) used in Initialize, and SEncParamExt
  // which is a superset of SEncParamBase (cleared with GetDefaultParams) used
  // in InitializeExt.
#if 0
  SEncParamExt init_params;
  openh264_encoder_->GetDefaultParams(&init_params);
  if (codec_settings_.mode == kRealtimeVideo) {
    init_params.iUsageType = CAMERA_VIDEO_REAL_TIME;
  } else if (codec_settings_.mode == kScreensharing) {
    init_params.iUsageType = SCREEN_CONTENT_REAL_TIME;
  } else {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  init_params.iPicWidth = codec_settings_.width;
  init_params.iPicHeight = codec_settings_.height;
  // |init_params| uses bit/s, |codec_settings_| uses kbit/s.
  init_params.iTargetBitrate = codec_settings_.targetBitrate * 1000;
  init_params.iMaxBitrate = codec_settings_.maxBitrate * 1000;
  // Rate Control mode
  init_params.iRCMode = RC_BITRATE_MODE;
  init_params.fMaxFrameRate = static_cast<float>(codec_settings_.maxFramerate);

  // The following parameters are extension parameters (they're in SEncParamExt,
  // not in SEncParamBase).
  init_params.bEnableFrameSkip =
      codec_settings_.codecSpecific.H264.frameDroppingOn;
  // |uiIntraPeriod|    - multiple of GOP size
  // |keyFrameInterval| - number of frames
  init_params.uiIntraPeriod =
      codec_settings_.codecSpecific.H264.keyFrameInterval;
  init_params.uiMaxNalSize = 0;
  // Threading model: use auto.
  //  0: auto (dynamic imp. internal encoder)
  //  1: single thread (default value)
  // >1: number of threads
  init_params.iMultipleThreadIdc = NumberOfThreads(init_params.iPicWidth,
                                                   init_params.iPicHeight,
                                                   number_of_cores);
  // The base spatial layer 0 is the only one we use.
  init_params.sSpatialLayers[0].iVideoWidth        = init_params.iPicWidth;
  init_params.sSpatialLayers[0].iVideoHeight       = init_params.iPicHeight;
  init_params.sSpatialLayers[0].fFrameRate         = init_params.fMaxFrameRate;
  init_params.sSpatialLayers[0].iSpatialBitrate    = init_params.iTargetBitrate;
  init_params.sSpatialLayers[0].iMaxSpatialBitrate = init_params.iMaxBitrate;
  // Slice num according to number of threads.
  init_params.sSpatialLayers[0].sSliceCfg.uiSliceMode = SM_AUTO_SLICE;
#endif

  // Initialize encoded image. Default buffer size: size of unencoded data.
  encoded_image_._size = CalcBufferSize(
      kI420, codec_settings_.width, codec_settings_.height);
  encoded_image_._buffer = new uint8_t[encoded_image_._size];
  encoded_image_buffer_.reset(encoded_image_._buffer);
  encoded_image_._completeFrame = true;
  encoded_image_._encodedWidth = 0;
  encoded_image_._encodedHeight = 0;
  encoded_image_._length = 0;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264EncoderImpl::Release() {
  if(has_init_ == true)
  {
    ReleaseVenc(venc_ch_);
  }
  
  if (encoded_image_._buffer != nullptr) {
    encoded_image_._buffer = nullptr;
    encoded_image_buffer_.reset();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264EncoderImpl::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  encoded_image_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264EncoderImpl::SetRates(uint32_t bitrate, uint32_t framerate) {
  if (bitrate <= 0 || framerate <= 0) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

    codec_settings_.targetBitrate = bitrate;
    codec_settings_.maxFramerate = framerate;
    VencSetBitRate(venc_ch_,bitrate);
  //codec_settings_.maxFramerate = 25;

  //SBitrateInfo target_bitrate;
  //memset(&target_bitrate, 0, sizeof(SBitrateInfo));
  //target_bitrate.iLayer = SPATIAL_LAYER_ALL,
  //target_bitrate.iBitrate = codec_settings_.targetBitrate * 1000;
  //openh264_encoder_->SetOption(ENCODER_OPTION_BITRATE,
  //                            &target_bitrate);
  //float max_framerate = static_cast<float>(codec_settings_.maxFramerate);
  //openh264_encoder_->SetOption(ENCODER_OPTION_FRAME_RATE,
  //                             &max_framerate);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264EncoderImpl::Encode(
    const VideoFrame& frame, const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  if (!IsInitialized()) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (frame.IsZeroSize()) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (!encoded_image_callback_) {
    LOG(LS_WARNING) << "InitEncode() has been called, but a callback function "
                    << "has not been set with RegisterEncodeCompleteCallback()";
    ReportError();
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (frame.width()  != codec_settings_.width ||
      frame.height() != codec_settings_.height) {
    LOG(LS_WARNING) << "Encoder initialized for " << codec_settings_.width
                    << "x" << codec_settings_.height << " but trying to encode "
                    << frame.width() << "x" << frame.height() << " frame.";
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_SIZE;
  }

  bool force_key_frame = false;
  if (frame_types != nullptr) {
    // We only support a single stream.
    RTC_DCHECK_EQ(frame_types->size(), static_cast<size_t>(1));
    // Skip frame?
    if ((*frame_types)[0] == kEmptyFrame) {
      return WEBRTC_VIDEO_CODEC_OK;
    }
    // Force key frame?
    force_key_frame = (*frame_types)[0] == kVideoFrameKey;
  }
  if (force_key_frame) {
    // API doc says ForceIntraFrame(false) does nothing, but calling this
    // function forces a key frame regardless of the |bIDR| argument's value.
    // (If every frame is a key frame we get lag/delays.)
    //openh264_encoder_->ForceIntraFrame(true);
    VencRequestIDR(venc_ch_, true);
  }

  VEncStream stream;
  RtpFagParam rtp_param;
  // Split encoded image up into fragments. This also updates |encoded_image_|.
  RTPFragmentationHeader frag_header;
  
  stream.width = frame.width();
  stream.height = frame.height();
  stream.ntp_time_ms = frame.ntp_time_ms();
  stream.py = const_cast<uint8_t*>(frame.video_frame_buffer()->DataY());
  stream.pu = const_cast<uint8_t*>(frame.video_frame_buffer()->DataU());
  stream.pv = const_cast<uint8_t*>(frame.video_frame_buffer()->DataV());
/*
  if(venc_ch_ == -1)
  {
    venc_ch_ = *(unsigned char *)stream.py;    
  }
  stream.ch = venc_ch_;
*/
  stream.CopyEncData = RtpFragmentize;//call this fun when enc success
  rtp_param.encoded_image = &encoded_image_;
  rtp_param.encoded_image_buffer = &encoded_image_buffer_;
  rtp_param.frag_header = &frag_header;            
  stream.param = (void *)&rtp_param;//const_cast<void*>(rtp_param);

  encoded_image_._length = 0;
  encoded_image_._encodedWidth = frame.width();
  encoded_image_._encodedHeight = frame.height();
  encoded_image_._timeStamp = frame.timestamp();
  encoded_image_.ntp_time_ms_ = frame.ntp_time_ms();
  encoded_image_.capture_time_ms_ = frame.render_time_ms();
  encoded_image_.rotation_ = frame.rotation();
  //encoded_image_._frameType = ConvertToVideoFrameType(info.eFrameType);
  
  // Encode!
  int enc_ret = EncodeVideoStream(&stream);
  if (enc_ret != 0) {
    LOG(LS_ERROR) << "H264 frame encoding failed, EncodeFrame returned "
                  << enc_ret << ".";
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Encoder can skip frames to save bandwidth in which case
  // |encoded_image_._length| == 0.
  if (encoded_image_._length > 0) {
    // Deliver encoded image.
    CodecSpecificInfo codec_specific;
    codec_specific.codecType = kVideoCodecH264;
    encoded_image_callback_->Encoded(encoded_image_,
                                     &codec_specific,
                                     &frag_header);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

bool H264EncoderImpl::IsInitialized() const {
  return true;//openh264_encoder_ != nullptr;
}

void H264EncoderImpl::ReportInit() {
  if (has_reported_init_)
    return;
  RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264EncoderImpl.Event",
                            kH264EncoderEventInit,
                            kH264EncoderEventMax);
  has_reported_init_ = true;
}

void H264EncoderImpl::ReportError() {
  if (has_reported_error_)
    return;
  RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264EncoderImpl.Event",
                            kH264EncoderEventError,
                            kH264EncoderEventMax);
  has_reported_error_ = true;
}

int32_t H264EncoderImpl::SetChannelParameters(
    uint32_t packet_loss, int64_t rtt) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264EncoderImpl::SetPeriodicKeyFrames(bool enable) {
  return WEBRTC_VIDEO_CODEC_OK;
}

void H264EncoderImpl::OnDroppedFrame() {
}

}  // namespace webrtc