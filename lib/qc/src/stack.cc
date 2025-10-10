#include <qc/stack.h>

namespace Qd {
	Stack::Stack(size_t capacity) {
		mData = new Element[capacity];
	}

	Stack::~Stack() {
		delete[] mData;
	}

	void Stack::push(int64_t i) {
		if (mSize >= mCapacity) {
			// Handle stack overflow (could throw an exception or resize the stack)
			return;
		}
		mData[mSize].value.i = i;
		mData[mSize].type = Element::Type::INT;
		mSize++;
	}

	void Stack::push(double d) {
		if (mSize >= mCapacity) {
			// Handle stack overflow (could throw an exception or resize the stack)
			return;
		}
		mData[mSize].value.d = d;
		mData[mSize].type = Element::Type::DOUBLE;
		mSize++;
	}

	void Stack::push(void* p) {
		if (mSize >= mCapacity) {
			// Handle stack overflow (could throw an exception or resize the stack)
			return;
		}
		mData[mSize].value.p = p;
		mData[mSize].type = Element::Type::PTR;
		mSize++;
	}

	void Stack::push(const char* s) {
		if (mSize >= mCapacity) {
			// Handle stack overflow (could throw an exception or resize the stack)
			return;
		}
		mData[mSize].value.s = const_cast<char*>(s); // Note: This does not copy the string
		mData[mSize].type = Element::Type::STR;
		mSize++;
	}

	void Stack::pop() {
		if (mSize == 0) {
			// Handle stack underflow (could throw an exception)
			return;
		}
		mSize--;
	}

	int64_t Stack::topInt() const {
		if (mSize == 0 || mData[mSize - 1].type != Element::Type::INT) {
			// Handle error (could throw an exception)
			return 0;
		}
		return mData[mSize - 1].value.i;
	}

	double Stack::topDouble() const {
		if (mSize == 0 || mData[mSize - 1].type != Element::Type::DOUBLE) {
			// Handle error (could throw an exception)
			return 0.0;
		}
		return mData[mSize - 1].value.d;
	}

	void* Stack::topPtr() const {
		if (mSize == 0 || mData[mSize - 1].type != Element::Type::PTR) {
			// Handle error (could throw an exception)
			return nullptr;
		}
		return mData[mSize - 1].value.p;
	}

	const char* Stack::topStr() const {
		if (mSize == 0 || mData[mSize - 1].type != Element::Type::STR) {
			// Handle error (could throw an exception)
			return nullptr;
		}
		return mData[mSize - 1].value.s;
	}
}
