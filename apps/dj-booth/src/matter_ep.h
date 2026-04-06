#pragma once

#include "state.h"

void matterBegin(LightState& state);
bool matterStarted();
void matterSync(const LightState& state);
