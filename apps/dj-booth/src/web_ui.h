#pragma once

#include "audio.h"
#include "state.h"

void webBegin(LightState& lightState, AudioState& audioState);
void webLoop(bool allowPush = true);
