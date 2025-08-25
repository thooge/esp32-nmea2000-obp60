// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "GwLog.h"
#include "Pagedata.h"

typedef struct {
    QueueHandle_t queue;
    GwLog* logger = nullptr;
    uint sensitivity = 100;
#ifdef BOARD_OBP40S3
    bool use_syspage = true;
#endif
} KbTaskData;

void initKeys(CommonData &commonData);
void createKeyboardTask(KbTaskData *param);
