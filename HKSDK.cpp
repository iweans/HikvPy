#include <iostream>
#include <stdio.h>
#include <opencv2/opencv.hpp>
#include "hikvision/HCNetSDK.h"
#include "string.h"
#include "hikvision/LinuxPlayM4.h"
#include <unistd.h>
#include <thread>
#include <mutex>
#include<pybind11/pybind11.h>
#include<pybind11/numpy.h>
#include<pybind11/stl.h>
using namespace cv;
using namespace std;

namespace py = pybind11;

LONG nPort;
int iFrameHeight = 2160;
int iFrameWidth = 3840;
int c = 1;

list<Mat> frames;
mutex mut;

LONG lUserID;
LONG lRealPlayHandle;

py::array_t<unsigned char> cv_mat_uint8_3c_to_numpy(cv::Mat input) {

    py::array_t<unsigned char> dst = py::array_t<unsigned char>({ input.rows,input.cols,3}, input.data);
    return dst;
}

void yv12toYUV(char *outYuv, char *inYv12, int width, int height, int widthStep)
{
	int col, row;
	unsigned int Y, U, V;
	int tmp;
	int idx;

	for (row = 0; row<height; row++)
	{
		idx = row * widthStep;
		int rowptr = row*width;

		for (col = 0; col<width; col++)
		{

			tmp = (row / 2)*(width / 2) + (col / 2);
			Y = (unsigned int)inYv12[row*width + col];
			U = (unsigned int)inYv12[width*height + width*height / 4 + tmp];
			V = (unsigned int)inYv12[width*height + tmp];
			outYuv[idx + col * 3] = Y;
			outYuv[idx + col * 3 + 1] = U;
			outYuv[idx + col * 3 + 2] = V;
		}
	}
}

Mat Yv12ToRGB(uchar *pBuffer, int width, int height)
{
    Mat result(height, width, CV_8UC3);
    uchar y, cb, cr;

    long ySize = width * height;
    long uSize;
    uSize = ySize >> 2;

    //assert(bufferSize == ySize + uSize * 2);

    uchar *output = result.data;
    uchar *pY = pBuffer;
    uchar *pU = pY + ySize;
    uchar *pV = pU + uSize;

    uchar r, g, b;
    for (int i = 0; i<uSize; ++i)
    {
        for (int j = 0; j<4; ++j)
        {
            y = pY[i * 4 + j];
            cb = pU[i];
            cr = pV[i];

            //ITU-R standard
            b = saturate_cast<uchar>(y + 1.772*(cb - 128));
            g = saturate_cast<uchar>(y - 0.344*(cb - 128) - 0.714*(cr - 128));
            r = saturate_cast<uchar>(y + 1.402*(cr - 128));

            *output++ = b;
            *output++ = g;
            *output++ = r;
        }
    }
    return result;
}

bool YV12_to_RGB32(unsigned char* pYV12, unsigned char* pRGB32, int iWidth, int iHeight)
{
    if (!pYV12 || !pRGB32)
        return false;
 
    const long nYLen = long(iHeight * iWidth);
    const int nHfWidth = (iWidth >> 1);
 
    if (nYLen < 1 || nHfWidth < 1)
        return false;
 
    unsigned char* yData = pYV12;
    unsigned char* vData = pYV12 + iWidth*iHeight + (iHeight / 2)*(iWidth / 2);//&vData[nYLen >> 2];
    unsigned char* uData = pYV12 + iWidth*iHeight;// &yData[nYLen];
    if (!uData || !vData)
        return false;
 
    int rgb[4];
    int jCol, iRow;
    for (iRow = 0; iRow < iHeight; iRow++)
    {
        for (jCol = 0; jCol < iWidth; jCol++)
        {
            rgb[3] = 1;
 
            int Y = yData[iRow*iWidth + jCol];
            int U = uData[(iRow / 2)*(iWidth / 2) + (jCol / 2)];
            int V = vData[(iRow / 2)*(iWidth / 2) + (jCol / 2)];
            int R = Y + (U - 128) + (((U - 128) * 103) >> 8);
            int G = Y - (((V - 128) * 88) >> 8) - (((U - 128) * 183) >> 8);
            int B = Y + (V - 128) + (((V - 128) * 198) >> 8);
 
            // r分量值 
            R = R<0 ? 0 : R;
            rgb[2] = R > 255 ? 255 : R;
            // g分量值
            G = G<0 ? 0 : G;
            rgb[1] = G>255 ? 255 : G;
            // b分量值 
            B = B<0 ? 0 : B;
            rgb[0] = B>255 ? 255 : B;
            pRGB32[4 * (iRow*iWidth + jCol) + 0] = rgb[0];
            pRGB32[4 * (iRow*iWidth + jCol) + 1] = rgb[1];
            pRGB32[4 * (iRow*iWidth + jCol) + 2] = rgb[2];
            pRGB32[4 * (iRow*iWidth + jCol) + 3] = rgb[3];
        }
    }
 
    return true;
}


//视频为YUV数据(YV12)，音频为PCM数据
void CALLBACK DecFun(int lPort, char *pBuf, int nSize, FRAME_INFO *pFrameInfo, void *nReserved1, int nReserved2)
{
    long lFrameType = pFrameInfo->nType;
    int giCameraFrameWidth = pFrameInfo->nWidth;
	int giCameraFrameHeight = pFrameInfo->nHeight;
    if (lFrameType == T_YV12)
    {
        Mat dst(pFrameInfo->nHeight,pFrameInfo->nWidth,CV_8UC3);//这里nHeight为720,nWidth为1280,8UC3表示8bit uchar 无符号类型,3通道值
        Mat src(pFrameInfo->nHeight + pFrameInfo->nHeight/2,pFrameInfo->nWidth,CV_8UC1,(uchar*)pBuf);
        cvtColor(src,dst,CV_YUV2BGR_YV12);


        lock_guard<mutex> lk(mut);
        if (frames.size() >= 2) {
            frames.pop_front();
        }
        frames.push_back(dst);
    }
}

void CALLBACK fRealDataCallBack(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer,DWORD dwBufSize,void* dwUser)
{
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
                if (!PlayM4_OpenStream(nPort, pBuffer, dwBufSize, iFrameHeight * iFrameWidth))  //打开流接口
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
            break;
        case NET_DVR_STREAMDATA:
            if (dwBufSize > 0 && nPort != -1)
            {
                if (!PlayM4_InputData(nPort, pBuffer, dwBufSize))
                {
                    break;
                    // Sleep(10);
                    // inData=PlayM4_InputData(nPort, pBuffer, dwBufSize);
                }
            }
            break;
    }
}

void readC(char* ip, char* username, char* password)
{
    // NET_DVR_LOCAL_SDK_PATH struComPath = {0};
    // sprintf(struComPath.sPath, "/home/jskj/桌面/ipc_demo/lib"); //HCNetSDKCom文件夹所在的路径
    // NET_DVR_SetSDKInitCfg(NET_SDK_INIT_CFG_SDK_PATH, &struComPath);

    NET_DVR_Init();
    // long lUserID;
    //login
    NET_DVR_USER_LOGIN_INFO struLoginInfo = {0};
    NET_DVR_DEVICEINFO_V40 struDeviceInfoV40 = {0};
    struLoginInfo.bUseAsynLogin = false;

    struLoginInfo.wPort = 8000;
    memcpy(struLoginInfo.sDeviceAddress, ip, NET_DVR_DEV_ADDRESS_MAX_LEN);
    memcpy(struLoginInfo.sUserName, username, NAME_LEN);
    memcpy(struLoginInfo.sPassword, password, NAME_LEN);

    lUserID = NET_DVR_Login_V40(&struLoginInfo, &struDeviceInfoV40);

    if (lUserID < 0)
    {
        printf("pyd1---Login error, %d\n", NET_DVR_GetLastError());
        return;
    }


    // LONG lRealPlayHandle;
    //HWND hWnd = GetConsoleWindow(); Do not set the handle twice, or you get shacking image.
    NET_DVR_PREVIEWINFO struPlayInfo = { 0 };
    struPlayInfo.hPlayWnd = NULL;         //需要SDK解码时句柄设为有效值，仅取流不解码时可设为空
    struPlayInfo.lChannel = 1;       //预览通道号
    struPlayInfo.dwStreamType = 0;       //0-主码流，1-子码流，2-码流3，3-码流4，以此类推
    struPlayInfo.dwLinkMode = 0;       //0- TCP方式，1- UDP方式，2- 多播方式，3- RTP方式，4-RTP/RTSP，5-RSTP/HTTP
    struPlayInfo.bBlocked = 0;    //0- 非阻塞取流,1- 阻塞取流

    //lRealPlayHandle = NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, g_RealDataCallBack_V30, NULL);
    lRealPlayHandle = NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, fRealDataCallBack, NULL);
    if (lRealPlayHandle < 0)
    {
        printf("NET_DVR_RealPlay_V40 error, %d\n", NET_DVR_GetLastError());
        NET_DVR_Logout(lUserID);
        NET_DVR_Cleanup();
        return;
    }

    DWORD dwReturnLen;
    NET_DVR_COMPRESSIONCFG_V30 struParams = {0};
    bool iRet = NET_DVR_GetDVRConfig(lUserID, NET_DVR_GET_COMPRESSCFG_V30, 1, \
    &struParams, sizeof(NET_DVR_COMPRESSIONCFG_V30), &dwReturnLen);

    // sleep(-1);
    //cvWaitKey(0);
//    Sleep(-1); //-1 : forever;

    //logout
    // NET_DVR_Logout_V30(lUserID);
    // NET_DVR_Cleanup();

}

// void show()
// {
//     while (true)
//     {
//         lock_guard<mutex> lk(mut);
//         if (frames.size() > 0)
//         {
//             cout << "xsdasdasd asd " << endl;
//             Mat frame = (Mat)frames.front();
//             frames.pop_front();
//             imwrite("t.bmp", frame);
//         }
//         sleep(0.5);
//     }
// }

py::tuple getframe()
{
    Mat orgFrame;
    mut.lock();
    if (frames.size() > 0)
    {
        orgFrame = (Mat)frames.front();
        frames.clear();
    }
    mut.unlock();
    if (!orgFrame.empty())
    {
        py::array_t<unsigned char> frame = cv_mat_uint8_3c_to_numpy(orgFrame);
        return py::make_tuple(true, frame);
    }
    return py::make_tuple(false, NULL);
}

// py::array_t<unsigned char> getframe()
// {
//     py::array_t<unsigned char> frame;
//     lock_guard<mutex> lk(mut);
//     if (frames.size() > 0)
//     {
//         char* fm = (char*)frames.front();
//         frame = py::array_t<unsigned char>({1080, 1920, 3}, reinterpret_cast<unsigned char*>(fm));
//         frames.pop_front();
//     }
//     return frame;
// }



void open(char* ip, char* username, char* password) {
    // thread t(readC);
    // t.detach();
    readC(ip, username, password);
}

void release() {
    NET_DVR_StopRealPlay(lRealPlayHandle);
    //注销用户
    NET_DVR_Logout(lUserID);
    NET_DVR_Cleanup();
}

PYBIND11_MODULE(HKSDK, m) {
    m.doc() = "hksdk module";
    // Add bindings here
    m.def("getframe", &getframe);
    m.def("open", &open);
    m.def("release", &release);
}
