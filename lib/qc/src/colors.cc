#include <qc/colors.h>

namespace Qd {

	bool Colors::mEnabled = true; // Default: colors enabled

	void Colors::setEnabled(bool enabled) {
		mEnabled = enabled;
	}

	bool Colors::isEnabled() {
		return mEnabled;
	}

	const char* Colors::reset() {
		return mEnabled ? "\033[0m" : "";
	}

	const char* Colors::bold() {
		return mEnabled ? "\033[1m" : "";
	}

	const char* Colors::red() {
		return mEnabled ? "\033[1;31m" : "";
	}

	const char* Colors::magenta() {
		return mEnabled ? "\033[1;35m" : "";
	}

	const char* Colors::cyan() {
		return mEnabled ? "\033[1;36m" : "";
	}

	const char* Colors::green() {
		return mEnabled ? "\033[1;32m" : "";
	}

} // namespace Qd
