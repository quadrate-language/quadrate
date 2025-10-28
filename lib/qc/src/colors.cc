#include <qc/colors.h>

namespace Qd {

	bool Colors::enabled_ = true; // Default: colors enabled

	void Colors::setEnabled(bool enabled) {
		enabled_ = enabled;
	}

	bool Colors::isEnabled() {
		return enabled_;
	}

	const char* Colors::reset() {
		return enabled_ ? "\033[0m" : "";
	}

	const char* Colors::bold() {
		return enabled_ ? "\033[1m" : "";
	}

	const char* Colors::red() {
		return enabled_ ? "\033[1;31m" : "";
	}

	const char* Colors::magenta() {
		return enabled_ ? "\033[1;35m" : "";
	}

	const char* Colors::cyan() {
		return enabled_ ? "\033[1;36m" : "";
	}

	const char* Colors::green() {
		return enabled_ ? "\033[1;32m" : "";
	}

} // namespace Qd
