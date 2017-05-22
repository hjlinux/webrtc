/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/examples/peerconnection/client/peer_connection_client.h"

#include "webrtc/examples/peerconnection/client/defaults.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/nethelpers.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/json.h"
#include "webrtc/base/md5digest.h"
#include "webrtc/base/stringencode.h"

#ifdef WIN32
#include "webrtc/base/win32socketserver.h"
#endif

using rtc::sprintfn;

namespace {

#define PROT_HEAD_LEN    18

// This is our magical hangup signal.
//const char kByeMessage[] = "BYE";
// Delay between server connection retries, in milliseconds
const int kReconnectDelay = 1000;
const int kHeartbeattDelay = 60000;
const int kCalltimeoutDelay = 30000;
const int kSockettimeoutDelay = 4000;
const int kSocketDeleteDelay = 1000;

const char kPkgType[] = "pkgtype";
const char kMsgType[] = "msgtype";
const char kSeq[] = "sequence";

rtc::AsyncSocket* CreateSocket(int family) {
  rtc::Thread* thread = rtc::Thread::Current();
  ASSERT(thread != NULL);
  return thread->socketserver()->CreateAsyncSocket(family, SOCK_STREAM);
}

std::string Md5(const std::string& input) {
  rtc::Md5Digest md5;
  return rtc::ComputeDigest(&md5, input);
}
/*
struct CallMessage
{
	std::string call_id;
	std::string method;
};*/

}  // namespace

PeerConnectionClient::PeerConnectionClient()
  : callback_(NULL),
    resolver_(NULL),
    state_(NOT_CONNECTED),
    my_id_(-1){
	body_len_ = 0;
	prot_verion_ = "01";
	sequence_ = 1;
	listen_port_ = 0;
}

PeerConnectionClient::~PeerConnectionClient() {
}

void PeerConnectionClient::InitSocketSignals(rtc::AsyncSocket* socket) {
  ASSERT(socket != NULL);
  socket->SignalCloseEvent.connect(this, &PeerConnectionClient::OnClose);
  socket->SignalConnectEvent.connect(this, &PeerConnectionClient::OnConnect);
  socket->SignalReadEvent.connect(this, &PeerConnectionClient::OnRead);
}

std::string PeerConnectionClient::id() const {
  return client_id_;
}

std::string PeerConnectionClient::ip() const {
  return local_ip_;
}

bool PeerConnectionClient::is_connected() const {
  return my_id_ != -1;
}

const Peers& PeerConnectionClient::peers() const {
  return peers_;
}

void PeerConnectionClient::SetClientId(std::string client_id)
{
	client_id_ = client_id;
}
void PeerConnectionClient::SetClientMac(std::string client_mac)
{
	client_mac_ = client_mac;
}

void PeerConnectionClient::SetClientIp(std::string client_ip)
{
	local_ip_ = client_ip;
}

void PeerConnectionClient::RegisterObserver(
    PeerConnectionClientObserver* callback) {
  ASSERT(!callback_);
  callback_ = callback;
}

void PeerConnectionClient::Connect(const std::string& server, int port,
                                   const std::string& client_name) {
  ASSERT(!server.empty());
  ASSERT(!client_name.empty());

  LOG(INFO) << "Connect";
  rtc::Thread::Current()->PostDelayed(kHeartbeattDelay, this, HEARTBEAT);//keep alive timer

  if (state_ != NOT_CONNECTED) {
    LOG(WARNING)
        << "The client must not be connected before you can call Connect()";
    callback_->OnServerConnectionFailure();
    return;
  }

  if (server.empty() || client_name.empty()) {
    callback_->OnServerConnectionFailure();
    return;
  }

  if (port <= 0)
    port = kDefaultServerPort;

  server_address_.SetIP(server);
  server_address_.SetPort(port);
  client_name_ = client_name;

  if (server_address_.IsUnresolvedIP()) {
    state_ = RESOLVING;
    resolver_ = new rtc::AsyncResolver();
    resolver_->SignalDone.connect(this, &PeerConnectionClient::OnResolveResult);
    resolver_->Start(server_address_);
  } else {
    DoConnect();
  }
}

bool PeerConnectionClient::DirConnect(std::string& peer_id, std::string& call_id, 
											const std::string& server, int port)
{
	rtc::SocketAddress address;
	DirSocket dir_peer;
	LOG(INFO) << "Direct Connect to " << server;
	if (server.empty()) {
		callback_->OnServerConnectionFailure();
		return false;
	}
	if (port <= 0)
		port = kDefaultServerPort;
	address.SetIP(server);
	address.SetPort(port);
	
	dir_peer.socket = CreateSocket(address.ipaddr().family());
	if (!dir_peer.socket || address.IsNil())
	{
		LOG(INFO) << "Direct Connect to a invalid addr " << server;
		return false;
	}
	dir_peer.peer_ip = address.ToString();
	dir_peer.port = address.port();
	dir_peer.call_id = call_id;
	dir_peer.peer_id = peer_id;
	InitSocketSignals(dir_peer.socket);
	int err = dir_peer.socket->Connect(address);
	if (err == SOCKET_ERROR)
	{
		LOG(INFO) << "Direct Connect failed " << server;
		return false;
	}
	MonitorSocketTimeout(dir_peer.socket);
	dir_sockets_.push_back(dir_peer);
	return true;
}

void PeerConnectionClient::DirListen(int port) {
	rtc::SocketAddress address;
	int ret=0;
	/*if (state_ != NOT_CONNECTED) {
	LOG(WARNING)
	<< "The client must not be connected before you can call Connect()";
	callback_->OnServerConnectionFailure();
	return;
	}*/

	if (port <= 0 && listen_port_ == 0)
		port = kDefaultServerPort;
	listen_port_ = port;

	address.SetIP("0.0.0.0");
	address.SetPort(port);
	listen_socket_.reset(CreateSocket(address.ipaddr().family()));

	InitSocketSignals(listen_socket_.get());
	ret = listen_socket_->Bind(address);
	printf("-----listen bind ret %d\n", ret);
	ret = listen_socket_->Listen(5);
	printf("-----listen listen ret %d\n", ret);
}

void PeerConnectionClient::DirKeepAlive(void)
{
	//DirSocket dir_peer;
	for(DirSockets::iterator it = dir_sockets_.begin(); it != dir_sockets_.end(); ++it)  
	{
		SendCallKeepAlive(local_ip_, it->peer_ip, it->call_id);
	}
}

void PeerConnectionClient::CheckSocket(rtc::AsyncSocket* socket)
{
	//rtc::SocketAddress address;
	int state;
	if (!socket)
		return;
	//address = socket->GetRemoteAddress();
	if ((state = socket->GetState()) != rtc::AsyncSocket::CS_CONNECTED)
	{
		if (state != rtc::AsyncSocket::CS_CLOSED)
		{
			LOG(WARNING) << "socket connect time out, state " << state << " CS_CONNECTING";
			socket->Close();
			OnClose(socket, -1);
		}
		
		if (socket != control_socket_.get())
		{
			delete socket;
		}
	}
}

void PeerConnectionClient::OnResolveResult(
    rtc::AsyncResolverInterface* resolver) {
  if (resolver_->GetError() != 0) {
    callback_->OnServerConnectionFailure();
    resolver_->Destroy(false);
    resolver_ = NULL;
    state_ = NOT_CONNECTED;
  } else {
    server_address_ = resolver_->address();
    DoConnect();
  }
}

int PeerConnectionClient::Sequence(void)
{
	sequence_++;
	if (sequence_ > 65535)
		sequence_ = 1;
	return sequence_;
}

//INTERACT<01,3F2504E04F8911D39A0C0305E82C3301,00102>{"firstName":"Brett","lastName":"McLaughlin","email":"aaaa"}
void PeerConnectionClient::GenerateHead(char *buffer, int len)
{
	sprintfn(buffer, PROT_HEAD_LEN + 1, "INTERACT<%s,%05d>", prot_verion_.c_str(), len);
}

void PeerConnectionClient::GenerateConnAuth(void)
{
	char buffer[1024];
	std::string json_body;
	Json::FastWriter writer;
	Json::Value jmessage;

	jmessage["pkgtype"] = "connect";
	jmessage["msgtype"] = "authenticate";
	jmessage["sequence"] = Sequence();
	jmessage["terminal"] = client_id_;
	json_body = writer.write(jmessage);
	GenerateHead(buffer, json_body.size());
	onconnect_data_ = buffer + json_body;
}

//如果连接失败，会再次重连
void PeerConnectionClient::DoConnect() {
	control_socket_.reset(CreateSocket(server_address_.ipaddr().family()));
	InitSocketSignals(control_socket_.get());
	GenerateConnAuth();

	bool ret = ConnectControlSocket();
	if (ret)
		state_ = SIGNING_IN;
		//MonitorSocketTimeout(control_socket_.get());//暂时屏蔽，后期开启
	if (!ret) {
		callback_->OnServerConnectionFailure();
	}
}

bool PeerConnectionClient::IsSendingMessage() {
  return state_ == CONNECTED &&
         control_socket_->GetState() != rtc::Socket::CS_CLOSED;
}

bool PeerConnectionClient::SignOut() {
	if (state_ == NOT_CONNECTED || state_ == SIGNING_OUT)
		return true;

	if (control_socket_->GetState() == rtc::Socket::CS_CLOSED) 
	{
		state_ = SIGNING_OUT;
	}
	else
	{
		SendConnLogout();
		state_ = SIGNING_OUT_WAITING;
	}
	return true;
}

void PeerConnectionClient::Close() {
  control_socket_->Close();
  onconnect_data_.clear();
  peers_.clear();
  if (resolver_ != NULL) {
    resolver_->Destroy(false);
    resolver_ = NULL;
  }
  my_id_ = -1;
  state_ = NOT_CONNECTED;
}

bool PeerConnectionClient::ConnectControlSocket() {
  ASSERT(control_socket_->GetState() == rtc::Socket::CS_CLOSED);
  int err = control_socket_->Connect(server_address_);
  if (err == SOCKET_ERROR) {
    Close();
    return false;
  }
  return true;
}

void PeerConnectionClient::OnConnect(rtc::AsyncSocket* socket) {
	if (socket == control_socket_.get())
	{
		ASSERT(!onconnect_data_.empty());
		LOG(INFO) << "on connect";
		size_t sent = socket->Send(onconnect_data_.c_str(), onconnect_data_.length());
		ASSERT(sent == onconnect_data_.length());
		RTC_UNUSED(sent);
		onconnect_data_.clear();
	}
	else if (socket == listen_socket_.get())//to be del
	{
		printf("-----------listern connect \n");
	}
	else
	{
		DirSocket* peer_socket = FindDirSocket(socket);
		if (!peer_socket)
			return;
		bool ret = callback_->OnDirConnected(peer_socket->peer_id, peer_socket->call_id);
		LOG(WARNING) << "-----dir on connected callid " << peer_socket->call_id 
		            << " peerid " << peer_socket->peer_id;
		if (ret == false)//create offer failed
		{
			CloseDirSocket(peer_socket->call_id);
		}

		//rtc::SocketAddress address = socket->GetLocalAddress();
		//std::string ip = address.ToString();
		//printf("local ip %s\n", ip.c_str());
	}
}

/*
void PeerConnectionClient::OnMessageFromPeer(int peer_id,
                                             const std::string& message) {
  if (message.length() == (sizeof(kByeMessage) - 1) &&
      message.compare(kByeMessage) == 0) {
    callback_->OnPeerDisconnected(peer_id);
  } else {
    callback_->OnMessageFromPeer(peer_id, message);
  }
}*/

//INTERACT<01,00102>
bool PeerConnectionClient::ReadIntoBuffer(rtc::AsyncSocket* socket, 
                                                std::string* data, 
                                                std::string &body_data) 
{
	char buffer[0xffff] = {0};
	size_t len;
	bool ret = false;

	if (socket->GetState() != rtc::AsyncSocket::CS_CONNECTED)
	{
		printf("-----read buf socket close, return\n");
		return false;
	}
	
	do {
		int bytes = socket->Recv(buffer, sizeof(buffer));
		if (bytes <= 0)
			break;
		data->append(buffer, bytes);
	} while (true);

	printf("-----read buf %s\n", buffer);

	while (true)
	{
		if (data->size() < (PROT_HEAD_LEN + body_len_))//body_len 默认为0
			break;
		size_t i = data->find("INTERACT");
		if (i != std::string::npos) 
		{
			LOG(INFO) << "Headers received";

			if (data->substr(i + 9, 2) == prot_verion_ 
			    && data->compare(i + 8, 1, "<") == 0
			    && data->compare(i + 17, 1, ">") == 0)
			{
				len = std::atoi(data->c_str() + i + 12);
				if (len > 0)
				{
					body_len_ = len;
				}
				else
				{
					data->erase(0, i + PROT_HEAD_LEN); //删除前部分数据
					continue;
				}
			}
			else
			{
				data->erase(0, i + PROT_HEAD_LEN); //删除前部分数据
				continue;
			}

			if (data->size() < (PROT_HEAD_LEN + body_len_))
				break;

			LOG(INFO) << "Body received";
			body_data = data->substr(i + PROT_HEAD_LEN, body_len_);
			data->erase(0, i + PROT_HEAD_LEN + body_len_);
			body_len_ = 0;
			ret = true;
			break;
		}
		else
		{
			break;
		}
	}
	return ret;
}

void PeerConnectionClient::OnRead(rtc::AsyncSocket* socket) {
	std::string body_data;
	if (socket == control_socket_.get())
	{
		while (ReadIntoBuffer(socket, &control_data_, body_data)) 
		{
			bool ok = HandleMessage(body_data);
			if (!ok)
			{
				LOG(WARNING) << "HandleMessage failed.";
			}
		}
	}
	else if (socket == listen_socket_.get())
	{
		rtc::SocketAddress addr;
		DirSocket dir_peer;
		printf("-----listen on read\n");
		//dir_peer.socket.reset(listen_socket_->Accept(&addr));
		dir_peer.socket = listen_socket_->Accept(&addr);
		printf("-----accept peer addr %s, %d\n",addr.ToString().c_str(), addr.port());
		dir_peer.peer_ip = addr.ToString();
		dir_peer.port = addr.port();
		dir_peer.call_id = dir_peer.peer_ip;// + "_" 
						//+ std::to_string(dir_peer.port);//temporary callid for check timeout
		//InitSocketSignals(dir_peer.socket.get());
		InitSocketSignals(dir_peer.socket);
		//dir_sockets_.push_back(std::move(dir_peer));
		dir_sockets_.push_back(dir_peer);
	}
	else //direct connect socket
	{
		DirSocket* peer_socket = FindDirSocket(socket);
		if (!peer_socket)
			return;

		printf("-----on read\n");
		while (ReadIntoBuffer(socket, &peer_socket->recv_data, body_data)) 
		{
			bool ok = HandleCallMessage(body_data, peer_socket->call_id);
			if (!ok)
			{
				LOG(WARNING) << "HandleCallMessage failed.";
			}
		}
	}
}

PeerConnectionClient::DirSocket* PeerConnectionClient::FindDirSocket(rtc::AsyncSocket* socket)
{
	for(DirSockets::iterator it = dir_sockets_.begin(); it != dir_sockets_.end(); ++it)  
	{  
		//if (it->socket.get() == socket)
		if (it->socket == socket)
		{
			return &(*it);
		}
	}
	return NULL;
}

PeerConnectionClient::DirSocket* PeerConnectionClient::FindDirSocket(std::string& call_id)
{
	for(DirSockets::iterator it = dir_sockets_.begin(); it != dir_sockets_.end(); ++it)  
	{  
		if (it->call_id == call_id)
		{
			return &(*it);
		}
	}
	return NULL;
}

void PeerConnectionClient::EraseDirSocket(DirSocket* dir)
{
	for(DirSockets::iterator it = dir_sockets_.begin(); it != dir_sockets_.end(); ++it)  
	{
		if (&(*it) == dir)
		{
			dir->socket = NULL;
			dir_sockets_.erase(it);
			return;
		}
	}
}

void PeerConnectionClient::CloseDirSocket(std::string& call_id)
{
	DirSocket* peer_socket = FindDirSocket(call_id);
	if (!peer_socket)
		return;
	peer_socket->socket->Close();
	MonitorSocketTimeout(peer_socket->socket, kSocketDeleteDelay);//free socket after kSocketDeleteDelay
	LOG(WARNING) << "-----close direct socket, callid " << peer_socket->call_id;
	EraseDirSocket(peer_socket);
}

/*
int PeerConnectionClient::GetResponseStatus(const std::string& response) {
  int status = -1;
  size_t pos = response.find(' ');
  if (pos != std::string::npos)
    status = atoi(&response[pos + 1]);
  return status;
}
*/
bool PeerConnectionClient::HandleMessage(const std::string& message)
{
	Json::Reader reader;
	Json::Value jmessage;
	if (!reader.parse(message, jmessage)) 
	{
		LOG(WARNING) << "Received unknown message. " << message;
		return false;
	}
	std::string type;
	std::string json_object;

	rtc::GetStringFromJsonObject(jmessage, kPkgType, &type);
	if (!type.empty()) //not empty
	{
		if (type == "connect") 
		{
			HandleConnect(jmessage);
		}
		else if (type == "conference") 
		{
			HandleConference(jmessage);
		}
		else if (type == "call") 
		{
			HandleCall(jmessage);
		}
		else
		{
			LOG(WARNING) << "Received unknown pkgtype. " << message;
			return false;
		}
	}
	else
	{
		return false;
	}

	return true;
}

bool PeerConnectionClient::HandleCallMessage(const std::string& message, std::string& call_id)
{
	Json::Reader reader;
	Json::Value jmessage;
	if (!reader.parse(message, jmessage)) 
	{
		LOG(WARNING) << "Received unknown message. " << message;
		return false;
	}
	std::string type;
	std::string json_object;

	rtc::GetStringFromJsonObject(jmessage, kPkgType, &type);
	if (!type.empty()) //not empty
	{
		if (type == "call") 
		{
			HandleCall(jmessage, &call_id);
		}
		else
		{
			LOG(WARNING) << "Received unknown pkgtype. " << message;
			return false;
		}
	}
	else
	{
		return false;
	}

	return true;
}

bool PeerConnectionClient::SendMessage(Json::Value& jmessage)
{
	char head[20];
	std::string message, call_id;
	std::string json_body;
	Json::FastWriter writer;

	LOG(INFO) << jmessage["pkgtype"];
	json_body = writer.write(jmessage);
	GenerateHead(head, json_body.size());
	message = head + json_body;
	
	call_id = jmessage["callid"].asString();
	DirSocket* dir_s = FindDirSocket(call_id);
	if (dir_s)
	{
		LOG(INFO) << "dir SendMessage";
		size_t ret = dir_s->socket->Send(message.c_str(), message.length());
		if (ret != message.length())
		{
			LOG(WARNING) << "dir SendMessage failed, len %d, send %d." << message.length() << ret;
			return false;
		}
		return true;
	}

	if (jmessage["pkgtype"] != "connect" && state_ != CONNECTED)
	{
		LOG(WARNING) << "SendMessage failed, state is not CONNECTED, pkgtype %s." << jmessage["pkgtype"];
		return false;
	}
	size_t sent = control_socket_->Send(message.c_str(), message.length());
	if (sent != message.length())
	{
		LOG(WARNING) << "SendMessage failed, len %d, send %d." << message.length() << sent;
		return false;
	}
	//message.clear();
	return true;
}

void PeerConnectionClient::SendConnLogin(std::string &nonces)
{
	std::string id;
	std::string buff;
	Json::Value jmessage;

	jmessage["pkgtype"] = "connect";
	jmessage["msgtype"] = "login";
	jmessage["sequence"] = Sequence();
	jmessage["terminal"] = client_id_;
	buff = client_id_ + nonces + client_mac_;
	id = Md5(buff);
	jmessage["id"] = id.c_str();
	SendMessage(jmessage);
}

void PeerConnectionClient::SendConnLogout(void)
{
	Json::Value jmessage;

	jmessage["pkgtype"] = "connect";
	jmessage["msgtype"] = "logout";
	jmessage["sequence"] = Sequence();
	SendMessage(jmessage);
}

void PeerConnectionClient::SendConnHeart(void)
{
	Json::Value jmessage;
	if (state_ != CONNECTED)
		return;

	jmessage["pkgtype"] = "connect";
	jmessage["msgtype"] = "heartbeat";
	jmessage["sequence"] = Sequence();
	SendMessage(jmessage);
	//rtc::Thread::Current()->PostDelayed(kHeartbeattDelay, this, HEARTBEAT);
}

bool PeerConnectionClient::HandleConnect(const Json::Value &jmessage)
{
	std::string type;
	unsigned int seq;
	
	rtc::GetStringFromJsonObject(jmessage, kMsgType, &type);
	rtc::GetUIntFromJsonObject(jmessage, kSeq, &seq);
	if (type == "authenticate")
	{
		std::string nonces;
		rtc::GetStringFromJsonObject(jmessage, "nonces", &nonces);
		//if (state_ != SIGNING_IN || sequence_ != seq)
		//{
		//	return false;
		//}
		SendConnLogin(nonces);
	}
	else if (type == "login")
	{
		std::string result;
		if (state_ != SIGNING_IN)// || sequence_ != seq)
		{
			return false;
		}

		rtc::GetStringFromJsonObject(jmessage, "result", &result);
		if (result == "success")
		{
			std::string sysinfo;
			rtc::GetStringFromJsonObject(jmessage, "systeminfo", &sysinfo);
			//"systeminfo","stun:192.168.2.1:1000,turn:192.168.3.2:2000"
			callback_->OnSignedIn(sysinfo);
			state_ = CONNECTED;
			//rtc::Thread::Current()->PostDelayed(kHeartbeattDelay, this, HEARTBEAT);
		}
		else
		{
			LOG(WARNING) << "Login failed.";
		}
	}
	else if (type == "logout")
	{
		std::string result;
		if (state_ != SIGNING_OUT_WAITING)// || sequence_ != seq)
		{
			return false;
		}

		rtc::GetStringFromJsonObject(jmessage, "result", &result);
		if (result == "success")
		{
			callback_->OnDisconnected();
			state_ = SIGNING_OUT;
		}
		else
		{
			LOG(WARNING) << "Logout failed.";
		}
	}
	else if (type == "heartbeat")
	{
		std::string result;
		if (state_ != CONNECTED)// || sequence_ != seq)
		{
			return false;
		}

		rtc::GetStringFromJsonObject(jmessage, "result", &result);
		if (result == "success")
		{
			LOG(WARNING) << "heartbeat success.";
		}
		else
		{
			LOG(WARNING) << "Hearbeat failed.";
		}
	}
	else if (type == "getlist")
	{

	}
	else if (type == "state")
	{

	}
	return true;
}

void PeerConnectionClient::SendConfCreate(std::string& lecturer, 
                           std::vector<std::string>& memb_array,
                           int start_time, int duration,
                           std::string& description)
{
	Json::Value jmessage;
	Json::Value memb;
	if (state_ != CONNECTED)
		return;

	jmessage["pkgtype"] = "conference";
	jmessage["msgtype"] = "create";
	jmessage["lecturer"] = lecturer;
	memb = rtc::StringVectorToJsonArray(memb_array);
	jmessage["member"] = memb;//strArray.push_back("hello");
	jmessage["starttime"] = start_time;
	jmessage["duration"] = duration;
	jmessage["description"] = description;
	jmessage["sequence"] = Sequence();
	SendMessage(jmessage);
	//rtc::Thread::Current()->PostDelayed(kHeartbeattDelay, this, HEARTBEAT);//超时功能
	//MessageQueue::Clear(MessageHandler* phandler,uint32_t id,MessageList* removed)
}

void PeerConnectionClient::SendConfNotifyAck(std::string& conf_id, unsigned int seq)
{
	Json::Value jmessage;

	jmessage["pkgtype"] = "conference";
	jmessage["msgtype"] = "notify";
	jmessage["sequence"] = seq;
	jmessage["id"] = conf_id;
	jmessage["result"] = "success";
	SendMessage(jmessage);
}

void PeerConnectionClient::SendConfClose(std::string& conf_id)
{
	Json::Value jmessage;

	jmessage["pkgtype"] = "conference";
	jmessage["msgtype"] = "close";
	jmessage["sequence"] = Sequence();
	jmessage["id"] = conf_id;
	SendMessage(jmessage);
}

bool PeerConnectionClient::HandleConference(const Json::Value &jmessage)
{
	std::string type;
	unsigned int seq;
	
	rtc::GetStringFromJsonObject(jmessage, kMsgType, &type);
	rtc::GetUIntFromJsonObject(jmessage, kSeq, &seq);
	if (type == "create")
	{
		std::string conf_id;
		std::string result;
		int err = 0;
		rtc::GetStringFromJsonObject(jmessage, "id", &conf_id);
		rtc::GetStringFromJsonObject(jmessage, "result", &result);
		if (result == "success")
		{
			LOG(WARNING) << "create conf success.";
			callback_->OnConfCreate(conf_id, err);
		}
		else
		{
			LOG(WARNING) << "create conf failed.";
			err = -1;
			callback_->OnConfCreate(conf_id, err);
		}
	}
	else if (type == "notify")
	{
		std::string lecturer;
		std::string conf_id;
		Json::Value memb;
		std::vector<std::string> memb_array;
		int start_time;
		int duration;
		std::string description;

		rtc::GetStringFromJsonObject(jmessage, "id", &conf_id);
		rtc::GetStringFromJsonObject(jmessage, "lecturer", &lecturer);
		rtc::GetValueFromJsonObject(jmessage, "member", &memb);
		rtc::JsonArrayToStringVector(memb, &memb_array);
		rtc::GetIntFromJsonObject(jmessage, "starttime", &start_time);
		rtc::GetIntFromJsonObject(jmessage, "duration", &duration);
		rtc::GetStringFromJsonObject(jmessage, "description", &description);

		callback_->OnConfNotify(conf_id, lecturer, memb_array, start_time, 
								duration, description);
	    SendConfNotifyAck(conf_id, seq);
	}
	else if (type == "close")
	{
		std::string result;

		rtc::GetStringFromJsonObject(jmessage, "result", &result);
		LOG(WARNING) << "on close conf.";
		callback_->OnCloseConf(result);
	}
	else if (type == "addmemb")
	{

	}
	else if (type == "delmemb")
	{

	}
	return true;
}

bool PeerConnectionClient::SendCallOffer(std::string& from, std::string& to, 
											   std::string& callid, std::string& sdp)
{
	Json::Value jmessage;
	int seq = Sequence();
	jmessage["pkgtype"] = "call";
	jmessage["msgtype"] = "offer";
	jmessage["sequence"] = seq;
	jmessage["callid"] = callid;
	jmessage["from"] = from;
	jmessage["to"] = to;
	jmessage["sdp"] = sdp;
	bool ret = SendMessage(jmessage);
	if (ret)
		InsertCallTimeoutFun(callid, "offer", seq);
	return ret;
}

bool PeerConnectionClient::SendCallAnswer(std::string& from, std::string& to, 
											   std::string& callid, std::string& sdp)
{
	Json::Value jmessage;
	int seq = Sequence();
	jmessage["pkgtype"] = "call";
	jmessage["msgtype"] = "answer";
	jmessage["sequence"] = seq;
	jmessage["callid"] = callid;
	jmessage["from"] = from;
	jmessage["to"] = to;
	jmessage["sdp"] = sdp;
	bool ret = SendMessage(jmessage);
	if (ret)
		InsertCallTimeoutFun(callid, "answer", seq);
	return ret;
}

//candidate
bool PeerConnectionClient::SendCallCandidate(std::string& from, std::string& to,
													std::string& callid, std::string& sdpmid, 
													int index, std::string& candidate)
{
	Json::Value jmessage;
	int seq = Sequence();
	jmessage["pkgtype"] = "call";
	jmessage["msgtype"] = "candidate";
	jmessage["sequence"] = seq;
	jmessage["callid"] = callid;
	jmessage["from"] = from;
	jmessage["to"] = to;
	jmessage["mlineindex"] = index;
	jmessage["sdpmid"] = sdpmid;
	jmessage["candidate"] = candidate;
	bool ret = SendMessage(jmessage);
	if (ret)
		InsertCallTimeoutFun(callid, "candidate", seq);
	return ret;
}

bool PeerConnectionClient::SendCallRinging(std::string& from, std::string& to, 
													std::string& callid, unsigned int seq)
{
	Json::Value jmessage;
	jmessage["pkgtype"] = "call";
	jmessage["msgtype"] = "ringing";
	jmessage["callid"] = callid;
	jmessage["from"] = from;
	jmessage["to"] = to;
	jmessage["sequence"] = seq;
	bool ret = SendMessage(jmessage);
	return ret;
}

bool PeerConnectionClient::SendCallFailed(std::string& from, std::string& to,
												std::string& callid, int code, unsigned int seq)
{
	Json::Value jmessage;
	jmessage["pkgtype"] = "call";
	jmessage["msgtype"] = "failed";
	jmessage["callid"] = callid;
	jmessage["from"] = from;
	jmessage["to"] = to;
	jmessage["sequence"] = seq;
	jmessage["errorcode"] = code;
	bool ret = SendMessage(jmessage);
	return ret;
}

bool PeerConnectionClient::SendCallBye(std::string& from, std::string& to, std::string& callid)
{
	Json::Value jmessage;
	int seq = Sequence();
	jmessage["pkgtype"] = "call";
	jmessage["msgtype"] = "bye";
	jmessage["from"] = from;
	jmessage["to"] = to;
	jmessage["sequence"] = seq;
	jmessage["callid"] = callid;
	bool ret = SendMessage(jmessage);
	if (ret)
		InsertCallTimeoutFun(callid, "bye", seq);
	return ret;
}

bool PeerConnectionClient::SendCallKeepAlive(std::string& from, std::string& to, 
													std::string& callid)
{
	Json::Value jmessage;
	int seq = Sequence();
	jmessage["pkgtype"] = "call";
	jmessage["msgtype"] = "keepalive";
	jmessage["from"] = from;
	jmessage["to"] = to;
	jmessage["sequence"] = seq;
	jmessage["callid"] = callid;
	bool ret = SendMessage(jmessage);
	if (ret)
		InsertCallTimeoutFun(callid, "keepalive", seq);
	return ret;
}

bool PeerConnectionClient::SendCallAck(std::string& from, std::string& to, std::string& callid,
											std::string& method, unsigned int seq)
{
	Json::Value jmessage;
	jmessage["pkgtype"] = "call";
	jmessage["msgtype"] = "ack";
	jmessage["callid"] = callid;
	jmessage["from"] = from;
	jmessage["to"] = to;
	jmessage["sequence"] = seq;
	jmessage["method"] = method;
	bool ret = SendMessage(jmessage);
	return ret;
}

void PeerConnectionClient::InsertCallTimeoutFun(std::string& callid, 
      										std::string method, unsigned short seq)
{
	CallMessage *msg = new CallMessage();
	msg->call_id = callid;
	msg->method = method;
	call_timeout_msg_[CALL+seq] = msg;
	rtc::Thread::Current()->PostDelayed(kCalltimeoutDelay, this, CALL+seq, (rtc::MessageData*)msg);//超时功能
}

void PeerConnectionClient::DeleteCallTimeoutFun(unsigned short seq)
{
	//rtc::Thread::Current()->Clear(this, CALL + seq, NULL);
	CallMessageMap::iterator iter = call_timeout_msg_.find(CALL+seq);
	if (iter != call_timeout_msg_.end())
	{
		delete iter->second;
		call_timeout_msg_.erase(iter);
	}
}

void PeerConnectionClient::MonitorSocketTimeout(rtc::AsyncSocket* socket, int delay)
{
	if (delay <= 0)
	{
		delay = kSockettimeoutDelay;
	}
	rtc::Thread::Current()->PostDelayed(delay, this, CHECKSOCKET, (rtc::MessageData*)socket);//超时功能
}

bool PeerConnectionClient::HandleCall(const Json::Value& jmessage, std::string* call_id)
{
	std::string type, callid;
	unsigned int seq = 0;
	int ret = 0;
	
	rtc::GetStringFromJsonObject(jmessage, kMsgType, &type);
	rtc::GetUIntFromJsonObject(jmessage, kSeq, &seq);
	rtc::GetStringFromJsonObject(jmessage, "callid", &callid);
	if (call_id != NULL)//for direct connect, save callid to dir_socket
	{
		*call_id = callid;
	}
	
	if (type == "offer")
	{
		std::string from, to, sdp;
		rtc::GetStringFromJsonObject(jmessage, "from", &from);
		rtc::GetStringFromJsonObject(jmessage, "to", &to);
		rtc::GetStringFromJsonObject(jmessage, "sdp", &sdp);
		LOG(WARNING) << "Recv offer.";
		ret = callback_->OnRecvOffer(callid, from, to, sdp);//根据返回值确定回复内容
		if (ret == 0)
		{
			SendCallRinging(to, from, callid, seq);
		}
		else
		{
			SendCallFailed(to, from, callid, ret, seq);
		}
	}
	else if (type == "ringing")
	{
		LOG(WARNING) << "Recv ringing.";
		DeleteCallTimeoutFun(seq & 0xffff);
		callback_->OnRecvRinging(callid);
	}
	else if (type == "answer")
	{
		std::string from, to, sdp;
		rtc::GetStringFromJsonObject(jmessage, "from", &from);
		rtc::GetStringFromJsonObject(jmessage, "to", &to);
		rtc::GetStringFromJsonObject(jmessage, "sdp", &sdp);
		LOG(WARNING) << "Recv answer.";
		ret = callback_->OnRecvAnswer(callid, to, from, sdp);
		if (ret == 0)
		{
			SendCallAck(to, from, callid, type, seq);
			callback_->OnSendCandidate(callid);
		}
		else
		{
			SendCallFailed(to, from, callid, ret, seq);
		}
	}
	else if (type == "candidate")
	{
		std::string sdpmid, candidate, from, to;
		int index;
		rtc::GetStringFromJsonObject(jmessage, "from", &from);
		rtc::GetStringFromJsonObject(jmessage, "to", &to);
		rtc::GetIntFromJsonObject(jmessage, "mlineindex", &index);
		rtc::GetStringFromJsonObject(jmessage, "sdpmid", &sdpmid);
		rtc::GetStringFromJsonObject(jmessage, "candidate", &candidate);	
		LOG(WARNING) << "Recv candidate.";
		callback_->OnRecvCandidate(callid, index, sdpmid, candidate);
		SendCallAck(to, from, callid, type, seq);
	}
	else if (type == "keepalive")
	{
		std::string from, to;
		rtc::GetStringFromJsonObject(jmessage, "from", &from);
		rtc::GetStringFromJsonObject(jmessage, "to", &to);	
		LOG(WARNING) << "Recv keepalive.";
		SendCallAck(to, from, callid, type, seq);
	}
	else if (type == "ack")
	{
		std::string method;
		rtc::GetStringFromJsonObject(jmessage, "method", &method);
		LOG(WARNING) << "Recv ack.";
		DeleteCallTimeoutFun(seq & 0xffff);
		callback_->OnRecvAck(callid, method);
		if (method == "bye")
			CloseDirSocket(callid);
	}
	else if (type == "bye")
	{
		std::string from, to;
		rtc::GetStringFromJsonObject(jmessage, "from", &from);
		rtc::GetStringFromJsonObject(jmessage, "to", &to);
		LOG(WARNING) << "Recv bye.";
		callback_->OnRecvBye(callid);
		SendCallAck(to, from, callid, type, seq);
	}
	else if (type == "failed")
	{
		int error;
		rtc::GetIntFromJsonObject(jmessage, "errorcode", &error);
		LOG(WARNING) << "Recv failed.";
		DeleteCallTimeoutFun(seq & 0xffff);
		callback_->OnRecvFailed(callid, error);
		CloseDirSocket(callid);
	}
	return true;
}

void PeerConnectionClient::OnClose(rtc::AsyncSocket* socket, int err) {
	LOG(INFO) << __FUNCTION__;
	socket->Close();

    if (socket == control_socket_.get()) 
    {
		LOG(WARNING) << "Connection refused; retrying in 1 seconds";
		rtc::Thread::Current()->PostDelayed(kReconnectDelay, this, RECONNECT);

		//Close();
		//callback_->OnDisconnected();
    } 
	else if (socket == listen_socket_.get())
	{
		printf("-----listen onclose\n");
	}
    else 
    {
		DirSocket* peer_socket = FindDirSocket(socket);
		if (!peer_socket)
			return;
		callback_->OnDirDisconnected(peer_socket->call_id);
		LOG(WARNING) << "-----dir onclose callid " << peer_socket->call_id<<" " <<socket->GetState();
		MonitorSocketTimeout(socket, kSocketDeleteDelay);//free socket after kSocketDeleteDelay
		EraseDirSocket(peer_socket);
    }
}
//PostDelayed call back
void PeerConnectionClient::OnMessage(rtc::Message* msg) {
	printf("onmessage id %d\n", msg->message_id);
	if (msg->message_id == RECONNECT)
	{
		DoConnect();
	}
	else if (msg->message_id == HEARTBEAT)
	{
		SendConnHeart();
		DirKeepAlive();
		rtc::Thread::Current()->PostDelayed(kHeartbeattDelay, this, HEARTBEAT);
	}
	else if (msg->message_id == CHECKSOCKET)
	{
		rtc::AsyncSocket* socket = reinterpret_cast<rtc::AsyncSocket*>(msg->pdata);
		CheckSocket(socket);
	}
	else if (msg->message_id & CALL)//call message timeout
	{
		CallMessageMap::iterator iter = call_timeout_msg_.find(msg->message_id);
		if (iter == call_timeout_msg_.end())
		{
			return;
		}
		CallMessage* data = reinterpret_cast<CallMessage*>(msg->pdata);
		callback_->OnCallMsgTimeout(data->call_id, data->method);
		callback_->OnDirDisconnected(data->call_id);
		CloseDirSocket(data->call_id);
		delete data;
		call_timeout_msg_.erase(iter);
	}
	else if (msg->message_id & CONF)//conf message timeout
	{
		SendConnHeart();
	}
}


