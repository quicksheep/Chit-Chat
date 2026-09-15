#pragma once

#include "ChatRenderer.h"

#include "AEConfig.h"
#include "AE_Effect.h"

namespace chitchat {

PF_Err ClearWorld(PF_InData* in_data, PF_EffectWorld* dst);
PF_Err CompositeOver(PF_InData* in_data, PF_EffectWorld* dst, const BitmapBGRA& overlay);

} // namespace chitchat
