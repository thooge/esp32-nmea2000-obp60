#pragma once

#include <Arduino.h>
#include <vector>
#include <map>
#include "Graphics.h"  // for Point and Rect

class ConfigMenuItem {
private:
    String label;
    uint16_t value;
    String unit;
    String valtype; // "int" | "bool"
    uint16_t min;
    uint16_t max;
    std::vector<uint16_t> steps;
    uint16_t step;
    int8_t position; // counted fom 0

public:
    ConfigMenuItem(String itemtype, String itemlabel, uint16_t itemval, String itemunit);
    void setRange(uint16_t valmin, uint16_t valmax, std::vector<uint16_t> steps);
    bool checkRange(uint16_t checkval);
    String getLabel();
    uint16_t getValue();
    bool setValue(uint16_t newval);
    void incValue();
    void decValue();
    String getUnit();
    uint16_t getStep();
    void setStep(uint16_t newstep);
    int8_t getPos();
    void setPos(int8_t newpos);
    String getType();
};

class ConfigMenu {
private:
    String title;
    std::map <String,ConfigMenuItem*> items;
    std::map <uint8_t,String> index;
    int8_t activeitem = -1; // refers to position of item
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    void (*fptrCallback)();

public:
    ConfigMenu(String title, uint16_t menu_x, uint16_t menu_y);
    ConfigMenuItem* addItem(String key, String label, String valtype, uint16_t val, String valunit);
    void setItemDimension(uint16_t itemwidth, uint16_t itemheight);
    int8_t getActiveIndex();
    void setItemActive(String key);
    ConfigMenuItem* getActiveItem();
    ConfigMenuItem* getItemByIndex(uint8_t index);
    ConfigMenuItem* getItemByKey(String key);
    uint8_t getItemCount();
    void goPrev();
    void goNext();
    Point getXY();
    Rect getRect();
    Rect getItemRect(int8_t index);
    void setCallback(void (*callback)());
    void storeValues();
};
