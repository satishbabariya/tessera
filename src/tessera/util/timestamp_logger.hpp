
#ifndef TESSERA_UTIL_TIMESTAMP_LOGGER_HPP
#define TESSERA_UTIL_TIMESTAMP_LOGGER_HPP

#include <tessera/util/logger.hpp>
#include <tessera/util/timestamp_formatter.hpp>


namespace tessera {
namespace util {

class TimestampStderrLogger : public Logger {
public:
    using Precision = TimestampFormatter::Precision;
    using Config = TimestampFormatter::Config;

    explicit TimestampStderrLogger(Config = {}, Level = LogCategory::realm.get_default_level_threshold());

protected:
    void do_log(const LogCategory& category, Logger::Level, const std::string& message) final;

private:
    TimestampFormatter m_formatter;
};


} // namespace util
} // namespace tessera

#endif // TESSERA_UTIL_TIMESTAMP_LOGGER_HPP
