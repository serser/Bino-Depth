// BinoDepth.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <thread>

#include "pxcbase.h"
#include "pxcsensemanager.h"
#include "pxcmetadata.h"
#include "service/pxcsessionservice.h"
#include "pxccapture.h"
#include "pxccapturemanager.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <mutex>  

using namespace std;
using namespace cv;

bool CreateFolder(const wchar_t * path)
{
	if (!CreateDirectory(path, NULL)){
		return false;
	}
	return true;
}

void writeDepth(PXCImage::ImageData cdata, PXCImage::ImageInfo cinfo, int k, int cnt){
	char filename[50];
	sprintf_s(filename, "depth/depth-%d-%d.bin", k, cnt);
	std::ofstream ofs(filename, std::ofstream::binary);
	ofs.write((const char *)cdata.planes[0], (cinfo.height * cinfo.width)*sizeof(UINT16));
	ofs.close();
}

int _tmain(int argc, char *argv[])
try
{
	CreateFolder(L"depth");
	CreateFolder(L"data1");
	CreateFolder(L"data2");
	PXCSession *pSession;
	PXCSession::ImplDesc *pDesc;
	PXCSenseManager *pSenseManager = PXCSenseManager::CreateInstance();
	PXCSenseManager *pSenseManager2 = PXCSenseManager::CreateInstance();	

	// Initialize session
	pSession = PXCSession::CreateInstance();
	pDesc = new PXCSession::ImplDesc();

	pDesc->group = PXCSession::ImplGroup::IMPL_GROUP_SENSOR;
	pDesc->subgroup = PXCSession::ImplSubgroup::IMPL_SUBGROUP_VIDEO_CAPTURE;

	// Enumerate Devices
	std::string temp;
	PXCCapture::DeviceInfo dvinfos[2];
	for (int m = 0;; m++)
	{
		PXCSession::ImplDesc desc2;
		if (pSession->QueryImpl(pDesc, m, &desc2) < pxcStatus::PXC_STATUS_NO_ERROR)
		{
			break;
		}
		wstring ws(desc2.friendlyName); string str(ws.begin(), ws.end());
		std::cout << "Module[" << m << "]:  " << str.c_str() << std::endl;

		PXCCapture *pCap;
		pSession->CreateImpl<PXCCapture>(&desc2, &pCap);

		// print out all device information
		for (int d = 0;; d++)
		{
			PXCCapture::DeviceInfo dinfo;
			if (pCap->QueryDeviceInfo(d, &dinfo) < pxcStatus::PXC_STATUS_NO_ERROR)
			{
				break;
			};
			wstring ws(dinfo.name); string str(ws.begin(), ws.end());
			std::cout << "Device[" << d << "]:  " << str.c_str() << dinfo.duid << std::endl; // << "Serial: " << dinfo.serial << "didx:" << dinfo.didx << std::endl;

			if (dinfo.duid != 0){
				std::cout << "Device " << d << " set " << endl;
				dvinfos[d] = dinfo;
			}
		}
	}

	// Select Camera 1
	PXCCaptureManager *captureMgr = pSenseManager->QueryCaptureManager();
	captureMgr->FilterByDeviceInfo(&dvinfos[0]);	
	pSenseManager->EnableStream(PXCCapture::STREAM_TYPE_DEPTH, 640, 480, 30);
	pSenseManager->EnableStream(PXCCapture::STREAM_TYPE_COLOR, 1920, 1080, 30);
	pSenseManager->Init();

	// Select Camera 2
	PXCCaptureManager *captureMgr2 = pSenseManager2->QueryCaptureManager();
	captureMgr2->FilterByDeviceInfo(&dvinfos[1]);
	pSenseManager2->EnableStream(PXCCapture::STREAM_TYPE_COLOR, 1920, 1080, 30);
	pSenseManager2->Init();

	namedWindow("Depth 1", CV_WINDOW_AUTOSIZE);
	namedWindow("Color 1", CV_WINDOW_AUTOSIZE);
	namedWindow("Color 2", CV_WINDOW_AUTOSIZE);

	int cnt = 0;
	bool isWrite = false;
	// Read frames
	while (1) {
		Sleep(10);
		//camera 1
		if (pSenseManager->AcquireFrame(false) >= PXC_STATUS_NO_ERROR)  {
			PXCCapture::Sample * sample = pSenseManager->QuerySample();
			if (sample->depth) {
				PXCImage *depth = sample->depth;
				PXCImage::ImageData cdata;
				PXCImage::ImageInfo cinfo = depth->QueryInfo();
				if (depth->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_RGB32, &cdata) >= PXC_STATUS_NO_ERROR){
					Mat Depth = Mat(cinfo.height, cinfo.width, CV_8UC4, (void*)cdata.planes[0]);
					imshow("Depth 1", Depth);
					int key = waitKey(10);
					// Pressing Esc
					if (key == 27){
						break;
					}
				}
				depth->ReleaseAccess(&cdata);
			}
			if (sample->color) {
				PXCImage *color = sample->color;
				PXCImage::ImageData cdata;
				PXCImage::ImageInfo cinfo = color->QueryInfo();
				if (color->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_RGB32, &cdata) >= PXC_STATUS_NO_ERROR){
					Mat Color = Mat(cinfo.height, cinfo.width, CV_8UC4, (void*)cdata.planes[0]);
					imshow("Color 1", Color);
					int key = waitKey(10);
					if (key == 27){
						break;
					}
					// Start Saving (s)
					if (key == 115){
						isWrite = true;
					}
					if (isWrite){
						char filename[50];
						sprintf_s(filename, "data1/frame-%d-%d.jpg", 1, cnt);
						imwrite(filename, Color);
					}
				}
				color->ReleaseAccess(&cdata);
			}
		}
		pSenseManager->ReleaseFrame();

		//camera 2
		if (pSenseManager2->AcquireFrame(false) >= PXC_STATUS_NO_ERROR) {
			PXCCapture::Sample * sample = pSenseManager2->QuerySample();
			if (sample->color) {
				PXCImage *color = sample->color;
				PXCImage::ImageData cdata;
				PXCImage::ImageInfo cinfo = color->QueryInfo();
				if (color->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_RGB32, &cdata) >= PXC_STATUS_NO_ERROR){
					Mat Color = Mat(cinfo.height, cinfo.width, CV_8UC4, (void*)cdata.planes[0]);
					imshow("Color 2", Color);
					int key = waitKey(10);
					if (key == 27){
						break;
					}
					if (isWrite){
						char filename[50];
						sprintf_s(filename, "data2/frame-%d-%d.jpg", 2, cnt);
						imwrite(filename, Color);
						isWrite = false;
						cnt++;
					}
				}
				color->ReleaseAccess(&cdata);
			}
		}
		pSenseManager2->ReleaseFrame();
	}

	pSenseManager->Release();
	pSenseManager2->Release();

	return 0;
}
catch (const char *c)
{
	std::cerr << "Program aborted: " << c << "\n";
	MessageBox(GetActiveWindow(), (LPCWSTR)c, L"FAIL", 0);
}
catch (std::exception e)
{
	std::cerr << "Program aborted: " << e.what() << "\n";
	MessageBox(GetActiveWindow(), (LPCWSTR)e.what(), L"FAIL", 0);
}
