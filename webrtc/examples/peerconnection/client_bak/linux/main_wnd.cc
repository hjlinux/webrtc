#include "webrtc/examples/peerconnection/client/linux/main_wnd.h"

#include <stddef.h>
#include <pthread.h> 
#include <iostream>

#include "webrtc/examples/peerconnection/client/defaults.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringutils.h"

#include <sys/msg.h>  
#include <errno.h>
#include "param_conf.h"
#include "resource_note.h"
#include "sqlite_interface.h"
#include "message_queue.h"
#include <stdio.h>
//#include "display.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>  
#include <string.h>
#include <sys/types.h>  
#include <unistd.h>   
#include <arpa/inet.h>  
#include <netinet/in.h>  
#include <errno.h>  
#include <strings.h>


using rtc::sprintfn;

int AppMsgCreate(int start_flag);
int AppProcessMsg(int msgid, int type, int block);
extern int q_flag;
std::string name;



namespace {

#define UI_CMD  6

    int start_callback(void *param,int ncount,char **pvalue,char **pname)
    {
        printf("%s=%s\n",pname[2],pvalue[2]);
        MainProc *p = (MainProc *)param;
        p->SetTeacher();
        setTeacher();
        std::string* msg = new std::string("dirconnect");
        p->QueueAppThreadCallback(UI_CMD, pvalue[2], msg);
        return 0;
    }

#if 1
    void* ProcThread(void *arg)  
    {  
        int gmsgid = 0;
        printf( "in proc thread\n");  

        gmsgid = AppMsgCreate(0);

        while(1)
        {
            AppProcessMsg(gmsgid, 2, 1);
            //usleep(100);
        }
        return NULL;  
    }
#endif


    int GetSocketData(char *data,int len)  
    {         
        static int socketfd = 0;
        int port = 8000;  
        struct sockaddr_in sin,pin;  
        int temp_sock_descriptor;
        socklen_t address_size;  
        char host_name[20];  
        int i, on=1;  

        if(socketfd <= 0) {
            socketfd = socket(AF_INET,SOCK_STREAM,0);  
            bzero(&sin,sizeof(sin));  
            sin.sin_family = AF_INET;  
            sin.sin_addr.s_addr = INADDR_ANY;  
            sin.sin_port = htons(port);  
            if(bind(socketfd,(struct sockaddr *)&sin,sizeof(sin)) == -1)  
            {
                perror("call to bind");  
                exit(1);  
            }
            if(listen(socketfd,100) == -1)  
            {
                perror("call to listem");  
                exit(1);  
            }
            printf("Accpting connections...\n");  
            int bReuseaddr = 1;
            setsockopt(socketfd,SOL_SOCKET ,SO_REUSEADDR,&bReuseaddr,sizeof(bReuseaddr));
        }

        address_size = sizeof(pin);  
        temp_sock_descriptor = accept(socketfd,(struct sockaddr *)&pin,&address_size);  
        if(temp_sock_descriptor == -1)  
        {   
            perror("call to accept");  
            exit(1);  
        }
        int ret;  
        if((ret=recv(temp_sock_descriptor,data,len,0)) == -1)  
        {   
            perror("call to recv");  
            exit(1);  
        }   
        if(data[ret-1] == '\n')
            data[ret-1] = '\0';

        data[ret] = '\0';
        inet_ntop(AF_INET,&pin.sin_addr,host_name,sizeof(host_name));  
        printf("received from client(%s):%s\n",host_name,data);  
        close(temp_sock_descriptor);
    }


    typedef struct
    {
        int netType;
        char userID[32];
        char userIP[16];
        unsigned short userPort;
    }ST_MSG_CLASS_GUI_NOTIFY_START;

    ST_MSG_CLASS_GUI_NOTIFY_START notice[3];

    void* ProcThreadInput(void *arg)  
    {  
        char *data;
        char str[50]={0};
        MainProc *p = (MainProc *)arg;
        printf( "in proc input thread\n");  

        int msgid = AppMsgCreate(0);
        while(1)
        {
#if 0
            //std::cin.getline(str, 40);
            GetSocketData(str,40);
            data = str;
#else
#if 1
            int msgType;
            int ret = RECV2(&msgType, notice);
            if(ret < 0) {
                printf("msgrcv:%d\n",ret);
                continue;    
            }
            int i;
            char method[128];
            switch(msgType) {
                case 1:
                    p->SetTeacher();
                    setTeacher();
                    //connect
                    for(i=0;i<3;i++) {
                        if(notice[i].netType == 1) {
                            std::string* msg = new std::string("connect");
                            p->QueueAppThreadCallback(UI_CMD, notice[i].userID, msg);
                        } else if(notice[i].netType == 2) {
                            std::string* msg = new std::string("dirconnect");
                            sprintf(method,"%s:%s",notice[i].userIP,notice[i].userPort);
                            p->QueueAppThreadCallback(UI_CMD, method, msg);
                        }
                    }
                    break;
                case 2:
                    std::string* msg = new std::string("disconnect");
                    for(i=0;i<3;i++) {
                        if(notice[i].netType == 1) {
                            p->QueueAppThreadCallback(UI_CMD, notice[i].userID, msg);
                        } else if(notice[i].netType == 2) {
                            p->QueueAppThreadCallback(UI_CMD, notice[i].userIP, msg);
                        }
                    }
                    //bye
                    break;    
            }
#else
            memset(str,0,50);
            int ret = msgrcv(msgid, (void*)str, 50, 3, MSG_NOERROR);//IPC_NOWAIT
            if(ret < 0) {
                printf("msgrcv:%d\n",ret);
                continue;    
            }
            //printf("type=%d,ret=%d\n",*(int *)str,ret);
            data = str + 4;
#endif
#endif

#if 0
            printf("msgrcv:%s\n",data);
            //std::cout << str << std::endl;
            if (strstr(data, "list_start") != NULL)
            {
                //std::string* msg = new std::string("list");
                //p->QueueAppThreadCallback(UI_CMD, NULL, msg);

                //char sql[256] = "select * from user_info_local";
                //zk_sqlite3_exec(sql,start_callback,p);
            }
            else if (data[0] == 'c')
            {
                std::string peer_id = data + 2;
                if (peer_id.size() > 0)
                {
                    std::cout << "connect to peerid " << peer_id << std::endl;
                }
#if 1
                p->SetTeacher();
                setTeacher();
#endif
                std::string* msg = new std::string("connect");
                p->QueueAppThreadCallback(UI_CMD, peer_id, msg);
            }
            else if (data[0] == 'd')
            {
                std::string peer_ip = data + 2;
                if (peer_ip.size() > 0)
                {
                    std::cout << "connect to ip " << peer_ip << std::endl;
                }
#if 1
                p->SetTeacher();
                setTeacher();
                std::string* msg = new std::string("dirconnect");
                p->QueueAppThreadCallback(UI_CMD, peer_ip, msg);
#else
#endif
            }
            else if (data[0] == 'b')
            {
                std::string call_id = data + 2;
                if (call_id.size() > 0)
                {
                    std::cout << "disconnect to callid " << call_id << std::endl;
                }
                std::string* msg = new std::string("disconnect");
                p->QueueAppThreadCallback(UI_CMD, call_id, msg);
            }
            else if (data[0] == 'q')
            {
                std::string* msg = new std::string("quit");
                std::string id;
                p->QueueAppThreadCallback(UI_CMD, id, msg);
                q_flag = 0;
            }
#endif
        }
        return NULL;  
    }


    bool ProcThreadCreate(void *p)
    {
        pthread_t th;
        int ret = 0;
        int arg = 10;
        //int *thread_ret = NULL;
        ret = pthread_create(&th, NULL, ProcThread, &arg);
        if(ret != 0)
        {
            printf( "Create thread error!\n");
            return -1;	
        }

        ret = pthread_create(&th, NULL, ProcThreadInput, p);
        if(ret != 0)
        {
            printf( "Create thread input error!\n");
            return -1;	
        }

        printf( "Create proc thread success!\n");
        return 1;
    }

#if 0
    struct AppThreadCallbackData {
        explicit AppThreadCallbackData(MainProcCallback* cb, int id, void* d)
            : callback(cb), msg_id(id), data(d) {}
        MainProcCallback* callback;
        int msg_id;
        void* data;
    };
#endif

    struct AppThreadCallbackData 
    {
        MainProcCallback* callback;
        int msg_id;
        std::string call_id;
        void* data;
    };


    bool HandleAppThreadCallback(void *data) {
        AppThreadCallbackData* cb_data = reinterpret_cast<AppThreadCallbackData*>(data);
        cb_data->callback->UIThreadCallback(cb_data->msg_id, cb_data->call_id, cb_data->data);
        delete cb_data;
        return false;
    }

#if 1
    bool Redraw(void *data) {
        MainProc* proc = reinterpret_cast<MainProc*>(data);
        proc->OnRedraw();
        return false;
    }
#endif

    struct AppThreadCallbackMsg
    {
        bool (*fun)(void *data);  
        void* cb_data;//AppThreadCallbackData
    };


    struct AppThreadMsg
    {
        long int msg_type;
        struct AppThreadCallbackMsg msg;
    };


    int AppMsgSend(int msgid, int type, struct AppThreadMsg *data)
    {
        int size;
        if (msgid < 0)
            return -1;

        data->msg_type = type;//1
        size = sizeof(struct AppThreadMsg) - sizeof(long int);

        if(msgsnd(msgid, (void*)data, size, 0) == -1)  
        {  
            printf("msgsnd failed\n");	
            return -1;	
        }  
        return 0;
    }
}  // namespace

int AppMsgCreate(int start_flag)
{
    int msgid = -1;  

    if (start_flag == 1)
    {
        msgid = msgget((key_t)1234, 0666 | IPC_CREAT | IPC_EXCL);	
        if(msgid == -1)  
        {  
            if (errno == EEXIST)
            {
                int ret = 0;
                char msgp[1400];
                msgid = msgget((key_t)1234, 0666 | IPC_CREAT);	
                if(msgid < 0) {
                    return msgid;    
                }
                while(1) {
                    ret = msgrcv(msgid, msgp, 1400-4,0,IPC_NOWAIT);
                    if((ret < 0) && (errno == ENOMSG))
                        break;
                }
                return msgid;
                /*
                   printf("msgqueue is exist, delete, %d.\n", errno);
                   msgid = msgget((key_t)1234, 0666 | IPC_CREAT);
                   msgctl(msgid, IPC_RMID, 0);
                 */
            }
        }
        else
        {
            return msgid;
        }
    }

    msgid = msgget((key_t)1234, 0666 | IPC_CREAT);	
    if(msgid == -1)  
    {  
        printf("msgget failed with error: %d\n", errno);  
    } 
    return msgid;
}


int AppProcessMsg(int msgid, int type, int block)
{

    struct AppThreadMsg data;
    int size, ret;
    long int msgtype = type; //0
    int isblock = IPC_NOWAIT;

    if (block == 1)
    {
        isblock = 0;//block
    }

    size = sizeof(struct AppThreadMsg) - sizeof(long int);
    ret = msgrcv(msgid, (void*)&data, size, msgtype, isblock);//IPC_NOWAIT
    if(ret == -1)  
    {  
        return 0;  
    }

    data.msg.fun(data.msg.cb_data);

    return 0;
}

//
// MainProc implementation.
//

MainProc::MainProc(const char* server, int port, bool autoconnect,
        bool autocall)
    : server_(server), autoconnect_(autoconnect), autocall_(autocall) {
        char buffer[10];
        sprintfn(buffer, sizeof(buffer), "%i", port);
        port_ = buffer;
    }

MainProc::~MainProc() {

}

void MainProc::SetTeacher() {
    callback_->SetTeacher2();
}
//observer proc
void MainProc::RegisterObserver(MainProcCallback* callback) {
    callback_ = callback;
}

void MainProc::StartLocalRenderer(webrtc::VideoTrackInterface* local_video) {
    local_renderer_.reset(new VideoRenderer(this, local_video));
}

void MainProc::StopLocalRenderer() {
    local_renderer_.reset();
}

void MainProc::StartRemoteRenderer(
        webrtc::VideoTrackInterface* remote_video) {
    remote_renderer_.reset(new VideoRenderer(this, remote_video));
}

void MainProc::StartRemoteRenderer(
        webrtc::VideoTrackInterface* remote_video,
        std::string& call_id, int ch, int disp_pos) {

    if (ch == 0)
    {
        remote_renderers_[call_id].reset(new VideoRenderer(this, remote_video));
        remote_renderers_[call_id]->SetDisplay(call_id, ch, disp_pos);
    }
    else if (ch == 1)
    {
        remote_renderer2_.reset(new VideoRenderer(this, remote_video));
        remote_renderer2_->SetDisplay(call_id, ch, disp_pos);
    }
}

void MainProc::StopRemoteRenderer(std::string& call_id) {
    RendererMap::iterator iter = remote_renderers_.find(call_id);
    if (iter != remote_renderers_.end())
        remote_renderers_.erase(iter);
    //remote_renderers_[call_id].reset();
    remote_renderer2_.reset();
}

void MainProc::QueueAppThreadCallback(int msg_id, std::string call_id, void* data) {
    struct AppThreadMsg datamsg;
    struct AppThreadCallbackData *pdata;

    pdata = new AppThreadCallbackData;
    pdata->callback = callback_;
    pdata->msg_id = msg_id; 
    pdata->call_id = call_id;
    pdata->data = data;

    datamsg.msg.fun = HandleAppThreadCallback;
    datamsg.msg.cb_data = (void *)pdata; 

    AppMsgSend(msgid_, 1, &datamsg);
}


bool MainProc::Create() {

    msgid_ = AppMsgCreate(1);
    //DisplayInit();
    ProcThreadCreate(this);

    return 0;
}

bool MainProc::Destroy() {
    //destroy >>??

    return true;
}


bool MainProc::Init() {
    int port = port_.length() ? atoi(port_.c_str()) : 0;

    callback_->StartLogin(server_, port);

    return true;
}

void MainProc::UpdateCallState(std::string& call_id, std::string& peer_id, 
        std::string& state)
{



}

void MainProc::OnRedraw() {
    //gdk_threads_enter();
#if 1
    VideoRenderer* remote_renderer = remote_renderers_[name].get();
    if (remote_renderer && remote_renderer->image() != NULL) {
        int width = remote_renderer->width();
        int height = remote_renderer->height();

        //printf("%d,%d\n", width, height);

        //DisplayYuv((unsigned char *)remote_renderer->image(),width,height);

    }
#endif
}

MainProc::VideoRenderer::VideoRenderer(
        MainProc* main_proc,
        webrtc::VideoTrackInterface* track_to_render)
    : width_(0),
    height_(0),
    main_proc_(main_proc),
    rendered_track_(track_to_render) {
        rendered_track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
    }

MainProc::VideoRenderer::~VideoRenderer() {
    rendered_track_->RemoveSink(this);
}

void MainProc::VideoRenderer::SetSize(int width, int height) {
    //gdk_threads_enter();

    if (width_ == width && height_ == height) {
        return;
    }

    width_ = width;
    height_ = height;
    image_.reset(new uint8_t[width * height * 4]);
    //gdk_threads_leave();
}

void MainProc::VideoRenderer::SetDisplay(std::string& call_id, int ch, int disp_pos)
{
    name_ = call_id;
    name = call_id;
    ch_ = ch;
    disp_pos_ = disp_pos;
}

void MainProc::VideoRenderer::OnFrame(
        const cricket::VideoFrame& video_frame) {
    //gdk_threads_enter();

    struct AppThreadMsg datamsg;

    const cricket::VideoFrame* frame = video_frame.GetCopyWithRotationApplied();

    SetSize(frame->width(), frame->height());

    //int size = width_ * height_ * 4;
    int size = width_ * height_;
    // int disp_id;


    //frame->video_frame_buffer()
    //printf("\nyuv %d,%d,%d\n",
    //frame->video_frame_buffer()->StrideY(),
    //frame->video_frame_buffer()->StrideU(),
    //frame->video_frame_buffer()->StrideV());

    //DisplayYuv(main_proc_->pdisplay_, (unsigned char *)frame->video_frame_buffer()->DataY(),
    //  (unsigned char *)frame->video_frame_buffer()->DataU(),
    //  (unsigned char *)frame->video_frame_buffer()->DataV());

#if 0

    VideoRenderer* remote_renderer = remote_renderer_.get();
    if (remote_renderer && remote_renderer->image() != NULL) {
        int width = remote_renderer->width();
        int height = remote_renderer->height();


        //const uint32_t* image =
        //    reinterpret_cast<const uint32_t*>(remote_renderer->image());

        DisplayYuv((unsigned char *)remote_renderer->image(),width,height);
#endif

        //return;

#if 1
        char *p = (char *)frame->video_frame_buffer()->DataY();
        if(p)
        {
            int disp_id = p[0];
            SetDisplayInfo(disp_id, (char *)name_.c_str(), ch_, disp_pos_);
        }
        return;
#endif

        memcpy(image_.get(), frame->video_frame_buffer()->DataY(), size);
        memcpy(image_.get() + size, frame->video_frame_buffer()->DataU(), size/4);
        memcpy(image_.get() + size + size/4, frame->video_frame_buffer()->DataV(), size/4);

        //printf("\nwidth %d,%d\n\n\n",this->width(),this->height());
        //printf("-------------width %d, height %d\n",width_ , height_);
#if 0
        return;

        // TODO(henrike): Convert directly to RGBA
        frame->ConvertToRgbBuffer(cricket::FOURCC_ARGB,
                image_.get(),
                size,
                width_ * 4);
        // Convert the B,G,R,A frame to R,G,B,A, which is accepted by GTK.
        // The 'A' is just padding for GTK, so we can use it as temp.
        uint8_t* pix = image_.get();
        uint8_t* end = image_.get() + size;
        while (pix < end) {
            pix[3] = pix[0];     // Save B to A.
            pix[0] = pix[2];  // Set Red.
            pix[2] = pix[3];  // Set Blue.
            pix[3] = 0xFF;     // Fixed Alpha.
            pix += 4;
        }
#endif
        //gdk_threads_leave();

        //g_idle_add(Redraw, main_wnd_);
        datamsg.msg.fun = Redraw;
        datamsg.msg.cb_data = (void *)main_proc_; 

        AppMsgSend(main_proc_->msgid_, 2, &datamsg);
    }

