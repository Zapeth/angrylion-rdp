#include "z64.h"

#if (PJ64_PLUGIN_API)
	#include "gfx_1.3.h"
#else
	#include "m64p_common.h"
	#include "m64p_config.h"
	#include "m64p_plugin.h"
	#include "osal/dynamiclib.h"
#endif

const int screen_width = 1024, screen_height = 768;

LPDIRECTDRAW7 lpdd = NULL;
LPDIRECTDRAWSURFACE7 lpddsprimary;
LPDIRECTDRAWSURFACE7 lpddsback;
DDSURFACEDESC2 ddsd;
HRESULT res;
RECT dst, src;
INT32 pitchindwords;

int ProcessDListShown = 0;
extern UINT32 command_counter;

extern GFX_INFO gfx;

int rdp_init();
int rdp_close();
int rdp_update();
void rdp_process_list(void);

#if (PJ64_PLUGIN_API)

void fatalerror(const char *err, ...)
{
	char VsprintfBuffer[2000];
	va_list arg;
	va_start(arg, err);
	vsnprintf(VsprintfBuffer, sizeof(VsprintfBuffer), err, arg);
#ifdef _WIN32
	MessageBoxA(0, VsprintfBuffer, "RDP: fatal error", MB_OK);
#else
	fprintf(stderr, VsprintfBuffer);
#endif
	va_end(arg);
	exit(0);
}

void popmessage(const char *err, ...)
{
	char VsprintfBuffer[2000];
	va_list arg;
	va_start(arg, err);
	vsnprintf(VsprintfBuffer, sizeof(VsprintfBuffer), err, arg);
#ifdef _WIN32
	MessageBoxA(0, VsprintfBuffer, "RDP: warning", MB_OK);
#else
	fprintf(stdout, VsprintfBuffer);
#endif
	va_end(arg);
}

EXPORT void CALL CaptureScreen(char *Directory)
{
}

EXPORT void CALL ChangeWindow(void)
{
}

EXPORT void CALL CloseDLL(void)
{
}

EXPORT void CALL DllAbout(HWND hParent)
{
	popmessage("angrylion's RDP, unpublished beta. MESS source code used.");
}

EXPORT void CALL DllConfig(HWND hParent)
{
	popmessage("Nothing to configure");
}

EXPORT void CALL DllTest(HWND hParent)
{
}

EXPORT void CALL ReadScreen(void **dest, long *width, long *height)
{
}

EXPORT void CALL DrawScreen(void)
{
}

EXPORT void CALL GetDllInfo(PLUGIN_INFO *PluginInfo)
{
	PluginInfo->Version = 0x0103;
	PluginInfo->Type  = PLUGIN_TYPE_GFX;
	snprintf(PluginInfo->Name, sizeof(PluginInfo->Name), "My little plugin");

	PluginInfo->NormalMemory = TRUE;
	PluginInfo->MemoryBswaped = TRUE;
}

#else	// !(PJ64_PLUGIN_API)

// version info
#define RDP_PLUGIN_VERSION			0x000100	// plugin version
#define VIDEO_PLUGIN_API_VERSION	0x020200	// supported core video api version
#define CONFIG_API_VERSION			0x020300	// supported core config api version
#define CONFIG_PARAM_VERSION		0			// config section version

#define VERSION_PRINTF_SPLIT(x) (((x) >> 16) & 0xffff), (((x) >> 8) & 0xff), ((x) & 0xff)

// global window pointer
SDL_Window *window = NULL;

// core callbacks
void (*l_RenderCallback)(int) = NULL;
static void (*l_DebugCallback)(void *, int, const char *) = NULL;
static void *l_DebugCallContext = NULL;
static int l_PluginInit = 0;

static m64p_handle l_ConfigSection;

ptr_PluginGetVersion       CoreGetVersion = NULL;

// definitions of pointers to Core config functions
ptr_ConfigOpenSection      ConfigOpenSection = NULL;
ptr_ConfigDeleteSection    ConfigDeleteSection = NULL;
ptr_ConfigSaveSection      ConfigSaveSection = NULL;
ptr_ConfigSetParameter     ConfigSetParameter = NULL;
ptr_ConfigGetParameter     ConfigGetParameter = NULL;
ptr_ConfigGetParameterHelp ConfigGetParameterHelp = NULL;
ptr_ConfigSetDefaultInt    ConfigSetDefaultInt = NULL;
ptr_ConfigSetDefaultFloat  ConfigSetDefaultFloat = NULL;
ptr_ConfigSetDefaultBool   ConfigSetDefaultBool = NULL;
ptr_ConfigSetDefaultString ConfigSetDefaultString = NULL;
ptr_ConfigGetParamInt      ConfigGetParamInt = NULL;
ptr_ConfigGetParamFloat    ConfigGetParamFloat = NULL;
ptr_ConfigGetParamBool     ConfigGetParamBool = NULL;
ptr_ConfigGetParamString   ConfigGetParamString = NULL;

void DebugMessage(int level, const char *message, va_list args)
{
	char msgbuf[1024];

	if (l_DebugCallback == NULL)
		return;

	vsnprintf(msgbuf, sizeof(msgbuf), message, args);

	l_DebugCallback(l_DebugCallContext, level, msgbuf);
}

// wrap these around mupen64's debug callback for now
void fatalerror(const char *err, ...)
{
	va_list args;
	va_start(args, err);
	DebugMessage(M64MSG_ERROR, err, args);
	va_end(args);
}
void popmessage(const char *err, ...)
{
	va_list args;
	va_start(args, err);
	DebugMessage(M64MSG_INFO, err, args);
	va_end(args);
}

int LoadConfig(void)
{
	const char *section_name = "rdp-angrylion";
	int config_version;

	if (ConfigOpenSection(section_name, &l_ConfigSection) != M64ERR_SUCCESS)
	{
		popmessage("Couldn't open config section '%s'", section_name);
		return 1;
	}

	if (ConfigGetParameter(l_ConfigSection, "Version", M64TYPE_INT, &config_version, sizeof(int)) != M64ERR_SUCCESS)
	{
		popmessage("No version number found in '%s' config section, using defaults.", section_name);

		if (ConfigSaveSection != NULL)
		{
			popmessage("[STUB] Saving defaults to '%s' config section.", section_name);
			// todo: move saving of config defaults into a separate function
			if (ConfigDeleteSection(section_name) != M64ERR_SUCCESS)
			{
				popmessage("Couldn't delete config section '%s'", section_name);
			}
			else if (ConfigOpenSection(section_name, &l_ConfigSection) != M64ERR_SUCCESS)
			{
				popmessage("Couldn't open config section '%s'", section_name);
			}
			else if (ConfigSetDefaultInt(l_ConfigSection, "Version", CONFIG_PARAM_VERSION, "Config version number, don't change this") != M64ERR_SUCCESS)
			{
				popmessage("Couldn't set default values in config section '%s'", section_name);
			}
			else if (ConfigSaveSection(section_name) != M64ERR_SUCCESS)
			{
				popmessage("Couldn't save default values in config section '%s'", section_name);
			}
		}
	}
	else if (config_version != CONFIG_PARAM_VERSION)
	{
		popmessage("Found %s '%s' config section, using defaults.",
			(config_version < CONFIG_PARAM_VERSION) ? "outdated" : "incompatible", section_name);
		// todo: update config section to newer version or replace entirely with defaults
	}
	else
	{
		// todo: add config options that can/should be loaded
		popmessage("Successfully loaded '%s' config section.", section_name);
	}
	return 0;
}

EXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type *PluginType, int *PluginVersion, int *APIVersion, const char **PluginNamePtr, int *Capabilities)
{
	// set version info for core
	if (PluginType != NULL)
		*PluginType = M64PLUGIN_GFX;

	if (PluginVersion != NULL)
		*PluginVersion = RDP_PLUGIN_VERSION;

	if (APIVersion != NULL)
		*APIVersion = VIDEO_PLUGIN_API_VERSION;

	if (PluginNamePtr != NULL)
		*PluginNamePtr = "mupen64plus-rdp-angrylion Video Plugin";

	if (Capabilities != NULL)
		*Capabilities = 0;

	return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle CoreLibHandle, void *Context, void (*DebugCallback)(void *, int, const char *))
{
	ptr_CoreGetAPIVersions CoreGetAPIVersions;
	int ConfigAPIVersion, DebugAPIVersion;

	if (l_PluginInit != 0)
		return M64ERR_ALREADY_INIT;

	// set the callback function for plugin debug info
	l_DebugCallback = DebugCallback;
	l_DebugCallContext = Context;

	// attach and call the CoreGetAPIVersions function, check Config API version for compatibility
	CoreGetAPIVersions = (ptr_CoreGetAPIVersions)osal_dynlib_getproc(CoreLibHandle, "CoreGetAPIVersions");
	if (CoreGetAPIVersions == NULL)
	{
		fatalerror("Core emulator broken; no CoreGetAPIVersions() function found.");
		return M64ERR_INCOMPATIBLE;
	}
	CoreGetAPIVersions(&ConfigAPIVersion, &DebugAPIVersion, NULL, NULL);
	if ((ConfigAPIVersion & 0xffff0000) != (CONFIG_API_VERSION & 0xffff0000))
	{
		fatalerror("Emulator core Config API (v%i.%i.%i) incompatible with plugin (v%i.%i.%i)",
				VERSION_PRINTF_SPLIT(ConfigAPIVersion), VERSION_PRINTF_SPLIT(CONFIG_API_VERSION));
		return M64ERR_INCOMPATIBLE;
	}

	// Get the core config function pointers from the library handle
	ConfigOpenSection = (ptr_ConfigOpenSection)osal_dynlib_getproc(CoreLibHandle, "ConfigOpenSection");
	ConfigDeleteSection = (ptr_ConfigDeleteSection)osal_dynlib_getproc(CoreLibHandle, "ConfigDeleteSection");
	ConfigSetParameter = (ptr_ConfigSetParameter)osal_dynlib_getproc(CoreLibHandle, "ConfigSetParameter");
	ConfigGetParameter = (ptr_ConfigGetParameter)osal_dynlib_getproc(CoreLibHandle, "ConfigGetParameter");
	ConfigSetDefaultInt = (ptr_ConfigSetDefaultInt)osal_dynlib_getproc(CoreLibHandle, "ConfigSetDefaultInt");
	ConfigSetDefaultFloat = (ptr_ConfigSetDefaultFloat)osal_dynlib_getproc(CoreLibHandle, "ConfigSetDefaultFloat");
	ConfigSetDefaultBool = (ptr_ConfigSetDefaultBool)osal_dynlib_getproc(CoreLibHandle, "ConfigSetDefaultBool");
	ConfigSetDefaultString = (ptr_ConfigSetDefaultString)osal_dynlib_getproc(CoreLibHandle, "ConfigSetDefaultString");
	ConfigGetParamInt = (ptr_ConfigGetParamInt)osal_dynlib_getproc(CoreLibHandle, "ConfigGetParamInt");
	ConfigGetParamFloat = (ptr_ConfigGetParamFloat)osal_dynlib_getproc(CoreLibHandle, "ConfigGetParamFloat");
	ConfigGetParamBool = (ptr_ConfigGetParamBool)osal_dynlib_getproc(CoreLibHandle, "ConfigGetParamBool");
	ConfigGetParamString = (ptr_ConfigGetParamString)osal_dynlib_getproc(CoreLibHandle, "ConfigGetParamString");
	if (!ConfigOpenSection   || !ConfigDeleteSection   || !ConfigSetParameter   || !ConfigGetParameter ||
		!ConfigSetDefaultInt || !ConfigSetDefaultFloat || !ConfigSetDefaultBool || !ConfigSetDefaultString ||
		!ConfigGetParamInt   || !ConfigGetParamFloat   || !ConfigGetParamBool   || !ConfigGetParamString)
	{
		fatalerror("Couldn't connect to Core configuration functions");
		return M64ERR_INCOMPATIBLE;
	}
	// 2.1.0
	ConfigSaveSection = (ptr_ConfigSaveSection)osal_dynlib_getproc(CoreLibHandle, "ConfigSaveSection");
	if (ConfigAPIVersion >= 0x020100 && !ConfigSaveSection)
	{
		fatalerror("Couldn't connect to Core ConfigSaveSection() function");
		return M64ERR_INCOMPATIBLE;
	}

	// need core version to verify version of GFX_INFO struct
	CoreGetVersion = (ptr_PluginGetVersion)osal_dynlib_getproc(CoreLibHandle, "PluginGetVersion");
	if (CoreGetVersion == NULL)
	{
		fatalerror("Core emulator broken; no PluginGetVersion() function found.");
		return M64ERR_INCOMPATIBLE;
	}

	if (LoadConfig() != 0)
		return M64ERR_FILES;

	// initialize SDL Video subsystem (under the assumption that a different component is responsible for SDL_Init/Quit calls)
	if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
		popmessage("(WARNING) An SDL_Video system was already initialized");

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
	{
		fatalerror("SDL_InitSubSystem failed: %s", SDL_GetError());
		return M64ERR_SYSTEM_FAIL;
	}

	l_PluginInit = 1;
	return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL PluginShutdown(void)
{
	if (l_PluginInit == 0)
		return M64ERR_NOT_INIT;

	// reset some local variables
	l_RenderCallback = NULL;
	l_DebugCallback = NULL;
	l_DebugCallContext = NULL;

	CoreGetVersion = NULL;

	ConfigOpenSection = NULL;
	ConfigDeleteSection = NULL;
	ConfigSaveSection = NULL;
	ConfigSetParameter = NULL;
	ConfigGetParameter = NULL;
	ConfigGetParameterHelp = NULL;
	ConfigSetDefaultInt = NULL;
	ConfigSetDefaultFloat = NULL;
	ConfigSetDefaultBool = NULL;
	ConfigSetDefaultString = NULL;
	ConfigGetParamInt = NULL;
	ConfigGetParamFloat = NULL;
	ConfigGetParamBool = NULL;
	ConfigGetParamString = NULL;

	if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
		SDL_QuitSubSystem(SDL_INIT_VIDEO);

	l_PluginInit = 0;
	return M64ERR_SUCCESS;
}

EXPORT void CALL ChangeWindow(void)
{
	popmessage("ChangeWindow called");
}

EXPORT void CALL ReadScreen2(void *dest, int *width, int *height, int front)
{
	popmessage("ReadScreen2 called");
}

EXPORT void CALL SetRenderingCallback(void (*callback)(int))
{
	popmessage("[SetRenderingCallback] Screenshots and OSD are not supported (yet?)");
	l_RenderCallback = callback;
}

EXPORT void CALL ResizeVideoOutput(int width, int height)
{
	popmessage("ResizeVideoOutput called");
}

#endif	// !(PJ64_PLUGIN_API)

GFX_INFO gfx;
EXPORT BOOL CALL InitiateGFX(GFX_INFO Gfx_Info)
{
	gfx = Gfx_Info;
#if (!PJ64_PLUGIN_API)
	if (CoreGetVersion != NULL)
	{
		int core_version;
		CoreGetVersion(NULL, &core_version, NULL, NULL, NULL);
		popmessage("[InitiateGFX] Got core version %i.%i.%i", VERSION_PRINTF_SPLIT(core_version));

		// gfx version member was added in 2.5.1
		if (core_version >= 0x020501)
		{
			// the extra member are not really used for anything right now, but they might be useful in the future
			popmessage("[InitiateGFX] Got gfx struct version %i", gfx.version);
		}
	}
#endif
	return TRUE;
}

EXPORT void CALL MoveScreen(int x, int y)
{
	POINT p;
	p.x = p.y = 0;
#if (PJ64_PLUGIN_API)
	RECT statusrect;
	GetClientRect(gfx.hWnd, &dst);
	ClientToScreen(gfx.hWnd, &p);
	OffsetRect(&dst, p.x, p.y);
	GetClientRect(gfx.hStatusBar, &statusrect);
	dst.bottom -= statusrect.bottom;
#else
	HWND hWnd = GetActiveWindow();

	GetClientRect(hWnd, &dst);
	ClientToScreen(hWnd, &p);
	OffsetRect(&dst, p.x, p.y);
#endif
}

EXPORT void CALL ProcessDList(void)
{
	if (!ProcessDListShown)
	{
		popmessage("ProcessDList - not supported");
		ProcessDListShown = 1;
	}
}

EXPORT void CALL ProcessRDPList(void)
{
	rdp_process_list();
}

EXPORT void CALL RomClosed(void)
{
	rdp_close();
	if (lpddsback != NULL)
	{
		IDirectDrawSurface_Release(lpddsback);
		lpddsback = NULL;
	}
	if (lpddsprimary != NULL)
	{
		IDirectDrawSurface_Release(lpddsprimary);
		lpddsprimary = NULL;
	}
	if (lpdd != NULL)
	{
		IDirectDraw_Release(lpdd);
		lpdd = NULL;
	}
#if !(PJ64_PLUGIN_API)
	if (window != NULL)
	{
		SDL_DestroyWindow(window);
		window = NULL;
	}
#endif
	command_counter = 0;
}

#if (PJ64_PLUGIN_API)
EXPORT void CALL RomOpen(void)
#else
EXPORT int CALL RomOpen(void)
#endif
{
#if !(PJ64_PLUGIN_API)
	// clean up any existing resources, just in case
	if (window != NULL)
	{
		SDL_DestroyWindow(window);
		window = NULL;
	}
	window = SDL_CreateWindow(
		"mupen64plus-rdp-angrylion",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_width, screen_height,
		SDL_WINDOW_SHOWN// | SDL_WINDOW_RESIZABLE
	);
	if (window == NULL)
	{
		fatalerror("SDL_CreateWindow failed: %s", SDL_GetError());
		goto romopen_error;
	}
	HWND hWnd = GetActiveWindow();

	RECT bigrect, smallrect;

	GetWindowRect(hWnd,&bigrect);
	GetClientRect(hWnd,&smallrect);
	int rightdiff = screen_width - smallrect.right;
	int bottomdiff = screen_height - smallrect.bottom;

	MoveWindow(hWnd, bigrect.left, bigrect.top, bigrect.right - bigrect.left + rightdiff, bigrect.bottom - bigrect.top + bottomdiff, TRUE);
#else	// (PJ64_PLUGIN_API)
	RECT bigrect, smallrect, statusrect;

	GetWindowRect(gfx.hWnd,&bigrect);
	GetClientRect(gfx.hWnd,&smallrect);
	int rightdiff = screen_width - smallrect.right;
	int bottomdiff = screen_height - smallrect.bottom;
	if (gfx.hStatusBar)
	{
		GetClientRect(gfx.hStatusBar, &statusrect);
		bottomdiff += statusrect.bottom;
	}
	MoveWindow(gfx.hWnd, bigrect.left, bigrect.top, bigrect.right - bigrect.left + rightdiff, bigrect.bottom - bigrect.top + bottomdiff, TRUE);
#endif	// (PJ64_PLUGIN_API)

	DDPIXELFORMAT ftpixel;
	LPDIRECTDRAWCLIPPER lpddcl;

#ifdef __cplusplus
	res = DirectDrawCreateEx(NULL, (LPVOID*)&lpdd, IID_IDirectDraw7, NULL);
#else
	res = DirectDrawCreateEx(NULL, (LPVOID*)&lpdd, &IID_IDirectDraw7, NULL);
#endif
	if(res != DD_OK)
	{
		fatalerror("Couldn't create a DirectDraw object");
		goto romopen_error;
	}

#if (PJ64_PLUGIN_API)
	res = IDirectDraw_SetCooperativeLevel(lpdd, gfx.hWnd, DDSCL_NORMAL);
#else
	res = IDirectDraw_SetCooperativeLevel(lpdd, hWnd, DDSCL_NORMAL);
#endif
	if(res != DD_OK)
	{
		fatalerror("Couldn't set a cooperative level. Error code %x", res);
		goto romopen_error;
	}

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

	res = IDirectDraw_CreateSurface(lpdd, &ddsd, &lpddsprimary, NULL);
	if(res != DD_OK)
	{
		fatalerror("CreateSurface for a primary surface failed. Error code %x", res);
		goto romopen_error;
	}

	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	ddsd.dwWidth = PRESCALE_WIDTH;
	ddsd.dwHeight = PRESCALE_HEIGHT;
	memset(&ftpixel, 0, sizeof(ftpixel));
	ftpixel.dwSize = sizeof(ftpixel);
	ftpixel.dwFlags = DDPF_RGB;
	ftpixel.dwRGBBitCount = 32;
	ftpixel.dwRBitMask = 0xff0000;
	ftpixel.dwGBitMask = 0xff00;
	ftpixel.dwBBitMask = 0xff;
	ddsd.ddpfPixelFormat = ftpixel;
	res = IDirectDraw_CreateSurface(lpdd, &ddsd, &lpddsback, NULL);
	if (res == DDERR_INVALIDPIXELFORMAT)
	{
		fatalerror("ARGB8888 is not supported. You can try changing desktop color depth to 32-bit, but most likely that won't help.");
		goto romopen_error;
	}
	else if(res != DD_OK)
	{
		fatalerror("CreateSurface for a secondary surface failed. Error code %x", res);
		goto romopen_error;
	}

	res = IDirectDrawSurface_GetSurfaceDesc(lpddsback, &ddsd);
	if (res != DD_OK)
	{
		fatalerror("GetSurfaceDesc failed.");
		goto romopen_error;
	}
	if ((ddsd.lPitch & 3) || ddsd.lPitch < (PRESCALE_WIDTH << 2))
	{
		fatalerror("Pitch of a secondary surface is either not 32 bit aligned or two small.");
		goto romopen_error;
	}
	pitchindwords = ddsd.lPitch >> 2;

	res = IDirectDraw_CreateClipper(lpdd, 0, &lpddcl, NULL);
	if (res != DD_OK)
	{
		fatalerror("Couldn't create a clipper.");
		goto romopen_error;
	}
#if (PJ64_PLUGIN_API)
	res = IDirectDrawClipper_SetHWnd(lpddcl, 0, gfx.hWnd);
#else
	res = IDirectDrawClipper_SetHWnd(lpddcl, 0, hWnd);
#endif
	if (res != DD_OK)
	{
		fatalerror("Couldn't register a windows handle as a clipper.");
		goto romopen_error;
	}
	res = IDirectDrawSurface_SetClipper(lpddsprimary, lpddcl);
	if (res != DD_OK)
	{
		fatalerror("Couldn't attach a clipper to a surface.");
		goto romopen_error;
	}

	src.top = src.left = 0;
	src.bottom = 0;
	src.right = PRESCALE_WIDTH;

	POINT p;
	p.x = p.y = 0;
#if (PJ64_PLUGIN_API)
	GetClientRect(gfx.hWnd, &dst);
	ClientToScreen(gfx.hWnd, &p);
	OffsetRect(&dst, p.x, p.y);
	GetClientRect(gfx.hStatusBar, &statusrect);
	dst.bottom -= statusrect.bottom;
#else
	GetClientRect(hWnd, &dst);
	ClientToScreen(hWnd, &p);
	OffsetRect(&dst, p.x, p.y);
#endif

	DDBLTFX ddbltfx;
	ddbltfx.dwSize = sizeof(DDBLTFX);
	ddbltfx.dwFillColor = 0;
	res = IDirectDrawSurface_Blt(lpddsprimary, &dst, NULL, 0, DDBLT_COLORFILL | DDBLT_WAIT, &ddbltfx);
	src.bottom = PRESCALE_HEIGHT;
	res = IDirectDrawSurface_Blt(lpddsback, &src, NULL, 0, DDBLT_COLORFILL | DDBLT_WAIT, &ddbltfx);

	if (rdp_init() != 0)
	{
		fatalerror("rdp_init failed");
		goto romopen_error;
	}

#if (PJ64_PLUGIN_API)
	// no return value here, just exit
romopen_error:
	return;
#else
	return 1;
romopen_error:
	return 0;
#endif
}

EXPORT void CALL ShowCFB(void)
{
	popmessage("ShowCFB called");
	rdp_update();
}

EXPORT void CALL UpdateScreen(void)
{
	rdp_update();
}

EXPORT void CALL ViStatusChanged(void)
{
}

EXPORT void CALL ViWidthChanged(void)
{
}

#if (PJ64_PLUGIN_API)

EXPORT void CALL FBWrite(DWORD addr, DWORD size)
{
}

EXPORT void CALL FBWList(FrameBufferModifyEntry *plist, DWORD size)
{
}

EXPORT void CALL FBRead(DWORD addr)
{
}

EXPORT void CALL FBGetFrameBufferInfo(void *pinfo)
{
}

#else	// !(PJ64_PLUGIN_API)

EXPORT void CALL FBWrite(unsigned int addr, unsigned int size)
{
}

EXPORT void CALL FBRead(unsigned int addr)
{
}

EXPORT void CALL FBGetFrameBufferInfo(void *p)
{
}

#endif	// !(PJ64_PLUGIN_API)
