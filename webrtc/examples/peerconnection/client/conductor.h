/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_EXAMPLES_PEERCONNECTION_CLIENT_CONDUCTOR_H_
#define WEBRTC_EXAMPLES_PEERCONNECTION_CLIENT_CONDUCTOR_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/examples/peerconnection/client/main_wnd.h"
#include "webrtc/examples/peerconnection/client/linux/main_wnd.h"
#include "webrtc/examples/peerconnection/client/peer_connection_client.h"

namespace webrtc {
class VideoCaptureModule;
}  // namespace webrtc

namespace cricket {
class VideoRenderer;
}  // namespace cricket

class Conductor
  : //public webrtc::PeerConnectionObserver,
    //public webrtc::CreateSessionDescriptionObserver,
    public rtc::RefCountInterface,
    public PeerConnectionClientObserver,
    public MainProcCallback {
 public:
  enum CallbackID {
    MEDIA_CHANNELS_INITIALIZED = 1,
    PEER_CONNECTION_CLOSED,
    SEND_MESSAGE_TO_PEER,
    NEW_STREAM_ADDED,
    STREAM_REMOVED,
    UI_CMD,
  };

  Conductor(PeerConnectionClient* client, MainProc* main_proc);

  bool connection_active() const;

  virtual void Close();

  // protected:
  class PeerConnecter 
	: public webrtc::PeerConnectionObserver,
	  public webrtc::CreateSessionDescriptionObserver
  {
	public:
		PeerConnecter(std::string& call_id, std::string& local_id, 
							  std::string& peer_id, Conductor *conductor);
		~PeerConnecter();
		enum State {
		INIT,
		RINGING,
		CONNECTED,
		BYE,
		};
		//
		// PeerConnectionObserver implementation.
		//
		void OnSignalingChange(
		  webrtc::PeerConnectionInterface::SignalingState new_state) override{};
		void OnAddStream(webrtc::MediaStreamInterface* stream) override;
		void OnRemoveStream(webrtc::MediaStreamInterface* stream) override;
		void OnDataChannel(webrtc::DataChannelInterface* channel) override {}
		void OnRenegotiationNeeded() override {}
		void OnIceConnectionChange(
		  webrtc::PeerConnectionInterface::IceConnectionState new_state) override{};
		void OnIceGatheringChange(
		  webrtc::PeerConnectionInterface::IceGatheringState new_state) override{};
		void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
		void OnIceConnectionReceivingChange(bool receiving) override {}

		// CreateSessionDescriptionObserver implementation.
		virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
		virtual void OnFailure(const std::string& error);
		struct Candidate {
			std::string sdp_mid;
			std::string candidate;
			int mline_index;
		};
		typedef std::vector<Candidate> Candidates;
		//protected:
		std::string call_id_;
		State state_;
		std::string local_id_;
		std::string peer_id_;
		Candidates candidates_;
		Conductor *conductor_;
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  };
  
  typedef std::map<std::string, rtc::scoped_refptr<PeerConnecter> > ConnecterMap;
  typedef std::pair<std::string, rtc::scoped_refptr<PeerConnecter> > ConnecterPair;

 protected:
  ~Conductor();
  bool InitPeerConnectionFactory();
  PeerConnecter* InitializePeerConnection(std::string& call_id, std::string& local_id, 
							     std::string& peer_id);
  //bool InitializePeerConnection2();
  //bool ReinitializePeerConnectionForLoopback();
  PeerConnecter *CreatePeerConnection(bool dtls, std::string& call_id,
 													std::string& local_id, std::string& peer_id);
  //bool CreatePeerConnection2(bool dtls);
  void DeletePeerConnection(std::string* call_id);
  void AddStreams(PeerConnecter *connecter);
 // void AddStreams2();
  cricket::VideoCapturer* OpenVideoCaptureDevice(int id);

  //
  // PeerConnectionObserver implementation.
  //
#if 0
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override{};
  void OnAddStream(webrtc::MediaStreamInterface* stream) override;
  void OnRemoveStream(webrtc::MediaStreamInterface* stream) override;
  void OnDataChannel(webrtc::DataChannelInterface* channel) override {}
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override{};
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override{};
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override {}
#endif
  //
  // PeerConnectionClientObserver implementation.
  //

  virtual void OnSignedIn(const std::string& sysinfo);

  virtual void OnDisconnected(std::string* call_id = NULL);

  //virtual void OnPeerConnected(int id, const std::string& name);

  //virtual void OnPeerDisconnected(std::string& call_id);

  //virtual void OnMessageFromPeer(int peer_id, const std::string& message);

  //virtual void OnMessageSent(int err);

  virtual void OnServerConnectionFailure();

  virtual void OnConfCreate(const std::string& conf_id, int err);
  virtual void OnConfNotify(std::string& conf_id, std::string& lecturer, 
	                           std::vector<std::string>& memb_array,
	                           int start_time, int duration,
	                           std::string& description);
  virtual void OnCloseConf(std::string& result);
  virtual int OnRecvOffer(std::string& callid, std::string& from, std::string& to, 
  								std::string& sdp);
  virtual void OnRecvRinging(std::string& callid);
  virtual int OnRecvAnswer(std::string& callid, std::string& from, 
  								std::string& to, std::string& sdp);
  virtual void OnSendCandidate(std::string& callid);
  virtual void OnRecvCandidate(std::string& callid, int index, 
							          std::string& sdpmid, std::string& candidate);
  virtual void OnRecvAck(std::string& callid, std::string& method);
  virtual void OnRecvBye(std::string& callid);
  virtual void OnRecvFailed(std::string& callid, int error);
  virtual void OnCallMsgTimeout(std::string& callid, std::string& method);
  virtual bool OnDirConnected(std::string& peer_id, std::string& call_id);
  virtual void OnDirDisconnected(std::string& call_id);
  virtual void OnGetGroupInfo(Json::Value& group1, Json::Value& group2, 
  												Json::Value& group3);
  virtual void OnGetUserInfo(Json::Value& terminal);
  //
  // MainProcCallback implementation.
  //
  virtual void SetTeacher2();
  virtual void StartLogin(const std::string& server, int port);
  virtual void DisconnectFromServer();
  virtual bool ConnectToPeer(std::string& peer_id);
  virtual bool ConnectToPeer(std::string& peer_id, std::string& peer_ip, int peer_port);
  virtual void DisconnectFromCurrentPeer(std::string& call_id);
  virtual void DisconnectFromCallid(std::string& call_id);
  virtual void SendCandidate(PeerConnecter *connecter);
  virtual void UIThreadCallback(int msg_id, std::string call_id, void* data);
  virtual PeerConnecter* FindConnecter(std::string& call_id);
  virtual PeerConnecter* FindPeeridConnecter(std::string& peer_id);
#if 0
  // CreateSessionDescriptionObserver implementation.
  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
  virtual void OnFailure(const std::string& error);
#endif
 protected:
 	struct MessageData
	{
		std::string msg_type;//sdp or candidate
		//sdp
		std::string type;
		std::string sdp;
		//candidate
		std::string sdp_mid;
		std::string candidate;
		int mline_index;
	};
  
  // Send a message to the remote peer.
  void SendMessage(const MessageData& message, const std::string& call_id);
  std::string GenCallid(void);
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;
  PeerConnectionClient* client_;
  MainProc* main_proc_;
  //std::deque<MsgType*> pending_messages_;//std::string*
  std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >
      active_streams_;
  std::string server_;
  std::string stun_server_;
  std::string local_id_;
  std::string uuid_;
  ConnecterMap active_connecter_;
  bool lecturer_; //ÊÇ·ñÎªÖ÷½²
};

#endif  // WEBRTC_EXAMPLES_PEERCONNECTION_CLIENT_CONDUCTOR_H_
