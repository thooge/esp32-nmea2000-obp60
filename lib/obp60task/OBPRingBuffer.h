#pragma once
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

template <typename T>
class RingBuffer {
private:
    std::vector<T> buffer;
    size_t capacity;
    size_t head; // Points to the next insertion position
    size_t first; // Points to the first (oldest) valid element
    size_t last; // Points to the last (newest) valid element
    size_t count; // Number of valid elements currently in buffer
    bool is_Full; // Indicates that all buffer elements are used and ringing is in use
    T MIN_VAL; // lowest possible value of buffer
    T MAX_VAL; // highest possible value of buffer of type <T>

    // metadata for buffer
    int updFreq; // Update frequency in milliseconds
    int smallest; // Value range of buffer: smallest value
    int biggest; // Value range of buffer: biggest value

public:
    RingBuffer(size_t size);
    void add(const T& value); // Add a new value to  buffer
    T get(size_t index) const; // Get value at specific position (0-based index from oldest to newest)
    T getFirst() const; // Get the first (oldest) value in buffer
    T getLast() const; // Get the last (newest) value in buffer
    T getMin() const; // Get the lowest value in buffer
    T getMin(size_t amount) const; // Get minimum value of the last <amount> values of buffer
    T getMax() const; // Get the highest value in buffer
    T getMax(size_t amount) const; // Get maximum value of the last <amount> values of buffer
    T getMid() const; // Get mid value between <min> and <max> value in buffer
    T getMid(size_t amount) const; // Get mid value between <min> and <max> value of the last <amount> values of buffer
    T getRng(T center, size_t amount) const; // Get maximum difference of last <amount> of buffer values to center value
    T getMedian() const; // Get the median value in buffer
    T getMedian(size_t amount) const; // Get the median value of the last <amount> values of buffer
    size_t getCapacity() const; // Get the buffer capacity (maximum size)
    size_t getCurrentSize() const; // Get the current number of elements in buffer
    bool isEmpty() const; // Check if buffer is empty
    bool isFull() const; // Check if buffer is full
    T getMinVal(); // Get lowest possible value for buffer; used for initialized buffer data
    T getMaxVal(); // Get highest possible value for buffer
    void clear(); // Clear buffer
    T operator[](size_t index) const;
    std::vector<T> getAllValues() const; // Operator[] for convenient access (same as get())

};

#include "OBPRingBuffer.tpp"