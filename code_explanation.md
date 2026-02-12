# Code Explanation: main.cpp Workflow and SDK Integration

This document outlines the workflow of the `main.cpp` sample application, detailing how it initializes the VzCoal SDK, manages device connections, and handles data acquisition.

## 1. High-Level Workflow

The application follows a standard synchronous workflow:
1.  **Initialization**: Load SDK and find devices.
2.  **Connection**: Create a device object for the first found device.
3.  **Operation Loop**: Enter a command-line menu loop (`_OnProcessDevice`) to interact with the device.
4.  **Data Acquisition**: Start detection/capture and handle data asynchronously via a callback class (`CVzDeviceDataCallBack`).
5.  **Shutdown**: Cleanup resources upon exit.

## 2. SDK Initialization & Device Connection (`main` function)

The entry point `main()` performs the initial setup:

1.  **Init SDK**: `VzCoalSDKInit()` initializes the SDK library.
2.  **Find Device**:
    *   `VzCreateCoalDeviceFinder(&pIDeviceFinder)` creates a finder utility.
    *   `pIDeviceFinder->FindDevice()` scans the network for available devices.
    *   `pIDeviceFinder->QueryDeviceInfo(...)` retrieves details (IP, SN) of the found device.
3.  **Create Device Instance**:
    *   `VzCreateCoalDevice(&sEyeCBInfo, &pICoalDevice)` creates the main control object (`IVzCoalDevice`) for the specific device found.
4.  **Enter Main Loop**: Calls `_OnProcessDevice(pICoalDevice)` to start user interaction.

## 3. Main Operation Loop (`_OnProcessDevice`)

This function contains the main application logic:

1.  **Callback Setup**:
    *   Creates an instance of `CVzDeviceDataCallBack` (derived from `IVzDeviceDataCallBack`).
    *   This object will receive all data streams (images, point clouds, volume data) from the device.
2.  **Menu Loop (`for(;;)` switch-case)**:
    *   Displays options (1-19) to the user.
    *   Takes user input to trigger specific SDK actions.

## 4. Depth Map Implementation Deep Dive (Detailed Operation)

This section details exactly how the depth map is configured, generated, and saved.

### step 1: Configuration (`_OnMenuDetect` inside `main.cpp`)

Before starting detection (Option 15), we configure the depth map generation parameters.

1.  **Calculate Resolution (`fScale`)**:
    *   **Goal**: Determine how many mm each pixel represents.
    *   `fYScale`: Resolution of the laser scan across the belt width (e.g., ~0.75mm/point).
    *   `fXScale`: Resolution along the belt length, determined by Belt Speed / Frame Rate (e.g., ~1.67mm/line).
    *   **Logic**:
        *   We compare `fYScale` and `fXScale`.
        *   **Standard Mode**: Set `fScale` to `max(fYScale, fXScale)` (usually ~1.67mm) to keep pixels square. Result: Lower resolution (~570px wide).
        *   **High Resolution Mode** (`g_bForceHighRes = true`): Force `fScale = fYScale` (~0.75mm). Result: Higher resolution (~1264px wide), capturing full detail.

2.  **Configure Parameters (`SVzDepthMapImgCtlPara`)**:
    *   `eDepthNormalizingMode = keDepthNormalizingMode_Indicated`: We manually specify the Z-range.
    *   `fWHPixelResolution = fScale`: Sets the calculated mm/pixel resolution.
    *   **Normalization Range**:
        *   `fNormalizingStartVal = sRangeZ.min - 5.0f`: Minimum height (white/highest point) + buffer.
        *   `fNormalizingEndVal = sRangeZ.max + 5.0f`: Maximum height (black/background) + buffer.
        *   This maps the physical Z-heights to 0-255 grayscale values tightly around the conveyor belt surface for maximum contrast.

3.  **Apply Settings**:
    *   `pICoalDevice->SetDynamicDepthMapParam(sDepthMapParam)` sends this configuration to the SDK.

### Step 2: Generation (SDK Internal)

The SDK uses these parameters to:
1.  Filter points based on **Detect ROI**.
2.  Subtract the background using **Calibration Data** (`RecordEmptyArea`).
3.  Project the remaining points onto a 2D grid with cell size = `fScale` (0.75mm).
4.  Normalize the Z-values to 8-bit pixels based on the `StartVal`/`EndVal` range.

### Step 3: Saving (`CVzDeviceDataCallBack::OnOutputDepthMap`)

When the SDK finishes generating a depth map (accumulating ~10 frames), it calls this function.

1.  **Check Flag**: Verifies `g_bSaveDepthMap` is enabled.
2.  **Create Directory**: Checks/Creates `DepthMaps` folder.
3.  **Generate Filename**: Uses `GetLocalTime` to create a timestamp: `DepthMap_YYYYMMDD_HHMMSS_MS.png`.
4.  **Save**: Calls `m_pDevice->SaveImage(szFileName, &sDepthMap)`.

### Summary of Key SDK Interactions

| Action | Function Call | Purpose |
| :--- | :--- | :--- |
| **Init** | `VzCoalSDKInit` | Load SDK |
| **Find** | `VzCreateCoalDeviceFinder` | Locate devices |
| **Connect** | `VzCreateCoalDevice` | Connect to device |
| **Config** | `SetDynamicDepthMapParam` | Configure depth map (Res, Norm) |
| **Config** | `SetDetectROI` | Set scan area (Coverage) |
| **Action** | `StartDetect` | Start data stream |
| **Action** | `RecordEmptyArea` | Calibrate empty belt |
| **Data** | `IVzDeviceDataCallBack` | Receive async data |
