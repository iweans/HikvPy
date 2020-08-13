#include <iostream>
#include <stdio.h>
#include <opencv2/opencv.hpp>
#include "hikvision/HCNetSDK.h"
#include "hikvision/LinuxPlayM4.h"
#include <unistd.h>
#include <map>
#include <mutex>
#include<pybind11/pybind11.h>
#include<pybind11/numpy.h>
#include<pybind11/stl.h>
using namespace cv;
using namespace std;

namespace py = pybind11;

// Frame *bufFrame = NULL;
// mutex mut;

// map<int, int> handleToPort;
// map<int, int> portToHandle;
// map<int, Frame*> handleToFrame;
// map<int, Mutex*> handleToMutex;

class Frame
{
public:
    Frame()
    {
        this->width = 0;
        this->height = 0;
        this->frame_content = NULL;
    }
    Frame(int width, int height, int length, unsigned char* buf)
    {
        this->width = width;
        this->height = height;
        this->length = length;
        this->frame_content = new unsigned char[length];
        memcpy(this->frame_content, buf, length);
    }
    ~Frame()
    {   
        if (frame_content != NULL)
        {
            delete [] frame_content;
            frame_content = NULL;
        }       
    }
    int width;
    int height;
    int length;
    unsigned char* frame_content;
    // unsigned char* convertRGB();
};

map<int, int> handleToPort;
map<int, int> portToHandle;
map<int, Frame*> handleToFrame;
map<int, Mutex*> handleToMutex;
bool initStatus = false;
mutex mut;


py::array_t<unsigned char> cv_mat_uint8_3c_to_numpy(cv::Mat input) {

    py::array_t<unsigned char> dst = py::array_t<unsigned char>({ input.rows,input.cols,3}, input.data);
    return dst;
}

//视频为YUV数据(YV12)，音频为PCM数据
void CALLBACK DecFun(int lPort, char *pBuf, int nSize, FRAME_INFO *pFrameInfo, void *nReserved1, int nReserved2)
{
    auto ph = portToHandle.find(lPort);
    if (ph == portToHandle.end())
    {
        return;
    }
    long lFrameType = pFrameInfo->nType;
    int giCameraFrameWidth = pFrameInfo->nWidth;
	int giCameraFrameHeight = pFrameInfo->nHeight;
    if (lFrameType == T_YV12)
    {
        Frame* frame = new Frame(giCameraFrameWidth, giCameraFrameHeight, nSize, (uchar*)pBuf);

        auto hm = handleToMutex.find(ph->second);
        if (hm == handleToMutex.end())
        {
            return;
        }
        hm->second->lock();
        auto hf = handleToFrame.find(ph->second);
        // delete[] hf->second;
        if (hf->second != NULL)
        {
            delete hf->second;
        }
        hf->second = frame;
        hm->second->unlock();
    }
}

void CALLBACK fRealDataCallBack(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer,DWORD dwBufSize,void* dwUser)
{
    if (handleToPort.find(lRealHandle) == handleToPort.end())
    {
        cout << "handlerId not found !!!=====>" << lRealHandle << endl;
        return;
    }
    int nPort = handleToPort.find(lRealHandle)->second;
    switch (dwDataType)
    {
        case NET_DVR_SYSHEAD:  //系统头
            if (!PlayM4_GetPort(&nPort))   //获取播放库未使用的通道号
            {
                break;
            }
            std::cout << "系统头" << dwBufSize << std::endl;
            if (dwBufSize > 0)
            {
                if (!PlayM4_SetStreamOpenMode(nPort, STREAME_REALTIME)) //设置实时流播放模式
                {
                    break;
                }
                if (!PlayM4_OpenStream(nPort, pBuffer, dwBufSize, 1024 * 1024))  //打开流接口
                {
                    break;
                }
                //解码实时回调
                if (!PlayM4_SetDecCallBack(nPort, DecFun))
                {
                    break;
                }
                if (!PlayM4_Play(nPort, NULL)) //播放开始
                {
                    break;
                }
            }
            cout << "port===============" << nPort << endl;
            handleToPort[lRealHandle] = nPort;
            portToHandle[nPort] = lRealHandle;
            break;
        case NET_DVR_STREAMDATA:
            if (dwBufSize > 0 && nPort != -1)
            {
                if (!PlayM4_InputData(nPort, pBuffer, dwBufSize))
                {
                    break;
                }
            }
            break;
    }
}

py::tuple getFrame(int handleId)
{
    int width = 0;
    int height = 0;
    unsigned char *data = NULL;
    auto hm = handleToMutex.find(handleId);
    if (hm == handleToMutex.end())
    {
        return py::make_tuple(false, NULL);
    }
    hm->second->lock();
    auto hf = handleToFrame.find(handleId);
    if (hf == handleToFrame.end())
    {
        hm->second->unlock();
        return py::make_tuple(false, NULL);
    }
    if (hf->second == NULL)
    {
        hm->second->unlock();
        return py::make_tuple(false, NULL);
    }
    width = hf->second->width;
    height = hf->second->height;
    int length = hf->second->length;
    data = new unsigned char[length];
    memcpy(data, hf->second->frame_content, length);
    hm->second->unlock();

    if (data != NULL)
    {
        Mat dst(height, width, CV_8UC3);//8UC3表示8bit uchar 无符号类型,3通道值
        Mat src(height + height / 2, width, CV_8UC1, data);
        cvtColor(src,dst,CV_YUV2BGR_YV12);

        py::array_t<unsigned char> rsFrame = cv_mat_uint8_3c_to_numpy(dst);
        delete [] data;
        return py::make_tuple(true, rsFrame);
    }
    delete [] data;
    return py::make_tuple(false, NULL);
}

py::tuple init()
{
    mut.lock();
    if (initStatus)
    {
        mut.unlock();
        return py::make_tuple(true, 0);
    }
    bool rsCode = NET_DVR_Init();
    if (rsCode) 
    {
        initStatus = true;
        mut.unlock();
        return py::make_tuple(true, 0);
    }
    mut.unlock();
    return py::make_tuple(false, NET_DVR_GetLastError());
}

py::tuple login(char* ip, char* username, char* password)
{
    NET_DVR_USER_LOGIN_INFO struLoginInfo = {0};
    NET_DVR_DEVICEINFO_V40 struDeviceInfoV40 = {0};
    struLoginInfo.bUseAsynLogin = false;

    struLoginInfo.wPort = 8000;
    strcpy(struLoginInfo.sDeviceAddress, ip);
    strcpy(struLoginInfo.sUserName, username);
    strcpy(struLoginInfo.sPassword, password);

    int userId = NET_DVR_Login_V40(&struLoginInfo, &struDeviceInfoV40);
    if (userId < 0)
    {
        return py::make_tuple(userId, NET_DVR_GetLastError());
    }
    return py::make_tuple(userId, 0);
}

py::tuple open(int userId)
{
    NET_DVR_PREVIEWINFO struPlayInfo = { 0 };
    struPlayInfo.hPlayWnd = NULL;         //需要SDK解码时句柄设为有效值，仅取流不解码时可设为空
    struPlayInfo.lChannel = 1;       //预览通道号
    struPlayInfo.dwStreamType = 0;       //0-主码流，1-子码流，2-码流3，3-码流4，以此类推
    struPlayInfo.dwLinkMode = 0;       //0- TCP方式，1- UDP方式，2- 多播方式，3- RTP方式，4-RTP/RTSP，5-RSTP/HTTP
    struPlayInfo.bBlocked = 0;    //0- 非阻塞取流,1- 阻塞取流

    int handlerId = NET_DVR_RealPlay_V40(userId, &struPlayInfo, fRealDataCallBack, NULL);
    if (handlerId < 0)
    {
        return py::make_tuple(handlerId, NET_DVR_GetLastError());
    }
    handleToPort[handlerId] = -1;
    handleToMutex[handlerId] = new Mutex();
    handleToFrame[handlerId] = NULL;
    return py::make_tuple(handlerId, 0);
}

py::tuple logout(int userId)
{
    bool rsCode = NET_DVR_Logout(userId);
    if (rsCode)
    {
        return py::make_tuple(true, 0);
    }
    return py::make_tuple(false, NET_DVR_GetLastError());
}

py::tuple closeCamera(int handler) {
    auto hp = handleToPort.find(handler);
    if (hp == handleToPort.end())
    {
        return py::make_tuple(true, 0);
    }
    handleToPort.erase(hp->first);
    int port = hp->second;
    PlayM4_Stop(port);
    PlayM4_CloseStream(port);
    PlayM4_FreePort(port);
    NET_DVR_StopRealPlay(handler);
    cout << "***********************\n";
    auto ph = portToHandle.find(port);
    if (ph != portToHandle.end())
    {
        portToHandle.erase(port);
    }
    auto hm = handleToMutex.find(handler);
    if (hm != handleToMutex.end())
    {
        hm->second->lock();
        auto hf = handleToFrame.find(handler);
        if (hf != handleToFrame.end())
        {
            delete hf->second;
            handleToFrame.erase(hf->first);
        }
        hm->second->unlock();
        handleToMutex.erase(hm->first);
        delete hm->second;
    }
    return py::make_tuple(true, 0);
}

PYBIND11_MODULE(HKSDK, m) {
    m.doc() = "hksdk module";
    // Add bindings here
    m.def("init", &init);
    m.def("login", &login);
    m.def("logout", &logout);
    m.def("get_frame", &getFrame);
    m.def("open", &open);
    m.def("close", &closeCamera);
}
