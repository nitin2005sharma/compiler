#ifndef TRACE_HPP
#define TRACE_HPP

#include <iostream>
#include <string>

class TraceLogger {
    bool enabled_;
    std::ostream* out_;

public:
    explicit TraceLogger(bool enabled = false, std::ostream* out = &std::cout)
        : enabled_(enabled), out_(out) {}

    bool enabled() const { return enabled_; }

    void log(const std::string& phase, const std::string& message) const {
        if (!enabled_ || out_ == nullptr) {
            return;
        }

        (*out_) << "[TRACE][" << phase << "] " << message << std::endl;
    }
};

#endif
