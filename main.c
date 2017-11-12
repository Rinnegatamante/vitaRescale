#include <libk/string.h>
#include <libk/stdio.h>
#include <libk/ctype.h>
#include <vitasdk.h>
#include <taihen.h>
#include "renderer.h"

#define HOOKS_NUM 1       // Hooked functions num
#define RES_NUM   4       // Available resolutions

static uint8_t current_hook;
static SceUID hooks[HOOKS_NUM];
static tai_hook_ref_t refs[HOOKS_NUM];
static int timer = 0;
static char titleid[16];
static uint16_t w[RES_NUM] = {480, 640, 720, 960};
static uint16_t h[RES_NUM] = {272, 368, 408, 544};
static uint8_t i = 0;
static uint32_t oldpad;
static uint32_t offset = 0xDEADBEEF;
tai_module_info_t info = {0};

// Simplified generic hooking function
void hookFunction(uint32_t nid, const void *func){
	hooks[current_hook] = taiHookFunctionImport(&refs[current_hook],TAI_MAIN_MODULE,TAI_ANY_LIBRARY,nid,func);
	current_hook++;
}

// Patch game resolution
void patchResolution(uint32_t addr, uint16_t w, uint16_t h){
	taiInjectData(info.modid, 1, addr, &w, sizeof(uint16_t));
	taiInjectData(info.modid, 1, addr+4, &h, sizeof(uint16_t));
	timer = 200;
	offset = addr;
}

int sceDisplaySetFrameBuf_patched(const SceDisplayFrameBuf *pParam, int sync) {
	
	// Updating renderer status
	updateFramebuf(pParam);
	
	// Checking input
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);
	if (offset != 0xDEADBEEF){
		if ((pad.buttons & SCE_CTRL_LTRIGGER) && (pad.buttons & SCE_CTRL_CROSS) && (pad.buttons & SCE_CTRL_TRIANGLE)){
			if (!((oldpad & SCE_CTRL_LTRIGGER) && (oldpad & SCE_CTRL_CROSS) && (oldpad & SCE_CTRL_TRIANGLE))){
				i = (i + 1) % RES_NUM;
				patchResolution(offset, w[i], h[i]);
			}
		}
	}
	
	// Drawing info on screen
	if (timer > 0){
		setTextColor(0x00FFFFFF);
		drawStringF(5, 5, "Resolution set at %hux%hu.", w[i], h[i]);
		drawStringF(5, 25, "SetFrameBuf Resolution: %dx%d.", pParam->width, pParam->height);
		timer--;
	}
	
	oldpad = pad.buttons;
	return TAI_CONTINUE(int, refs[0], pParam, sync);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
	
	// Getting eboot.bin info
	info.size = sizeof(tai_module_info_t);
	taiGetModuleInfo(TAI_MAIN_MODULE, &info);
	
	// Checking if game is patchable
	sceAppMgrAppParamGetString(0, 12, titleid , 256);
	if (strncmp(titleid, "PCSB00048", 9) == 0) patchResolution(0x53E0, w[i], h[i]);      // Ridge Racer (EU)
	else if (strncmp(titleid, "PCSB00861", 9) == 0) patchResolution(0x77CC, w[i], h[i]); // Digimon Story: Cybersleuth (EU)
	
	hookFunction(0x7A410B64, sceDisplaySetFrameBuf_patched);
	
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
	
	// Freeing hooks
	while (current_hook-- > 0){
		taiHookRelease(hooks[current_hook], refs[current_hook]);
	}
	
	return SCE_KERNEL_STOP_SUCCESS;	
}