#pragma once

#define PF_DEEP_COLOR_AWARE 1

#include "AEConfig.h"

#ifdef AE_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEGP_SuiteHandler.h"
#include "AEFX_SuiteHelper.h"
#include "Smart_Utils.h"
#include "String_Utils.h"

#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUG_VERSION 0
#define STAGE_VERSION PF_Stage_RELEASE
#define BUILD_VERSION 1

enum {
	CHITCHAT_INPUT = 0,
	CHITCHAT_APPEAR,
	CHITCHAT_EDIT,
	CHITCHAT_FADE,
	CHITCHAT_OFFSET,
	CHITCHAT_RADIUS,
	CHITCHAT_PADDING,
	CHITCHAT_SPACING,
	CHITCHAT_MAXW,
	CHITCHAT_MARGINX,
	CHITCHAT_MARGINY,
	CHITCHAT_FONT,
	CHITCHAT_FONTSIZE,
	CHITCHAT_BOLD,
	CHITCHAT_YOU_BUBBLE,
	CHITCHAT_YOU_TEXT,
	CHITCHAT_THEM_BUBBLE,
	CHITCHAT_THEM_TEXT,
	CHITCHAT_NUM_PARAMS
};

enum {
	CHITCHAT_DISK_APPEAR = 1,
	CHITCHAT_DISK_EDIT,
	CHITCHAT_DISK_FADE,
	CHITCHAT_DISK_OFFSET,
	CHITCHAT_DISK_RADIUS,
	CHITCHAT_DISK_PADDING,
	CHITCHAT_DISK_SPACING,
	CHITCHAT_DISK_MAXW,
	CHITCHAT_DISK_MARGINX,
	CHITCHAT_DISK_MARGINY,
	CHITCHAT_DISK_FONT,
	CHITCHAT_DISK_FONTSIZE,
	CHITCHAT_DISK_BOLD,
	CHITCHAT_DISK_YOU_BUBBLE,
	CHITCHAT_DISK_YOU_TEXT,
	CHITCHAT_DISK_THEM_BUBBLE,
	CHITCHAT_DISK_THEM_TEXT
};

#ifdef __cplusplus
extern "C" {
#endif

DllExport PF_Err
EntryPointFunc(PF_Cmd cmd,
			   PF_InData* in_data,
			   PF_OutData* out_data,
			   PF_ParamDef* params[],
			   PF_LayerDef* output,
			   void* extra);

#ifdef __cplusplus
}
#endif
