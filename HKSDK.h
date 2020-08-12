#ifndef _HKSDK_H_
#define _HKSDK_H_

#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;

class HKSDK
{
public:
    HKSDK();
    ~HKSDK();
    bool open(const string& ip, const string& username, const string& password, int width, int height);
    bool isOpen();
    void release();
    py::tuple getFrame();
    void CALLBACK decFun(int lPort, char *pBuf, int nSize, FRAME_INFO *pFrameInfo, void *nReserved1, int nReserved2);
    void CALLBACK fRealDataCallBack(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer,DWORD dwBufSize,void* dwUser);


private:
    int width;
    int height;
    int port;
    bool status;

}