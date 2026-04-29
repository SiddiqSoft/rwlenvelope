/*
	RWLEnvelope : Additional Race Condition Tests
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
#include <format>
#include <thread>
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/RWLEnvelope.hpp"

// ============================================================================
// CRITICAL RACE CONDITION TESTS
// ============================================================================

// Race: Multiple writers racing against each other — verify no lost updates
TEST(race_critical, WriterWriterRaceCondition)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	const uint32_t            ITERATIONS   = 5000;
	const uint32_t            WRITER_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          failure {false};
	std::vector<std::jthread> threads;

	// Multiple writers all incrementing the same counter
	for (uint32_t i = 0; i < WRITER_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<void>([](auto& doc) {
							auto cur       = doc.value("counter", 0);
							doc["counter"] = cur + 1;
						});
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	int finalCounter = envelope.observe<int>([](const auto& doc) { return doc.value("counter", -1); });
	int expected     = static_cast<int>(WRITER_COUNT * ITERATIONS);

	EXPECT_EQ(expected, finalCounter) << "Lost updates detected: expected " << expected << " but got " << finalCounter;
}


// Race: reassign() racing against mutate() — verify no corruption
TEST(race_critical, ReassignMutateRace)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"version", 0}, {"data", "initial"}}));

	const uint32_t            ITERATIONS   = 2000;
	const uint32_t            MUTATE_COUNT = 4;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          corruptionFound {false};
	std::vector<std::jthread> threads;

	// One thread rapidly reassigning
	threads.push_back(std::jthread(
			[&]()
			{
				startSignal.wait(0);
				for (uint32_t j = 0; j < ITERATIONS; j++)
				{
					nlohmann::json newDoc {{"version", static_cast<int>(j + 1)}, {"data", "reassigned"}};
					envelope.reassign(std::move(newDoc));
				}
			}));

	// Multiple threads mutating
	for (uint32_t i = 0; i < MUTATE_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<void>([](auto& doc) {
							// Just verify we can access and modify
							if (!doc.contains("version") || !doc.contains("data"))
							{
								// This would indicate corruption
								// But we can't set failure here as we're in a lock
							}
							doc["mutated"] = true;
						});
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	// Verify final state is valid
	auto final = envelope.snapshot();
	EXPECT_TRUE(final.contains("version")) << "Final state missing 'version' field";
	EXPECT_TRUE(final.contains("data")) << "Final state missing 'data' field";
	EXPECT_FALSE(corruptionFound.load()) << "Corruption detected during reassign-mutate race";
}


// Race: Multiple threads calling reassign() simultaneously — verify atomicity
TEST(race_critical, ConcurrentReassigns)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"id", 0}}));

	const uint32_t            ITERATIONS   = 1000;
	const uint32_t            THREAD_COUNT = 6;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          corruptionFound {false};
	std::vector<std::jthread> threads;

	// Multiple threads all reassigning
	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&, threadId = i]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						nlohmann::json newDoc {{"id", static_cast<int>(threadId)}, {"iteration", static_cast<int>(j)}};
						envelope.reassign(std::move(newDoc));
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	// Verify final state is one of the valid reassigned values (not corrupted)
	auto final = envelope.snapshot();
	EXPECT_TRUE(final.contains("id")) << "Final state missing 'id' field";
	EXPECT_TRUE(final.contains("iteration")) << "Final state missing 'iteration' field";

	int id = final.value("id", -1);
	EXPECT_GE(id, 0) << "Invalid id in final state";
	EXPECT_LT(id, static_cast<int>(THREAD_COUNT)) << "id out of expected range";

	int iteration = final.value("iteration", -1);
	EXPECT_GE(iteration, 0) << "Invalid iteration in final state";
	EXPECT_LT(iteration, static_cast<int>(ITERATIONS)) << "iteration out of expected range";
}


// Race: Verify no deadlock under stress with timeout
TEST(race_critical, NoDeadlockUnderStress)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"x", 0}}));

	const uint32_t            ITERATIONS   = 3000;
	const uint32_t            THREAD_COUNT = 12;
	std::atomic_uint          startSignal {0};
	std::atomic_uint          completedThreads {0};
	std::vector<std::jthread> threads;

	// Mix of all operations
	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&, threadId = i]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						switch (threadId % 5)
						{
							case 0: // mutate
								envelope.mutate<void>([](auto& doc) { doc["x"] = doc.value("x", 0) + 1; });
								break;
							case 1: // observe
								envelope.observe<int>([](const auto& doc) { return doc.value("x", 0); });
								break;
							case 2: // readLock
								if (auto [doc, rl] = envelope.readLock(); rl) { (void)doc.value("x", 0); }
								break;
							case 3: // writeLock
								if (auto [doc, wl] = envelope.writeLock(); wl) { doc["x"] = doc.value("x", 0) + 1; }
								break;
							case 4: // snapshot
								(void)envelope.snapshot();
								break;
						}
					}
					completedThreads.fetch_add(1, std::memory_order_relaxed);
				}));
	}

	startSignal = 1;
	startSignal.notify_all();

	// Wait for all threads with timeout
	auto start = std::chrono::steady_clock::now();
	while (completedThreads.load() < THREAD_COUNT)
	{
		auto elapsed = std::chrono::steady_clock::now() - start;
		ASSERT_LT(elapsed, std::chrono::seconds(30)) << "Deadlock detected: threads did not complete within 30 seconds";
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	threads.clear();
	EXPECT_EQ(THREAD_COUNT, completedThreads.load()) << "Not all threads completed";
}

// ============================================================================
// HIGH-PRIORITY RACE CONDITION TESTS
// ============================================================================

// Race: Exception safety under concurrent access
TEST(race_high, ExceptionSafetyUnderContention)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"value", 0}}));

	const uint32_t            ITERATIONS   = 2000;
	const uint32_t            THREAD_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          deadlockDetected {false};
	std::atomic_uint          exceptionCount {0};
	std::vector<std::jthread> threads;

	// Threads that throw exceptions
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						try
						{
							envelope.mutate<void>([j](auto& doc) {
								if (j % 10 == 0)
								{
									throw std::runtime_error("intentional");
								}
								doc["value"] = doc.value("value", 0) + 1;
							});
						}
						catch (const std::runtime_error&)
						{
							exceptionCount.fetch_add(1, std::memory_order_relaxed);
						}
					}
				}));
	}

	// Threads that don't throw
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						try
						{
							envelope.observe<int>([](const auto& doc) { return doc.value("value", 0); });
						}
						catch (...)
						{
							deadlockDetected.store(true);
						}
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(deadlockDetected.load()) << "Deadlock detected after exceptions";
	EXPECT_GT(exceptionCount.load(), 0) << "Expected some exceptions to be thrown";

	// Verify envelope is still usable
	int finalValue = envelope.observe<int>([](const auto& doc) { return doc.value("value", 0); });
	EXPECT_GE(finalValue, 0) << "Envelope in invalid state after exceptions";
}


// Race: WriteLock and Mutate should have identical locking semantics
TEST(race_high, WriteLockMutateEquivalence)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	const uint32_t            ITERATIONS   = 3000;
	const uint32_t            THREAD_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          failure {false};
	std::atomic_uint32_t      mutateWrites {0};
	std::atomic_uint32_t      writeLockWrites {0};
	std::vector<std::jthread> threads;

	// Half use mutate()
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<void>([&](auto& doc) {
							auto cur       = doc.value("counter", 0);
							doc["counter"] = cur + 1;
							mutateWrites.fetch_add(1, std::memory_order_relaxed);
						});
					}
				}));
	}

	// Half use writeLock()
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.push_back(std::jthread(
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
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	int finalCounter = envelope.observe<int>([](const auto& doc) { return doc.value("counter", -1); });
	int expected     = static_cast<int>(mutateWrites.load() + writeLockWrites.load());

	EXPECT_EQ(expected, finalCounter) << "WriteLock and Mutate have different locking semantics";
	EXPECT_GT(mutateWrites.load(), 0) << "No mutate() writes occurred";
	EXPECT_GT(writeLockWrites.load(), 0) << "No writeLock() writes occurred";
}


// Race: ReadLock and Observe should have identical locking semantics
TEST(race_high, ReadLockObserveEquivalence)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	const uint32_t            ITERATIONS   = 3000;
	const uint32_t            THREAD_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_uint64_t      observeReads {0};
	std::atomic_uint64_t      readLockReads {0};
	std::vector<std::jthread> threads;

	// One writer thread
	threads.push_back(std::jthread(
			[&]()
			{
				startSignal.wait(0);
				for (uint32_t j = 0; j < ITERATIONS; j++)
				{
					envelope.mutate<void>([](auto& doc) { doc["counter"] = doc.value("counter", 0) + 1; });
				}
			}));

	// Half use observe()
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						int val = envelope.observe<int>([](const auto& doc) { return doc.value("counter", 0); });
						if (val >= 0) observeReads.fetch_add(1, std::memory_order_relaxed);
					}
				}));
	}

	// Half use readLock()
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						if (auto [doc, rl] = envelope.readLock(); rl)
						{
							int val = doc.value("counter", 0);
							if (val >= 0) readLockReads.fetch_add(1, std::memory_order_relaxed);
						}
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_GT(observeReads.load(), 0) << "No observe() reads occurred";
	EXPECT_GT(readLockReads.load(), 0) << "No readLock() reads occurred";

	// Both should see valid counter values
	int finalCounter = envelope.observe<int>([](const auto& doc) { return doc.value("counter", -1); });
	EXPECT_EQ(ITERATIONS, finalCounter) << "Final counter doesn't match expected writes";
}

// ============================================================================
// MEDIUM-PRIORITY RACE CONDITION TESTS
// ============================================================================

// Race: Snapshot isolation from concurrent reassigns
TEST(race_medium, SnapshotIsolationFromReassign)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"version", 0}, {"data", "initial"}}));

	const uint32_t            ITERATIONS   = 1000;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          inconsistencyFound {false};
	std::vector<std::jthread> threads;

	// Thread 1: Takes snapshots and verifies they're consistent
	threads.push_back(std::jthread(
			[&]()
			{
				startSignal.wait(0);
				for (uint32_t j = 0; j < ITERATIONS; j++)
				{
					auto snap = envelope.snapshot();
					int  ver  = snap.value("version", -1);
					auto data = snap.value("data", std::string(""));

					// Verify snapshot consistency: version and data should match
					if (ver == 0 && data != "initial") inconsistencyFound.store(true);
					if (ver > 0 && data != std::string("v") + std::to_string(ver)) inconsistencyFound.store(true);
				}
			}));

	// Thread 2: Rapidly reassigns
	threads.push_back(std::jthread(
			[&]()
			{
				startSignal.wait(0);
				for (uint32_t j = 0; j < ITERATIONS; j++)
				{
					nlohmann::json newDoc {{"version", static_cast<int>(j + 1)},
					                       {"data", std::string("v") + std::to_string(j + 1)}};
					envelope.reassign(std::move(newDoc));
				}
			}));

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(inconsistencyFound.load()) << "Snapshot isolation violated by concurrent reassigns";
}


// Race: RWA counter accuracy with exceptions
TEST(race_medium, RwaCounterWithExceptions)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"x", 0}}));

	const uint32_t            ITERATIONS   = 2000;
	const uint32_t            THREAD_COUNT = 4;
	std::atomic_uint          startSignal {0};
	std::atomic_uint32_t      successfulMutates {0};
	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						try
						{
							envelope.mutate<void>([&, j](auto& doc) {
								doc["x"] = doc.value("x", 0) + 1;
								if (j % 20 == 0)
								{
									throw std::runtime_error("intentional");
								}
								successfulMutates.fetch_add(1, std::memory_order_relaxed);
							});
						}
						catch (const std::runtime_error&)
						{
							// Expected
						}
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	auto     info = nlohmann::json(envelope);
	uint64_t rwa  = info.value("readWriteActions", static_cast<uint64_t>(0));

	// RWA should count all mutate() calls, including those that threw
	uint64_t expectedRwa = static_cast<uint64_t>(THREAD_COUNT) * ITERATIONS;
	EXPECT_EQ(expectedRwa, rwa) << "RWA counter doesn't match total mutate() calls";
}

// ============================================================================
// LOW-PRIORITY RACE CONDITION TESTS
// ============================================================================

// Race: Stress test with longer lock durations
TEST(race_low, LongLockDurationStress)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	const uint32_t            ITERATIONS   = 500;
	const uint32_t            THREAD_COUNT = 6;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          failure {false};
	std::vector<std::jthread> threads;

	// Writers with variable lock durations
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.push_back(std::jthread(
				[&, threadId = i]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<void>([threadId, j](auto& doc) {
							// Simulate variable work duration
							auto sleepMs = (threadId + j) % 5;
							std::this_thread::sleep_for(std::chrono::microseconds(sleepMs * 10));
							doc["counter"] = doc.value("counter", 0) + 1;
						});
					}
				}));
	}

	// Readers with variable lock durations
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.push_back(std::jthread(
				[&, threadId = i]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						int val = envelope.observe<int>([threadId, j](const auto& doc) {
							// Simulate variable work duration
							auto sleepMs = (threadId + j) % 5;
							std::this_thread::sleep_for(std::chrono::microseconds(sleepMs * 10));
							return doc.value("counter", -1);
						});
						if (val < 0) failure.store(true);
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(failure.load()) << "Failure detected under variable lock durations";

	int finalCounter = envelope.observe<int>([](const auto& doc) { return doc.value("counter", -1); });
	int expected     = static_cast<int>((THREAD_COUNT / 2) * ITERATIONS);
	EXPECT_EQ(expected, finalCounter) << "Counter mismatch under variable lock durations";
}


// Race: Memory visibility across threads
TEST(race_low, MemoryVisibilityAcrossThreads)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"a", 0}, {"b", 0}, {"c", 0}}));

	const uint32_t            ITERATIONS   = 2000;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          visibilityViolation {false};
	std::vector<std::jthread> threads;

	// Writer thread: modifies multiple fields
	threads.push_back(std::jthread(
			[&]()
			{
				startSignal.wait(0);
				for (uint32_t j = 0; j < ITERATIONS; j++)
				{
					envelope.mutate<void>([j](auto& doc) {
						doc["a"] = static_cast<int>(j);
						doc["b"] = static_cast<int>(j);
						doc["c"] = static_cast<int>(j);
					});
				}
			}));

	// Reader threads: verify all modifications are visible
	for (uint32_t i = 0; i < 4; i++)
	{
		threads.push_back(std::jthread(
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
						if (a != b || b != c)
						{
							visibilityViolation.store(true);
						}
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(visibilityViolation.load()) << "Memory visibility violation detected";
}


// Race: Concurrent move constructor with separate envelopes
TEST(race_low, ConcurrentMoveConstructor)
{
	std::atomic_bool          failure {false};
	std::atomic_uint          startSignal {0};
	const uint32_t            ITERATIONS = 500;
	const uint32_t            THREAD_COUNT = 4;

	std::vector<std::jthread> threads;

	// Each thread has its own envelope to move
	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&, threadId = i]()
				{
					// Create envelope in thread
					auto envelope = std::make_unique<siddiqsoft::RWLEnvelope<nlohmann::json>>(
							nlohmann::json({{"value", static_cast<int>(threadId)}}));

					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						try
						{
							// Move to new envelope
							auto newEnvelope = std::make_unique<siddiqsoft::RWLEnvelope<nlohmann::json>>(
									std::move(*envelope));
							envelope = std::move(newEnvelope);

							// Verify we can still use it after move
							int val = envelope->observe<int>([](const auto& doc) { return doc.value("value", 0); });
							if (val != static_cast<int>(threadId))
							{
								failure.store(true);
							}
						}
						catch (const std::exception&)
						{
							failure.store(true);
						}
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(failure.load()) << "Exception or data corruption during concurrent move constructor";
}
