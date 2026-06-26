/*
	RWLEnvelope : Observe and Mutate API Comparison Tests
	Reader Writer lock wrapper for object access
	Repo: https://github.com/SiddiqSoft/RWLEnvelope

	BSD 3-Clause License

	Copyright (c) 2021, Siddiq Software LLC
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice, this
	list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

	3. Neither the name of the copyright holder nor the names of its
	contributors may be used to endorse or promote products derived from
	this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <chrono>
#include <cstdint>
#include <format>
#include <thread>
#include <atomic>
#include <vector>
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/RWLEnvelope.hpp"

// ============================================================================
// OBSERVE AND MUTATE API COMPARISON TESTS
// ============================================================================


// Test: observe() does not increment RWA counter
TEST(observe_mutate_api, ObserveDoesNotIncrementRwa)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"x", 0}}));

	auto initialInfo = nlohmann::json(envelope);
	auto initialRwa  = initialInfo.value("readWriteActions", static_cast<uint64_t>(0));

	// Call observe() multiple times
	for (int i = 0; i < 100; i++)
	{
		envelope.observe<>([](const auto& doc) noexcept { return doc.value("x", 0); });
	}

	auto finalInfo = nlohmann::json(envelope);
	auto finalRwa  = finalInfo.value("readWriteActions", static_cast<uint64_t>(0));

	// RWA should not have changed
	EXPECT_EQ(initialRwa, finalRwa) << "observe() should not increment RWA counter";
}

// Test: Asymmetry between observe() and mutate() regarding RWA counter
TEST(observe_mutate_api, RwaAsymmetry)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	const uint32_t OBSERVE_COUNT = 50;
	const uint32_t MUTATE_COUNT  = 50;

	// Perform observe operations
	for (uint32_t i = 0; i < OBSERVE_COUNT; i++)
	{
		envelope.observe<>([](const auto& doc) noexcept { return doc.value("counter", 0); });
	}

	// Perform mutate operations
	for (uint32_t i = 0; i < MUTATE_COUNT; i++)
	{
		envelope.mutate<>([](auto& doc) noexcept { doc["counter"] = doc.value("counter", 0) + 1; });
	}

	auto     info = nlohmann::json(envelope);
	uint64_t rwa  = info.value("readWriteActions", static_cast<uint64_t>(0));

	// RWA should only count mutate() calls, not observe() calls
	EXPECT_EQ(static_cast<uint64_t>(MUTATE_COUNT), rwa)
			<< "RWA counter (" << rwa << ") should only count mutate() calls (" << MUTATE_COUNT << "), not observe() calls";
}

#if defined(DISABLED_DUE_TO_API_CHANGE)
// Test: observe() and mutate() exception safety with partial modifications
TEST(observe_mutate_api, ExceptionSafetyPartialModification)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"a", 1}, {"b", 2}, {"c", 3}}));

	// First, verify initial state
	EXPECT_EQ(1, envelope.observe<>([](const auto& doc) noexcept { return doc.value("a", 0); }));

	// Mutate with exception after partial modification
	try
	{
		envelope.mutate<>(
				[](auto& doc) noexcept
				{
					doc["a"] = 100;
					doc["b"] = 200;
					throw std::runtime_error("intentional");
					doc["c"] = 300; // This won't execute
				});
	}
	catch (const std::runtime_error&)
	{
		// Expected
	}

	// Verify partial modifications were persisted (lock was released properly)
	EXPECT_EQ(100, envelope.observe<>([](const auto& doc) noexcept { return doc.value("a", 0); }));
	EXPECT_EQ(200, envelope.observe<>([](const auto& doc) noexcept { return doc.value("b", 0); }));
	EXPECT_EQ(3, envelope.observe<>([](const auto& doc) noexcept { return doc.value("c", 0); }));

	// Verify envelope is still usable (not deadlocked)
	envelope.mutate<>([](auto& doc) noexcept { doc["a"] = 1000; });
	EXPECT_EQ(1000, envelope.observe<>([](const auto& doc) noexcept { return doc.value("a", 0); }));
}

// Test: observe() exception safety doesn't deadlock
TEST(observe_mutate_api, ObserveExceptionSafety)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"x", 42}}));

	// observe() throws
	try
	{
		envelope.observe<>([](const auto& doc) noexcept { throw std::runtime_error("observe throw"); });
	}
	catch (const std::runtime_error&)
	{
		// Expected
	}

	// Verify envelope is still usable
	EXPECT_EQ(42, envelope.observe<>([](const auto& doc) noexcept { return doc.value("x", 0); }));
	envelope.mutate<>([](auto& doc) noexcept { doc["x"] = 43; });
	EXPECT_EQ(43, envelope.observe<>([](const auto& doc) noexcept { return doc.value("x", 0); }));
}
#endif

// Test: Concurrent observe() calls don't block each other
TEST(observe_mutate_api, ConcurrentObserveNonBlocking)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"data", "immutable"}}));

	const uint32_t            THREAD_COUNT = 16;
	const uint32_t            ITERATIONS   = 1000;
	std::atomic_uint          startSignal {0};
	std::atomic_uint64_t      totalReads {0};
	std::vector<std::jthread> threads;

	auto start = std::chrono::steady_clock::now();

	// All threads call observe() concurrently
	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						std::string data =
								envelope.observe<>([](const auto& doc) noexcept { return doc.value("data", std::string("")); });
						EXPECT_EQ("immutable", data);
						totalReads.fetch_add(1, std::memory_order_relaxed);
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	auto elapsed = std::chrono::steady_clock::now() - start;

	EXPECT_EQ(static_cast<uint64_t>(THREAD_COUNT) * ITERATIONS, totalReads.load());
	// With proper shared locking, this should complete quickly (all threads run concurrently)
	// If it takes too long, it indicates readers are blocking each other (bug)
	EXPECT_LT(elapsed, std::chrono::seconds(10)) << "observe() calls appear to be blocking each other";
}

// Test: mutate() blocks concurrent observe() calls
TEST(observe_mutate_api, MutateBlocksObserve)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	std::atomic_uint          startSignal {0};
	std::atomic_uint          observerCount {0};
	std::vector<std::jthread> threads;

	// Writer thread that holds the lock for a while
	threads.emplace_back(
			[&]()
			{
				startSignal.wait(0);
				envelope.mutate<>(
						[&](auto& doc) noexcept
						{
							// Simulate work
							std::this_thread::sleep_for(std::chrono::milliseconds(100));
							doc["counter"] = 1;
						});
			});

	// Reader threads that try to observe while writer holds lock
	for (uint32_t i = 0; i < 4; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					// Small delay to ensure writer starts first
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					envelope.observe<>(
							[&](const auto& doc) noexcept
							{
								observerCount.fetch_add(1, std::memory_order_relaxed);
								return doc.value("counter", 0);
							});
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	// All observers should have completed (they were blocked by writer, then executed)
	EXPECT_EQ(4, observerCount.load());
}

// Test: RWA counter behavior with concurrent mutate() calls
TEST(observe_mutate_api, RwaCounterConcurrentAccuracy)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"x", 0}}));

	const uint32_t            ITERATIONS   = 2000;
	const uint32_t            THREAD_COUNT = 6;
	std::atomic_uint          startSignal {0};
	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<>([](auto& doc) noexcept { doc["x"] = doc.value("x", 0) + 1; });
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	auto     info = nlohmann::json(envelope);
	uint64_t rwa  = info.value("readWriteActions", static_cast<uint64_t>(0));

	uint64_t expected = static_cast<uint64_t>(THREAD_COUNT) * ITERATIONS;
	EXPECT_EQ(expected, rwa) << "RWA counter (" << rwa << ") != expected (" << expected << ")";
}

// Test: observe() and mutate() preserve data integrity under concurrent access
TEST(observe_mutate_api, DataIntegrityUnderConcurrency)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"a", 0}, {"b", 0}, {"c", 0}}));

	const uint32_t            ITERATIONS   = 2000;
	const uint32_t            THREAD_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          inconsistency {false};
	std::vector<std::jthread> threads;

	// Writers: update all fields together
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<>(
								[j](auto& doc) noexcept
								{
									doc["a"] = static_cast<int>(j);
									doc["b"] = static_cast<int>(j);
									doc["c"] = static_cast<int>(j);
								});
					}
				});
	}

	// Readers: verify all fields are equal (consistent snapshot)
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						auto snap = envelope.snapshot();
						int  a    = snap.value("a", -1);
						int  b    = snap.value("b", -1);
						int  c    = snap.value("c", -1);

						if (a != b || b != c) { inconsistency.store(true); }
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(inconsistency.load()) << "Data integrity violation: fields not updated atomically";
}
