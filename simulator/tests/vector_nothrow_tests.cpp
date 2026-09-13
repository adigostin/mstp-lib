
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"

namespace vector_nothrow_test_support
{
	bool fail_allocations = false;

	inline void* malloc(size_t size)
	{
		return fail_allocations ? nullptr : std::malloc(size);
	}

	inline void free(void* p)
	{
		std::free(p);
	}
}

#define malloc vector_nothrow_test_support::malloc
#define free vector_nothrow_test_support::free
#include "edge/vector_nothrow.h"
#undef malloc
#undef free

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

struct tracked_vector_element
{
	static inline uint32_t destructed = 0;
	static inline uint32_t moved = 0;
	static inline uint32_t assigned = 0;

	int value;

	tracked_vector_element(int value) : value(value) { }
	tracked_vector_element(const tracked_vector_element& other) : value(other.value) { }
	tracked_vector_element(tracked_vector_element&& other) noexcept : value(other.value) { moved++; }
	tracked_vector_element& operator=(tracked_vector_element&& other) noexcept
	{
		value = other.value;
		assigned++;
		return *this;
	}
	~tracked_vector_element() { destructed++; }
};

TEST_CLASS(vector_nothrow_tests)
{
public:
	TEST_METHOD(TryInsertCoversEmptyFullAndAvailableCapacity)
	{
		vector_nothrow<int> values;

		Assert::IsTrue(values.try_insert(values.begin(), 2));
		Assert::IsTrue(values.try_insert(values.begin(), 1));
		Assert::IsTrue(values.try_insert(values.end(), 4));
		Assert::IsTrue(values.try_insert(values.begin() + 2, 3));

		Assert::AreEqual<uint32_t>(4, values.size());
		Assert::AreEqual(1, values[0]);
		Assert::AreEqual(2, values[1]);
		Assert::AreEqual(3, values[2]);
		Assert::AreEqual(4, values[3]);

		vector_nothrow<int> reserved;
		Assert::IsTrue(reserved.try_reserve(4));
		Assert::IsTrue(reserved.try_insert(reserved.begin(), 7));
		Assert::AreEqual<uint32_t>(1, reserved.size());
		Assert::AreEqual(7, reserved[0]);
	}

	TEST_METHOD(TryInsertReturnsFalseWhenElementAllocationFails)
	{
		vector_nothrow<int> values;
		Assert::IsTrue(values.try_push_back(1));

		vector_nothrow_test_support::fail_allocations = true;
		bool inserted = values.try_insert(values.begin(), 2);
		vector_nothrow_test_support::fail_allocations = false;

		Assert::IsFalse(inserted);
		Assert::AreEqual<uint32_t>(1, values.size());
		Assert::AreEqual(1, values[0]);
	}

	TEST_METHOD(TryInsertDestroysMovedElements)
	{
		tracked_vector_element::destructed = 0;

		{
			vector_nothrow<tracked_vector_element> values;
			Assert::IsTrue(values.try_push_back(tracked_vector_element(1)));
			Assert::IsTrue(values.try_push_back(tracked_vector_element(2)));
			Assert::IsTrue(values.try_push_back(tracked_vector_element(3)));
			Assert::IsTrue(values.try_push_back(tracked_vector_element(4)));

			tracked_vector_element value(5);
			Assert::IsTrue(values.try_insert(values.begin() + 2, std::move(value)));
			Assert::AreEqual<uint32_t>(5, values.size());
			Assert::IsTrue(tracked_vector_element::destructed >= 4u);
		}

		Assert::IsTrue(tracked_vector_element::destructed >= 9u);
	}

	TEST_METHOD(TryInsertCallsMoveConstructor)
	{
		vector_nothrow<tracked_vector_element> values;
		Assert::IsTrue(values.try_push_back(tracked_vector_element(1)));
		Assert::IsTrue(values.try_reserve(4));

		tracked_vector_element::moved = 0;
		tracked_vector_element value(2);
		Assert::IsTrue(values.try_insert(values.begin(), std::move(value)));

		Assert::AreEqual<uint32_t>(2, values.size());
		Assert::AreEqual<uint32_t>(2, tracked_vector_element::moved);
		Assert::AreEqual(2, values[0].value);
		Assert::AreEqual(1, values[1].value);
	}

	TEST_METHOD(TryInsertCallsMoveAssignmentWhenShiftingElements)
	{
		vector_nothrow<tracked_vector_element> values;
		Assert::IsTrue(values.try_push_back(tracked_vector_element(1)));
		Assert::IsTrue(values.try_push_back(tracked_vector_element(2)));
		Assert::IsTrue(values.try_push_back(tracked_vector_element(3)));
		Assert::IsTrue(values.try_reserve(4));

		tracked_vector_element::assigned = 0;
		tracked_vector_element value(4);
		Assert::IsTrue(values.try_insert(values.begin(), std::move(value)));

		Assert::AreEqual<uint32_t>(3, tracked_vector_element::assigned);
		Assert::AreEqual(4, values[0].value);
		Assert::AreEqual(1, values[1].value);
		Assert::AreEqual(2, values[2].value);
		Assert::AreEqual(3, values[3].value);
	}
};
