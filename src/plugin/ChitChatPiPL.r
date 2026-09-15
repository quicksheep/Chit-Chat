#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
	#include "AE_General.r"
#endif

resource 'PiPL' (16000) {
	{
		Kind { AEEffect },
		Name { "Chit Chat" },
		Category { "Text" },

#ifdef AE_OS_WIN
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 { "EntryPointFunc" },
	#elif defined(AE_PROC_ARM64)
		CodeWinARM64 { "EntryPointFunc" },
	#endif
#else
	#ifdef AE_PROC_INTELx64
		CodeMacIntel64 { "EntryPointFunc" },
	#endif
	#ifdef AE_PROC_ARM64
		CodeMacARM64 { "EntryPointFunc" },
	#endif
#endif

		AE_PiPL_Version { 2, 0 },
		AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
		/* PF_VERSION(1, 0, 0, PF_Stage_RELEASE, 2) — must match ChitChat.h + GlobalSetup */
		AE_Effect_Version { 525828 },
		AE_Effect_Info_Flags { 0 },
		/* Must match GlobalSetup: DEEP_COLOR | NON_PARAM_VARY | WIDE_TIME | SEND_UPDATE_PARAMS_UI */
		AE_Effect_Global_OutFlags { 100663302 },
		/* SMART_RENDER | FLOAT_COLOR | I_MIX_GUID | GET_FLATTENED | THREADED | MUTABLE_SEQ */
		AE_Effect_Global_OutFlags_2 { 413144064 },
		AE_Effect_Match_Name { "ChitChat" },
		AE_Reserved_Info { 0 }
	}
};
