/*
  Menu system for online configuration
*/
#include "ConfigMenu.h"

ConfigMenuItem::ConfigMenuItem(String itemtype, String itemlabel, uint16_t itemval, String itemunit) {
    if (! (itemtype == "int" or itemtype == "bool")) {
        valtype = "int";
    } else {
        valtype = itemtype;
    }
    label = itemlabel;
    min = 0;
    max = std::numeric_limits<uint16_t>::max();
    value = itemval;
    unit = itemunit;
}

void ConfigMenuItem::setRange(uint16_t valmin, uint16_t valmax, std::vector<uint16_t> valsteps) {
    min = valmin;
    max = valmax;
    steps = valsteps;
};

bool ConfigMenuItem::checkRange(uint16_t checkval) {
    return (checkval >= min) and (checkval <= max);
}

String ConfigMenuItem::getLabel() {
    return label;
};

uint16_t ConfigMenuItem::getValue() {
    return value;
}

bool ConfigMenuItem::setValue(uint16_t newval) {
    if (valtype == "int") {
        if (newval >= min and newval <= max) {
            value = newval;
            return true;
        }
        return false; // out of range
    } else if (valtype == "bool") {
        value = (newval != 0) ? 1 : 0;
        return true;
    }
    return false; // invalid type
};

void ConfigMenuItem::incValue() {
    // increase value by step
    if (valtype == "int") {
        if (value + step < max) {
            value += step;
        } else {
            value = max;
        }
    } else if (valtype == "bool") {
        value = !value;
    }
};

void ConfigMenuItem::decValue() {
    // decrease value by step
    if (valtype == "int") {
        if (value - step > min) {
            value -= step;
        } else {
            value = min;
        }
    } else if (valtype == "bool") {
        value = !value;
    }
};

String ConfigMenuItem::getUnit() {
    return unit;
}

uint16_t ConfigMenuItem::getStep() {
    return step;
}

void ConfigMenuItem::setStep(uint16_t newstep) {
    if (std::find(steps.begin(), steps.end(), newstep) == steps.end()) {
        return; // invalid step: not in list of possible steps
    }
    step = newstep;
}

int8_t ConfigMenuItem::getPos() {
    return position;
};

void ConfigMenuItem::setPos(int8_t newpos) {
    position = newpos;
};

String ConfigMenuItem::getType() {
    return valtype;
}

ConfigMenu::ConfigMenu(String menutitle, uint16_t menu_x, uint16_t menu_y) {
    title = menutitle;
    x = menu_x;
    y = menu_y;
};

ConfigMenuItem* ConfigMenu::addItem(String key, String label, String valtype, uint16_t val, String valunit) {
    if (items.find(key) != items.end()) {
        // duplicate keys not allowed
        return nullptr;
    }
    ConfigMenuItem *itm = new ConfigMenuItem(valtype, label, val, valunit);
    items.insert(std::pair<String, ConfigMenuItem*>(key, itm));
    // Append key to index, index starting with 0
    int8_t ix = items.size() - 1;
    index[ix] = key;
    itm->setPos(ix);
    return itm;
};

void ConfigMenu::setItemDimension(uint16_t itemwidth, uint16_t itemheight) {
    w = itemwidth;
    h = itemheight;
};

void ConfigMenu::setItemActive(String key) {
    if (items.find(key) != items.end()) {
        activeitem = items[key]->getPos();
    } else {
        activeitem = -1;
    }
};

int8_t ConfigMenu::getActiveIndex() {
    return activeitem;
}

ConfigMenuItem* ConfigMenu::getActiveItem() {
    if (activeitem < 0) {
        return nullptr;
    }
    return items[index[activeitem]];
};

ConfigMenuItem* ConfigMenu::getItemByIndex(uint8_t ix) {
    if (ix > index.size() - 1) {
        return nullptr;
    }
    return items[index[ix]];
};

ConfigMenuItem* ConfigMenu::getItemByKey(String key) {
    if (items.find(key) == items.end()) {
        return nullptr;
    }
    return items[key];
};

uint8_t ConfigMenu::getItemCount() {
    return items.size();
};

void ConfigMenu::goPrev() {
    if (activeitem == 0) {
        activeitem = items.size() - 1;
    } else {
        activeitem--;
    }
}

void ConfigMenu::goNext() {
    if (activeitem == items.size() - 1) {
        activeitem = 0;
    } else {
        activeitem++;
    }
}

Point ConfigMenu::getXY() {
    return {static_cast<double>(x), static_cast<double>(y)};
}

Rect ConfigMenu::getRect() {
    return {static_cast<double>(x), static_cast<double>(y),
            static_cast<double>(w), static_cast<double>(h)};
}

Rect ConfigMenu::getItemRect(int8_t index) {
    return {static_cast<double>(x), static_cast<double>(y + index * h),
            static_cast<double>(w), static_cast<double>(h)};
}

void ConfigMenu::setCallback(void (*callback)()) {
    fptrCallback = callback;
}

void ConfigMenu::storeValues() {
    if (fptrCallback) {
        fptrCallback();
    }
}
