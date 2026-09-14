#include "ChitChat.h"
#include "SPBasic.h"

#include "../core/AppearTimeline.h"
#include "../core/ChatArb.h"
#include "../core/ChatLayout.h"
#include "../core/MessageParse.h"
#include "../win/ChatRenderer.h"
#include "../win/TranscriptDialog.h"
#include "Composite.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace {

std::mutex g_renderer_mutex;
chitchat::ChatRenderer g_renderer;

struct SequenceData {
	chitchat::ArbBlock messages;
};

struct LockedMessages {
	PF_Handle handle = nullptr;
	chitchat::ArbBlock* block = nullptr;
};

struct PreRenderPack {
	chitchat::ArbBlock messages;
};

static void DeletePreRenderPack(void* p) {
	delete static_cast<PreRenderPack*>(p);
}

static bool CopyValidArb(const void* p, chitchat::ArbBlock* out) {
	if (!p || !out) {
		return false;
	}
	const auto* block = reinterpret_cast<const chitchat::ArbBlock*>(p);
	if (!chitchat::ValidateArb(*block)) {
		return false;
	}
	*out = *block;
	return true;
}

static bool CopyArbFromHandle(PF_Handle h, AEGP_SuiteHandler* suites, chitchat::ArbBlock* out) {
	if (!h || !out) {
		return false;
	}
	if (suites) {
		void* p = suites->HandleSuite1()->host_lock_handle(h);
		if (p) {
			const bool ok = CopyValidArb(p, out);
			suites->HandleSuite1()->host_unlock_handle(h);
			if (ok) {
				return true;
			}
		}
	}
	if (*h && CopyValidArb(*h, out)) {
		return true;
	}
	return false;
}

static PF_ConstHandle GetConstSequenceHandle(PF_InData* in_data) {
	if (!in_data->pica_basicP) {
		return nullptr;
	}
	const void* suiteP = nullptr;
	if (in_data->pica_basicP->AcquireSuite(kPFEffectSequenceDataSuite, kPFEffectSequenceDataSuiteVersion1, &suiteP) != kSPNoError || !suiteP) {
		return nullptr;
	}
	auto* seq_suite = static_cast<const PF_EffectSequenceDataSuite1*>(suiteP);
	PF_ConstHandle const_h = nullptr;
	if (seq_suite->PF_GetConstSequenceData) {
		seq_suite->PF_GetConstSequenceData(in_data->effect_ref, &const_h);
	}
	in_data->pica_basicP->ReleaseSuite(kPFEffectSequenceDataSuite, kPFEffectSequenceDataSuiteVersion1);
	return const_h;
}

// UI thread: in_data->sequence_data. Threaded Smart Render: that pointer is NULL.
static bool LoadMessageBlock(PF_InData* in_data, AEGP_SuiteHandler* suites, chitchat::ArbBlock* out) {
	if (!out) {
		return false;
	}
	chitchat::ClearArb(out);
	if (in_data->sequence_data && CopyArbFromHandle(in_data->sequence_data, suites, out)) {
		return true;
	}
	PF_ConstHandle const_h = GetConstSequenceHandle(in_data);
	if (!const_h) {
		return false;
	}
	auto as_handle = reinterpret_cast<PF_Handle>(const_cast<void*>(static_cast<const void*>(const_h)));
	if (CopyArbFromHandle(as_handle, suites, out)) {
		return true;
	}
	if (CopyValidArb(const_h, out)) {
		return true;
	}
	const void* poked = *reinterpret_cast<const void* const*>(const_h);
	return poked && CopyValidArb(poked, out);
}

static LockedMessages LockMessages(PF_InData* in_data, AEGP_SuiteHandler* suites) {
	LockedMessages locked;
	locked.handle = in_data->sequence_data;
	if (!locked.handle) {
		return locked;
	}
	auto* seq = reinterpret_cast<SequenceData*>(suites->HandleSuite1()->host_lock_handle(locked.handle));
	if (!seq) {
		locked.handle = nullptr;
		return locked;
	}
	locked.block = &seq->messages;
	return locked;
}

static void UnlockMessages(AEGP_SuiteHandler* suites, const LockedMessages& locked) {
	if (locked.handle) {
		suites->HandleSuite1()->host_unlock_handle(locked.handle);
	}
}

static PF_Err SequenceSetup(PF_InData* in_data, PF_OutData* out_data) {
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	PF_Handle h = suites.HandleSuite1()->host_new_handle(sizeof(SequenceData));
	if (!h) {
		return PF_Err_OUT_OF_MEMORY;
	}
	auto* seq = reinterpret_cast<SequenceData*>(suites.HandleSuite1()->host_lock_handle(h));
	if (!seq) {
		suites.HandleSuite1()->host_dispose_handle(h);
		return PF_Err_OUT_OF_MEMORY;
	}
	chitchat::DefaultArb(&seq->messages);
	suites.HandleSuite1()->host_unlock_handle(h);
	out_data->sequence_data = h;
	return PF_Err_NONE;
}

static PF_Err SequenceSetdown(PF_InData* in_data, PF_OutData* out_data) {
	if (in_data->sequence_data) {
		AEGP_SuiteHandler suites(in_data->pica_basicP);
		suites.HandleSuite1()->host_dispose_handle(in_data->sequence_data);
	}
	out_data->sequence_data = nullptr;
	return PF_Err_NONE;
}

static PF_Err SequenceResetup(PF_InData* in_data, PF_OutData* out_data) {
	out_data->sequence_data = in_data->sequence_data;
	if (!out_data->sequence_data) {
		return SequenceSetup(in_data, out_data);
	}
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	auto* seq = reinterpret_cast<SequenceData*>(suites.HandleSuite1()->host_lock_handle(out_data->sequence_data));
	// Never DefaultArb here: a validate miss must not wipe user transcripts.
	if (seq) {
		suites.HandleSuite1()->host_unlock_handle(out_data->sequence_data);
	}
	return PF_Err_NONE;
}

static PF_Err GetFlattenedSequenceData(PF_InData* in_data, PF_OutData* out_data) {
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	if (!in_data->sequence_data) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	auto* src = reinterpret_cast<SequenceData*>(suites.HandleSuite1()->host_lock_handle(in_data->sequence_data));
	if (!src) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	PF_Handle h = suites.HandleSuite1()->host_new_handle(sizeof(SequenceData));
	if (!h) {
		suites.HandleSuite1()->host_unlock_handle(in_data->sequence_data);
		return PF_Err_OUT_OF_MEMORY;
	}
	auto* dst = reinterpret_cast<SequenceData*>(suites.HandleSuite1()->host_lock_handle(h));
	if (!dst) {
		suites.HandleSuite1()->host_dispose_handle(h);
		suites.HandleSuite1()->host_unlock_handle(in_data->sequence_data);
		return PF_Err_OUT_OF_MEMORY;
	}
	*dst = *src;
	suites.HandleSuite1()->host_unlock_handle(h);
	suites.HandleSuite1()->host_unlock_handle(in_data->sequence_data);
	out_data->sequence_data = h;
	return PF_Err_NONE;
}

static PF_Err About(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* /*params*/[], PF_LayerDef* /*output*/) {
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg,
										  "Chit Chat v%d.%d\rMessage bubbles for After Effects.\rApply to a solid, edit messages, and keyframe Appear ID.",
										  MAJOR_VERSION, MINOR_VERSION);
	return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData* /*in_data*/, PF_OutData* out_data, PF_ParamDef* /*params*/[], PF_LayerDef* /*output*/) {
	out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION, STAGE_VERSION, BUILD_VERSION);
	out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE |
						  PF_OutFlag_NON_PARAM_VARY |
						  PF_OutFlag_WIDE_TIME_INPUT |
						  PF_OutFlag_SEND_UPDATE_PARAMS_UI;
	out_data->out_flags2 = PF_OutFlag2_SUPPORTS_SMART_RENDER |
						   PF_OutFlag2_FLOAT_COLOR_AWARE |
						   PF_OutFlag2_SUPPORTS_THREADED_RENDERING |
						   PF_OutFlag2_SUPPORTS_GET_FLATTENED_SEQUENCE_DATA |
						   PF_OutFlag2_I_MIX_GUID_DEPENDENCIES |
						   PF_OutFlag2_MUTABLE_RENDER_SEQUENCE_DATA_SLOWER;
	return PF_Err_NONE;
}

static PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* /*params*/[], PF_LayerDef* /*output*/) {
	PF_Err err = PF_Err_NONE;
	PF_ParamDef def;

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Appear ID",
						 0, 64, 0, 64, 1,
						 PF_Precision_INTEGER,
						 0,
						 PF_ParamFlag_CANNOT_INTERP,
						 CHITCHAT_DISK_APPEAR);

	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON("Transcript", "Edit Messages...", 0, PF_ParamFlag_SUPERVISE, CHITCHAT_DISK_EDIT);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Fade In Time",
						 0, 5, 0, 2, chitchat::kDefaultFadeSec,
						 PF_Precision_HUNDREDTHS,
						 0,
						 0,
						 CHITCHAT_DISK_FADE);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Appear Offset",
						 0, 400, 0, 120, chitchat::kDefaultOffsetPx,
						 PF_Precision_HUNDREDTHS,
						 0,
						 0,
						 CHITCHAT_DISK_OFFSET);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Corner Radius",
						 0, 200, 0, 80, chitchat::kDefaultRadius,
						 PF_Precision_HUNDREDTHS, 0, 0, CHITCHAT_DISK_RADIUS);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Padding",
						 0, 80, 0, 40, chitchat::kDefaultPadding,
						 PF_Precision_HUNDREDTHS, 0, 0, CHITCHAT_DISK_PADDING);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Bubble Spacing",
						 0, 200, 0, 60, chitchat::kDefaultSpacing,
						 PF_Precision_HUNDREDTHS, 0, 0, CHITCHAT_DISK_SPACING);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Max Bubble Width %",
						 20, 100, 30, 90, chitchat::kDefaultMaxWidthPct,
						 PF_Precision_HUNDREDTHS, 0, 0, CHITCHAT_DISK_MAXW);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Side Margin",
						 0, 200, 0, 80, chitchat::kDefaultMarginX,
						 PF_Precision_HUNDREDTHS, 0, 0, CHITCHAT_DISK_MARGINX);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Bottom Margin",
						 0, 400, 0, 80, chitchat::kDefaultMarginY,
						 PF_Precision_HUNDREDTHS, 0, 0, CHITCHAT_DISK_MARGINY);

	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUPX("Font", 6, 1, "Segoe UI|Arial|Calibri|Roboto|Georgia|Courier New", 0, CHITCHAT_DISK_FONT);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Font Size",
						 6, 128, 8, 48, chitchat::kDefaultFontSize,
						 PF_Precision_HUNDREDTHS, 0, 0, CHITCHAT_DISK_FONTSIZE);

	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX("Bold", "Bold", FALSE, 0, CHITCHAT_DISK_BOLD);

	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("You Bubble", 0, 122, 255, CHITCHAT_DISK_YOU_BUBBLE);
	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("You Text", 255, 255, 255, CHITCHAT_DISK_YOU_TEXT);
	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("Them Bubble", 229, 229, 234, CHITCHAT_DISK_THEM_BUBBLE);
	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("Them Text", 0, 0, 0, CHITCHAT_DISK_THEM_TEXT);

	out_data->num_params = CHITCHAT_NUM_PARAMS;
	return err;
}

static PF_Err UserChangedParam(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* /*params*/[], PF_LayerDef* /*output*/, const PF_UserChangedParamExtra* extra) {
	if (!extra || extra->param_index != CHITCHAT_EDIT) {
		return PF_Err_NONE;
	}

	AEGP_SuiteHandler suites(in_data->pica_basicP);
	std::vector<chitchat::Message> messages;
	LockedMessages locked = LockMessages(in_data, &suites);
	if (locked.block) {
		chitchat::ArbToMessages(*locked.block, &messages);
		UnlockMessages(&suites, locked);
	}
	std::string transcript = chitchat::FormatTranscript(messages);
	std::string error;
	if (!chitchat::EditTranscriptDialog(nullptr, &transcript, &error)) {
		return PF_Err_NONE;
	}
	std::vector<chitchat::Message> parsed;
	if (!chitchat::ParseTranscript(transcript, &parsed, &error)) {
		return PF_Err_NONE;
	}
	locked = LockMessages(in_data, &suites);
	if (!locked.block) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	chitchat::MessagesToArb(parsed, locked.block);
	UnlockMessages(&suites, locked);
	out_data->sequence_data = in_data->sequence_data;
	out_data->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
	return PF_Err_NONE;
}

static int AppearIdFromParam(const PF_ParamDef& def) {
	const int v = static_cast<int>(std::floor(def.u.fs_d.value + 0.5));
	if (v < 0) {
		return 0;
	}
	if (v > chitchat::kMaxAppearId) {
		return chitchat::kMaxAppearId;
	}
	return v;
}

static int CheckoutAppearId(PF_InData* in_data, A_long time) {
	PF_ParamDef def;
	AEFX_CLR_STRUCT(def);
	if (PF_CHECKOUT_PARAM(in_data, CHITCHAT_APPEAR, time, in_data->time_step, in_data->time_scale, &def) != PF_Err_NONE) {
		return 0;
	}
	const int v = AppearIdFromParam(def);
	PF_CHECKIN_PARAM(in_data, &def);
	return v;
}

static double FindAppearTimeSec(PF_InData* in_data, int message_id, int current_id, std::map<A_long, int>* cache) {
	if (message_id <= 0 || current_id < message_id || !cache) {
		return -1.0;
	}
	const A_long now = std::max<A_long>(0, in_data->current_time);
	const A_long step = std::max<A_long>(1, in_data->time_step);
	const double scale = in_data->time_scale > 0 ? static_cast<double>(in_data->time_scale) : 1.0;

	auto get = [&](A_long t) {
		t = std::max<A_long>(0, t);
		auto it = cache->find(t);
		if (it != cache->end()) {
			return it->second;
		}
		const int v = CheckoutAppearId(in_data, t);
		(*cache)[t] = v;
		return v;
	};

	if (get(now) < message_id) {
		return -1.0;
	}
	if (get(0) >= message_id) {
		return 0.0;
	}

	A_long lo = 0;
	A_long hi = now;
	const A_long max_long = (std::numeric_limits<A_long>::max)();
	int iters = 0;
	while (hi > lo && (hi - lo) > step && iters < chitchat::kMaxAppearSearchIters) {
		++iters;
		A_long mid = lo + (hi - lo) / 2;
		if (step > 1) {
			mid = (mid / step) * step;
		}
		if (mid <= lo) {
			if (lo > max_long - step) {
				break;
			}
			mid = lo + step;
		}
		if (mid >= hi) {
			break;
		}
		if (get(mid) >= message_id) {
			hi = mid;
		} else {
			lo = mid;
		}
	}
	if (get(hi) >= message_id) {
		return static_cast<double>(hi) / scale;
	}
	// Visible at current time (slider or keyframe) but first-crossing search failed.
	return 0.0;
}

static float Chan8(A_u_char c) {
	return c / 255.0f;
}

static PF_Err RenderChat(PF_InData* in_data, PF_EffectWorld* input, PF_EffectWorld* output, const chitchat::ArbBlock* pre_messages) {
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	if (input && output) {
		ERR(suites.WorldTransformSuite1()->copy(in_data->effect_ref, input, output, nullptr, nullptr));
	}
	if (err) {
		return err;
	}

	PF_ParamDef p_fade, p_off, p_rad, p_pad, p_spc, p_maxw, p_mx, p_my, p_font, p_fs, p_bold;
	PF_ParamDef p_yb, p_yt, p_tb, p_tt, p_appear;
	AEFX_CLR_STRUCT(p_fade);
	AEFX_CLR_STRUCT(p_off);
	AEFX_CLR_STRUCT(p_rad);
	AEFX_CLR_STRUCT(p_pad);
	AEFX_CLR_STRUCT(p_spc);
	AEFX_CLR_STRUCT(p_maxw);
	AEFX_CLR_STRUCT(p_mx);
	AEFX_CLR_STRUCT(p_my);
	AEFX_CLR_STRUCT(p_font);
	AEFX_CLR_STRUCT(p_fs);
	AEFX_CLR_STRUCT(p_bold);
	AEFX_CLR_STRUCT(p_yb);
	AEFX_CLR_STRUCT(p_yt);
	AEFX_CLR_STRUCT(p_tb);
	AEFX_CLR_STRUCT(p_tt);
	AEFX_CLR_STRUCT(p_appear);

	const A_long t = in_data->current_time;
	const A_long ts = in_data->time_step;
	const A_long sc = in_data->time_scale;

	PF_ParamDef* checked[17] = {};
	int n_checked = 0;
	auto checkout = [&](PF_ParamDef* def, A_long param_index) {
		if (err) {
			return;
		}
		err = PF_CHECKOUT_PARAM(in_data, param_index, t, ts, sc, def);
		if (!err && n_checked < 17) {
			checked[n_checked++] = def;
		}
	};
	checkout(&p_fade, CHITCHAT_FADE);
	checkout(&p_off, CHITCHAT_OFFSET);
	checkout(&p_rad, CHITCHAT_RADIUS);
	checkout(&p_pad, CHITCHAT_PADDING);
	checkout(&p_spc, CHITCHAT_SPACING);
	checkout(&p_maxw, CHITCHAT_MAXW);
	checkout(&p_mx, CHITCHAT_MARGINX);
	checkout(&p_my, CHITCHAT_MARGINY);
	checkout(&p_font, CHITCHAT_FONT);
	checkout(&p_fs, CHITCHAT_FONTSIZE);
	checkout(&p_bold, CHITCHAT_BOLD);
	checkout(&p_yb, CHITCHAT_YOU_BUBBLE);
	checkout(&p_yt, CHITCHAT_YOU_TEXT);
	checkout(&p_tb, CHITCHAT_THEM_BUBBLE);
	checkout(&p_tt, CHITCHAT_THEM_TEXT);
	checkout(&p_appear, CHITCHAT_APPEAR);
	if (err) {
		for (int i = 0; i < n_checked; ++i) {
			PF_CHECKIN_PARAM(in_data, checked[i]);
		}
		return err;
	}

	const int appear_id = AppearIdFromParam(p_appear);
	std::vector<chitchat::Message> all;
	chitchat::ArbBlock block;
	chitchat::ClearArb(&block);
	bool have = pre_messages && pre_messages->count > 0 && CopyValidArb(pre_messages, &block);
	if (!have) {
		AEGP_SuiteHandler hs(in_data->pica_basicP);
		have = LoadMessageBlock(in_data, &hs, &block);
	}
	if (have) {
		chitchat::ArbToMessages(block, &all);
	}

	const float ds = (in_data->downsample_x.den != 0)
						 ? (static_cast<float>(in_data->downsample_x.num) / static_cast<float>(in_data->downsample_x.den))
						 : 1.0f;

	chitchat::StyleSettings style;
	style.fade_in_sec = static_cast<float>(p_fade.u.fs_d.value);
	style.appear_offset_px = static_cast<float>(p_off.u.fs_d.value) * ds;
	style.corner_radius = static_cast<float>(p_rad.u.fs_d.value) * ds;
	style.padding = static_cast<float>(p_pad.u.fs_d.value) * ds;
	style.spacing = static_cast<float>(p_spc.u.fs_d.value) * ds;
	style.max_width_pct = static_cast<float>(p_maxw.u.fs_d.value);
	style.margin_x = static_cast<float>(p_mx.u.fs_d.value) * ds;
	style.margin_y = static_cast<float>(p_my.u.fs_d.value) * ds;
	style.font_index = (std::max)(0, static_cast<int>(p_font.u.pd.value) - 1);
	style.font_size = static_cast<float>(p_fs.u.fs_d.value) * ds;
	style.bold = p_bold.u.bd.value != 0;
	style.you_bubble_r = Chan8(p_yb.u.cd.value.red);
	style.you_bubble_g = Chan8(p_yb.u.cd.value.green);
	style.you_bubble_b = Chan8(p_yb.u.cd.value.blue);
	style.you_text_r = Chan8(p_yt.u.cd.value.red);
	style.you_text_g = Chan8(p_yt.u.cd.value.green);
	style.you_text_b = Chan8(p_yt.u.cd.value.blue);
	style.them_bubble_r = Chan8(p_tb.u.cd.value.red);
	style.them_bubble_g = Chan8(p_tb.u.cd.value.green);
	style.them_bubble_b = Chan8(p_tb.u.cd.value.blue);
	style.them_text_r = Chan8(p_tt.u.cd.value.red);
	style.them_text_g = Chan8(p_tt.u.cd.value.green);
	style.them_text_b = Chan8(p_tt.u.cd.value.blue);

	PF_CHECKIN_PARAM(in_data, &p_fade);
	PF_CHECKIN_PARAM(in_data, &p_off);
	PF_CHECKIN_PARAM(in_data, &p_rad);
	PF_CHECKIN_PARAM(in_data, &p_pad);
	PF_CHECKIN_PARAM(in_data, &p_spc);
	PF_CHECKIN_PARAM(in_data, &p_maxw);
	PF_CHECKIN_PARAM(in_data, &p_mx);
	PF_CHECKIN_PARAM(in_data, &p_my);
	PF_CHECKIN_PARAM(in_data, &p_font);
	PF_CHECKIN_PARAM(in_data, &p_fs);
	PF_CHECKIN_PARAM(in_data, &p_bold);
	PF_CHECKIN_PARAM(in_data, &p_yb);
	PF_CHECKIN_PARAM(in_data, &p_yt);
	PF_CHECKIN_PARAM(in_data, &p_tb);
	PF_CHECKIN_PARAM(in_data, &p_tt);
	PF_CHECKIN_PARAM(in_data, &p_appear);

	if (appear_id <= 0 || all.empty() || !output) {
		return PF_Err_NONE;
	}
	if (output->width > chitchat::kMaxRenderWidth || output->height > chitchat::kMaxRenderHeight ||
		output->width <= 0 || output->height <= 0) {
		return PF_Err_NONE;
	}

	const double now_sec = (sc > 0) ? (static_cast<double>(t) / static_cast<double>(sc)) : 0.0;
	std::map<A_long, int> cache;
	struct AppearItem {
		chitchat::Message msg;
		double appear_at = 0.0;
	};
	std::vector<AppearItem> items;
	items.reserve(all.size());
	for (const auto& msg : all) {
		if (msg.id > appear_id) {
			continue;
		}
		double appear_at = FindAppearTimeSec(in_data, msg.id, appear_id, &cache);
		if (appear_at < 0.0) {
			appear_at = 0.0;
		}
		items.push_back({msg, appear_at});
	}

	chitchat::LayoutInput layout;
	layout.layer_w = static_cast<float>(output->width);
	layout.layer_h = static_cast<float>(output->height);
	layout.style = style;

	const float max_bubble = layout.layer_w * (style.max_width_pct / 100.0f);
	chitchat::BitmapBGRA overlay;
	bool drew = false;
	{
		std::lock_guard<std::mutex> lock(g_renderer_mutex);
		if (!g_renderer.Init()) {
			return PF_Err_INTERNAL_STRUCT_DAMAGED;
		}
		for (const auto& item : items) {
			chitchat::MeasuredBubble mb;
			mb.id = item.msg.id;
			mb.sender = item.msg.sender;
			if (!g_renderer.Measure(style, item.msg.text, max_bubble, &mb)) {
				mb.width = (std::min)(max_bubble, 120.0f * ds);
				mb.height = (style.font_size + style.padding * 2.0f);
				mb.text = item.msg.text;
			}
			layout.bubbles.push_back(mb);
			layout.age_sec.push_back(now_sec - item.appear_at);
		}

		const std::vector<chitchat::DrawBubble> draw = chitchat::LayoutBubbles(layout);
		if (!draw.empty() && g_renderer.Render(style, draw, output->width, output->height, &overlay)) {
			drew = true;
		}
	}

	if (drew) {
		err = chitchat::CompositeOver(in_data, output, overlay);
	}

	return err;
}

static PF_Err SmartPreRender(PF_InData* in_data, PF_OutData* /*out_data*/, PF_PreRenderExtra* extra) {
	PF_Err err = PF_Err_NONE;
	PF_RenderRequest req = extra->input->output_request;
	PF_CheckoutResult in_result;
	AEFX_CLR_STRUCT(in_result);
	req.preserve_rgb_of_zero_alpha = FALSE;
	req.rect.left = 0;
	req.rect.top = 0;
	req.rect.right = in_data->width;
	req.rect.bottom = in_data->height;
	ERR(extra->cb->checkout_layer(in_data->effect_ref, CHITCHAT_INPUT, CHITCHAT_INPUT, &req, in_data->current_time,
								  in_data->time_step, in_data->time_scale, &in_result));
	UnionLRect(&in_result.result_rect, &extra->output->result_rect);
	UnionLRect(&in_result.max_result_rect, &extra->output->max_result_rect);
	UnionLRect(&in_result.max_result_rect, &extra->output->result_rect);
	extra->output->flags = static_cast<PF_RenderOutputFlags>(extra->output->flags | PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS);

	auto* pack = new (std::nothrow) PreRenderPack();
	if (pack) {
		chitchat::ClearArb(&pack->messages);
		AEGP_SuiteHandler suites(in_data->pica_basicP);
		if (LoadMessageBlock(in_data, &suites, &pack->messages) && pack->messages.count > 0) {
			extra->output->pre_render_data = pack;
			extra->output->delete_pre_render_data_func = DeletePreRenderPack;
			if (extra->cb->GuidMixInPtr) {
				extra->cb->GuidMixInPtr(in_data->effect_ref, static_cast<A_u_long>(sizeof(pack->messages)), &pack->messages);
			}
		} else {
			delete pack;
		}
	}
	return err;
}

static PF_Err SmartRender(PF_InData* in_data, PF_OutData* /*out_data*/, PF_SmartRenderExtra* extra) {
	PF_Err err = PF_Err_NONE;
	PF_Err err2 = PF_Err_NONE;
	PF_EffectWorld* input = nullptr;
	PF_EffectWorld* output = nullptr;
	ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, CHITCHAT_INPUT, &input));
	ERR(extra->cb->checkout_output(in_data->effect_ref, &output));
	const chitchat::ArbBlock* pre = nullptr;
	if (extra->input && extra->input->pre_render_data) {
		pre = &static_cast<const PreRenderPack*>(extra->input->pre_render_data)->messages;
	}
	if (!err) {
		err = RenderChat(in_data, input, output, pre);
	}
	if (input) {
		ERR2(extra->cb->checkin_layer_pixels(in_data->effect_ref, CHITCHAT_INPUT));
	}
	return err;
}

static PF_Err RenderLegacy(PF_InData* in_data, PF_OutData* /*out_data*/, PF_ParamDef* params[], PF_LayerDef* output) {
	return RenderChat(in_data, &params[CHITCHAT_INPUT]->u.ld, output, nullptr);
}

} // namespace

extern "C" DllExport PF_Err
EntryPointFunc(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output, void* extra) {
	PF_Err err = PF_Err_NONE;
	try {
		switch (cmd) {
		case PF_Cmd_ABOUT:
			err = About(in_data, out_data, params, output);
			break;
		case PF_Cmd_GLOBAL_SETUP:
			err = GlobalSetup(in_data, out_data, params, output);
			break;
		case PF_Cmd_PARAMS_SETUP:
			err = ParamsSetup(in_data, out_data, params, output);
			break;
		case PF_Cmd_SEQUENCE_SETUP:
			err = SequenceSetup(in_data, out_data);
			break;
		case PF_Cmd_SEQUENCE_RESETUP:
			err = SequenceResetup(in_data, out_data);
			break;
		case PF_Cmd_SEQUENCE_SETDOWN:
			err = SequenceSetdown(in_data, out_data);
			break;
		case PF_Cmd_GET_FLATTENED_SEQUENCE_DATA:
			err = GetFlattenedSequenceData(in_data, out_data);
			break;
		case PF_Cmd_USER_CHANGED_PARAM:
			err = UserChangedParam(in_data, out_data, params, output, reinterpret_cast<const PF_UserChangedParamExtra*>(extra));
			break;
		case PF_Cmd_SMART_PRE_RENDER:
			err = SmartPreRender(in_data, out_data, reinterpret_cast<PF_PreRenderExtra*>(extra));
			break;
		case PF_Cmd_SMART_RENDER:
			err = SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra*>(extra));
			break;
		case PF_Cmd_RENDER:
			err = RenderLegacy(in_data, out_data, params, output);
			break;
		default:
			break;
		}
	} catch (PF_Err thrown) {
		err = thrown;
	} catch (...) {
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}
