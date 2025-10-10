#ifndef QD_QC_STACK_H
#define QD_QC_STACK_H

#include <stddef.h>
#include <stdint.h>

namespace Qd {
	class Stack {
	public:
		explicit Stack(size_t capacity);
		~Stack();

		void push(int64_t i);
		void push(double d);
		void push(void* p);
		void push(const char* s);

		void pop();
		int64_t topInt() const;
		double topDouble() const;
		void* topPtr() const;
		const char* topStr() const;

	private:
		struct Element {
			union Value {
				int64_t i;
				double d;
				void* p;
				char* s;
			} value;

			enum class Type {
				INT,
				DOUBLE,
				PTR,
				STR
			} type;
		};

		Element* mData = nullptr;
		size_t mCapacity = 0;
		size_t mSize = 0;
	};
}

#endif
