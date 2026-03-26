#include "VzCoalDeviceAPI.h"
#include "VzCoalDeviceHeader.h"
#include "windows.h"
#include <conio.h>
#include <direct.h>
#include <fstream>
#include <stdio.h>
#include <vector>

// Auto-run config (no menu input required)
static float g_fPlySegmentLengthMM = 1000.0f;
static float g_fLineRateHz = 30.0f;
static float g_fBeltSpeedMMPerSec = 300.0f;
static int g_nMaxPlySegmentFiles = 10; // 0 means unlimited

static bool g_bEnableSavePly = true;
static int g_nSavedPlySegmentCount = 0;

static float GetMmPerFrame() {
  if (g_fLineRateHz <= 0.0f) {
    return 0.0f;
  }
  return g_fBeltSpeedMMPerSec / g_fLineRateHz;
}

static int GetLinesPerSegment() {
  if (g_fPlySegmentLengthMM <= 0.0f) {
    return 0;
  }
  float mmPerFrame = GetMmPerFrame();
  if (mmPerFrame <= 0.0f) {
    return 0;
  }
  int lines = (int)(g_fPlySegmentLengthMM / mmPerFrame + 0.5f);
  return lines > 0 ? lines : 1;
}

class CPlySegmentCallback : public IVzDeviceDataCallBack {
public:
  CPlySegmentCallback() {
    m_hasLastFrame = false;
    m_lastFrameIdx = 0;
    m_accumulatedLines = 0;
    m_prevSaveEnabled = false;
  }

  virtual ~CPlySegmentCallback() { ; }

  virtual void OnOutputImage(SVzNLImageData *pLeftImage,
                             SVzNLImageData *pRightImage) {}

  virtual void OnOutputRGBImage(SVzNLImageData *pRGBImage) {}

  virtual void OnBeginOutputObjResult(int nObjIdx, VzBool bClearScreen) {}

  virtual void OnOutputObjResult(SVzNLPointXYZRGBA *p3DPoint, int nCount,
                                 unsigned long long nFrameIdx,
                                 unsigned long long llTimeStamp,
                                 unsigned short shBlockID) {
    if (!g_bEnableSavePly) {
      if (m_prevSaveEnabled) {
        m_points.clear();
        m_accumulatedLines = 0;
        m_hasLastFrame = false;
      }
      m_prevSaveEnabled = false;
      return;
    }

    if (!m_prevSaveEnabled) {
      m_points.clear();
      m_accumulatedLines = 0;
      m_hasLastFrame = false;
    }
    m_prevSaveEnabled = true;

    if (!m_hasLastFrame) {
      m_lastFrameIdx = nFrameIdx;
      m_hasLastFrame = true;
      m_accumulatedLines += 1;
    } else if (nFrameIdx > m_lastFrameIdx) {
      m_accumulatedLines += (int)(nFrameIdx - m_lastFrameIdx);
      m_lastFrameIdx = nFrameIdx;
    }

    if (p3DPoint != nullptr && nCount > 0) {
      m_points.insert(m_points.end(), p3DPoint, p3DPoint + nCount);
    }

    int linesPerSegment = GetLinesPerSegment();
    if (linesPerSegment <= 0) {
      return;
    }

    if (m_accumulatedLines >= linesPerSegment) {
      if (SavePlySegment()) {
        m_accumulatedLines -= linesPerSegment;
        if (m_accumulatedLines < 0) {
          m_accumulatedLines = 0;
        }
        m_points.clear();
      }
    }
  }

  virtual void OnEndOutputObjResult(float fVolume, SVzNLPointXYZRGBA *pTopPoint) {}

  virtual void OnOutputTotleResult(double dVolume) {}

  virtual void OnOutputDepthMap(const SVzNLImageData &sDepthMap) {}

  virtual void OnOutputRGBAnd3DPointMap(SVzNLImageData *pRGBImage,
                                        SVzNLImageData *pDepthImage,
                                        SVzNLPointXYZRGBA *p3DToPointMap,
                                        unsigned long long llTimeStamp) {}

private:
  bool SavePlySegment() {
    if (m_points.empty()) {
      return false;
    }

    if (g_nMaxPlySegmentFiles > 0 &&
        g_nSavedPlySegmentCount >= g_nMaxPlySegmentFiles) {
      g_bEnableSavePly = false;
      return false;
    }

    _mkdir("PlySegments");

    SYSTEMTIME st;
    GetLocalTime(&st);
    char fileName[256] = {0};
    sprintf_s(fileName,
              "PlySegments/Segment_%04d%02d%02d_%02d%02d%02d_%03d_%04d.ply",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
              st.wMilliseconds, g_nSavedPlySegmentCount + 1);

    std::ofstream ofs(fileName, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
      printf("Failed to open PLY file: %s\n", fileName);
      return false;
    }

    ofs << "ply\n";
    ofs << "format ascii 1.0\n";
    ofs << "element vertex " << m_points.size() << "\n";
    ofs << "property float x\n";
    ofs << "property float y\n";
    ofs << "property float z\n";
    ofs << "end_header\n";

    for (size_t i = 0; i < m_points.size(); ++i) {
      const SVzNLPointXYZRGBA &pt = m_points[i];
      ofs << pt.x << " " << pt.y << " " << pt.z << "\n";
    }
    ofs.close();

    ++g_nSavedPlySegmentCount;
    printf("Saved PLY segment: %s (segment=%d, targetLength=%.2f mm, lines=%d)\n",
           fileName, g_nSavedPlySegmentCount, g_fPlySegmentLengthMM,
           GetLinesPerSegment());

    if (g_nMaxPlySegmentFiles > 0 &&
        g_nSavedPlySegmentCount >= g_nMaxPlySegmentFiles) {
      g_bEnableSavePly = false;
      printf("Reached max PLY segment files: %d, auto stopping.\n",
             g_nMaxPlySegmentFiles);
    }

    return true;
  }

private:
  std::vector<SVzNLPointXYZRGBA> m_points;
  bool m_hasLastFrame;
  unsigned long long m_lastFrameIdx;
  int m_accumulatedLines;
  bool m_prevSaveEnabled;
};

static bool ConfigureDetectDepthMap(IVzCoalDevice *pDevice) {
  int nPointCount = 0;
  SVzNLPointXYZRGBA *pBaseLine = pDevice->GetBaseLaserLine(nPointCount);
  if (nPointCount <= 0 || pBaseLine == nullptr) {
    printf("No base laser line. Please calibrate first.\n");
    return false;
  }

  SVzNLRangeD rangeY = {99999999., -99999999.};
  SVzNLRangeD rangeZ = {99999999., -99999999.};
  SVzNLPointXYZRGBA *pCur = pBaseLine;
  for (int i = 0; i < nPointCount; ++i) {
    if (rangeY.min > pCur->y) {
      rangeY.min = pCur->y;
    }
    if (rangeY.max < pCur->y) {
      rangeY.max = pCur->y;
    }
    if (rangeZ.min > pCur->z) {
      rangeZ.min = pCur->z;
    }
    if (rangeZ.max < pCur->z) {
      rangeZ.max = pCur->z;
    }
    ++pCur;
  }

  float fYScale = (rangeY.max - rangeY.min) / nPointCount;
  float fXScale = 0.0f;
  if (keCoalWorkMode_Encoder == pDevice->GetWorkMode()) {
    fXScale = pDevice->GetDistancePerPls();
  } else if (keCoalWorkMode_FixedSpeed == pDevice->GetWorkMode()) {
    unsigned int nMinFrame = 0;
    unsigned int nMaxFrame = 0;
    pDevice->QueryFrameRange(nMinFrame, nMaxFrame);
    int nFrameRate = min(nMaxFrame, pDevice->GetFrameRate());
    if (nFrameRate <= 0) {
      nFrameRate = 1;
    }
    fXScale = pDevice->GetSpeed() / nFrameRate;
  } else if (keCoalWorkMode_MasterEncode == pDevice->GetWorkMode()) {
    fXScale = pDevice->GetDistancePerPls();
  }

  float fScale = fYScale;
  if (fScale < fXScale) {
    fScale = fXScale;
  }

  SVzDepthMapImgCtlPara sDepthMapParam;
  sDepthMapParam.eDepthNormalizingMode = keDepthNormalizingMode_Indicated;
  sDepthMapParam.nImageMargin = 0;
  sDepthMapParam.eBitDepth = keImageBitDepthType_8bit;
  sDepthMapParam.fDepthResolution = (rangeZ.max - rangeZ.min) / 255;
  sDepthMapParam.fWHPixelResolution = fScale;
  sDepthMapParam.fNormalizingStartVal = rangeZ.min - 5.0f;
  sDepthMapParam.fNormalizingEndVal = rangeZ.max + 5.0f;
  sDepthMapParam.fNormalizingMargin = 0.0f;

  pDevice->SetDynamicDepthMapParam(sDepthMapParam);
  printf("Detect depth map configured: scale=%.4f mm/pixel\n", fScale);
  return true;
}

int main() {
  printf("Starting VzCoalSegmentSaver...\n");
  printf("Config: Segment=%.2f mm, LineRate=%.2f Hz, BeltSpeed=%.2f mm/s, Lines/Segment=%d, MaxFiles=%d\n",
         g_fPlySegmentLengthMM, g_fLineRateHz, g_fBeltSpeedMMPerSec,
         GetLinesPerSegment(), g_nMaxPlySegmentFiles);

  VzCoalSDKInit();

  bool bFindDeviceSuccess = false;
  SVzNLEyeCBInfo sEyeCBInfo;
  IVzDeviceFinder *pFinder = nullptr;
  VzCreateCoalDeviceFinder(&pFinder);
  if (pFinder) {
    unsigned int nFindDevCnt = pFinder->FindDevice();
    printf("Found %d devices.\n", nFindDevCnt);
    for (unsigned int i = 0; i < nFindDevCnt; ++i) {
      pFinder->QueryDeviceInfo(i, sEyeCBInfo);
      bFindDeviceSuccess = true;
      break;
    }
  }

  if (!bFindDeviceSuccess) {
    printf("No device found.\n");
    VzCoalSDKDestroy();
    return -1;
  }

  IVzCoalDevice *pDevice = nullptr;
  VzCreateCoalDevice(&sEyeCBInfo, &pDevice);
  if (pDevice == nullptr) {
    printf("Failed to create device.\n");
    VzCoalSDKDestroy();
    return -2;
  }

  CPlySegmentCallback callback;

  // In practice, detect often depends on capture stream to become stable.
  int nErr = pDevice->StartCapture(&callback);
  if (nErr != 0) {
    printf("StartCapture failed: %d\n", nErr);
    delete pDevice;
    VzCoalSDKDestroy();
    return -3;
  }
  printf("Capture started. Warming up...\n");
  Sleep(1000);

  pDevice->EnableDynamicDepthPic(VzTrue);
  if (!ConfigureDetectDepthMap(pDevice)) {
    pDevice->StopCapture();
    delete pDevice;
    VzCoalSDKDestroy();
    return -4;
  }

  nErr = pDevice->StartDetect(&callback);
  if (nErr != 0) {
    printf("StartDetect failed: %d\n", nErr);
    pDevice->StopCapture();
    delete pDevice;
    VzCoalSDKDestroy();
    return -5;
  }

  printf("Detect started. Saving segments to PlySegments/.\n");
  printf("Press any key to stop early.\n");

  while (true) {
    if (_kbhit()) {
      (void)_getch();
      printf("Manual stop requested.\n");
      break;
    }

    if (g_nMaxPlySegmentFiles > 0 &&
        g_nSavedPlySegmentCount >= g_nMaxPlySegmentFiles) {
      break;
    }

    Sleep(100);
  }

  pDevice->StopDetect();
  pDevice->StopCapture();

  printf("Stopped. Total saved segments: %d\n", g_nSavedPlySegmentCount);

  delete pDevice;
  VzCoalSDKDestroy();
  printf("Exiting.\n");
  return 0;
}
