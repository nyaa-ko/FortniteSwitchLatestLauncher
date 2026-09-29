#pragma once

// Lightweight in-memory + on-disk logger.
// Memory is capped and the log file is rotated so the SD card never fills up.

#include "Common.h"
#include <deque>

enum class LogLevel { Info, Warning, Error, Debug };

struct LogEntry {
    LogLevel level;
    std::string time;
    std::string message;
};

class Logger {
  public:
    static Logger &get() {
        static Logger inst;
        return inst;
    }

    void init() {
        ensureDir(CFG_ROOT);
        ensureDir(CFG_LOGDIR);
        rotateIfNeeded();
        debugEnabled_ = false;
    }

    void setDebug(bool v) { debugEnabled_ = v; }

    void log(LogLevel lvl, const std::string &msg) {
        if (lvl == LogLevel::Debug && !debugEnabled_)
            return;
        LogEntry e{lvl, nowIso(), msg};
        entries_.push_back(e);
        if (entries_.size() > kMaxEntries)
            entries_.pop_front();
        appendToFile(e);
    }

    void info(const std::string &m) { log(LogLevel::Info, m); }
    void warn(const std::string &m) { log(LogLevel::Warning, m); }
    void error(const std::string &m) { log(LogLevel::Error, m); }
    void debug(const std::string &m) { log(LogLevel::Debug, m); }

    const std::deque<LogEntry> &entries() const { return entries_; }

    void clear() {
        entries_.clear();
        std::remove(CFG_LOGFILE);
        info("Logs cleared");
    }

    // "Export" a snapshot the user can copy off the SD card.
    bool exportTo(const std::string &path) {
        std::string out;
        for (const auto &e : entries_)
            out += format(e) + "\n";
        return writeWholeFile(path, out);
    }

    static const char *levelName(LogLevel l) {
        switch (l) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARNING";
        case LogLevel::Error:
            return "ERROR";
        default:
            return "DEBUG";
        }
    }

    static const char *levelColor(LogLevel l) {
        switch (l) {
        case LogLevel::Warning:
            return C_WARN;
        case LogLevel::Error:
            return C_ERR;
        case LogLevel::Debug:
            return C_DIM;
        default:
            return C_MUTED;
        }
    }

    static std::string format(const LogEntry &e) {
        return "[" + std::string(levelName(e.level)) + "] " + e.time + " " + e.message;
    }

  private:
    static const size_t kMaxEntries = 300;
    static const long kMaxFileBytes = 256 * 1024;

    std::deque<LogEntry> entries_;
    bool debugEnabled_ = false;

    void appendToFile(const LogEntry &e) {
        FILE *f = std::fopen(CFG_LOGFILE, "a");
        if (!f)
            return;
        std::fprintf(f, "%s\n", format(e).c_str());
        std::fclose(f);
    }

    void rotateIfNeeded() {
        struct stat st;
        if (stat(CFG_LOGFILE, &st) == 0 && st.st_size > kMaxFileBytes) {
            std::remove(CFG_LOGDIR "/launcher.log.1");
            rename(CFG_LOGFILE, CFG_LOGDIR "/launcher.log.1");
        }
    }
};

#define LOGI(msg) Logger::get().info(msg)
#define LOGW(msg) Logger::get().warn(msg)
#define LOGE(msg) Logger::get().error(msg)
#define LOGD(msg) Logger::get().debug(msg)
