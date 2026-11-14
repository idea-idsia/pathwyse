#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>
#include <algorithm>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "param.h"

namespace Logger {

    static constexpr int WIDTH = 44;
    static constexpr int KEY_WIDTH = 32;

    inline bool enabled(int verbosity) {return Parameters::getVerbosity() >= verbosity;}

    inline const char* c(const char* code) {
#ifdef _WIN32
        static bool tty = _isatty(_fileno(stdout));
#else
        static bool tty = isatty(fileno(stdout));
#endif
        return tty ? code : "";
    }

    inline std::string format_key(const std::string& key) {
        std::string padded = "  " + key;
        if (key.size() < static_cast<size_t>(KEY_WIDTH))
            padded += std::string(KEY_WIDTH - key.size(), ' ');
        padded += ": ";
        return padded;
    }

    inline void warn(const std::string& msg) {
        if (not enabled(VERB_DETAIL)) return;
        std::cout << c(YELLOW) << "Warning: " << msg << c(COL_RESET) << std::endl;
    }

    inline void error(const std::string& msg) {
        std::cout << c(RED) << "Error: " << msg << c(COL_RESET) << std::endl;
    }

    template<typename... Args>
    void debug(Args&&... args) {
        if (Parameters::getVerbosity() != VERB_DEBUG) return;
        std::cout << c(GRAY);
        (std::cout << ... << args);
        std::cout << c(COL_RESET) << std::endl;
    }

    inline void log(const std::string& msg, int verbosity = VERB_DETAIL, const char* color = COL_RESET) {
        if (not enabled(verbosity)) return;
        std::cout << c(color) << msg << c(COL_RESET) << std::endl;
    }

    inline void log(const std::string& key, const std::string& value, int verbosity = VERB_STD, const char* color = COL_RESET) {
        if (not enabled(verbosity)) return;
        std::cout << format_key(key) << c(color) << value << c(COL_RESET) << std::endl;
    }

    inline void divider(int verbosity = VERB_STD) {
        if (not enabled(verbosity)) return;
        std::cout << c(GRAY) << std::string(WIDTH, '-') << c(COL_RESET) << std::endl;
    }

}

#endif