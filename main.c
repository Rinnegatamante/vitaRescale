#include <libk/string.h>
#include <libk/stdio.h>
#include <libk/ctype.h>
#include <vitasdk.h>
#include <taihen.h>
#include "renderer.h"

#define HOOKS_NUM 1       // Hooked functions num

static uint8_t current_hook;
static SceUID hooks[HOOKS_NUM], fd;
static tai_hook_ref_t refs[HOOKS_NUM];
static int timer = 0;
static char buf[128], fname[128], titleid[16];
static uint16_t w3d, h3d, wfb, hfb;
tai_module_info_t info = {0};

// Simplified generic hooking function
void hookFunction(uint32_t nid, const void *func){
	hooks[current_hook] = taiHookFunctionImport(&refs[current_hook],TAI_MAIN_MODULE,TAI_ANY_LIBRARY,nid,func);
	current_hook++;
}

// Patch game resolution functions
void patchPlainResolution(int seg, uint32_t addr, uint16_t w, uint16_t h){ // Plain resolution value
	if ((!w) || (!h)) return;
	taiInjectData(info.modid, seg, addr, &w, sizeof(uint16_t));
	taiInjectData(info.modid, seg, addr+4, &h, sizeof(uint16_t));
	timer = 200;
}

int sceDisplaySetFrameBuf_patched(const SceDisplayFrameBuf *pParam, int sync) {
	
	// Updating renderer status
	updateFramebuf(pParam);

	// Drawing info on screen
	if (timer > 0){
		setTextColor(0x00FFFFFF);
		if (w3d) drawStringF(5, 5, "3D Rendering: %hux%hu.", w3d, h3d);
		else drawStringF(5, 5, "3D Rendering: Native");
		if (wfb) drawStringF(5, 25, "Base Rendering: %hux%hu.", wfb, hfb);
		else drawStringF(5, 25, "Base Rendering: Native");
		timer--;
	}
	
	return TAI_CONTINUE(int, refs[0], pParam, sync);
}

int sceDisplaySetFrameBuf_patched2(const SceDisplayFrameBuf *pParam, int sync) {
	
	// Updating renderer status
	updateFramebuf(pParam);

	// Drawing info on screen
	setTextColor(0x00FFFFFF);
	drawStringF(5, 5, "ERROR: 0x%08X", fd);
	
	return TAI_CONTINUE(int, refs[0], pParam, sync);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
	
	// Getting eboot.bin info
	info.size = sizeof(tai_module_info_t);
	taiGetModuleInfo(TAI_MAIN_MODULE, &info);
	
	// Getting desired resolution from config file
	sceAppMgrAppParamGetString(0, 12, titleid , 256);
	sprintf(fname, "app0:/vitaRescale.txt");
	fd = sceIoOpen(fname, SCE_O_RDONLY, 0777);
	if (fd >= 0){
		int nbytes = sceIoRead(fd, buf, 128);
		buf[nbytes] = 0;
		sceIoClose(fd);
		sscanf(buf,"%hux%hu;%hux%hu", &wfb, &hfb, &w3d, &h3d);
	
		// Checking if game is patchable
		if (strncmp(titleid, "PCSB00048", 9) == 0){       // Ridge Racer (EU)
			patchPlainResolution(1, 0x53E0, w3d, h3d);      // 3D Rendering
			patchPlainResolution(1, 0x54A8, wfb, hfb);      // SetFrameBuf params
			patchPlainResolution(0, 0x1E7538, wfb, hfb);    // ? (960x544 on retail game)
		}else if (strncmp(titleid, "PCSB00245", 9) == 0){ // Persona 4 Golden (EU)
			patchPlainResolution(1, 0xDBCFC, wfb, hfb);
			w3d = wfb;
			h3d = hfb;
		}else if (strncmp(titleid, "PCSE00120", 9) == 0){ // Persona 4 Golden (US)
			patchPlainResolution(1, 0xDBCEC, wfb, hfb);
			w3d = wfb;
			h3d = hfb;
		}else if (strncmp(titleid, "PCSG00563", 9) == 0){ // Persona 4 Golden (JP)
			patchPlainResolution(1, 0xDBD9C, wfb, hfb);
			w3d = wfb;
			h3d = hfb;
		}else if (strncmp(titleid, "PCSB00861", 9) == 0){ // Digimon Story: Cybersleuth (EU)
			patchPlainResolution(1, 0x77CC, wfb, hfb);      // Full Rendering
			w3d = wfb;
			h3d = hfb;
		}
	
		hookFunction(0x7A410B64, sceDisplaySetFrameBuf_patched);
	}else hookFunction(0x7A410B64, sceDisplaySetFrameBuf_patched2);
	
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
	
	// Freeing hooks
	while (current_hook-- > 0){
		taiHookRelease(hooks[current_hook], refs[current_hook]);
	}
	
	return SCE_KERNEL_STOP_SUCCESS;	
}
