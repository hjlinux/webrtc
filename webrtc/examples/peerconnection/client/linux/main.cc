#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"

#include "webrtc/examples/peerconnection/client/conductor.h"
#include "webrtc/examples/peerconnection/client/flagdefs.h"
#include "webrtc/examples/peerconnection/client/linux/main_wnd.h"
#include "webrtc/examples/peerconnection/client/peer_connection_client.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "message_queue.h"
#include "class_interface.h"
#include "sqlite_interface.h"

#if 1
extern int AppMsgCreate(int start_flag);
extern int AppProcessMsg(int msgid, int type, int block);
int q_flag = 1;

int gmsgid = 0;

class CustomSocketServer : public rtc::PhysicalSocketServer {
    public:
        CustomSocketServer(rtc::Thread* thread, MainProc* proc)
            : thread_(thread), proc_(proc), conductor_(NULL), client_(NULL) {}
        virtual ~CustomSocketServer() {}

        void set_client(PeerConnectionClient* client) { client_ = client; }
        void set_conductor(Conductor* conductor) { conductor_ = conductor; }

        // Override so that we can also pump the GTK message loop.
        virtual bool Wait(int cms, bool process_io) {
            // Pump GTK events.
            // TODO(henrike): We really should move either the socket server or UI to a
            // different thread.  Alternatively we could look at merging the two loops
            // by implementing a dispatcher for the socket server and/or use
            // g_main_context_set_poll_func.
            //while (gtk_events_pending())
            //  gtk_main_iteration();
            //while(1)
            {
                //printf("mani thread \n");
                //sleep(1);
            }
            AppProcessMsg(gmsgid, 1, 0);

            //usleep(0);

            //if (!wnd_->IsWindow() && !conductor_->connection_active() &&
            //    client_ != NULL && !client_->is_connected()) {
            //  thread_->Quit();
            //}
            if (q_flag == 0)
            {
                thread_->Quit();
            }
            return rtc::PhysicalSocketServer::Wait(1/*cms == -1 ? 1 : cms*/,process_io);
            //return 0;
        }

    protected:
        rtc::Thread* thread_;
        MainProc* proc_;
        Conductor* conductor_;
        PeerConnectionClient* client_;
};
#endif


void getMacAddr(char *macStr)
{
    const char *device = "eth0";
    struct ifreq req; 
    int s32Ret;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    strcpy(req.ifr_name, device);
    s32Ret = ioctl(sock, SIOCGIFHWADDR, &req);
    close(sock);
    if (s32Ret != -1) {
        sprintf(macStr, "%02X%02X%02X%02X%02X%02X",
                req.ifr_hwaddr.sa_data[0]&0xff,
                req.ifr_hwaddr.sa_data[1]&0xff,
                req.ifr_hwaddr.sa_data[2]&0xff,
                req.ifr_hwaddr.sa_data[3]&0xff,
                req.ifr_hwaddr.sa_data[4]&0xff,
                req.ifr_hwaddr.sa_data[5]&0xff);
    }    
}

int main(int argc, char* argv[]) {

    char server[16] = "192.168.13.155";
    unsigned short port = 20000;
    //std::string id = argv[1], mac = argv[2], ip = argv[3];
    std::string id = "1", mac = "", ip = "127.0.0.1";

    zk_sqlite3_open();
    zk_get_server(server,&port);
    struct ifaddrs * ifAddrStruct=NULL;
    void * tmpAddrPtr=NULL;
    getifaddrs(&ifAddrStruct);
    while(ifAddrStruct) {
        tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
        char addressBuffer[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
        //    printf("%s IP Address %s\n",ifAddrStruct->ifa_name, addressBuffer);    
        if(strcmp(ifAddrStruct->ifa_name,"eth0") == 0) {
            ip = addressBuffer;
        }
        ifAddrStruct=ifAddrStruct->ifa_next;
    }
    char macBuff[64];
    getMacAddr(macBuff);
    mac = macBuff;
#if 0
    mac = "00096F002E48";
#endif
    char user_id[32];
    zk_get_user_id(user_id);
    id = user_id;
    printf("server:%s:%d\n",server,port);
    printf("local user_id:%s\n",id.c_str());
    printf("local ip:%s\n",ip.c_str());
    printf("local mac:%s\n",mac.c_str());

    message_queue_init();
    online_class_init();

    MainProc proc(server, port, false, false);
    proc.Create();

    rtc::AutoThread auto_thread;
    rtc::Thread* thread = rtc::Thread::Current();
    CustomSocketServer socket_server(thread, &proc);
    thread->set_socketserver(&socket_server);
    gmsgid = AppMsgCreate(0);
    rtc::InitializeSSL();
    // Must be constructed after we set the socketserver.
    PeerConnectionClient client;
    client.SetClientId(id);
    client.SetClientMac(mac);
    client.SetClientIp(ip);
    rtc::scoped_refptr<Conductor> conductor(
            new rtc::RefCountedObject<Conductor>(&client, &proc));
    // socket_server.set_client(&client);
    // socket_server.set_conductor(conductor);

    //sleep(1);
    proc.Init();

    thread->Run();

    // gtk_main();
    proc.Destroy();

    thread->set_socketserver(NULL);
    // TODO(henrike): Run the Gtk main loop to tear down the connection.
    /*
       while (gtk_events_pending()) {
       gtk_main_iteration();
       }
     */
    rtc::CleanupSSL();
    return 0;
}
