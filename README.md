# Dungeon Keeper 2 - RTX Remix Path Tracing via dxwrapper

**Real-time path tracing on a 1999 DirectX 6 game.**

This project achieves path tracing on Dungeon Keeper 2 using a modified [dxwrapper](https://github.com/elishacloud/dxwrapper) and NVIDIA RTX Remix. This is believed to be the first successful implementation of RTX path tracing on a true DirectX 6 title.

**Key Achievement:** Reverse-projecting pre-transformed screen-space vertices (XYZRHW) back into world-space 3D geometry that RTX Remix can ray trace.

**Date achieved:** 2026-01-19

---

## Why This Is Unusual

RTX Remix officially supports **DirectX 8 and 9** games only. NVIDIA's own documentation states there are no known wrapper solutions for DirectX 7 or older to fixed-function D3D9. Dungeon Keeper 2 is DirectX **6** - a step further back than even the unsupported DX7 tier.

The core blocker wasn't just the API version gap. DK2 uses **pre-transformed vertices** (`D3DFVF_XYZRHW`) - the game projects all 3D geometry to 2D screen coordinates on the CPU before sending it to the GPU. RTX Remix needs world-space 3D geometry to cast rays. No existing wrapper handled this inverse projection problem.

---

## Confirmed: DirectX 6 Game

Binary analysis of `DKII.exe` confirmed the exact DirectX interfaces.

**Import Table:**
```
DLL: DDRAW.dll
    DirectDrawCreate        (NOT DirectDrawCreateEx - which DX7 requires)
```

**COM Interface GUIDs Found:**

| Interface | DirectX Version | Found | Address |
|-----------|-----------------|-------|---------|
| **IDirectDraw4** | **DX6** | **YES** | **0x0066bca8** |
| IDirectDraw7 | DX7 | NO | - |
| **IDirect3D3** | **DX6** | **YES** | **0x0066bfc8** |
| IDirect3D7 | DX7 | NO | - |

**Disassembly proof:**
```asm
0x56f3a3: push 0x66bca8    ; IID_IDirectDraw4 GUID
0x56f3a8: mov ecx, [eax]
0x56f3ab: call [ecx]       ; QueryInterface()

0x56f4ab: push 0x66bfc8    ; IID_IDirect3D3 GUID
0x56f4b0: push eax
0x56f4b3: call [ecx]       ; QueryInterface()
```

Additional evidence: MMX Software Renderer dated "1998/9" in the binary. No shader references (DX8+), no Hardware T&L (DX7+). Game shipped June 1999.

---

## Architecture

### Conversion Chain

```
DKII.exe (32-bit, DirectX 6)
    |
    v
dxwrapper.dll (32-bit) - Custom build with XYZRHW conversion
    |
    +-- ddraw.dll wrapper - Converts DDraw7 -> D3D9
    |
    v
d3d9.dll (32-bit stub) - NV Bridge client
    |
    v  [IPC across 32->64 bit boundary]
    |
NV Bridge (64-bit)
    |
    v
RTX Remix Runtime (64-bit) - Path tracing
    |
    v
GPU (RTX hardware)
```

### Data Flow
```
DK2 (IDirectDraw4 / IDirect3D3)
    | [compatibility shim]
DDraw7 interface
    | [dxwrapper Dd7to9 conversion]
D3D9 + XYZRHW->World reconstruction
    | [RTX Remix interception]
Path Traced Output
```

---

## The XYZRHW Problem and Solution

### The Problem

DirectX 6/7 games can use **pre-transformed vertices** (`D3DFVF_XYZRHW`):
- Vertices are already projected to 2D screen space by the CPU
- Format: `(X, Y, Z, RHW)` where X,Y are pixel coordinates on screen
- RTX Remix needs **world-space 3D geometry** to cast rays
- No existing wrapper solved this

### The Solution

An **inverse projection pipeline** implemented in dxwrapper converts screen-space vertices back to world space:

```
Screen Space (X, Y, Z, RHW)
    | [Inverse View-Projection Matrix]
World Space (X, Y, Z)
    | [Synthetic View + Projection matrices]
RTX Remix Path Tracing
```

#### Core Vertex Conversion (IDirect3DDeviceX.cpp)

```cpp
for (UINT x = 0; x < dwVertexCount; x++)
{
    float* srcpos = (float*)sourceVertex;
    float* trgtpos = (float*)targetVertex;

    // Transform from screen space back to world space
    DirectX::XMVECTOR xpos = DirectX::XMVectorSet(srcpos[0], srcpos[1], srcpos[2], srcpos[3]);
    DirectX::XMVECTOR xpos_global = DirectX::XMVector3TransformCoord(
        xpos, ConvertHomogeneous.ToWorld_ViewMatrixInverse);
    xpos_global = DirectX::XMVectorDivide(xpos_global, DirectX::XMVectorSplatW(xpos_global));

    // Write world-space vertex (XYZ, not XYZRHW)
    trgtpos[0] = DirectX::XMVectorGetX(xpos_global);
    trgtpos[1] = DirectX::XMVectorGetY(xpos_global);
    trgtpos[2] = DirectX::XMVectorGetZ(xpos_global);

    // Copy remaining vertex data (colors, UVs, etc.)
    std::memcpy(targetVertex + sizeof(float) * 3, sourceVertex + sizeof(float) * 4, restSize);

    sourceVertex += stride;
    targetVertex += targetStride;
}
```

#### Inverse Matrix Calculation

```cpp
DirectX::XMMATRIX toViewSpace = DirectX::XMLoadFloat4x4((DirectX::XMFLOAT4X4*)&view);
DirectX::XMMATRIX vp = DirectX::XMMatrixMultiply(viewMatrix, proj);
DirectX::XMMATRIX vpinv = DirectX::XMMatrixInverse(nullptr, vp);
DirectX::XMMATRIX depthoffset = DirectX::XMMatrixTranslation(0.0f, 0.0f, depthOffset);

// Full transform: screen -> NDC -> clip -> view -> world
ConvertHomogeneous.ToWorld_ViewMatrixInverse =
    DirectX::XMMatrixMultiply(depthoffset, DirectX::XMMatrixMultiply(toViewSpace, vpinv));
```

### Critical Bug Fix: Matrix Timing

RTX Remix detects the camera by reading transform matrices. Two things had to be right:

1. **Set matrices in BeginScene, BEFORE draw calls** - RTX Remix reads them during draw call processing, not at end of frame
2. **Don't restore the original matrices after drawing** - the original code was overwriting the synthetic camera matrices, preventing RTX Remix from detecting the camera

---

## Files Modified in dxwrapper

### Settings/Settings.h
Added configuration options to the `VISIT_CONFIG_SETTINGS` macro and `CONFIG` struct:
```cpp
bool DdrawConvertHomogeneousW = false;       // Enable XYZRHW -> XYZ conversion
bool DdrawConvertHomogeneousToWorld = false;  // Reconstruct world-space geometry
float DdrawConvertHomogeneousToWorldFOV = 90.0f;
float DdrawConvertHomogeneousToWorldNearPlane = 1.0f;
float DdrawConvertHomogeneousToWorldFarPlane = 1000.0f;
float DdrawConvertHomogeneousToWorldDepthOffset = 0.0f;
```

### ddraw/IDirect3DDeviceX.cpp
- **BeginScene:** Camera matrix initialization before draw calls
- **DrawPrimitive:** XYZRHW vertex conversion to world space
- **DrawIndexedPrimitive:** Same XYZRHW conversion for indexed geometry
- **EndScene:** Removed matrix restoration that was breaking camera detection

### ddraw/IDirect3DTypes.h
Added `CONVERTHOMOGENEOUS` struct to hold matrices, intermediate buffers, and state.

### d3d9/d3d9.cpp
Modified to prefer already-loaded d3d9.dll (RTX Remix) over system DLL.

### ddraw/RemixAPI.h *(new file, 2026-01-19)*
RTX Remix C API header for direct camera control. Note: direct API may not work through the 32-to-64-bit NV Bridge - the D3D9 transform method is the working solution.

---

## Configuration

### dxwrapper.ini

```ini
; Conversion pipeline
Dd7to9                                      = 1
EnableD3d9Wrapper                           = 1

; XYZRHW vertex conversion (critical for RTX)
DdrawDisableLighting                        = 1
DdrawConvertHomogeneousW                    = 1
DdrawConvertHomogeneousToWorld              = 1
DdrawConvertHomogeneousToWorldFOV           = 60.0
DdrawConvertHomogeneousToWorldNearPlane     = 0.1
DdrawConvertHomogeneousToWorldFarPlane      = 5000.0
DdrawConvertHomogeneousToWorldDepthOffset   = 0.0

; Window mode
EnableWindowMode                            = 1
FullscreenWindowMode                        = 1
```

### rtx.conf

```ini
rtx.orthographicIsUI = False    ; Don't classify reconstructed geometry as UI
```

---

## Texture Replacement (2026-01-22)

Texture replacement is confirmed working. DK2 uses **pre-built 128x128 texture atlases** (not runtime-assembled). RTX Remix can hash these stably.

### Method

Create a USD mod structure:

```
rtx-remix/mods/<ModName>/
    mod.usda
    textures/
        <HASH>.dds
```

**mod.usda:**
```usda
#usda 1.0
(
    customLayerData = {
        string lightspeed_game_name = "Dungeon Keeper II"
        string lightspeed_layer_type = "replacement"
    }
    upAxis = "Y"
)

over "RootNode"
{
    over "Looks"
    {
        over "mat_<TEXTURE_HASH>"
        {
            over "Shader"
            {
                asset inputs:diffuse_texture = @./textures/<TEXTURE_HASH>.dds@ (
                    colorSpace = "auto"
                )
            }
        }
    }
}
```

Works for both environment textures and creature/sprite textures.

---

## Building

Requires Visual Studio 2022.

```
MSBuild dxwrapper.sln /p:Configuration=Release /p:Platform=Win32 /t:Build /m
```

Copy the built `dxwrapper.dll` to your Dungeon Keeper 2 game directory alongside the RTX Remix `d3d9.dll`.

---

## Credits

- [dxwrapper](https://github.com/elishacloud/dxwrapper) by elishacloud - the DirectX wrapper this is built on
- [dkollmann's fork](https://github.com/dkollmann/dxwrapper) - added the initial XYZRHW conversion framework (2023)
- [NVIDIA RTX Remix](https://github.com/NVIDIAGameWorks/rtx-remix) - the path tracing runtime

---

*Path tracing achieved: 2026-01-19*
*Texture replacement confirmed: 2026-01-22*
