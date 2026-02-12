#include "VzCoalDeviceAPI.h"
#include "VzCoalDeviceHeader.h"
#include "windows.h"
#include <direct.h>
#include <iostream>
#include <stdio.h>
#include <time.h>

static unsigned int g_nLoopTimes = 10;
static bool g_bSaveDepthMap = false;
static int g_nSaveDepthMapMaxFrames = 10;
static int g_nSavedDepthMapCount = 0;
// Force high resolution (use fYScale) to avoid downsampling at edges
static bool g_bForceHighRes = false;

/// @brief
/// 煤流数据回调函数
class CVzDeviceDataCallBack : public IVzDeviceDataCallBack {
public:
  CVzDeviceDataCallBack(IVzCoalDevice *pDevice) {
    m_nTimeIdx = 0;
    m_fTotleVolume = 0.f;
    m_pDevice = pDevice;
    m_nDepthMapIdx = 0;
  }
  virtual ~CVzDeviceDataCallBack() { ; }

public:
  /// @brief
  /// 输出图像
  /// <param name = "pLeftImage">[in]左图像</param>
  /// <param name = "pRightImage">[in]右图像</param>
  virtual void OnOutputImage(SVzNLImageData *pLeftImage,
                             SVzNLImageData *pRightImage);

  /// @brief
  /// RGB图像
  /// <param name = "pRGBImage">[in]RGB图像</param>
  virtual void OnOutputRGBImage(SVzNLImageData *pRGBImage);

  /// @brief
  /// 开始输出一个物体数据
  /// <param name = "nObjIdx">[in]物体的索引</param>
  virtual void OnBeginOutputObjResult(int nObjIdx, VzBool bClearScreen);

  /// @brief
  /// 结果输出
  /// <param name = "p3DPoint">[in]3D数据</param>
  /// <param name = "nCount">[in]数据个数</param>
  /// <param name = "nFrameIdx">[in]帧号</param>
  virtual void OnOutputObjResult(SVzNLPointXYZRGBA *p3DPoint, int nCount,
                                 unsigned long long nFrameIdx,
                                 unsigned long long llTimeStamp,
                                 unsigned short shBlockID);

  /// @brief
  /// 输出单个物体数据完成
  /// <param name = "nFrameIdx">[in]帧号</param>
  virtual void OnEndOutputObjResult(float fVolume,
                                    SVzNLPointXYZRGBA *pTopPoint);

  /// @brief
  /// 输出当前总体积
  virtual void OnOutputTotleResult(double dVolume);

  /// @brief 输出深度图像
  virtual void OnOutputDepthMap(const SVzNLImageData &sDepthMap);

  /// @brief 异步RGB输出RGB图像及其对应的深度图
  virtual void OnOutputRGBAnd3DPointMap(SVzNLImageData *pRGBImage,
                                        SVzNLImageData *pDepthImage,
                                        SVzNLPointXYZRGBA *p3DToPointMap,
                                        unsigned long long llTimeStamp);

protected:
  unsigned int m_nTimeIdx;
  float m_fTotleVolume;
  IVzCoalDevice *m_pDevice;
  int m_nDepthMapIdx;
};

/// @brief
/// 输出图像
/// <param name = "pLeftImage">[in]左图像</param>
/// <param name = "pRightImage">[in]右图像</param>
void CVzDeviceDataCallBack::OnOutputImage(SVzNLImageData *pLeftImage,
                                          SVzNLImageData *pRightImage) {
  printf("Left Image %p Right Image %p\r\n", pLeftImage, pRightImage);
}

/// @brief
/// RGB图像
/// <param name = "pRGBImage">[in]RGB图像</param>
void CVzDeviceDataCallBack::OnOutputRGBImage(SVzNLImageData *pRGBImage) {
  printf("RGB Image %p \r\n", pRGBImage);
}

/// @brief
/// 开始输出数据
/// <param name = "nObjIdx">[in]数据块号</param>
/// <param name = "bClearScreen">[in]清除屏幕</param>
void CVzDeviceDataCallBack::OnBeginOutputObjResult(int nObjIdx,
                                                   VzBool bClearScreen) {
  // printf("Begin Output Obj %d\r\n", nObjIdx);
}

/// @brief
/// 结果输出
/// <param name = "p3DPoint">[in]3D数据</param>
/// <param name = "nCount">[in]数据个数</param>
/// <param name = "nFrameIdx">[in]帧号</param>
void CVzDeviceDataCallBack::OnOutputObjResult(SVzNLPointXYZRGBA *p3DPoint,
                                              int nCount,
                                              unsigned long long nFrameIdx,
                                              unsigned long long llTimeStamp,
                                              unsigned short shBlockID) {
  // printf("Output Obj PointCount %d FrameIndex %lld\r\n", nCount, nFrameIdx);
}

/// @brief
/// 开始输出数据
/// <param name = "fVolume">[in]体积</param>
/// <param name = "pTopPoint">[in]最高的点</param>
void CVzDeviceDataCallBack::OnEndOutputObjResult(float fVolume,
                                                 SVzNLPointXYZRGBA *pTopPoint) {
  m_nTimeIdx++;
  m_fTotleVolume += fVolume;

  if (m_nTimeIdx >= g_nLoopTimes) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    printf("%04d-%02d-%02d %02d:%02d:%02d Volume: %.1f\r\n", st.wYear,
           st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
           m_fTotleVolume);

    m_nTimeIdx = 0;
    m_fTotleVolume = 0;
  }
}

/// @brief
/// 输出当前总体积
void CVzDeviceDataCallBack::OnOutputTotleResult(double dVolume) {}

/// @brief 输出深度图像
void CVzDeviceDataCallBack::OnOutputDepthMap(const SVzNLImageData &sDepthMap) {
  /*
  printf(
      "DepthMap Info: Size %d, Width %d, Height %d, BitDepth %d, Channels %d\n",
      sDepthMap.nBufferSize, sDepthMap.nWidth, sDepthMap.nHeight,
      sDepthMap.byBitDepth, sDepthMap.nChannels);

  // Check first few pixels
  if (sDepthMap.pBuffer && sDepthMap.nBufferSize > 0) {
    printf("Debug: First 20 pixels: ");
    for (int i = 0; i < 20 && i < sDepthMap.nBufferSize; i++) {
      printf("%d ", sDepthMap.pBuffer[i]);
    }
    printf("\n");
  }
  */

  if (m_pDevice && g_bSaveDepthMap &&
      g_nSavedDepthMapCount < g_nSaveDepthMapMaxFrames) {
    // Create directory if not exists
    _mkdir("DepthMaps");

    // Generate timestamp filename
    SYSTEMTIME st;
    GetLocalTime(&st);
    char szFileName[128] = {0};
    sprintf_s(szFileName,
              "DepthMaps/DepthMap_%04d%02d%02d_%02d%02d%02d_%03d.png", st.wYear,
              st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
              st.wMilliseconds);

    m_pDevice->SaveImage(szFileName, &sDepthMap);
    printf("Saved %s (%d/%d)\n", szFileName, ++g_nSavedDepthMapCount,
           g_nSaveDepthMapMaxFrames);

    if (g_nSavedDepthMapCount >= g_nSaveDepthMapMaxFrames) {
      printf("Finished saving %d depth maps.\n", g_nSavedDepthMapCount);
      g_bSaveDepthMap = false;
    }
  }
}

void CVzDeviceDataCallBack::OnOutputRGBAnd3DPointMap(
    SVzNLImageData *pRGBImage, SVzNLImageData *pDepthImage,
    SVzNLPointXYZRGBA *p3DToPointMap, unsigned long long llTimeStamp) {
  printf("RGB and DepthMap  %llu\n", llTimeStamp);
}

/// @brief
/// 获取设备信息
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuQueryDeviceInfo(IVzCoalDevice *pICoalDevice) {
  SVzNLEyeDeviceInfoEx *psDeviceInfoEx = pICoalDevice->QueryDeviceInfoEx();

  SVzVideoResolution sVideoRes;
  pICoalDevice->GetVideoRes(sVideoRes);

  char *pszDevIP = pICoalDevice->GetIP();

  char *pszVersion = pICoalDevice->GetVersion();
  printf("Version: %s\r\n", pszVersion);
  printf("IP: %s\r\n", pszDevIP);
  printf("VideoRes: %d %d\r\n", sVideoRes.nFrameWidth, sVideoRes.nFrameHeight);
}

/// @brief
/// 设置曝光
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuExpose(IVzCoalDevice *pICoalDevice) {
  unsigned int nExposeVal = pICoalDevice->GetExpose();
  printf("current Expose: %d\r\n", nExposeVal);
  printf("please input Expose:\r\n");
  scanf_s("%d", &nExposeVal);
  int nErrorCode = pICoalDevice->SetExpose(nExposeVal);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 设置帧率
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuFrameRate(IVzCoalDevice *pICoalDevice) {
  unsigned int nFrameRate = pICoalDevice->GetFrameRate();
  printf("current FrameRate: %d\r\n", nFrameRate);
  printf("please input FrameRate:\r\n");
  scanf_s("%d", &nFrameRate);

  unsigned int nMinFrame = 0;
  unsigned int nMaxFrame = 0;
  pICoalDevice->QueryFrameRange(nMinFrame, nMaxFrame);
  if (nFrameRate < nMinFrame || nFrameRate > nMaxFrame) {
    printf("Out of Range(FrameRate%d~%d)\r\n", nMinFrame, nMaxFrame);
    return;
  }

  int nErrorCode = pICoalDevice->SetFrameRate(nFrameRate);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 设置检测ROI
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuDetectROI(IVzCoalDevice *pICoalDevice) {
  SVzNLROIRect sLeftROI, sRightROI;
  pICoalDevice->GetDetectROI(sLeftROI, sRightROI);
  scanf_s("Left ROI %d %d %d %d", &sLeftROI.left, &sLeftROI.right,
          &sLeftROI.top, &sLeftROI.bottom);
  scanf_s("Right ROI %d %d %d %d", &sRightROI.left, &sRightROI.right,
          &sRightROI.top, &sRightROI.bottom);

  printf("please input LeftROI: L R T B\r\n");
  scanf_s("%d %d %d %d", &sLeftROI.left, &sLeftROI.right, &sLeftROI.top,
          &sLeftROI.bottom);
  printf("please input RightROI: L R T B\r\n");
  scanf_s("%d %d %d %d", &sRightROI.left, &sRightROI.right, &sRightROI.top,
          &sRightROI.bottom);
  int nErrorCode = pICoalDevice->SetDetectROI(sLeftROI, sRightROI);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 设置检测周期
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuLoopTime(IVzCoalDevice *pICoalDevice) {
  unsigned int nLoopTime = pICoalDevice->GetCalcLoopTime();
  printf("current LoopTime: %d\r\n", nLoopTime);
  printf("please input LoopTime:\r\n");
  scanf_s("%d", &nLoopTime);
  int nErrorCode = pICoalDevice->SetCalcLoopTime(nLoopTime);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 设置速度
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuSpeed(IVzCoalDevice *pICoalDevice) {
  double dSpeed = pICoalDevice->GetSpeed();
  printf("current Speed: %.1lf\r\n", dSpeed);
  printf("please input Speed:\r\n");
  scanf_s("%lf", &dSpeed);
  int nErrorCode = pICoalDevice->SetSpeed(dSpeed);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 设置编码器两个脉冲之间的距离
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuDistancePerPls(IVzCoalDevice *pICoalDevice) {
  float fDistance = pICoalDevice->GetDistancePerPls();
  printf("current DistancePerPls: %.1lf\r\n", fDistance);
  printf("please input DistancePerPls:\r\n");
  scanf_s("%f", &fDistance);
  int nErrorCode = pICoalDevice->SetDistancePerPls(fDistance);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 设置工作模式
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuWorkMode(IVzCoalDevice *pICoalDevice) {
  EVzCoalWorkMode eWorkMode = pICoalDevice->GetWorkMode();
  printf("current WorkMode: %d\r\n", eWorkMode);
  printf("please input WorkMode: 0:Encoder 1:FixedSpeed\r\n");
  scanf_s("%d", &eWorkMode);
  int nErrorCode = pICoalDevice->ChangeWorkMode(eWorkMode);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 设置增益
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuGain(IVzCoalDevice *pICoalDevice) {
  unsigned short shLGain, shRGain;
  pICoalDevice->GetGain(shLGain, shRGain);
  printf("current Gain: %d %d\r\n", shLGain, shRGain);

  unsigned int nLGain, nRGain;
  printf("please input gain: LeftGain RightGain");
  scanf_s("%d %d", &nLGain, &nRGain);
  shLGain = nLGain;
  shRGain = nRGain;
  int nErrorCode = pICoalDevice->SetGain(shLGain, shRGain);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 标定传送带
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuCalib(IVzCoalDevice *pICoalDevice) {
  printf("Calib");
  int nErrorCode = pICoalDevice->RecordEmptyArea();
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 重置工作距离
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuResetWorkRange(IVzCoalDevice *pICoalDevice) {
  float fYStart = 0.f;
  float fYEnd = 0.f;
  int nErrorCode = pICoalDevice->QueryWorkRange(fYStart, fYEnd);
  printf("YStart YEnd: %.1f %.1f\r\n", fYStart, fYEnd);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
  nErrorCode = pICoalDevice->ResetWorkRange(fYStart, fYEnd);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 设置激光亮度
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuLaserLightLevel(IVzCoalDevice *pICoalDevice) {
  int nMin = 0;
  int nMax = 0;
  int nErrorCode = pICoalDevice->GetLaserLightRange(nMin, nMax);
  printf("LaserLightRange nMin: %d, nMax: %d \r\n", nMin, nMax);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }

  int nLevel = 0;
  printf("please input level:");
  scanf_s("%d", &nLevel);
  if (nLevel > nMax)
    nLevel = nMax;
  if (nLevel < nMin)
    nLevel = nMin;
  nErrorCode = pICoalDevice->SetLaserLightLevel(nLevel);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 获取总体积
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuQueryTotleVol(IVzCoalDevice *pICoalDevice) {
  printf("Reset Totle Volume");
  double dTotleVolume = 0.;

  IVzCoalDataStore *pIDataStore = nullptr;
  pICoalDevice->QueryDataStore(&pIDataStore);
  int nErrorCode = pIDataStore->QueryTotleVolume(dTotleVolume);
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  } else {
    printf("TotleVolume %.1f:\r\n", dTotleVolume);
  }
}

/// @brief
/// 重置总体积
/// <param name="pICoalDevice">煤流设备</param>
static void _OnMenuResetTotleVol(IVzCoalDevice *pICoalDevice) {
  printf("Reset Totle Volume");
  IVzCoalDataStore *pIDataStore = nullptr;
  pICoalDevice->QueryDataStore(&pIDataStore);
  int nErrorCode = pIDataStore->ResetTotleVolume();
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 开始检测/停止检测
/// <param name="pICoalDevice">煤流设备</param>
/// <param name="pCallBack">数据回调类</param>
static void _OnMenuDetect(IVzCoalDevice *pICoalDevice,
                          CVzDeviceDataCallBack *pCallBack) {
  //
  pICoalDevice->EnableDynamicDepthPic(VzTrue);

  int nPointCount = 0;
  SVzNLPointXYZRGBA *pCur3DPoint = pICoalDevice->GetBaseLaserLine(nPointCount);
  if (nPointCount == 0) {
    printf("请先选择传送带标定\n");
    return;
  }

  SVzNLRangeD sRangeY = {99999999., -99999999.};
  SVzNLRangeD sRangeZ = {99999999., -99999999.};
  for (unsigned int nPtIdx = 0; nPtIdx < nPointCount; nPtIdx++) {
    if (sRangeY.min > pCur3DPoint->y) {
      sRangeY.min = pCur3DPoint->y;
    }
    if (sRangeY.max < pCur3DPoint->y) {
      sRangeY.max = pCur3DPoint->y;
    }
    if (sRangeZ.min > pCur3DPoint->z) {
      sRangeZ.min = pCur3DPoint->z;
    }
    if (sRangeZ.max < pCur3DPoint->z) {
      sRangeZ.max = pCur3DPoint->z;
    }
    pCur3DPoint++;
  }
  // printf("Debug: Z Range: Min %.2f, Max %.2f\n", sRangeZ.min, sRangeZ.max);

  float fYScale = (sRangeY.max - sRangeY.min) / nPointCount;
  float fXScale = 0.f;
  if (keCoalWorkMode_Encoder == pICoalDevice->GetWorkMode()) {
    fXScale = pICoalDevice->GetDistancePerPls();
  } else if (keCoalWorkMode_FixedSpeed ==
             pICoalDevice->GetWorkMode()) //< 固定速度模式
  {
    unsigned int nMinFrame = 0;
    unsigned int nMaxFrame = 0;
    pICoalDevice->QueryFrameRange(nMinFrame, nMaxFrame);
    int nFrameRate = min(nMaxFrame, pICoalDevice->GetFrameRate());
    fXScale = pICoalDevice->GetSpeed() / nFrameRate;
  } else if (keCoalWorkMode_MasterEncode == pICoalDevice->GetWorkMode()) {
    fXScale = pICoalDevice->GetDistancePerPls();
  }
  float fScale = fYScale;
  if (!g_bForceHighRes) {
    // Default behavior: Use larger scale (lower resolution) to keep pixels
    // square
    if (fScale < fXScale) {
      fScale = fXScale;
    }
  } else {
    // High Res behavior: Use smaller scale (higher resolution) to capture
    // details This may result in non-square pixels if fXScale != fYScale
    if (fScale > fYScale) {
      fScale = fYScale;
    }
    printf("Debug: Forcing High Resolution Mode (Scale = %.4f)\n", fScale);
  }

  printf("Debug: Coverage Info -- Y Range: %.2f to %.2f (Size: %.2f)\n",
         sRangeY.min, sRangeY.max, sRangeY.max - sRangeY.min);
  printf("Debug: Scale Info -- YScale: %.4f, XScale: %.4f, Final Resolution: "
         "%.4f mm/pixel\n",
         fYScale, fXScale, fScale);
  printf("Debug: Estimated Image Height (Pixels) = YRange / Resolution = %.2f "
         "/ %.4f = %.0f\n",
         sRangeY.max - sRangeY.min, fScale,
         (sRangeY.max - sRangeY.min) / fScale);

  /*
  printf("Debug: Scale Info: YScale %.4f, XScale %.4f, Final Scale %.4f\n",
         fYScale, fXScale, fScale);
  */

  SVzDepthMapImgCtlPara sDepthMapParam;
  sDepthMapParam.eDepthNormalizingMode =
      keDepthNormalizingMode_Indicated; // Specify fixed range for normalization
  sDepthMapParam.nImageMargin =
      0; // 输出深度图边缘预留空白区域宽度（单位：像素值）
  sDepthMapParam.eBitDepth = keImageBitDepthType_8bit;
  sDepthMapParam.fDepthResolution =
      (sRangeZ.max - sRangeZ.min) /
      255; // 深度图中深度值精度，例如：0.1 表示1个像素值对应0.1mm 实际距离
  sDepthMapParam.fWHPixelResolution =
      fScale; // 深度图中每个像素的精度，例如：0.1
              // 表示1个像素点对应0.1mm实际距离，eDepthNormalizingMode =
              // eDepthNormalizingMode_Indicated 时有效

  // Use tighter range for better contrast.
  // Z is typically distance. Lower Z = Higher object (Coal). Higher Z =
  // Background (Belt). Map [Min-5, Max+5] to [0, 255].
  // Reverted to tighter range (+/- 5.0mm) as requested.
  sDepthMapParam.fNormalizingStartVal = sRangeZ.min - 5.0f;
  sDepthMapParam.fNormalizingEndVal = sRangeZ.max + 5.0f;

  /*
  printf("Debug: Applied Normalization Range: %.2f - %.2f\n",
         sDepthMapParam.fNormalizingStartVal,
         sDepthMapParam.fNormalizingEndVal);
  */

  sDepthMapParam.fNormalizingMargin =
      0.f; // eDepthNormalizingMode = keDepthNormalizingMode_Auto
           // 时，在自动检测到的归一化区间上下预留的空白空间高度（单位mm）上下各预留指定高度空间

  /*
  printf("Debug: Setting DepthMapParam. Mode: %d, DepthResolution: %.4f\n",
         sDepthMapParam.eDepthNormalizingMode, sDepthMapParam.fDepthResolution);
  */

  pICoalDevice->SetDynamicDepthMapParam(sDepthMapParam);

  //
  int nErrorCode = 0;
  if (pICoalDevice->IsDetecting()) {
    nErrorCode = pICoalDevice->StopDetect();
  } else {
    nErrorCode = pICoalDevice->StartDetect(pCallBack);
  }
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

/// @brief
/// 开始采集图像/停止采集图像
/// <param name="pICoalDevice">煤流设备</param>
/// <param name="pCallBack">回调类</param>
static void _OnMenuCapture(IVzCoalDevice *pICoalDevice,
                           CVzDeviceDataCallBack *pCallBack) {
  int nErrorCode = 0;
  if (pICoalDevice->IsCapturing()) {
    nErrorCode = pICoalDevice->StopCapture();
  } else {
    nErrorCode = pICoalDevice->StartCapture(pCallBack);
  }
  if (0 != nErrorCode) {
    printf("ErrorCode %d:\r\n", nErrorCode);
  }
}

static void _OnMenuSetRecordTime() {
  printf("Set Record Loop Time\r\n");
  scanf_s("%d", &g_nLoopTimes);
}

static void _OnMenuEnableSaveDepthMap() {
  int nEnable = 0;
  printf("Enable Save Depth Map (0: Disable, 1: Enable): ");
  scanf_s("%d", &nEnable);
  g_bSaveDepthMap = (nEnable != 0);
  if (g_bSaveDepthMap) {
    g_nSavedDepthMapCount = 0; // Reset counter when enabling
    printf("Depth Map Saving Enabled. Max Frames: %d\n",
           g_nSaveDepthMapMaxFrames);
  } else {
    printf("Depth Map Saving Disabled.\n");
  }
}

static void _OnMenuSetSaveDepthMapFrames() {
  printf("Set Max Depth Map Frames to Save: ");
  scanf_s("%d", &g_nSaveDepthMapMaxFrames);
  printf("Max Frames set to %d.\n", g_nSaveDepthMapMaxFrames);
}

/// <summary>
/// 功能定义列表
/// </summary>
enum {
  keMenuID_QueryDevInfo = 1,     //< 获取设备信息
  keMenuID_Expose,               //< 设置曝光
  keMenuID_FrameRate,            //< 设置增益
  keMenuID_ROI,                  //< 设置ROI
  keMenuID_LoopTime,             //< 设置检测周期
  keMenuID_Speed,                //< 设置速度（匀速模式下使用）
  keMenuID_DistancePls,          //< 设置编码器间距(变速模式下使用)
  keMenuID_WorkMode,             //< 工作模式：匀速模式，变速模式(编码器模式)
  keMenuID_Gain,                 //< 设置增益
  keMenuID_Calib,                //< 标定传送带
  keMenuID_ResetWorkRange,       //< 重置工作范围
  keMenuID_QueryTotleVol,        //< 获取总煤量
  keMenuID_ResetTotleVol,        //< 重置煤流量
  keMenuID_LaserLightLevel,      //< 调节激光亮度
  keMenuID_StartStopDetect,      //< 开始/停止检测
  keMenuID_StartStopCapture,     //< 开始/停止采集
  keMenuID_SetLoopTime,          //< 设置记录时间
  keMenuID_EnableSaveDepthMap,   //< 启用/禁用保存深度图
  keMenuID_SetSaveDepthMapFrames //< 设置保存深度图帧数
};

// Process Device
static void _OnProcessDevice(IVzCoalDevice *pICoalDevice) {
  if (nullptr == pICoalDevice) {
    printf("Error Handle\r\n");
    return;
  }

  CVzDeviceDataCallBack *pCallBack = new CVzDeviceDataCallBack(pICoalDevice);
  for (;;) {
    printf("%d. Query Device Info\r\n", keMenuID_QueryDevInfo);
    printf("%d. Get/Set Expose\r\n", keMenuID_Expose);
    printf("%d. Get/Set FrameRate\r\n", keMenuID_FrameRate);
    printf("%d. Get/Set Detect ROI\r\n", keMenuID_ROI);
    printf("%d. Get/Set Loop Time\r\n", keMenuID_LoopTime);
    printf("%d. Get/Set Speed\r\n", keMenuID_Speed);
    printf("%d. Get/Set DistancePerPls\r\n", keMenuID_DistancePls);
    printf("%d. Get/Set WorkMode\r\n", keMenuID_WorkMode);
    printf("%d. Get/Set Gain\r\n", keMenuID_Gain);
    printf("%d. Calib\r\n", keMenuID_Calib);
    printf("%d. Reset Work Range\r\n", keMenuID_ResetWorkRange);
    printf("%d. Query Totle Volume\r\n", keMenuID_QueryTotleVol);
    printf("%d. Reset Totle Volume\r\n", keMenuID_ResetTotleVol);
    printf("%d. Set Laser Light Level\r\n", keMenuID_LaserLightLevel);
    printf("%d. Start/Stop Detect\r\n", keMenuID_StartStopDetect);
    printf("%d. Start/Stop Capture\r\n", keMenuID_StartStopCapture);
    printf("%d. Set Record Time\r\n", keMenuID_SetLoopTime);
    printf("%d. Enable/Disable Save Depth Map\r\n",
           keMenuID_EnableSaveDepthMap);
    printf("%d. Set Save Depth Map Frames\r\n", keMenuID_SetSaveDepthMapFrames);
    printf("0. Exit\r\n");

    unsigned int nMenuID = 0;
    printf("Please Select: ");
    scanf_s("%d", &nMenuID);
    if (0 == nMenuID) {
      break;
    }
    switch (nMenuID) {
    case keMenuID_QueryDevInfo: // 获取设备信息
    {
      _OnMenuQueryDeviceInfo(pICoalDevice);
      break;
    }
    case keMenuID_Expose: // 设置曝光
    {
      _OnMenuExpose(pICoalDevice);
      break;
    }
    case keMenuID_FrameRate: // 设置帧率
    {
      _OnMenuFrameRate(pICoalDevice);
      break;
    }
    case keMenuID_ROI: // 设置ROI
    {
      _OnMenuDetectROI(pICoalDevice);
      break;
    }
    case keMenuID_LoopTime: // 设置计算周期
    {
      _OnMenuLoopTime(pICoalDevice);
      break;
    }
    case keMenuID_Speed: // 设置速度(匀速模式)
    {
      _OnMenuSpeed(pICoalDevice);
      break;
    }
    case keMenuID_DistancePls: // 设置编码器间距(变速模式)
    {
      _OnMenuDistancePerPls(pICoalDevice);
      break;
    }
    case keMenuID_WorkMode: // 设置工作模式
    {
      _OnMenuWorkMode(pICoalDevice);
      break;
    }
    case keMenuID_Gain: // 设置增益
    {
      _OnMenuGain(pICoalDevice);
      break;
    }
    case keMenuID_LaserLightLevel: // 设置激光亮度
    {
      _OnMenuLaserLightLevel(pICoalDevice);
      break;
    }
    case keMenuID_StartStopDetect: // 开始/停止检测
    {
      _OnMenuDetect(pICoalDevice, pCallBack);
      break;
    }
    case keMenuID_StartStopCapture: // 开始/停止采集
    {
      _OnMenuCapture(pICoalDevice, pCallBack);
      break;
    }
    case keMenuID_SetLoopTime: {
      _OnMenuSetRecordTime();
      break;
    }
    case keMenuID_EnableSaveDepthMap: {
      _OnMenuEnableSaveDepthMap();
      break;
    }
    case keMenuID_SetSaveDepthMapFrames: {
      _OnMenuSetSaveDepthMapFrames();
      break;
    }
    case keMenuID_Calib: // 标定传送带
    {
      _OnMenuCalib(pICoalDevice);
      break;
    }
    case keMenuID_ResetWorkRange: // 重置工作范围(带参数标定传送带)
    {
      _OnMenuResetWorkRange(pICoalDevice);
      break;
    }
    case keMenuID_QueryTotleVol: // 获取总煤量
    {
      _OnMenuQueryTotleVol(pICoalDevice);
      break;
    }
    case keMenuID_ResetTotleVol: // 重置总煤量
    {
      _OnMenuResetTotleVol(pICoalDevice);
      break;
    }
    default: {
      break;
    }
    }
  }
  if (nullptr != pCallBack) {
    delete pCallBack;
    pCallBack = nullptr;
  }
}

int main() {
  printf("Starting VzCoalSample...\n");
  // Init SDK
  VzCoalSDKInit();
  printf("SDK Initialized.\n");

  // Find Device
  bool bFindDeviceSuccess = false;
  SVzNLEyeCBInfo sEyeCBInfo;
  IVzDeviceFinder *pIDeviceFinder = nullptr;
  VzCreateCoalDeviceFinder(&pIDeviceFinder);
  if (pIDeviceFinder) {
    unsigned int nFindDevCnt = pIDeviceFinder->FindDevice();
    printf("Found %d devices.\n", nFindDevCnt);
    for (int nDevIdx = 0; nDevIdx < nFindDevCnt; nDevIdx++) {
      pIDeviceFinder->QueryDeviceInfo(nDevIdx, sEyeCBInfo);
      bFindDeviceSuccess = true;
      break;
    }
  } else {
    printf("Failed to create DeviceFinder.\n");
  }

  // Create Device
  if (bFindDeviceSuccess) {
    printf("Device found, creating device object...\n");
    // 创建设备
    IVzCoalDevice *pICoalDevice = nullptr;
    VzCreateCoalDevice(&sEyeCBInfo, &pICoalDevice);

    // 处理设备
    _OnProcessDevice(pICoalDevice);

    if (nullptr != pICoalDevice) {
      delete pICoalDevice;
      pICoalDevice = nullptr;
    }
  } else {
    printf("No device found or finder failed.\n");
  }

  // Destroy SDK
  VzCoalSDKDestroy();
  printf("SDK Destroyed. Exiting.\n");
}
