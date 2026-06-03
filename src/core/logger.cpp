#include "logger.hpp"
#include <chrono>
#include <ctime>
#include <iostream>

static std::string nowHHMMSS() {
	using namespace std::chrono;
	std::time_t t = system_clock::to_time_t(system_clock::now());
	std::tm tm{};
#ifdef _WIN32
	localtime_s(&tm, &t);
#else
	localtime_r(&t, &tm);
#endif
	char buf[16];
	std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
	return std::string(buf);
}

void Logger::log(LogLevel level, const std::string& msg) {
	std::string ts = nowHHMMSS();   // build outside the lock
	{
		std::lock_guard<std::mutex> lk(mutex_);
		if (entries_.size() >= kMaxEntries)
			entries_.erase(entries_.begin());   // drop oldest
		entries_.push_back(LogEntry{ level, ts, msg });
		++revision_;
	}
	// Echo to stderr to preserve the original terminal behavior. The body holds no
	// trailing newline; one std::endl here matches the old single std::endl/"\n".
	std::cerr << "[" << ts << "] " << msg << std::endl;
}

void Logger::clear() {
	std::lock_guard<std::mutex> lk(mutex_);
	entries_.clear();
	++revision_;
}

std::vector<LogEntry> Logger::snapshot() {
	std::lock_guard<std::mutex> lk(mutex_);
	return entries_;
}

unsigned long long Logger::revision() {
	std::lock_guard<std::mutex> lk(mutex_);
	return revision_;
}
