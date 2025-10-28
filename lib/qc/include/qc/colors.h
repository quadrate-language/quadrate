#ifndef QD_QC_COLORS_H
#define QD_QC_COLORS_H

namespace Qd {

	// ANSI color codes for terminal output (gcc/clang style)
	class Colors {
	public:
		static void setEnabled(bool enabled);
		static bool isEnabled();

		// Text colors
		static const char* reset();
		static const char* bold();

		// Error/warning colors
		static const char* red();	  // Errors
		static const char* magenta(); // Warnings
		static const char* cyan();	  // Notes
		static const char* green();	  // Success

	private:
		static bool enabled_;
	};

} // namespace Qd

#endif // QD_QC_COLORS_H
