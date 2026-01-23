#pragma once

// RTX Remix API definitions for direct camera setup
// This allows bypassing D3D9 camera detection which doesn't work with pre-transformed vertices

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Calling convention for Remix API
#define REMIXAPI_CALL __stdcall
#define REMIXAPI_PTR REMIXAPI_CALL

// Error codes (from dxvk-remix public/include/remix/remix_c.h)
typedef enum remixapi_ErrorCode {
    REMIXAPI_ERROR_CODE_SUCCESS = 0,
    REMIXAPI_ERROR_CODE_GENERAL_FAILURE = 1,
    REMIXAPI_ERROR_CODE_LOAD_LIBRARY_FAILURE = 2,
    REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS = 3,
    REMIXAPI_ERROR_CODE_GET_PROC_ADDRESS_FAILURE = 4,
    REMIXAPI_ERROR_CODE_ALREADY_EXISTS = 5,
    REMIXAPI_ERROR_CODE_REGISTERING_NON_REMIX_D3D9_DEVICE = 6,
    REMIXAPI_ERROR_CODE_REMIX_DEVICE_WAS_NOT_REGISTERED = 7,
    REMIXAPI_ERROR_CODE_INCOMPATIBLE_VERSION = 8,
    REMIXAPI_ERROR_CODE_SET_DLL_DIRECTORY_FAILURE = 9,
    REMIXAPI_ERROR_CODE_GET_FULL_PATH_NAME_FAILURE = 10,
    REMIXAPI_ERROR_CODE_NOT_INITIALIZED = 11,
} remixapi_ErrorCode;

// Structure types (used for version control)
typedef enum remixapi_StructType {
    REMIXAPI_STRUCT_TYPE_NONE = 0,
    REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO = 1,
    REMIXAPI_STRUCT_TYPE_MATERIAL_INFO = 2,
    REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_PORTAL_EXT = 3,
    REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_TRANSLUCENT_EXT = 4,
    REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT = 5,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO = 6,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT = 7,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_RECT_EXT = 8,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISK_EXT = 9,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_CYLINDER_EXT = 10,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT = 11,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DOME_EXT = 12,
    REMIXAPI_STRUCT_TYPE_MESH_INFO = 13,
    REMIXAPI_STRUCT_TYPE_INSTANCE_INFO = 14,
    REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT = 15,
    REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT = 16,
    REMIXAPI_STRUCT_TYPE_CAMERA_INFO = 17,
    REMIXAPI_STRUCT_TYPE_CAMERA_INFO_PARAMETERIZED_EXT = 18,
    REMIXAPI_STRUCT_TYPE_PRESENT_INFO = 19,
    REMIXAPI_STRUCT_TYPE_STARTUP_INFO = 20,
} remixapi_StructType;

// Camera type
typedef enum remixapi_CameraType {
    REMIXAPI_CAMERA_TYPE_WORLD = 0,
    REMIXAPI_CAMERA_TYPE_SKY = 1,
    REMIXAPI_CAMERA_TYPE_VIEW_MODEL = 2,
} remixapi_CameraType;

// Basic types
typedef uint32_t remixapi_Bool;

// Float 3D vector
typedef struct remixapi_Float3D {
    float x;
    float y;
    float z;
} remixapi_Float3D;

// Transform (4x3 row-major matrix)
typedef struct remixapi_Transform {
    float matrix[3][4];
} remixapi_Transform;

// Camera info structure
typedef struct remixapi_CameraInfo {
    remixapi_StructType sType;
    void* pNext;
    remixapi_CameraType type;
    float view[4][4];
    float projection[4][4];
} remixapi_CameraInfo;

// Parameterized camera extension (optional - provides more camera parameters)
typedef struct remixapi_CameraInfoParameterizedEXT {
    remixapi_StructType sType;
    void* pNext;
    remixapi_Float3D position;
    remixapi_Float3D forward;
    remixapi_Float3D up;
    remixapi_Float3D right;
    float fovYInDegrees;
    float aspect;
    float nearPlane;
    float farPlane;
} remixapi_CameraInfoParameterizedEXT;

// Startup info structure
typedef struct remixapi_StartupInfo {
    remixapi_StructType sType;
    void* pNext;
    HWND hwnd;
    remixapi_Bool disableSrgbConversionForOutput;
    remixapi_Bool forceNoVkSwapchain;
    remixapi_Bool editorModeEnabled;
} remixapi_StartupInfo;

// Initialize library info structure
typedef struct remixapi_InitializeLibraryInfo {
    remixapi_StructType sType;
    void* pNext;
    uint64_t version;
} remixapi_InitializeLibraryInfo;

// Present info structure
typedef struct remixapi_PresentInfo {
    remixapi_StructType sType;
    void* pNext;
    HWND hwndOverride;
} remixapi_PresentInfo;

// Function pointer types
typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_Startup)(const remixapi_StartupInfo* info);
typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_Shutdown)(void);
typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_SetupCamera)(const remixapi_CameraInfo* info);
typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_Present)(const remixapi_PresentInfo* info);
typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_SetConfigVariable)(const char* var, const char* value);

// The main Remix API interface structure (correct order from dxvk-remix)
typedef struct remixapi_Interface {
    void* Shutdown;                         // PFN_remixapi_Shutdown
    void* CreateMaterial;                   // PFN_remixapi_CreateMaterial
    void* DestroyMaterial;                  // PFN_remixapi_DestroyMaterial
    void* CreateMesh;                       // PFN_remixapi_CreateMesh
    void* DestroyMesh;                      // PFN_remixapi_DestroyMesh
    PFN_remixapi_SetupCamera SetupCamera;   // PFN_remixapi_SetupCamera
    void* DrawInstance;                     // PFN_remixapi_DrawInstance
    void* CreateLight;                      // PFN_remixapi_CreateLight
    void* DestroyLight;                     // PFN_remixapi_DestroyLight
    void* DrawLightInstance;                // PFN_remixapi_DrawLightInstance
    PFN_remixapi_SetConfigVariable SetConfigVariable;
    void* dxvk_CreateD3D9;                  // PFN_remixapi_dxvk_CreateD3D9
    void* dxvk_RegisterD3D9Device;          // PFN_remixapi_dxvk_RegisterD3D9Device
    void* dxvk_GetExternalSwapchain;        // PFN_remixapi_dxvk_GetExternalSwapchain
    void* dxvk_GetVkImage;                  // PFN_remixapi_dxvk_GetVkImage
    void* dxvk_CopyRenderingOutput;         // PFN_remixapi_dxvk_CopyRenderingOutput
    void* dxvk_SetDefaultOutput;            // PFN_remixapi_dxvk_SetDefaultOutput
    void* pick_RequestObjectPicking;        // PFN_remixapi_pick_RequestObjectPicking
    void* pick_HighlightObjects;            // PFN_remixapi_pick_HighlightObjects
    PFN_remixapi_Startup Startup;           // PFN_remixapi_Startup
    PFN_remixapi_Present Present;           // PFN_remixapi_Present
} remixapi_Interface;

// Initialize library function type
typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_InitializeLibrary)(
    const remixapi_InitializeLibraryInfo* info,
    remixapi_Interface* out_result
);

// Version macro (major.minor.patch format)
#define REMIXAPI_VERSION_MAKE(major, minor, patch) \
    (((uint64_t)(major) << 48) | ((uint64_t)(minor) << 32) | ((uint64_t)(patch)))

// Current API version (0.5.2 from dxvk-remix)
#define REMIXAPI_VERSION_CURRENT REMIXAPI_VERSION_MAKE(0, 5, 2)

#ifdef __cplusplus
}
#endif

// C++ helper class for managing the Remix API
#ifdef __cplusplus

class RemixAPIManager {
private:
    static RemixAPIManager* s_instance;
    bool m_initialized;
    bool m_initAttempted;
    remixapi_Interface m_interface;
    PFN_remixapi_InitializeLibrary m_pfnInitialize;

    RemixAPIManager() : m_initialized(false), m_initAttempted(false), m_pfnInitialize(nullptr) {
        memset(&m_interface, 0, sizeof(m_interface));
    }

public:
    static RemixAPIManager& Instance() {
        static RemixAPIManager instance;
        return instance;
    }

    bool Initialize(const char** outError = nullptr) {
        if (m_initialized) {
            return true;
        }

        // If we already permanently failed (not a retry-able error), don't retry
        if (m_initAttempted && m_lastError != REMIXAPI_ERROR_CODE_NOT_INITIALIZED) {
            return false;
        }

        // Get the d3d9.dll module (should be RTX Remix's version)
        HMODULE hD3D9 = GetModuleHandleA("d3d9.dll");
        if (!hD3D9) {
            if (outError) *outError = "GetModuleHandleA(d3d9.dll) returned NULL";
            m_initAttempted = true;
            return false;
        }

        // Get the initialization function (only once)
        if (!m_pfnInitialize) {
            m_pfnInitialize = (PFN_remixapi_InitializeLibrary)GetProcAddress(hD3D9, "remixapi_InitializeLibrary");
            if (!m_pfnInitialize) {
                if (outError) *outError = "GetProcAddress(remixapi_InitializeLibrary) returned NULL";
                m_initAttempted = true;
                return false;
            }
        }

        // Initialize the library
        remixapi_InitializeLibraryInfo initInfo = {};
        initInfo.sType = REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
        initInfo.pNext = nullptr;
        initInfo.version = REMIXAPI_VERSION_CURRENT;

        remixapi_ErrorCode result = m_pfnInitialize(&initInfo, &m_interface);
        if (result != REMIXAPI_ERROR_CODE_SUCCESS) {
            m_lastError = result;
            // NOT_INITIALIZED is retry-able (Remix may not be ready yet)
            if (result != REMIXAPI_ERROR_CODE_NOT_INITIALIZED) {
                m_initAttempted = true; // Permanent failure
            }
            if (outError) *outError = "remixapi_InitializeLibrary failed";
            return false;
        }

        m_initialized = true;
        m_initAttempted = true;
        return true;
    }

    remixapi_ErrorCode GetLastError() const { return m_lastError; }

    bool IsInitialized() const { return m_initialized; }

    remixapi_ErrorCode SetupCamera(const remixapi_CameraInfo* info) {
        if (!m_initialized || !m_interface.SetupCamera) {
            return REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
        }
        return m_interface.SetupCamera(info);
    }

    remixapi_ErrorCode SetConfigVariable(const char* var, const char* value) {
        if (!m_initialized || !m_interface.SetConfigVariable) {
            return REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
        }
        return m_interface.SetConfigVariable(var, value);
    }

private:
    remixapi_ErrorCode m_lastError = REMIXAPI_ERROR_CODE_SUCCESS;
};

#endif
