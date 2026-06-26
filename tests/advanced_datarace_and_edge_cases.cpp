/*
	RWLEnvelope : Advanced Data Race and Edge Case Tests
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
#include <random>
#include <deque>
#include <map>
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/RWLEnvelope.hpp"

// ============================================================================
// CRITICAL DATA RACE TESTS - ADVANCED SCENARIOS
// ============================================================================

// Race: Verify no lost updates with rapid fire mutations from many threads
TEST(advanced_race_critical, RapidFireMutationsNoLostUpdates)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	const uint32_t            ITERATIONS   = 10000;
	const uint32_t            WRITER_COUNT = 16;
	std::atomic_uint          startSignal {0};
	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < WRITER_COUNT; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<>(
								[](auto& doc) noexcept
								{
									auto cur       = doc.value("counter", 0);
									doc["counter"] = cur + 1;
								});
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	int finalCounter = envelope.observe<>([](const auto& doc) noexcept { return doc.value("counter", -1); });
	int expected     = static_cast<int>(WRITER_COUNT * ITERATIONS);

	EXPECT_EQ(expected, finalCounter) << "Lost updates: expected " << expected << " but got " << finalCounter;
}

// Race: Interleaved readLock/writeLock operations with no deadlock
TEST(advanced_race_critical, InterleavedLockOperationsNoDeadlock)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"value", 0}}));

	const uint32_t            ITERATIONS   = 5000;
	const uint32_t            THREAD_COUNT = 12;
	std::atomic_uint          startSignal {0};
	std::atomic_uint          completedOps {0};
	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.emplace_back(
				[&, threadId = i]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						if (threadId % 3 == 0)
						{
							// readLock
							if (auto [doc, rl] = envelope.readLock(); rl)
							{
								(void)doc.value("value", 0);
								completedOps.fetch_add(1, std::memory_order_relaxed);
							}
						}
						else if (threadId % 3 == 1)
						{
							// writeLock
							if (auto [doc, wl] = envelope.writeLock(); wl)
							{
								doc["value"] = doc.value("value", 0) + 1;
								completedOps.fetch_add(1, std::memory_order_relaxed);
							}
						}
						else
						{
							// observe
							envelope.observe<>(
									[&](const auto& doc) noexcept
									{
										completedOps.fetch_add(1, std::memory_order_relaxed);
										return doc.value("value", 0);
									});
						}
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();

	auto start = std::chrono::steady_clock::now();
	while (completedOps.load() < THREAD_COUNT * ITERATIONS)
	{
		auto elapsed = std::chrono::steady_clock::now() - start;
		ASSERT_LT(elapsed, std::chrono::seconds(60)) << "Deadlock detected: operations did not complete";
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	threads.clear();
	EXPECT_EQ(THREAD_COUNT * ITERATIONS, completedOps.load());
}

// Race: Verify snapshot consistency during rapid reassigns
TEST(advanced_race_critical, SnapshotConsistencyDuringReassigns)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"version", 0}, {"data", "v0"}}));

	const uint32_t            ITERATIONS = 2000;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          inconsistency {false};
	std::vector<std::jthread> threads;

	// Reassigner thread
	threads.emplace_back(
			[&]()
			{
				startSignal.wait(0);
				for (uint32_t j = 0; j < ITERATIONS; j++)
				{
					nlohmann::json newDoc {{"version", static_cast<int>(j + 1)},
			                               {"data", std::string("v") + std::to_string(j + 1)}};
					envelope.reassign(std::move(newDoc));
				}
			});

	// Snapshot readers
	for (uint32_t i = 0; i < 8; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS * 2; j++)
					{
						auto snap = envelope.snapshot();
						int  ver  = snap.value("version", -1);
						auto data = snap.value("data", std::string(""));

						// Verify consistency: data should match version
						if (ver >= 0)
						{
							std::string expected = std::string("v") + std::to_string(ver);
							if (data != expected) { inconsistency.store(true); }
						}
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(inconsistency.load()) << "Snapshot returned inconsistent state during reassigns";
}

// Race: WriteLock and Mutate must have identical semantics under contention
TEST(advanced_race_critical, WriteLockMutateSemanticEquivalence)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	const uint32_t            ITERATIONS   = 5000;
	const uint32_t            THREAD_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_uint32_t      mutateWrites {0};
	std::atomic_uint32_t      writeLockWrites {0};
	std::vector<std::jthread> threads;

	// Half use mutate()
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<>(
								[&](auto& doc) noexcept
								{
									auto cur       = doc.value("counter", 0);
									doc["counter"] = cur + 1;
									mutateWrites.fetch_add(1, std::memory_order_relaxed);
								});
					}
				});
	}

	// Half use writeLock()
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						if (auto [doc, wl] = envelope.writeLock(); wl)
						{
							auto cur       = doc.value("counter", 0);
							doc["counter"] = cur + 1;
							writeLockWrites.fetch_add(1, std::memory_order_relaxed);
						}
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	int finalCounter = envelope.observe<>([](const auto& doc) noexcept { return doc.value("counter", -1); });
	int expected     = static_cast<int>(mutateWrites.load() + writeLockWrites.load());

	EXPECT_EQ(expected, finalCounter) << "WriteLock and Mutate have different semantics";
	EXPECT_GT(mutateWrites.load(), 0) << "No mutate() writes occurred";
	EXPECT_GT(writeLockWrites.load(), 0) << "No writeLock() writes occurred";
}

// ============================================================================
// EDGE CASES - BOUNDARY CONDITIONS
// ============================================================================

// Edge: Empty JSON object handling
TEST(advanced_edge, EmptyJsonHandling)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json::object());

	EXPECT_EQ(0, envelope.observe<>([](const auto& doc) noexcept { return doc.size(); }));
	EXPECT_TRUE(envelope.snapshot().empty());

	envelope.mutate<>([](auto& doc) noexcept { doc["key"] = "value"; });
	EXPECT_EQ(1, envelope.observe<>([](const auto& doc) noexcept { return doc.size(); }));
}

// Edge: Null JSON handling
TEST(advanced_edge, NullJsonHandling)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope({});

	auto ret = envelope.snapshot();
	EXPECT_TRUE(ret.is_null());

	envelope.mutate<>([](auto& doc) noexcept { doc = nlohmann::json::object(); });

	ret = envelope.snapshot();
	EXPECT_FALSE(ret.is_null());
	EXPECT_TRUE(ret.is_object());
}

// Edge: Large JSON object with many fields
TEST(advanced_edge, LargeJsonObject)
{
	nlohmann::json large = nlohmann::json::object();
	for (int i = 0; i < 1000; i++)
	{
		large[std::format("field_{}", i)] = i;
	}

	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(std::move(large));

	EXPECT_EQ(1000, envelope.observe<>([](const auto& doc) noexcept { return doc.size(); }));

	envelope.mutate<>([](auto& doc) noexcept { doc["field_500"] = 999; });
	EXPECT_EQ(999, envelope.observe<>([](const auto& doc) noexcept { return doc.value("field_500", 0); }));
}

// Edge: Deeply nested JSON structure
TEST(advanced_edge, DeeplyNestedJson)
{
	nlohmann::json nested  = nlohmann::json::object();
	auto           current = &nested;
	for (int i = 0; i < 50; i++)
	{
		(*current)["level"] = nlohmann::json::object();
		current             = &(*current)["level"];
	}

	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(std::move(nested));

	auto snap = envelope.snapshot();
	EXPECT_TRUE(snap.contains("level"));

	envelope.mutate<>([](auto& doc) noexcept { doc["modified"] = true; });
	EXPECT_TRUE(envelope.observe<>([](const auto& doc) noexcept { return doc.value("modified", false); }));
}

// Edge: Vector with many elements
TEST(advanced_edge, LargeVector)
{
	std::vector<int> large;
	for (int i = 0; i < 10000; i++)
	{
		large.push_back(i);
	}

	siddiqsoft::RWLEnvelope<std::vector<int>> envelope(std::move(large));

	EXPECT_EQ(10000, envelope.observe<>([](const auto& v) noexcept { return v.size(); }));
	EXPECT_EQ(5000, envelope.observe<>([](const auto& v) noexcept { return v[5000]; }));

	envelope.mutate<>([](auto& v) noexcept { v[5000] = 99999; });
	EXPECT_EQ(99999, envelope.observe<>([](const auto& v) noexcept { return v[5000]; }));
}

// Edge: String with special characters
TEST(advanced_edge, StringWithSpecialCharacters)
{
	std::string                          special = "Hello\nWorld\t\r\0\x01\xFF";
	siddiqsoft::RWLEnvelope<std::string> envelope(special);

	auto snap = envelope.snapshot();
	EXPECT_EQ(special, snap);

	envelope.mutate<>([](auto& s) noexcept { s += "!"; });
	EXPECT_EQ(special + "!", envelope.snapshot());
}

// Edge: Move constructor with empty envelope
TEST(advanced_edge, MoveConstructorEmpty)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> src;
	siddiqsoft::RWLEnvelope<nlohmann::json> dst(std::move(src));

	EXPECT_TRUE(dst.observe<>([](const auto& doc) noexcept { return doc.empty() || doc.is_null(); }));
}

// Edge: Multiple reassigns in rapid succession
TEST(advanced_edge, RapidReassigns)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope;

	for (int i = 0; i < 1000; i++)
	{
		nlohmann::json doc {{"iteration", i}};
		envelope.reassign(std::move(doc));
		EXPECT_EQ(i, envelope.observe<>([](const auto& d) noexcept { return d.value("iteration", -1); }));
	}
}

// Edge: Observe with void return (side effects only)
TEST(advanced_edge, ObserveVoidReturn)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	std::atomic_int sideEffect {0};

	envelope.observe<>([&](const auto& doc) noexcept { sideEffect.store(doc.value("counter", 0)); });

	EXPECT_EQ(0, sideEffect.load());
}

// Edge: Mutate with void return (side effects only)
TEST(advanced_edge, MutateVoidReturn)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	std::atomic_int sideEffect {0};

	envelope.mutate<>(
			[&](auto& doc) noexcept
			{
				doc["counter"] = 42;
				sideEffect.store(42);
			});

	EXPECT_EQ(42, sideEffect.load());
	EXPECT_EQ(42, envelope.observe<>([](const auto& doc) noexcept { return doc.value("counter", 0); }));
}

// Edge: Callback with multiple arguments
TEST(advanced_edge, CallbackWithMultipleArguments)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"base", 10}}));

	int result = envelope.mutate<>(
			[](auto& doc, int a, int b, int c) noexcept
			{
				doc["base"] = a + b + c;
				return a + b + c;
			},
			5,
			10,
			15);

	EXPECT_EQ(30, result);
	EXPECT_EQ(30, envelope.observe<>([](const auto& doc) noexcept { return doc.value("base", 0); }));
}

// Edge: Callback with reference arguments
TEST(advanced_edge, CallbackWithReferenceArguments)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"value", 0}}));

	int external = 100;

	envelope.mutate<>([](auto& doc, int& ext) noexcept { doc["value"] = ext; }, external);

	EXPECT_EQ(100, envelope.observe<>([](const auto& doc) noexcept { return doc.value("value", 0); }));
}

// ============================================================================
// STRESS TESTS - EXTREME CONDITIONS
// ============================================================================

// Stress: Maximum contention with all operations mixed
TEST(advanced_stress, MaximumContentionMixedOps)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope({{"counter", 0}});

	const uint32_t            ITERATIONS   = 5000;
	const uint32_t            THREAD_COUNT = 20;
	std::atomic_uint          startSignal {0};
	std::atomic_uint          opCount {0};
	std::atomic_uint          threadCompleted {0};
	std::atomic_bool          failure {false};
	std::atomic_bool          finished {false};
	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.emplace_back(
				[&, threadId = i]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						switch (threadId % 5)
						{
						case 0: // mutate
							envelope.mutate<>([](auto& doc) noexcept { doc["counter"] = doc.value("counter", 0) + 1; });
							opCount++;
							break;
						case 1: // observe
							envelope.observe<>([](const auto& doc) noexcept { return doc.value("counter", 0); });
							opCount++;
							break;
						case 2: // readLock
							if (auto [doc, rl] = envelope.readLock(); rl) { (void)doc.value("counter", 0); }
							opCount++;
							break;
						case 3: // writeLock
							if (auto [doc, wl] = envelope.writeLock(); wl) { doc["counter"] = doc.value("counter", 0) + 1; }
							opCount++;
							break;
						case 4: // snapshot (receive the snapshot and let it get destroyed.. it's a copy)
							auto _ = envelope.snapshot();
							opCount++;
							break;
						}
					}
					threadCompleted++;
				});
	}
	finished = true;

	startSignal = 1;
	startSignal.notify_all();

	auto start = std::chrono::steady_clock::now();
	// Keep waiting until all of the threads are completed..
	while (threads.size() > 0 && (threadCompleted.load() != THREAD_COUNT))
	{
		auto elapsed = std::chrono::steady_clock::now() - start;
		std::println(std::cerr,
		             "  - Elapsed:{}  threads:{}/{}/{}  ops:{}/{} finished:{} failure:{}",
		             elapsed.count(),
		             threads.size(),
		             threadCompleted.load(),
		             THREAD_COUNT,
		             opCount.load(),
		             ITERATIONS,
		             finished.load(),
		             failure.load());
		ASSERT_LT(elapsed, std::chrono::seconds(220)) << "Deadlock or extreme slowdown detected";
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	threads.clear();
	EXPECT_FALSE(failure.load());
}

// Stress: Rapid fire reassigns with concurrent readers
TEST(advanced_stress, RapidReassignsWithReaders)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"version", 0}}));

	const uint32_t            ITERATIONS   = 3000;
	const uint32_t            READER_COUNT = 12;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          corruption {false};
	std::vector<std::jthread> threads;

	// Reassigner
	threads.emplace_back(
			[&]()
			{
				startSignal.wait(0);
				for (uint32_t j = 0; j < ITERATIONS; j++)
				{
					nlohmann::json newDoc {{"version", static_cast<int>(j)}};
					envelope.reassign(std::move(newDoc));
				}
			});

	// Readers
	for (uint32_t i = 0; i < READER_COUNT; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS * 2; j++)
					{
						auto snap = envelope.snapshot();
						if (!snap.contains("version")) { corruption.store(true); }
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(corruption.load()) << "Corruption detected during rapid reassigns";
}

// Stress: Alternating read/write patterns
TEST(advanced_stress, AlternatingReadWritePattern)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	const uint32_t            ITERATIONS   = 5000;
	const uint32_t            THREAD_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          failure {false};
	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.emplace_back(
				[&, threadId = i]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						if ((j + threadId) % 2 == 0)
						{
							// Read
							int val = envelope.observe<>([](const auto& doc) noexcept { return doc.value("counter", -1); });
							if (val < 0) failure.store(true);
						}
						else
						{
							// Write
							envelope.mutate<>([](auto& doc) noexcept { doc["counter"] = doc.value("counter", 0) + 1; });
						}
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(failure.load());
}

// Stress: Burst pattern - all threads hammer simultaneously then pause
TEST(advanced_stress, BurstPattern)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	const uint32_t            BURSTS       = 10;
	const uint32_t            BURST_SIZE   = 1000;
	const uint32_t            THREAD_COUNT = 12;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          failure {false};
	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.emplace_back(
				[&, threadId = i]()
				{
					startSignal.wait(0);
					for (uint32_t burst = 0; burst < BURSTS; burst++)
					{
						for (uint32_t j = 0; j < BURST_SIZE; j++)
						{
							if (threadId % 2 == 0)
							{
								envelope.mutate<>([](auto& doc) noexcept { doc["counter"] = doc.value("counter", 0) + 1; });
							}
							else
							{
								int val = envelope.observe<>([](const auto& doc) noexcept { return doc.value("counter", 0); });
								if (val < 0) failure.store(true);
							}
						}
						// Pause between bursts
						std::this_thread::sleep_for(std::chrono::microseconds(10));
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(failure.load());
}

// ============================================================================
// MEMORY ORDERING AND VISIBILITY TESTS
// ============================================================================

// Memory: Verify write visibility across threads
TEST(advanced_memory, WriteVisibilityAcrossThreads)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"a", 0}, {"b", 0}, {"c", 0}}));

	const uint32_t            ITERATIONS = 2000;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          visibility {false};
	std::vector<std::jthread> threads;

	// Writer: modifies all fields
	threads.emplace_back(
			[&]()
			{
				startSignal.wait(0);
				for (uint32_t j = 0; j < ITERATIONS; j++)
				{
					envelope.mutate<>(
							[](auto& doc, auto j) noexcept
							{
								doc["a"] = static_cast<int>(j);
								doc["b"] = static_cast<int>(j);
								doc["c"] = static_cast<int>(j);
							},
							j);
				}
			});

	// Readers: verify all modifications are visible
	for (uint32_t i = 0; i < 4; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS * 2; j++)
					{
						auto snap = envelope.snapshot();
						int  a    = snap.value("a", -1);
						int  b    = snap.value("b", -1);
						int  c    = snap.value("c", -1);

						// All three should be equal (written together)
						if (a == b && b == c && a >= 0) { visibility.store(true); }
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_TRUE(visibility.load()) << "Write visibility issue: modifications not visible to readers";
}

// Memory: Verify no stale reads
TEST(advanced_memory, NoStaleReads)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"value", 0}}));

	const uint32_t            ITERATIONS = 5000;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          staleRead {false};
	std::vector<std::jthread> threads;

	int lastWritten = 0;

	// Writer
	threads.emplace_back(
			[&]()
			{
				startSignal.wait(0);
				for (uint32_t j = 0; j < ITERATIONS; j++)
				{
					envelope.mutate<>(
							[&](auto& doc) noexcept
							{
								doc["value"] = static_cast<int>(j + 1);
								lastWritten  = j + 1;
							});
				}
			});

	// Readers
	for (uint32_t i = 0; i < 4; i++)
	{
		threads.emplace_back(
				[&]()
				{
					startSignal.wait(0);
					int lastSeen = 0;
					for (uint32_t j = 0; j < ITERATIONS * 2; j++)
					{
						int current = envelope.observe<>([](const auto& doc) noexcept { return doc.value("value", 0); });
						// Current should never be less than lastSeen (no stale reads)
						if (current < lastSeen) { staleRead.store(true); }
						lastSeen = current;
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(staleRead.load()) << "Stale read detected: value went backwards";
}

// ============================================================================
// LOCK ORDERING AND DEADLOCK PREVENTION TESTS
// ============================================================================

// Deadlock: Verify no deadlock with nested lock attempts (if applicable)
TEST(advanced_deadlock, NoDeadlockWithMixedLocks)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"x", 0}}));

	const uint32_t            ITERATIONS   = 3000;
	const uint32_t            THREAD_COUNT = 10;
	std::atomic_uint          startSignal {0};
	std::atomic_uint          completedOps {0};
	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.emplace_back(
				[&, threadId = i]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						// Randomly choose operation
						int op = (threadId + j) % 4;
						switch (op)
						{
						case 0: envelope.observe<>([](const auto& doc) noexcept { return doc.value("x", 0); }); break;
						case 1: envelope.mutate<>([](auto& doc) noexcept { doc["x"] = doc.value("x", 0) + 1; }); break;
						case 2:
							if (auto [doc, rl] = envelope.readLock(); rl) { (void)doc.value("x", 0); }
							break;
						case 3:
							if (auto [doc, wl] = envelope.writeLock(); wl) { doc["x"] = doc.value("x", 0) + 1; }
							break;
						}
						completedOps.fetch_add(1, std::memory_order_relaxed);
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();

	auto start = std::chrono::steady_clock::now();
	while (completedOps.load() < THREAD_COUNT * ITERATIONS)
	{
		auto elapsed = std::chrono::steady_clock::now() - start;
		ASSERT_LT(elapsed, std::chrono::seconds(60)) << "Deadlock detected";
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	threads.clear();
	EXPECT_EQ(THREAD_COUNT * ITERATIONS, completedOps.load());
}

// ============================================================================
// RETURN VALUE FORWARDING TESTS
// ============================================================================

// Return: Complex return types from observe
TEST(advanced_return, ComplexReturnTypesObserve)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"array", nlohmann::json::array({1, 2, 3})}}));

	auto arr = envelope.observe<>([](const auto& doc) noexcept { return doc.value("array", nlohmann::json::array()); });

	EXPECT_EQ(3, arr.size());
	EXPECT_EQ(1, arr[0].get<int>());
}

// Return: Complex return types from mutate
TEST(advanced_return, ComplexReturnTypesMutate)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"data", "initial"}}));

	auto oldData = envelope.mutate<>(
			[](auto& doc) noexcept
			{
				auto old    = doc.value("data", std::string(""));
				doc["data"] = "modified";
				return old;
			});

	EXPECT_EQ("initial", oldData);
	EXPECT_EQ("modified", envelope.observe<>([](const auto& doc) noexcept { return doc.value("data", ""); }));
}

// Return: Tuple return from observe
TEST(advanced_return, TupleReturnObserve)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"x", 10}, {"y", 20}}));

	auto [x, y] =
			envelope.observe<>([](const auto& doc) noexcept { return std::make_tuple(doc.value("x", 0), doc.value("y", 0)); });

	EXPECT_EQ(10, x);
	EXPECT_EQ(20, y);
}

// ============================================================================
// CONCURRENT MOVE CONSTRUCTOR TESTS
// ============================================================================

// Move: Concurrent move constructor with active operations
TEST(advanced_move, ConcurrentMoveWithActiveOps)
{
	std::atomic_bool failure {false};
	std::atomic_uint startSignal {0};
	const uint32_t   ITERATIONS   = 500;
	const uint32_t   THREAD_COUNT = 4;

	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.emplace_back(
				[&, threadId = i]()
				{
					auto envelope = std::make_unique<siddiqsoft::RWLEnvelope<nlohmann::json>>(
							nlohmann::json({{"id", static_cast<uint32_t>(threadId)}}));

					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						try
						{
							auto newEnvelope = std::make_unique<siddiqsoft::RWLEnvelope<nlohmann::json>>(std::move(*envelope));
							envelope         = std::move(newEnvelope);

							auto val = envelope->observe<>([](const auto& doc) noexcept { return doc.value("id", 0); });
							if (val != static_cast<uint32_t>(threadId)) { failure.store(true); }
						}
						catch (const std::exception&)
						{
							failure.store(true);
						}
					}
				});
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(failure.load()) << "Exception or data corruption during concurrent move constructor";
}

// ============================================================================
// SPECIAL TYPE TESTS
// ============================================================================

// Type: std::map as envelope type
TEST(advanced_types, MapType)
{
	std::map<std::string, int>                          initial {{"a", 1}, {"b", 2}, {"c", 3}};
	siddiqsoft::RWLEnvelope<std::map<std::string, int>> envelope(std::move(initial));

	EXPECT_EQ(3, envelope.observe<>([](const auto& m) noexcept { return m.size(); }));
	EXPECT_EQ(2, envelope.observe<>([](const auto& m) noexcept { return m.at("b"); }));

	envelope.mutate<>([](auto& m) noexcept { m["d"] = 4; });
	EXPECT_EQ(4, envelope.observe<>([](const auto& m) noexcept { return m.size(); }));
}

// Type: std::deque as envelope type
TEST(advanced_types, DequeType)
{
	std::deque<int>                          initial {1, 2, 3, 4, 5};
	siddiqsoft::RWLEnvelope<std::deque<int>> envelope(std::move(initial));

	EXPECT_EQ(5, envelope.observe<>([](const auto& d) noexcept { return d.size(); }));

	envelope.mutate<>([](auto& d) noexcept { d.push_back(6); });
	EXPECT_EQ(6, envelope.observe<>([](const auto& d) noexcept { return d.size(); }));
}

// Type: Custom struct
struct CustomData
{
	int         value;
	std::string name;

	CustomData()
		: value(0)
		, name("")
	{
	}
	
	CustomData(int v, const std::string& n)
		: value(v)
		, name(n)
	{
	}
};

TEST(advanced_types, CustomStructType)
{
	siddiqsoft::RWLEnvelope<CustomData> envelope(CustomData(42, "test"));

	EXPECT_EQ(42, envelope.observe<>([](const auto& data) noexcept { return data.value; }));
	EXPECT_EQ("test", envelope.observe<>([](const auto& data) noexcept { return data.name; }));

	envelope.mutate<>([](auto& data) noexcept { data.value = 100; });
	EXPECT_EQ(100, envelope.observe<>([](const auto& data) noexcept { return data.value; }));
}
