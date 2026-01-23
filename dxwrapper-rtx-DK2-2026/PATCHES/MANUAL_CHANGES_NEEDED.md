# Manual Changes Needed for dxwrapper-rtx-DK2-2025

## Completed Automatically:
1. Settings.h - Added DdrawConvertHomogeneous* config options and DdrawLogTextureAtlas
2. IDirect3DTypes.h - Added CONVERTHOMOGENEOUS struct and required includes
3. IDirect3DDeviceX.h - Added ConvertHomogeneous member variable
4. d3d9.cpp - Applied RTX Remix d3d9.dll loading patch
5. Dllmain.cpp - Added RTX Remix detection code
6. RemixAPI.h - Copied from old version

## Manual Changes Required:

### IDirect3DDeviceX.cpp (Main RTX Remix Changes)

The full patch is in: `PATCHES/IDirect3DDeviceX.cpp.patch`

Key changes needed:

#### 1. Add include at top:
```cpp
#include "RemixAPI.h"
```

#### 2. BeginScene function - Add camera matrix initialization:
After `PrepDevice();` in BeginScene, add:
```cpp
// Set 3D camera matrices at START of scene for RTX Remix camera detection
// Must be done BEFORE draw calls, as camera is detected during draw call processing
if (Config.DdrawConvertHomogeneousToWorld)
{
    if (!ConvertHomogeneous.IsTransformViewSet)
    {
        // Compute view/projection matrices
        D3DVIEWPORT9 vp;
        (*d3d9Device)->GetViewport(&vp);
        const float width = (float)vp.Width;
        const float height = (float)vp.Height;
        const float fov = Config.DdrawConvertHomogeneousToWorldFOV;
        const float nearplane = Config.DdrawConvertHomogeneousToWorldNearPlane;
        const float farplane = Config.DdrawConvertHomogeneousToWorldFarPlane;
        const float ratio = width / height;

        // Create projection matrix (perspective)
        DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(
            fov * (3.14159265359f / 180.0f), ratio, nearplane, farplane);
        DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&ConvertHomogeneous.ToWorld_ProjectionMatrix, proj);

        // Create view matrix - isometric camera looking down at 45 degrees (DK2 specific)
        DirectX::XMVECTOR position = DirectX::XMVectorSet(0.0f, 800.0f, -800.0f, 0.0f);
        DirectX::XMVECTOR direction = DirectX::XMVector3Normalize(
            DirectX::XMVectorSet(0.0f, -0.707f, 0.707f, 0.0f));
        DirectX::XMVECTOR upVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookToLH(position, direction, upVector);
        DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&ConvertHomogeneous.ToWorld_ViewMatrix, viewMatrix);

        // Create screen-to-NDC matrix
        DirectX::XMMATRIX screenToNDC = DirectX::XMMatrixSet(
            2.0f / width, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / height, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
        );

        // Compute inverse matrix for vertex transformation
        DirectX::XMMATRIX vp_matrix = DirectX::XMMatrixMultiply(viewMatrix, proj);
        DirectX::XMMATRIX vpinv = DirectX::XMMatrixInverse(nullptr, vp_matrix);
        ConvertHomogeneous.ToWorld_ViewMatrixInverse = DirectX::XMMatrixMultiply(screenToNDC, vpinv);

        ConvertHomogeneous.IsTransformViewSet = true;
    }

    // Set matrices via D3D9 BEFORE any draw calls
    (*d3d9Device)->SetTransform(D3DTS_VIEW, &ConvertHomogeneous.ToWorld_ViewMatrix);
    (*d3d9Device)->SetTransform(D3DTS_PROJECTION, &ConvertHomogeneous.ToWorld_ProjectionMatrix);
}
```

#### 3. DrawPrimitive function - Add XYZRHW conversion:
In DrawPrimitive, after `UpdateVertices(...)`, add XYZRHW to world space conversion code.
(See full patch for complete code - approximately 80 lines)

#### 4. DrawIndexedPrimitive function - Similar XYZRHW conversion
(See full patch)

#### 5. SetTransform function - Modify default camera position for DK2:
Change the default camera position from (0,0,0) looking at (0,0,1) to:
```cpp
position = DirectX::XMVectorSet(0.0f, 800.0f, -800.0f, 0.0f);
direction = DirectX::XMVector3Normalize(DirectX::XMVectorSet(0.0f, -0.707f, 0.707f, 0.0f));
```

#### 6. SetTexture function - Add atlas logging (optional):
Add texture logging for atlas analysis if DdrawLogTextureAtlas is enabled.

### IDirectDrawSurfaceX.cpp/.h (Atlas Tracking - Optional)

The atlas tracking is optional and was used for debugging texture atlas issues.
See `PATCHES/IDirectDrawSurfaceX.cpp.patch` and `PATCHES/IDirectDrawSurfaceX.h.patch`

Key additions:
- Lock/Unlock tracking for texture atlas analysis
- LogAtlasTrackingAndReset() static function

## Build Instructions

After applying changes:
```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'C:\Users\Alex\Desktop\Dungeon Keeper 2\dxwrapper-rtx-DK2-2025\dxwrapper.sln' /p:Configuration=Release /p:Platform=Win32 /t:Build /m"
```

Copy DLL to game folder:
```
powershell -Command "Copy-Item 'C:\Users\Alex\Desktop\Dungeon Keeper 2\dxwrapper-rtx-DK2-2025\bin\Release\dxwrapper.dll' 'C:\Users\Alex\Desktop\Dungeon Keeper 2\' -Force"
```
