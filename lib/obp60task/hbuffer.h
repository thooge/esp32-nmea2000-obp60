#ifndef __HBUFFER_H__
#define __HBUFFER_H__

class HistoryBuffer {
public:
    HistoryBuffer(uint16_t size);
    void begin();
    void finish();
    uint16_t add();
    uint8_t* get() ;
    uint8_t getvalue(uint16_t dt);
    uint8_t getvalue3();
    void clear();
};
class History {
public:
    History();
    void *addSeries();
};

#endif
