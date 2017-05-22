/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/examples/peerconnection/client/conductor.h"

#include <memory>
#include <utility>
#include <vector>

#include "webrtc/api/test/fakeconstraints.h"
#include "webrtc/base/common.h"
#include "webrtc/base/json.h"
#include "webrtc/base/logging.h"
#include "webrtc/examples/peerconnection/client/defaults.h"
#include "webrtc/media/engine/webrtcvideocapturerfactory.h"
#include "webrtc/modules/video_capture/video_capture_factory.h"
#include "webrtc/base/md5digest.h"
#include "webrtc/base/stringencode.h"
#include <time.h>
#include "webrtc/base/signalthread.h"




namespace {
    /*
    // Names used for a IceCandidate JSON object.
    const char kCandidateSdpMidName[] = "sdpMid";
    const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
    const char kCandidateSdpName[] = "candidate";

    // Names used for a SessionDescription JSON object.
    const char kSessionDescriptionTypeName[] = "type";
    const char kSessionDescriptionSdpName[] = "sdp";
     */
#define DTLS_ON  true
#define DTLS_OFF false

    //typedef std::map<std::string, rtc::scoped_refptr<Conductor::PeerConnecter>> ConnectMap;
    //typedef std::pair<std::string, rtc::scoped_refptr<Conductor::PeerConnecter> > ConnectPair;

    class DummySetSessionDescriptionObserver
        : public webrtc::SetSessionDescriptionObserver {
            public:
                static DummySetSessionDescriptionObserver* Create() {
                    return
                        new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
                }
                virtual void OnSuccess() {
                    LOG(INFO) << "DummySetSession" << __FUNCTION__;
                }
                virtual void OnFailure(const std::string& error) {
                    LOG(INFO) << "DummySetSession" << __FUNCTION__ << " " << error;
                }

            protected:
                DummySetSessionDescriptionObserver() {}
                ~DummySetSessionDescriptionObserver() {}
        };

}//namespace

Conductor::Conductor(PeerConnectionClient* client, MainProc* main_proc)
						  : client_(client),
						    main_proc_(main_proc) {
	client_->RegisterObserver(this);
	main_proc->RegisterObserver(this);
	lecturer_ = false;
	local_id_ = client_->id();
	stun_server_ = "stun:192.168.13.218:19302";
}

Conductor::~Conductor() {
    //ASSERT(peer_connection_.get() == NULL);
}

bool Conductor::connection_active() const {
    //return peer_connection_.get() != NULL;
    return true;
}

void Conductor::Close() {
    client_->SignOut();
    DeletePeerConnection(NULL);
}

bool Conductor::InitPeerConnectionFactory() {
    LOG(INFO) << __FUNCTION__;

    if (peer_connection_factory_.get() != NULL)
        return true;
    //ASSERT(peer_connection_factory_.get() == NULL);

    peer_connection_factory_  = webrtc::CreatePeerConnectionFactory();

    if (!peer_connection_factory_.get()) {
        printf("Error Failed to initialize PeerConnectionFactory\n");
        DeletePeerConnection(0);
        return false;
    }
    return true;
}

Conductor::PeerConnecter* Conductor::InitializePeerConnection(std::string& call_id, std::string& local_id, 
        std::string& peer_id) {
    LOG(INFO) << __FUNCTION__;

    InitPeerConnectionFactory();

    ASSERT(peer_connection_factory_.get() != NULL);
    //ASSERT(peer_connection_.get() == NULL);

    PeerConnecter *connecter;

    connecter = CreatePeerConnection(DTLS_ON, call_id, local_id, peer_id);
    if (!connecter) {//DTLS_ON
        printf("Error CreatePeerConnection failed\n");
        //DeletePeerConnection();
        return NULL;
    }
    AddStreams(connecter);
    //return peer_connection_.get() != NULL;
    return connecter;
}

Conductor::PeerConnecter::PeerConnecter(
        std::string& call_id, std::string& local_id, std::string& peer_id, Conductor *conductor)
    : call_id_(call_id),
    local_id_(local_id),
    peer_id_(peer_id),
    conductor_(conductor){
        state_ = INIT;
    }

Conductor::PeerConnecter::~PeerConnecter() {

}

Conductor::PeerConnecter* Conductor::CreatePeerConnection(bool dtls, std::string& call_id,
        std::string& local_id, std::string& peer_id) {
    LOG(INFO) << __FUNCTION__;
    ASSERT(peer_connection_factory_.get() != NULL);
    //ASSERT(peer_connection_.get() == NULL);

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = stun_server_;//GetPeerConnectionString();//stun:192.168.13.214:19302
    config.servers.push_back(server);

    webrtc::FakeConstraints constraints;
    if (dtls) {
        constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
                "true");
    } else {
        constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
                "false");
    }

    rtc::scoped_refptr<PeerConnecter> connecter(
            new rtc::RefCountedObject<PeerConnecter>(call_id, local_id, peer_id, this));
    //connecter.reset(new PeerConnecter(peer_id, 1));
    connecter->peer_connection_ = peer_connection_factory_->CreatePeerConnection(
            config, &constraints, NULL, NULL, connecter);

    if (connecter->peer_connection_.get() != NULL)
    {
        active_connecter_.insert(ConnecterPair(call_id, connecter));
        return connecter;
    }
    else
    {
        //delete connecter;
        return NULL;
    }
}

void Conductor::DeletePeerConnection(std::string* call_id) {

    LOG(INFO) << __FUNCTION__;
    if (call_id)
    {
        printf("===erase active connecter, callid %s\n", call_id->c_str());
        ConnecterMap::iterator iter = active_connecter_.find(*call_id);
        printf("===erase active connecter222\n");
        if (iter != active_connecter_.end())
        {
            printf("===erase active connecter333\n");
            PeerConnecter* connecter = iter->second;
            printf("===erase active connecter444\n");
            main_proc_->StopRemoteRenderer(*call_id);
            printf("===erase active connecter555\n");
            connecter->peer_connection_ = NULL;
            printf("===erase active connecter, peer %s\n", connecter->peer_id_.c_str());
            active_connecter_.erase(iter);
        }
        else
        {
            printf("===erase active connecter,can not find callid %s\n", call_id->c_str());
        }
    }
    else
    {
        ConnecterMap::iterator it;
        for(it = active_connecter_.begin(); it != active_connecter_.end(); ++it)
        {
            PeerConnecter* connecter = it->second;
            std::string id = it->first;
            connecter->peer_connection_ = NULL;
            main_proc_->StopRemoteRenderer(id);
        }
        active_connecter_.clear();
    }

    if (active_connecter_.empty())
    {
        active_streams_.clear();
        //main_proc_->StopLocalRenderer();
        peer_connection_factory_ = NULL;
    }
}

//
// PeerConnectionObserver implementation.
//

// Called when a remote stream is added
void Conductor::PeerConnecter::OnAddStream(webrtc::MediaStreamInterface* stream) {
    LOG(INFO) << __FUNCTION__ << " " << stream->label() << " peer id " << this->peer_id_;
    stream->AddRef();
    conductor_->main_proc_->QueueAppThreadCallback(NEW_STREAM_ADDED, this->call_id_,
            stream);
}

void Conductor::PeerConnecter::OnRemoveStream(webrtc::MediaStreamInterface* stream) {
    LOG(INFO) << __FUNCTION__ << " " << stream->label() << " peer id " << this->peer_id_;
    stream->AddRef();
    conductor_->main_proc_->QueueAppThreadCallback(STREAM_REMOVED, this->call_id_,
            stream);
}

void Conductor::PeerConnecter::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();

    MessageData ice_msg;
    ice_msg.msg_type = "candidate";
    ice_msg.sdp_mid = candidate->sdp_mid();
    ice_msg.mline_index = candidate->sdp_mline_index();

    if (!candidate->ToString(&ice_msg.candidate)) {
        LOG(LS_ERROR) << "Failed to serialize candidate";
        return;
    }

    conductor_->SendMessage(ice_msg, this->call_id_);
}

//
// PeerConnectionClientObserver implementation.
//
//stun:192.168.13.214:19302
void Conductor::OnSignedIn(const std::string& sysinfo) {
    LOG(INFO) << __FUNCTION__ << sysinfo;
    stun_server_ = sysinfo;
    //notify online
}

void Conductor::OnDisconnected(std::string* call_id) {
    LOG(INFO) << __FUNCTION__ << call_id;
    //notify offline
}

void Conductor::OnServerConnectionFailure() {
    //main_wnd_->MessageBox("Error", ("Failed to connect to " + server_).c_str(),
    //                      true);
    printf("Failed to connect to server\n");
}

void Conductor::OnConfCreate(const std::string& conf_id, int err)
{

}

void Conductor::OnConfNotify(std::string& conf_id, std::string& lecturer, 
        std::vector<std::string>& memb_array,
        int start_time, int duration,
        std::string& description)
{

}

void Conductor::OnCloseConf(std::string& result)
{

}
//call
int Conductor::OnRecvOffer(std::string& callid, std::string& from, 
        std::string& to, std::string& sdp)
{
    LOG(INFO) << __FUNCTION__ << "callid " << callid << "from " << from 
        << "to " << to << "sdp\n" << sdp;

    PeerConnecter *connecter;
    if (to != local_id_ && to != client_->ip())
    {
        printf("callee can't match local ip or id, %s.\n", to.c_str());
        return 480;
    }
    if (!lecturer_ && active_connecter_.size() > 0)
    {
        //failed 486, one call default, return busy
        //client_->SendCallFailed(callid, 486);
        return 486;
    }

    if (FindConnecter(callid) == NULL)
    {
        //send ringing
        //client_->SendCallRinging(callid);
        std::string state = "incoming";
        main_proc_->UpdateCallState(callid, from, state);
    }
    else //repeated callid
    {
        //failed 480
        //client_->SendCallFailed(callid, 480);
        return 480;
    }

    //auto connect
    connecter = InitializePeerConnection(callid, to, from);
    if (connecter == NULL)
    {
        LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
        //client_->SendCallFailed(callid, 480);
        return 480;
    }

    std::string type = "offer";
    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface* session_description(
            webrtc::CreateSessionDescription(type, sdp, &error));
    if (!session_description) 
    {
        LOG(WARNING) << "Can't parse received session description message. "
            << "SdpParseError was: " << error.description;
        return 480;
    }

    connecter->peer_connection_->SetRemoteDescription(
            DummySetSessionDescriptionObserver::Create(), session_description);

    if (session_description->type() ==
            webrtc::SessionDescriptionInterface::kOffer) 
    {
        connecter->peer_connection_->CreateAnswer(connecter, NULL);
    }
    return 0;
}

void Conductor::OnRecvRinging(std::string& callid)
{
    LOG(INFO) << "recv ringing " << callid;
    PeerConnecter *connecter;
    std::string state = "ringing";

    connecter = FindConnecter(callid);
    if (connecter)
    {
        main_proc_->UpdateCallState(callid, connecter->peer_id_, state);
    }
    else
    {
        LOG(INFO) << "recv ringing, can not find callid " << callid;
    }
}

int Conductor::OnRecvAnswer(std::string& callid, std::string& from, 
        std::string& to, std::string& sdp)
{
    LOG(INFO) << __FUNCTION__ << "callid " << callid << "from " << from 
        << "to " << to;
    std::string state;
    PeerConnecter *connecter = FindConnecter(callid);
    if (connecter == NULL)
    {
        LOG(LS_ERROR) << "RecvAnswer, Failed to find callid " << callid;
        //send bye
        return 480;
    }

    std::string type = "answer";
    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface* session_description(
            webrtc::CreateSessionDescription(type, sdp, &error));
    if (!session_description) 
    {
        LOG(WARNING) << "Can't parse received session description message. "
            << "SdpParseError was: " << error.description;
        state = "disconnected";
        main_proc_->UpdateCallState(callid, connecter->peer_id_, state);
        //del call
        DeletePeerConnection(&callid);
        //send bye
        return 480;
    }

    connecter->peer_connection_->SetRemoteDescription(
            DummySetSessionDescriptionObserver::Create(), session_description);
    state = "connected";
    main_proc_->UpdateCallState(callid, connecter->peer_id_, state);
    return 0;
}

void Conductor::OnSendCandidate(std::string& callid)
{
    LOG(INFO) << __FUNCTION__ << "callid " << callid;
    PeerConnecter *connecter = FindConnecter(callid);

    if (connecter)
    {
        connecter->state_ = PeerConnecter::CONNECTED;
        SendCandidate(connecter);
    }
    else
    {
        LOG(LS_ERROR) << "OnSendCandidate, Failed to find callid " << callid;
    }
}

void Conductor::OnRecvCandidate(std::string& callid, int index, 
        std::string& sdpmid, std::string& sdp)
{
    PeerConnecter *connecter = FindConnecter(callid);
    if (!connecter)
    {
        LOG(LS_ERROR) << "OnRecvCandidate, Failed to find callid " << callid;
        return;
    }

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidateInterface> candidate(
            webrtc::CreateIceCandidate(sdpmid, index, sdp, &error));
    if (!candidate.get()) 
    {
        LOG(WARNING) << "Can't parse received candidate message. "
            << "SdpParseError was: " << error.description;
        return;
    }

    LOG(INFO) << " Received candidate :" << callid;
    if (!connecter->peer_connection_->AddIceCandidate(candidate.get())) 
    {
        LOG(WARNING) << "Failed to apply the received candidate";
        return;
    }
    //std::string method = "candidate";
    //client_->SendCallAck(callid, method);
}

void Conductor::OnRecvAck(std::string& callid, std::string& method)
{
    LOG(INFO) << "Received ack :" << method << " call id " << callid;
    if (method == "answer")
    {
        PeerConnecter *connecter = FindConnecter(callid);
        if (!connecter)
        {
            LOG(LS_ERROR) << "OnRecvAck, Failed to find callid " << callid;
            return;
        }
        connecter->state_ = PeerConnecter::CONNECTED;
        SendCandidate(connecter);
    }
    else if (method == "bye")
    {
        //close dir socket
        //client_->CloseDirSocket(callid);
    }
}

void Conductor::OnCallMsgTimeout(std::string& callid, std::string& method)
{
    LOG(INFO) << "Msg timeout :" << method << " call id " << callid;
}

void Conductor::OnRecvBye(std::string& callid)
{
    LOG(INFO) << "Our peer disconnected " << callid;
    main_proc_->QueueAppThreadCallback(PEER_CONNECTION_CLOSED, callid, NULL);
}

void Conductor::OnRecvFailed(std::string& callid, int error)
{
    LOG(INFO) << "RecvFailed " << callid << " code " << error;
    //client_->CloseDirSocket(callid);
    main_proc_->QueueAppThreadCallback(PEER_CONNECTION_CLOSED, callid, NULL);
}

//connect to peer
bool Conductor::OnDirConnected(std::string& peer_id, std::string& call_id)
{
	PeerConnecter *connecter;
	std::string ip = client_->ip();
	LOG(INFO) << __FUNCTION__ << peer_id;
	connecter = InitializePeerConnection(call_id, ip, peer_id);//local_id_
	if (connecter)
	{
		connecter->peer_connection_->CreateOffer(connecter, NULL);
		return true;
	}
	else 
	{
		LOG(INFO) << "Failed to initialize PeerConnection\n";
		main_proc_->QueueAppThreadCallback(PEER_CONNECTION_CLOSED, call_id, NULL);
		return false;
	}
}

void Conductor::OnDirDisconnected(std::string& call_id) 
{
	LOG(INFO) << __FUNCTION__ << call_id;
	//find connecter ,bye,else  connect failed
	main_proc_->QueueAppThreadCallback(PEER_CONNECTION_CLOSED, call_id, NULL);
}

//
// MainWndCallback implementation.
//
void Conductor::SetTeacher2() {
    lecturer_ = true;
}

void Conductor::StartLogin(const std::string& server, int port) {
    if (client_->is_connected())
        return;
    server_ = server;
    client_->Connect(server, port, GetPeerName());
    client_->DirListen(20000);
}

void Conductor::DisconnectFromServer() {
    if (client_->is_connected())
        client_->SignOut();
}

bool Conductor::ConnectToPeer(std::string& peer_id) 
{
    std::string call_id;
    PeerConnecter *connecter = FindPeeridConnecter(peer_id);

    LOG(INFO) << __FUNCTION__ << peer_id;
    if (connecter)
    {
        LOG(INFO) << "repeat peer id " << peer_id;
        return false;
    }

    call_id = GenCallid();
    connecter = InitializePeerConnection(call_id, local_id_, peer_id);
    if (connecter)
    {
        connecter->peer_connection_->CreateOffer(connecter, NULL);
    }
    else 
    {
        LOG(INFO) << "Failed to initialize PeerConnection\n";
        return false;
    }
    return true;
}

bool Conductor::ConnectToPeer(std::string& peer_id, std::string& peer_ip, int peer_port) 
{
    std::string call_id;
    PeerConnecter *connecter = FindPeeridConnecter(peer_id);

    LOG(INFO) << __FUNCTION__ << peer_id;
    if (connecter)
    {
        LOG(INFO) << "repeat peer id " << peer_id;
        return false;
    }
    call_id = GenCallid();
    int ret = client_->DirConnect(peer_id, call_id, peer_ip, peer_port);
    return ret;
}

cricket::VideoCapturer* Conductor::OpenVideoCaptureDevice(int id) {
    std::vector<std::string> device_names;
    {
        std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
                webrtc::VideoCaptureFactory::CreateDeviceInfo(id));
        if (!info) {
            return nullptr;
        }
        int num_devices = info->NumberOfDevices();
        for (int i = 0; i < num_devices; ++i) {
            const uint32_t kSize = 256;
            char name[kSize] = {0};
            char id[kSize] = {0};
            if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) {
                device_names.push_back(name);
            }
        }
    }

    printf("device name size %ld\n", device_names.size());
    cricket::WebRtcVideoDeviceCapturerFactory factory;
    cricket::VideoCapturer* capturer = nullptr;
#if 0
    for (const auto& name : device_names) {
        capturer = factory.Create(cricket::Device(name, 0));
        if (capturer) {
            break;
        }
    }
#endif

    capturer = factory.Create(cricket::Device(device_names[id], id));
    if (capturer) {
        printf("##create video capturer %d, %s success.\n", id, device_names[id].c_str());
    }
    else
        printf("##create video capturer %d, %s failed.\n", id, device_names[id].c_str());

    return capturer;
}
#if 1
void Conductor::AddStreams(PeerConnecter *connecter) {

    LOG(INFO) << __FUNCTION__;

    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream;

    if (active_streams_.find(kStreamLabel) == active_streams_.end())
        //return;  // Already added.
    {
#define AUDIO
#ifdef AUDIO
        rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
                peer_connection_factory_->CreateAudioTrack(
                    kAudioLabel, peer_connection_factory_->CreateAudioSource(NULL)));
#endif
        rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
                peer_connection_factory_->CreateVideoTrack(
                    kVideoLabel,
                    peer_connection_factory_->CreateVideoSource(OpenVideoCaptureDevice(0),
                        NULL)));
        //main_proc_->StartLocalRenderer(video_track);

#define VIDEO_2

        stream = peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);
#ifdef AUDIO
        stream->AddTrack(audio_track);//////
#endif
        stream->AddTrack(video_track);


#ifdef VIDEO_2
        if(lecturer_){
            rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track2(
                    peer_connection_factory_->CreateVideoTrack(
                        kVideoLabel2,
                        peer_connection_factory_->CreateVideoSource(OpenVideoCaptureDevice(1),NULL)));
            stream->AddTrack(video_track2);//video 2
        }
#endif

        typedef std::pair<std::string,
                rtc::scoped_refptr<webrtc::MediaStreamInterface> >
                    MediaStreamPair;
        active_streams_.insert(MediaStreamPair(stream->label(), stream));

    }
    else
    {
        std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >::iterator it;

        LOG(INFO) << __FUNCTION__;
        it = active_streams_.find(kStreamLabel);
        stream = it->second;
    }

    if (!connecter->peer_connection_->AddStream(stream)) {
        LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
    }

    //main_wnd_->SwitchToStreamingUI();
}
#else
void Conductor::AddStreams(PeerConnecter *connecter) {
    LOG(INFO) << __FUNCTION__;
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream;
    if (active_streams_.find(kStreamLabel) == active_streams_.end())
    {
#define AUDIO
#ifdef AUDIO
        rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
                peer_connection_factory_->CreateAudioTrack(
                    kAudioLabel, peer_connection_factory_->CreateAudioSource(NULL)));
#endif
        rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
                peer_connection_factory_->CreateVideoTrack(kVideoLabel,
                    peer_connection_factory_->CreateVideoSource(
                        OpenVideoCaptureDevice(0), NULL)));
        //main_proc_->StartLocalRenderer(video_track);

#define VIDEO_2
#ifdef VIDEO_2//video 2
        rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track2;
        if (lecturer_)
        {
            video_track2 = peer_connection_factory_->CreateVideoTrack(
                    kVideoLabel2,
                    peer_connection_factory_->CreateVideoSource(OpenVideoCaptureDevice(1),NULL));
        }
#endif
        stream = peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);
#ifdef AUDIO
        stream->AddTrack(audio_track);//////
#endif
        stream->AddTrack(video_track);
#ifdef VIDEO_2
        if(lecturer_){
            stream->AddTrack(video_track2);//video 2
        }
#endif
        typedef std::pair<std::string,
                rtc::scoped_refptr<webrtc::MediaStreamInterface> > MediaStreamPair;
        active_streams_.insert(MediaStreamPair(stream->label(), stream));
    }
    else// Already created.
    {
        std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >::iterator it;
        LOG(INFO) << __FUNCTION__;
        it = active_streams_.find(kStreamLabel);
        stream = it->second;
    }

    if (!connecter->peer_connection_->AddStream(stream)) {
        LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
    }
}
#endif

void Conductor::DisconnectFromCurrentPeer(std::string& peer_id) 
{
    LOG(INFO) << __FUNCTION__ << " " << peer_id;
    PeerConnecter *connecter = FindPeeridConnecter(peer_id);
    if (connecter)
    {
        client_->SendCallBye(connecter->local_id_, connecter->peer_id_, connecter->call_id_);
        DeletePeerConnection(&connecter->call_id_);
    }
}

void Conductor::DisconnectFromCallid(std::string& call_id)
{
    LOG(INFO) << __FUNCTION__ << " " << call_id;
    PeerConnecter *connecter = FindConnecter(call_id);
    if (connecter)
    {
        client_->SendCallBye(connecter->local_id_, connecter->peer_id_, call_id);
        DeletePeerConnection(&call_id);
    }
}

void Conductor::SendCandidate(PeerConnecter *connecter)
{
	for(PeerConnecter::Candidates::iterator it = connecter->candidates_.begin(); 
	    it != connecter->candidates_.end();)  
	{  
		//if (it->candidate.find(" tcp ") == std::string::npos)
		client_->SendCallCandidate(connecter->local_id_, connecter->peer_id_,
									connecter->call_id_, it->sdp_mid,
									it->mline_index, it->candidate);
		it = connecter->candidates_.erase(it);
	} 
}

void Conductor::UIThreadCallback(int msg_id, std::string call_id, void* data) 
{
    switch (msg_id) 
    {
        case PEER_CONNECTION_CLOSED://recv bye
            {
                LOG(INFO) << "PEER_CONNECTION_CLOSED " << call_id;
                DeletePeerConnection(&call_id);
                //client_->SendCallAck(*call_id, "bye");
                break;
            }
        case SEND_MESSAGE_TO_PEER:
            {
                LOG(INFO) << "SEND_MESSAGE_TO_PEER " << call_id;
                MessageData* msg = reinterpret_cast<MessageData*>(data);
                PeerConnecter *connecter = FindConnecter(call_id);
                if (!connecter)
                {
                    delete msg;
                    LOG(INFO) << "SEND_MESSAGE_TO_PEER can not find " << call_id;
                    return;
                }

                if (msg->msg_type == "sdp")
                {
                    if (msg->type == "offer")
                    {
                        client_->SendCallOffer(connecter->local_id_, connecter->peer_id_, 
                                call_id, msg->sdp);
                    }
                    else if (msg->type == "answer")
                    {
                        client_->SendCallAnswer(connecter->local_id_, connecter->peer_id_,
                                call_id, msg->sdp);
                    }
                }
                else if (msg->msg_type == "candidate")
                {
                    if (connecter->state_ == PeerConnecter::CONNECTED)
                    {
                        client_->SendCallCandidate(connecter->local_id_, connecter->peer_id_,
                                call_id, msg->sdp_mid,
                                msg->mline_index, msg->candidate);
                    }
                    else
                    {
                        PeerConnecter::Candidate cand;
                        cand.candidate = msg->candidate;
                        cand.mline_index = msg->mline_index;
                        cand.sdp_mid = msg->sdp_mid;
                        connecter->candidates_.push_back(cand);
                    }
                }
                delete msg;
                break;
            }
        case NEW_STREAM_ADDED:
            {
                webrtc::MediaStreamInterface* stream =
                    reinterpret_cast<webrtc::MediaStreamInterface*>(data);
                webrtc::VideoTrackVector tracks = stream->GetVideoTracks();
                if (!tracks.empty())
                {
                    LOG(INFO) << "NEW_STREAM_ADDED " << call_id;
                    if (FindConnecter(call_id))
                    {
                        //webrtc::VideoTrackInterface* track;
                        //const Peers &peers = client_->peers();
                        //std::string name = "unknown";
                        //Peers::const_iterator i = peers.find(connecter->peer_id_);
                        //if (i != peers.end())
                        //{
                        //	name = i->second;
                        //}
                        if (tracks.size() == 1)
                        {
                            main_proc_->StartRemoteRenderer(tracks[0], call_id, 0, 0);
                        }
                        else if (tracks.size() == 2)
                        {
                            main_proc_->StartRemoteRenderer(tracks[0], call_id, 0, 0);
                            main_proc_->StartRemoteRenderer(tracks[1], call_id, 1, 1);
                        }
                    }
                }
                stream->Release();
                break;
            }
        case STREAM_REMOVED:
            {
                // Remote peer stopped sending a stream.
                webrtc::MediaStreamInterface* stream =
                    reinterpret_cast<webrtc::MediaStreamInterface*>(
                            data);
                LOG(INFO) << "STREAM_REMOVED " << call_id;
                stream->Release();
                break;
            }
        case UI_CMD: 
            {
                std::string* msg = reinterpret_cast<std::string*>(data);
                if (!msg) {
                    break;
                }

                if (msg->compare("connect") == 0)//call
                {
                    printf("connect callid:%s\n",call_id.c_str());
                    ConnectToPeer(call_id);//peer_id
                }
                else if (msg->compare("dirconnect") == 0)//call
                {
                    int port = -1;
                    std::string ip,port_str;
                    int pos = call_id.find(':',0);
                    if(pos >= 0) {
                        ip = call_id.substr(0,pos-1);
                        port_str = call_id.substr(pos+1);
                        port = atoi(port_str.c_str());
                    } else {
                        ip = call_id;
                    }
                    printf("dirconnect ip=%s,port=%d\n",ip.c_str(),port);
                    ConnectToPeer(ip, ip, port);//peer_id
                }
                else if (msg->compare("disconnect") == 0)//bye
                {
                    printf("disconnect callid:%s\n",call_id.c_str());
                    DisconnectFromCurrentPeer(call_id);
                }
                else if (msg->compare("quit") == 0)
                {
                    DisconnectFromServer();
                }
                delete msg;
                break;
            }
        default:
            break;
    }
}

Conductor::PeerConnecter* Conductor::FindConnecter(std::string& call_id)
{
    PeerConnecter *connecter;
    ConnecterMap::iterator it;

    if ((it = active_connecter_.find(call_id)) != active_connecter_.end())
    {
        connecter = it->second;
        return connecter;
    }
    return NULL;
}

Conductor::PeerConnecter* Conductor::FindPeeridConnecter(std::string& peer_id)
{
    PeerConnecter *connecter;
    ConnecterMap::iterator it;
    for(it = active_connecter_.begin(); it != active_connecter_.end(); ++it)
    {
        connecter = it->second;
        printf("peer id %s, in %s\n",connecter->peer_id_.c_str(), peer_id.c_str());
        if (connecter->peer_id_ == peer_id)
        {
            return connecter;
        }
    }
    return NULL;
}

void Conductor::PeerConnecter::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
    MessageData local_sdp;

    LOG(INFO) << __FUNCTION__ << " call_id " << call_id_ << " peer id " << peer_id_;

    peer_connection_->SetLocalDescription(
            DummySetSessionDescriptionObserver::Create(), desc);

    //std::string sdp;
    local_sdp.msg_type = "sdp";
    desc->ToString(&local_sdp.sdp);
    local_sdp.type = desc->type();
#if 0
    Json::StyledWriter writer;
    Json::Value jmessage;
    jmessage[kSessionDescriptionTypeName] = desc->type();
    jmessage[kSessionDescriptionSdpName] = sdp;
    conductor_->SendMessage(writer.write(jmessage), peer_id_);
#endif

    conductor_->SendMessage(local_sdp, this->call_id_);
    //send offer or answer
}

void Conductor::PeerConnecter::OnFailure(const std::string& error) {
    LOG(LERROR) << error;
    //send failed
}

void Conductor::SendMessage(const MessageData& message, const std::string& call_id) {
    MessageData* msg = new MessageData(message);
    main_proc_->QueueAppThreadCallback(SEND_MESSAGE_TO_PEER, call_id, msg);
}

std::string Conductor::GenCallid(void) {
    rtc::Md5Digest md5;
    std::string str;
    time_t timep;
    time(&timep);
    str = ctime(&timep) + local_id_ + uuid_;
    uuid_ = rtc::ComputeDigest(&md5, str);
    return uuid_;
}

