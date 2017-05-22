/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_EXAMPLES_PEERCONNECTION_CLIENT_PEER_CONNECTION_CLIENT_H_
#define WEBRTC_EXAMPLES_PEERCONNECTION_CLIENT_PEER_CONNECTION_CLIENT_H_
#pragma once

#include <map>
#include <memory>
#include <string>

#include "webrtc/base/nethelpers.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/signalthread.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/json.h"

typedef std::map<std::string, std::string> Peers;

struct PeerConnectionClientObserver {
  virtual void OnSignedIn(const std::string& sysinfo) = 0;  // Called when we're logged on.
  virtual void OnDisconnected(std::string* call_id = NULL) = 0;
  //virtual void OnPeerConnected(int id, const std::string& name) = 0;
  //virtual void OnPeerDisconnected(std::string& call_id) = 0;
  //virtual void OnMessageFromPeer(int peer_id, const std::string& message) = 0;
  //virtual void OnMessageSent(int err) = 0;
  virtual void OnServerConnectionFailure() = 0;
  virtual void OnConfCreate(const std::string& conf_id, int err) = 0;
  virtual void OnConfNotify(std::string& conf_id, std::string& lecturer, 
	                           std::vector<std::string>& memb_array,
	                           int start_time, int duration,
	                           std::string& description) = 0;
  virtual void OnCloseConf(std::string& result) = 0;
  virtual int OnRecvOffer(std::string& callid, std::string& from, 
  								std::string& to, std::string& sdp) = 0;
  virtual void OnRecvRinging(std::string& callid) = 0;
  virtual int OnRecvAnswer(std::string& callid, std::string& caller, 
  								std::string& callee, std::string& sdp) = 0;
  virtual void OnSendCandidate(std::string& callid) = 0;
  virtual void OnRecvCandidate(std::string& callid, int index, 
							          std::string& sdpmid, std::string& candidate) = 0;
  virtual void OnRecvAck(std::string& callid, std::string& method) = 0;
  virtual void OnRecvBye(std::string& callid) = 0;
  virtual void OnRecvFailed(std::string& callid, int error) = 0;
  virtual void OnCallMsgTimeout(std::string& callid, std::string& method) = 0;
  virtual bool OnDirConnected(std::string& peer_id, std::string& call_id) = 0;
  virtual void OnDirDisconnected(std::string& call_id) = 0;
  virtual void OnGetGroupInfo(Json::Value& group1, Json::Value& group2, Json::Value& group3) = 0;
  virtual void OnGetUserInfo(Json::Value& terminal) = 0;

 protected:
  virtual ~PeerConnectionClientObserver() {}
};

class PeerConnectionClient : public sigslot::has_slots<>,
                             public rtc::MessageHandler {
 public:
  enum State {
    NOT_CONNECTED,
    RESOLVING,
    SIGNING_IN,
    CONNECTED,
    SIGNING_OUT_WAITING,
    SIGNING_OUT,
  };

  enum MsgId {
    RECONNECT,
    HEARTBEAT,
    CHECKSOCKET,
    CALL = 0x10000,
    CONF = 0x20000,
  };

  struct DirSocket
  {
	//std::unique_ptr<rtc::AsyncSocket> socket;
	rtc::AsyncSocket* socket;
	std::string call_id;
	std::string peer_id;
	std::string recv_data;
	std::string peer_ip;
	int port;
  };
  typedef std::vector<DirSocket> DirSockets;

  PeerConnectionClient();
  ~PeerConnectionClient();

  std::string id() const;
  std::string ip() const;
  bool is_connected() const;
  const Peers& peers() const;

  void RegisterObserver(PeerConnectionClientObserver* callback);

  void Connect(const std::string& server, int port,
                   const std::string& client_name);
  bool DirConnect(std::string& peer_id, std::string& call_id, 
						const std::string& server, int port);
  void DirListen(int port);
  void DirKeepAlive(void);
  void CheckSocket(rtc::AsyncSocket* socket);
  //bool SendToPeer(int peer_id, const std::string& message);
  //bool SendHangUp(int peer_id);
  bool IsSendingMessage();

  bool SignOut();

  bool SendCallOffer(std::string& from, std::string& to, 
						   std::string& callid, std::string& sdp);
  bool SendCallAnswer(std::string& from, std::string& to, 
							std::string& callid, std::string& sdp);
  bool SendCallCandidate(std::string& from, std::string& to, std::string& callid, 
  								std::string& sdpmid, int index, std::string& candidate);
  bool SendCallBye(std::string& from, std::string& to, std::string& callid);

  // implements the MessageHandler interface
  void OnMessage(rtc::Message* msg);
  void SetClientId(std::string client_id);
  void SetClientMac(std::string client_mac);
  void SetClientIp(std::string client_ip);
 protected:
  void DoConnect();
  void Close();
  void InitSocketSignals(rtc::AsyncSocket* socket);
  bool ConnectControlSocket();
  void OnConnect(rtc::AsyncSocket* socket);
  void OnHangingGetConnect(rtc::AsyncSocket* socket);
  //void OnMessageFromPeer(int peer_id, const std::string& message);

  // Returns true if the whole response has been read.
  bool ReadIntoBuffer(rtc::AsyncSocket* socket, std::string* data, std::string &body_data);
  void OnRead(rtc::AsyncSocket* socket);
  DirSocket* FindDirSocket(rtc::AsyncSocket* socket);
  DirSocket* FindDirSocket(std::string& call_id);
  void EraseDirSocket(DirSocket* dir);
  void CloseDirSocket(std::string& call_id);
  //int GetResponseStatus(const std::string& response);
  void OnClose(rtc::AsyncSocket* socket, int err);
  void OnResolveResult(rtc::AsyncResolverInterface* resolver);
  int Sequence(void);
  void GenerateHead(char *buffer, int len);
  void GenerateConnAuth(void);
  bool HandleMessage(const std::string& message);
  bool HandleCallMessage(const std::string& message, std::string& call_id);
  bool SendMessage(Json::Value& jmessage);
  void SendConnLogin(std::string& nonces);
  void SendConnGetGroup(void);
  void SendConnGetList(void);
  void SendConnLogout(void);
  void SendConnHeart(void);
  bool HandleConnect(const Json::Value& jmessage);
  void SendConfCreate(std::string& lecturer, 
                           std::vector<std::string>& memb_array,
                           int start_time, int duration,
                           std::string& description);
  void SendConfNotifyAck(std::string& conf_id, unsigned int seq);
  void SendConfClose(std::string& conf_id);
  bool HandleConference(const Json::Value& jmessage);

  bool SendCallRinging(std::string& from, std::string& to, 
  								std::string& callid, unsigned int seq);
  bool SendCallFailed(std::string& from, std::string& to, std::string& callid, 
  							int code, unsigned int seq);
  bool SendCallKeepAlive(std::string& from, std::string& to, 
													std::string& callid);
  bool SendCallAck(std::string& from, std::string& to, std::string& callid, 
  						std::string& method, unsigned int seq);
  void InsertCallTimeoutFun(std::string& callid, 
							std::string method, unsigned short seq);
  void DeleteCallTimeoutFun(unsigned short seq);
  void MonitorSocketTimeout(rtc::AsyncSocket* socket, int delay = -1);
  bool HandleCall(const Json::Value& jmessage, std::string* call_id = NULL);

  struct CallMessage
  {
	std::string call_id;
	std::string method;
  };
  typedef std::map<unsigned int, CallMessage* > CallMessageMap;

  PeerConnectionClientObserver* callback_;
  rtc::SocketAddress server_address_;
  rtc::AsyncResolver* resolver_;
  std::unique_ptr<rtc::AsyncSocket> control_socket_;
  std::unique_ptr<rtc::AsyncSocket> listen_socket_;
  std::unique_ptr<rtc::AsyncSocket> socket_;
  int listen_port_;
  DirSockets dir_sockets_;//direct connect sockets
  std::string onconnect_data_;
  std::string control_data_;
  std::string body_data_;
  size_t body_len_;
  std::string notification_data_;
  std::string client_name_;
  std::string client_mac_;
  std::string prot_verion_;
  std::string client_id_;
  std::string local_ip_;
  Peers peers_;
  State state_;
  int my_id_;
  unsigned short sequence_;
  CallMessageMap call_timeout_msg_;
};

#endif  // WEBRTC_EXAMPLES_PEERCONNECTION_CLIENT_PEER_CONNECTION_CLIENT_H_
