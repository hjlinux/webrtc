#ifndef PEERCONNECTION_PROCESS_H_
#define PEERCONNECTION_PROCESS_H_

#include <memory>
#include <string>

#include "webrtc/examples/peerconnection/client/main_wnd.h"
#include "webrtc/examples/peerconnection/client/peer_connection_client.h"


class MainProcCallback {
 public:
  virtual void SetTeacher2() = 0;
  virtual void StartLogin(const std::string& server, int port) = 0;
  virtual void DisconnectFromServer() = 0;
  virtual bool ConnectToPeer(std::string& peer_id) = 0;
  virtual void DisconnectFromCurrentPeer(std::string& call_id) = 0;
  virtual void UIThreadCallback(int msg_id, std::string call_id, void* data) = 0;
  virtual void Close() = 0;
 protected:
  virtual ~MainProcCallback() {}
};


// Implements the main UI of the peer connection client.
// This is functionally equivalent to the MainWnd class in the Windows
// implementation.

//user interface
class MainProc {
 public:
  MainProc(const char* server, int port, bool autoconnect, bool autocall);
  ~MainProc();

  virtual void SetTeacher();
  virtual void RegisterObserver(MainProcCallback* callback);//proc to conductor callback
  virtual void UpdateCallState(std::string& call_id, std::string& peer_id, std::string& state);
  //virtual void MessageBox(const char* caption, const char* text,
  //                        bool is_error);
  //virtual MainWindow::UI current_ui();
  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);
  virtual void StopLocalRenderer();
  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video,
    										std::string& call_id, int ch, int disp_pos);
  virtual void StopRemoteRenderer(std::string& call_id);

  virtual void QueueAppThreadCallback(int msg_id, std::string call_id, void* data);

  // Creates and shows the main window with the |Connect UI| enabled.
  bool Create();

  // Destroys the window.  When the window is destroyed, it ends the
  // main message loop.
  bool Destroy();
  bool Init();
  //void ConnectPeer(std::string& peer_id);
  //void DisconnectFromPeer(std::string& call_id);

  // Callback for when the main window is destroyed.
  //void OnDestroyed(GtkWidget* widget, GdkEvent* event);

  // Callback for when the user clicks the "Connect" button.
  //void OnClicked(GtkWidget* widget);

  // Callback for keystrokes.  Used to capture Esc and Return.
  //void OnKeyPress(GtkWidget* widget, GdkEventKey* key);

  // Callback when the user double clicks a peer in order to initiate a
  // connection.
  //void OnRowActivated(GtkTreeView* tree_view, GtkTreePath* path,
  //                    GtkTreeViewColumn* column);

  void OnRedraw();

 protected:
  class VideoRenderer : public rtc::VideoSinkInterface<cricket::VideoFrame> {
   public:
    VideoRenderer(MainProc* main_proc,
                  webrtc::VideoTrackInterface* track_to_render);
    virtual ~VideoRenderer();

    // VideoSinkInterface implementation
    void OnFrame(const cricket::VideoFrame& frame) override;

    const uint8_t* image() const { return image_.get(); }

    int width() const {
      return width_;
    }

    int height() const {
      return height_;
    }

    void SetDisplay(std::string& name,
    			  int ch,
				  int disp_pos);

   protected:
    void SetSize(int width, int height);
    std::unique_ptr<uint8_t[]> image_;
    int width_;
    int height_;
    MainProc* main_proc_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
    std::string name_;
    int ch_;
    int disp_pos_;
  };

  typedef std::map<std::string, std::unique_ptr<VideoRenderer> > RendererMap;

 protected:
  MainProcCallback* callback_;
  std::string server_;
  std::string port_;
  bool autoconnect_;
  bool autocall_;
  std::unique_ptr<VideoRenderer> local_renderer_;
  RendererMap remote_renderers_;
  std::unique_ptr<VideoRenderer> remote_renderer_;
  std::unique_ptr<VideoRenderer> remote_renderer2_;
  std::unique_ptr<uint8_t[]> draw_buffer_;
  int draw_buffer_size_;
  int msgid_;
};

#endif  // WEBRTC_EXAMPLES_PEERCONNECTION_CLIENT_LINUX_MAIN_WND_H_
