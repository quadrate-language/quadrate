#include <quadrate/qc/colors.h>

#include <cstdlib>
#include <unistd.h>

namespace Qd {

	namespace {

		// Colours default off when output is redirected or NO_COLOR is set, rather
		// than unconditionally on. Previously every consumer that did not remember
		// to call setEnabled() -- quadlint, for one -- wrote escape sequences into
		// pipes, files and CI logs.
		//
		// The flag is process-global while consumers differ in the stream they
		// colour (quadc writes diagnostics to stderr, quadlint writes issues to
		// stdout), so this requires *both* to be terminals. That is the
		// conservative direction: redirecting either stream loses colour on the
		// other, but no escape sequence is ever written into a redirect.
		bool defaultEnabled() {
			if (std::getenv("NO_COLOR") != nullptr) {
				return false;
			}
			return isatty(STDOUT_FILENO) != 0 && isatty(STDERR_FILENO) != 0;
		}

	} // namespace

	bool Colors::mEnabled = defaultEnabled();

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
