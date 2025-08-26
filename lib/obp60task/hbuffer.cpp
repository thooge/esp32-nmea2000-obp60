/* History Buffer
 *
 * Storage backed buffer for sensordata
 * Permanent storage only supported type: FRAM on I2C-Bus
 *  
 * Values can be 1 to 4 bytes in length
 * 
 * Header: 32 bytes of size
 *    0 0x00 HB00              4 magic number
 *    4 0x04 xxxxxxxxxxxxxxxx 16 name, space padded
 *   20 0x14 n                 1 byte size of values in buffer
 *   21 0x15 mm                2 buffer size in count of values
 *   23 0x17 dd                2 time step in seconds between values
 *   25 0x19 tttt              4 unix timestamp of head
 *   29 0x1d hh                2 head pointer
 *   31 0x1f 0xff              1 header end sign
 *
 *   32 0x20 ...                 start of buffer data
 * 
 * Usage example: 7 hours of data collected every 75 seconds
 * TODO
 * 
 */
 #include <stdint.h>
 #include <time.h>
 
 class HistoryBuffer {

private:
     // Header prototype for permanent storage
     uint8_t header[32] = {
         0x41, 0x48, 0x30, 0x30, // magic: HB00
         0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
         0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // empty name
         0x01, // byte size
         0x50, 0x01, // value count
         0x4b, 0x00, // time step
         0x00, 0x00, 0x00, 0x00, // unix time stamp
         0x00, 0x00, // head pointer
         0xff // end sign
     };
     uint16_t head = 0; // head pointer to next new value position
     time_t timestamp; // last modification time of head
     uint16_t delta_t; // time step in seconds

public:
     HistoryBuffer(uint16_t size) {
     }
     ~HistoryBuffer() {
         // free memory
     }
     void begin() {
         // 
     }
     void finish() {
     }
     uint16_t add() {
         // returns new head value pointer
         return 0;
     }
     uint8_t* get() {
         // returns complete buffer in order new to old
         return 0;
     }
     uint8_t getvalue(uint16_t dt) {
         // Return a single value delta seconds ago
         uint16_t index = head - abs(dt) / delta_t;
         return 0;
     }
     uint8_t getvalue3() {
         return 0;
     }
     bool clear() {
         // clears buffer and permanent storage
         return true;
     }
 };

class History {
public:
    History() {
    }
    ~History() {
    }
    void *addSeries() {
    }
};
