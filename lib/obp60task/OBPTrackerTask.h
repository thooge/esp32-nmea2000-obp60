// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "GwApi.h"

typedef struct {
    GwApi *api = nullptr;
    GwLog *logger = nullptr;
} TrackerData;

void createTrackerTask(TrackerData *param);
