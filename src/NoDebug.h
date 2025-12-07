#pragma once

// Define NO_DEBUG in build flags to remove runtime Serial output and saved strings.
#ifdef NO_DEBUG

class NullSerialClass {
public:
    void begin(unsigned long) {}
    template<typename T> int print(const T&) { return 0; }
    template<typename T> int println(const T&) { return 0; }
    int println() { return 0; }
    template<typename... Args> int printf(const char*, Args...) { return 0; }
    int available() { return 0; }
    int read() { return -1; }
};

static NullSerialClass NullSerialInstance;
#define Serial NullSerialInstance

#endif