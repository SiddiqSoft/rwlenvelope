/*
	RWLEnvelope : tests
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


// We need this to pull in the thread id
#include <cstddef>
#include <sstream>
#if defined(WIN32)
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#endif

#include <format>
#include <random>

#include <thread>
#include "gtest/gtest.h"

#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/RWLEnvelope.hpp"

#if defined(WIN32)
#include <processthreadsapi.h>
#endif

#if !__cpp_structured_bindings
#error "tests: Must have structured bindings C++17"
#endif

#if !__cpp_lib_jthread
#error "tests: Must have jthread C++20"
#endif

#if !__cpp_lib_format
#error "tests: Must have format C++20"
#endif


TEST(tests, Simple)
{
	try
	{
		siddiqsoft::RWLEnvelope<nlohmann::json> myContainer;

		myContainer.mutate<void>([](const auto& o) { EXPECT_EQ(0, o.size()); });
	}
	catch (...)
	{
		EXPECT_TRUE(false); // if we throw then the test fails.
	}
}


TEST(tests, TwoThreads)
{
	using namespace std;

	siddiqsoft::RWLEnvelope<nlohmann::json> myContainer;
	std::atomic_uint                        startSignal {0};
	std::vector<std::jthread>               readerPool;
	std::vector<std::jthread>               writerPool;
	std::atomic_uint32_t                    readCounter {0};
	std::atomic_uint32_t                    writeCounter {0};
	const uint32_t                          READ_LIMIT {100};
	const uint32_t                          WRITE_LIMIT {100};
	const uint32_t                          THREAD_COUNT = READ_LIMIT / 10;
	std::atomic_uint                        readerFinished {0};
	std::atomic_uint                        writerFinished {0};

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		readerPool.push_back(std::jthread(
				[&]()
				{
					thread_local std::mt19937               rng {std::random_device {}()};
					std::uniform_int_distribution<uint32_t> dist(0, READ_LIMIT - 1);
					startSignal.wait(0);
					for (uint32_t j = 0; j < READ_LIMIT; j++)
					{
						myContainer.observe<void>([&](auto const& o) { readCounter++; });
						std::this_thread::sleep_for(std::chrono::nanoseconds(dist(rng)));
					}
					readerFinished++;
					readerFinished.notify_all();
				}));
	}

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		readerPool.push_back(std::jthread(
				[&]()
				{
					thread_local std::mt19937               rng {std::random_device {}()};
					std::uniform_int_distribution<uint32_t> dist(0, WRITE_LIMIT - 1);
#if defined(WIN32)
					unsigned tid = GetCurrentThreadId();
#else
					std::stringstream ss {};
					ss << std::this_thread::get_id();
					auto tid = ss.str();
#endif
					startSignal.wait(0);
					for (uint32_t j = 0; j < WRITE_LIMIT; j++)
					{
						myContainer.mutate<void>(
								[&writeCounter, tid, j](auto& o)
								{
									o["lastThreadId"] = tid;
									o["j"]            = j;
									o["writeCount"]   = ++writeCounter;
								});
						std::this_thread::sleep_for(std::chrono::nanoseconds(dist(rng)));
					}
					writerFinished++;
					writerFinished.notify_all();
				}));
	}

	// Let's signal threads to start!
	startSignal = 1;
	startSignal.notify_all();

	// Join all threads (jthread destructor requests stop and joins)
	readerPool.clear();

	myContainer.observe<void>([&](auto const& o) { std::cerr << std::format("{} - {}\n", __func__, o.dump()) << std::endl; });
	std::cerr << std::format("{} - {}\n", __func__, myContainer.snapshot().dump()) << std::endl;

	EXPECT_EQ(WRITE_LIMIT / 10, writerFinished.load()) << myContainer.snapshot().dump();
	EXPECT_EQ(READ_LIMIT / 10, readerFinished.load()) << myContainer.snapshot().dump();

	EXPECT_EQ(READ_LIMIT * THREAD_COUNT, readCounter.load()) << myContainer.snapshot().dump();
	EXPECT_EQ(WRITE_LIMIT * THREAD_COUNT, writeCounter.load()) << myContainer.snapshot().dump();
}


TEST(tests, TwoThreadsBare)
{
	using namespace std;

	siddiqsoft::RWLEnvelope<nlohmann::json> myContainer;
	std::atomic_uint                        startSignal {0};
	std::vector<std::jthread>               readerPool;
	std::vector<std::jthread>               writerPool;
	std::atomic_uint32_t                    readCounter {0};
	std::atomic_uint32_t                    writeCounter {0};
	const uint32_t                          READ_LIMIT {100};
	const uint32_t                          WRITE_LIMIT {100};
	const uint32_t                          THREAD_COUNT = READ_LIMIT / 10;
	std::atomic_uint                        readerFinished {0};
	std::atomic_uint                        writerFinished {0};

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		readerPool.push_back(std::jthread(
				[&]()
				{
					thread_local std::mt19937               rng {std::random_device {}()};
					std::uniform_int_distribution<uint32_t> dist(0, READ_LIMIT - 1);
					startSignal.wait(0);
					for (uint32_t j = 0; j < READ_LIMIT; j++)
					{
						if (auto [o, rl] = myContainer.readLock(); rl) { readCounter++; }

						std::this_thread::sleep_for(std::chrono::milliseconds(dist(rng)));
					}
					readerFinished++;
					readerFinished.notify_all();
				}));
	}

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		readerPool.push_back(std::jthread(
				[&]()
				{
					thread_local std::mt19937               rng {std::random_device {}()};
					std::uniform_int_distribution<uint32_t> dist(0, WRITE_LIMIT - 1);
#if defined(WIN32)
					unsigned tid = GetCurrentThreadId();
#else
					std::stringstream ss {};
					ss << std::this_thread::get_id();
					auto tid = ss.str();
#endif
					startSignal.wait(0);
					for (uint32_t j = 0; j < WRITE_LIMIT; j++)
					{
						if (auto [o, rwl] = myContainer.writeLock(); rwl)
						{
							o["lastThreadId"] = tid;
							o["j"]            = j;
							o["writeCount"]   = ++writeCounter;
						};
						std::this_thread::sleep_for(std::chrono::milliseconds(dist(rng)));
					}
					writerFinished++;
					writerFinished.notify_all();
				}));
	}

	// Let's signal threads to start!
	startSignal = 1;
	startSignal.notify_all();

	// Join all threads (jthread destructor requests stop and joins)
	readerPool.clear();

	myContainer.observe<void>([&](auto const& o) { std::cerr << std::format("{} - {}\n", __func__, o.dump()) << std::endl; });
	std::cerr << std::format("{} - {}\n", __func__, myContainer.snapshot().dump()) << std::endl;

	EXPECT_EQ(WRITE_LIMIT / 10, writerFinished.load()) << myContainer.snapshot().dump();
	EXPECT_EQ(READ_LIMIT / 10, readerFinished.load()) << myContainer.snapshot().dump();

	EXPECT_EQ(READ_LIMIT * THREAD_COUNT, readCounter.load()) << myContainer.snapshot().dump();
	EXPECT_EQ(WRITE_LIMIT * THREAD_COUNT, writeCounter.load()) << myContainer.snapshot().dump();
}


// ---------------------------------------------------------------------------
// Edge case tests
// ---------------------------------------------------------------------------

// Edge: default-constructed envelope starts empty, reassign populates it
TEST(edge, DefaultConstructThenReassign)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope;

	// Default-constructed json is null, which has size 0
	EXPECT_EQ(0, envelope.observe<size_t>([](const auto& doc) { return doc.size(); }));
	EXPECT_TRUE(envelope.snapshot().empty() || envelope.snapshot().is_null());

	nlohmann::json payload {{"key", "value"}, {"num", 42}};
	envelope.reassign(std::move(payload));
	EXPECT_TRUE(payload.empty());

	EXPECT_EQ("value", envelope.observe<std::string>([](const auto& doc) { return doc.value("key", ""); }));
	EXPECT_EQ(42, envelope.observe<int>([](const auto& doc) { return doc.value("num", 0); }));
	EXPECT_EQ(2, envelope.snapshot().size());
}


// Edge: mutate with non-void return type forwards the value correctly
TEST(edge, MutateWithReturnValue)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 10}}));

	int oldValue = envelope.mutate<int>([](auto& doc) {
		int prev         = doc.value("counter", 0);
		doc["counter"]   = prev + 5;
		return prev;
	});

	EXPECT_EQ(10, oldValue);
	EXPECT_EQ(15, envelope.observe<int>([](const auto& doc) { return doc.value("counter", 0); }));

	// rwa should be 1 after one mutate
	auto info = nlohmann::json(envelope);
	EXPECT_EQ(1, info.value("readWriteActions", 0));
}


// Edge: observe with various return types
TEST(edge, ObserveReturnTypes)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"name", "test"}, {"count", 7}, {"active", true}}));

	auto name   = envelope.observe<std::string>([](const auto& doc) { return doc.value("name", ""); });
	auto count  = envelope.observe<int>([](const auto& doc) { return doc.value("count", 0); });
	auto active = envelope.observe<bool>([](const auto& doc) { return doc.value("active", false); });
	auto sz     = envelope.observe<size_t>([](const auto& doc) { return doc.size(); });

	EXPECT_EQ("test", name);
	EXPECT_EQ(7, count);
	EXPECT_TRUE(active);
	EXPECT_EQ(3, sz);
}


// Edge: RWLEnvelope with a non-JSON type (std::vector<int>)
TEST(edge, NonJsonType)
{
	siddiqsoft::RWLEnvelope<std::vector<int>> envelope(std::vector<int> {1, 2, 3});

	EXPECT_EQ(3, envelope.observe<size_t>([](const auto& v) { return v.size(); }));
	EXPECT_EQ(2, envelope.observe<int>([](const auto& v) { return v[1]; }));

	envelope.mutate<void>([](auto& v) { v.push_back(4); v.push_back(5); });
	EXPECT_EQ(5, envelope.observe<size_t>([](const auto& v) { return v.size(); }));

	auto snap = envelope.snapshot();
	EXPECT_EQ(std::vector<int>({1, 2, 3, 4, 5}), snap);

	// readLock / writeLock
	if (auto const& [v, rl] = envelope.readLock(); rl) { EXPECT_EQ(5, v.size()); }
	if (auto [v, wl] = envelope.writeLock(); wl) { v.clear(); }
	EXPECT_EQ(0, envelope.observe<size_t>([](const auto& v) { return v.size(); }));
}


// Edge: RWLEnvelope with std::string
TEST(edge, StringType)
{
	siddiqsoft::RWLEnvelope<std::string> envelope(std::string("hello"));

	EXPECT_EQ("hello", envelope.snapshot());

	envelope.mutate<void>([](auto& s) { s += " world"; });
	EXPECT_EQ("hello world", envelope.snapshot());

	envelope.reassign(std::string("replaced"));
	EXPECT_EQ("replaced", envelope.snapshot());
}


// Edge: move constructor leaves source empty and transfers rwa counter
TEST(edge, MoveSourceIsEmptyAndRwaTransferred)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> src(nlohmann::json({{"a", 1}}));

	// Perform 3 mutates on source
	src.mutate<void>([](auto& doc) { doc["a"] = 2; });
	src.mutate<void>([](auto& doc) { doc["a"] = 3; });
	src.mutate<void>([](auto& doc) { doc["a"] = 4; });

	auto srcInfo = nlohmann::json(src);
	EXPECT_EQ(3, srcInfo.value("readWriteActions", 0));

	// Move
	siddiqsoft::RWLEnvelope<nlohmann::json> dst(std::move(src));

	// Source should be empty
	EXPECT_TRUE(src.observe<bool>([](const auto& doc) { return doc.empty() || doc.is_null(); }));

	// Source rwa should be 0
	auto srcInfoAfter = nlohmann::json(src);
	EXPECT_EQ(0, srcInfoAfter.value("readWriteActions", -1));

	// Destination should have the data and the rwa counter
	EXPECT_EQ(4, dst.observe<int>([](const auto& doc) { return doc.value("a", 0); }));
	auto dstInfo = nlohmann::json(dst);
	EXPECT_EQ(3, dstInfo.value("readWriteActions", 0));
}


// Edge: throwing callback in mutate should not corrupt state
TEST(edge, ThrowingMutateCallback)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"value", 100}}));

	// This mutate should succeed
	envelope.mutate<void>([](auto& doc) { doc["value"] = 200; });
	EXPECT_EQ(200, envelope.observe<int>([](const auto& doc) { return doc.value("value", 0); }));

	// This mutate throws after partial modification
	try
	{
		envelope.mutate<void>([](auto& doc) {
			doc["value"] = 999;
			throw std::runtime_error("intentional");
		});
	}
	catch (const std::runtime_error&)
	{
		// expected
	}

	// The value was modified to 999 before the throw — the lock was released properly
	// and the envelope is still usable (not deadlocked)
	int val = envelope.observe<int>([](const auto& doc) { return doc.value("value", 0); });
	EXPECT_EQ(999, val);

	// Verify the envelope is not deadlocked by doing another mutate
	envelope.mutate<void>([](auto& doc) { doc["value"] = 300; });
	EXPECT_EQ(300, envelope.observe<int>([](const auto& doc) { return doc.value("value", 0); }));
}


// Edge: throwing callback in observe should not deadlock
TEST(edge, ThrowingObserveCallback)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"x", 42}}));

	try
	{
		envelope.observe<void>([](const auto& doc) { throw std::runtime_error("observe throw"); });
	}
	catch (const std::runtime_error&)
	{
		// expected
	}

	// Verify not deadlocked — can still read and write
	EXPECT_EQ(42, envelope.observe<int>([](const auto& doc) { return doc.value("x", 0); }));
	envelope.mutate<void>([](auto& doc) { doc["x"] = 43; });
	EXPECT_EQ(43, envelope.observe<int>([](const auto& doc) { return doc.value("x", 0); }));
}


// Edge: snapshot returns an independent copy that is not affected by later mutations
TEST(edge, SnapshotIsIndependentCopy)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"val", 1}}));

	auto snap1 = envelope.snapshot();
	envelope.mutate<void>([](auto& doc) { doc["val"] = 2; });
	auto snap2 = envelope.snapshot();
	envelope.mutate<void>([](auto& doc) { doc["val"] = 3; });

	// snap1 and snap2 should be independent of current state
	EXPECT_EQ(1, snap1.value("val", 0));
	EXPECT_EQ(2, snap2.value("val", 0));
	EXPECT_EQ(3, envelope.observe<int>([](const auto& doc) { return doc.value("val", 0); }));
}


// Edge: multiple reassigns overwrite correctly
TEST(edge, MultipleReassigns)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope;

	for (int i = 0; i < 10; i++)
	{
		nlohmann::json doc {{"iteration", i}};
		envelope.reassign(std::move(doc));
		EXPECT_TRUE(doc.empty());
		EXPECT_EQ(i, envelope.observe<int>([](const auto& d) { return d.value("iteration", -1); }));
	}
}


// ---------------------------------------------------------------------------
// Race condition / stress tests
// ---------------------------------------------------------------------------

// Race: writers increment a monotonic counter; readers verify it never goes backwards
TEST(stress, MonotonicCounterIntegrity)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	const uint32_t            ITERATIONS   = 5000;
	const uint32_t            WRITER_COUNT = 4;
	const uint32_t            READER_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          integrityViolation {false};
	std::vector<std::jthread> threads;

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

	for (uint32_t i = 0; i < READER_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					int lastSeen = 0;
					for (uint32_t j = 0; j < ITERATIONS * 2; j++)
					{
						int current = envelope.observe<int>([](const auto& doc) { return doc.value("counter", 0); });
						if (current < lastSeen) { integrityViolation.store(true); }
						lastSeen = current;
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(integrityViolation.load()) << "Counter went backwards �� torn read/write detected";
	int finalCounter = envelope.observe<int>([](const auto& doc) { return doc.value("counter", 0); });
	EXPECT_EQ(static_cast<int>(WRITER_COUNT * ITERATIONS), finalCounter);
}


// Race: snapshot must return internally consistent state (paired fields always equal)
TEST(stress, SnapshotConsistency)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"a", 0}, {"b", 0}}));

	const uint32_t            ITERATIONS   = 5000;
	const uint32_t            WRITER_COUNT = 4;
	const uint32_t            READER_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          inconsistencyFound {false};
	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < WRITER_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<void>([](auto& doc) {
							auto next = doc.value("a", 0) + 1;
							doc["a"]  = next;
							doc["b"]  = next;
						});
					}
				}));
	}

	for (uint32_t i = 0; i < READER_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS * 2; j++)
					{
						auto snap = envelope.snapshot();
						if (snap.value("a", -1) != snap.value("b", -2)) { inconsistencyFound.store(true); }
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(inconsistencyFound.load()) << "snapshot() returned inconsistent state (a != b)";
}


// Race: reassign() racing with observe() and snapshot()
TEST(stress, ConcurrentReassignVsObserve)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"version", 0}, {"data", "init"}}));

	const uint32_t            ITERATIONS   = 2000;
	const uint32_t            READER_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          corruptionFound {false};
	std::vector<std::jthread> threads;

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

	for (uint32_t i = 0; i < READER_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS * 2; j++)
					{
						auto snap = envelope.snapshot();
						int  ver  = snap.value("version", -1);
						auto data = snap.value("data", std::string(""));
						if (ver == 0)
						{
							if (data != "init") corruptionFound.store(true);
						}
						else if (ver > 0)
						{
							if (data != std::string("v") + std::to_string(ver)) corruptionFound.store(true);
						}
						else
						{
							corruptionFound.store(true);
						}
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(corruptionFound.load()) << "reassign() produced inconsistent state visible to readers";
	int finalVersion = envelope.observe<int>([](const auto& doc) { return doc.value("version", -1); });
	EXPECT_EQ(static_cast<int>(ITERATIONS), finalVersion);
}


// Race: zero-sleep maximum contention — no delays, all threads hammer the lock
TEST(stress, HighContentionZeroSleep)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"sum", 0}}));

	const uint32_t            ITERATIONS   = 10000;
	const uint32_t            WRITER_COUNT = 8;
	const uint32_t            READER_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_uint64_t      totalReads {0};
	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < WRITER_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<void>([](auto& doc) { doc["sum"] = doc.value("sum", 0) + 1; });
					}
				}));
	}

	for (uint32_t i = 0; i < READER_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						int val = envelope.observe<int>([](const auto& doc) { return doc.value("sum", -1); });
						EXPECT_GE(val, 0);
						totalReads.fetch_add(1, std::memory_order_relaxed);
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	int finalSum = envelope.observe<int>([](const auto& doc) { return doc.value("sum", -1); });
	EXPECT_EQ(static_cast<int>(WRITER_COUNT * ITERATIONS), finalSum);
	EXPECT_EQ(static_cast<uint64_t>(READER_COUNT) * ITERATIONS, totalReads.load());
}


// Race: all 5 API methods used concurrently on the same envelope
TEST(stress, MixedApiConcurrency)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"counter", 0}}));

	const uint32_t            ITERATIONS = 2000;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          failure {false};
	std::atomic_uint32_t      mutateCount {0};
	std::atomic_uint32_t      writeLockCount {0};
	std::vector<std::jthread> threads;

	// mutate() writers
	for (uint32_t i = 0; i < 4; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<void>([&](auto& doc) {
							doc["counter"] = doc.value("counter", 0) + 1;
							mutateCount.fetch_add(1, std::memory_order_relaxed);
						});
					}
				}));
	}

	// writeLock() writers
	for (uint32_t i = 0; i < 4; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						if (auto [doc, wl] = envelope.writeLock(); wl)
						{
							doc["counter"] = doc.value("counter", 0) + 1;
							writeLockCount.fetch_add(1, std::memory_order_relaxed);
						}
					}
				}));
	}

	// observe() readers
	for (uint32_t i = 0; i < 4; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						int val = envelope.observe<int>([](const auto& doc) { return doc.value("counter", -1); });
						if (val < 0) failure.store(true);
					}
				}));
	}

	// readLock() readers
	for (uint32_t i = 0; i < 4; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						if (auto const& [doc, rl] = envelope.readLock(); rl)
						{
							if (doc.value("counter", -1) < 0) failure.store(true);
						}
					}
				}));
	}

	// snapshot() readers
	for (uint32_t i = 0; i < 4; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						auto snap = envelope.snapshot();
						if (snap.value("counter", -1) < 0) failure.store(true);
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(failure.load()) << "A reader saw a negative counter — data corruption";
	int      finalCounter = envelope.observe<int>([](const auto& doc) { return doc.value("counter", -1); });
	uint32_t totalWrites  = mutateCount.load() + writeLockCount.load();
	EXPECT_EQ(static_cast<int>(totalWrites), finalCounter)
			<< "Final counter (" << finalCounter << ") != total writes (" << totalWrites << ")";
}


// Race: verify readWriteActions counter matches exact mutate() count under concurrency
TEST(stress, RwaCounterAccuracy)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"x", 0}}));

	const uint32_t            ITERATIONS   = 3000;
	const uint32_t            WRITER_COUNT = 6;
	const uint32_t            READER_COUNT = 6;
	std::atomic_uint          startSignal {0};
	std::vector<std::jthread> threads;

	for (uint32_t i = 0; i < WRITER_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<void>([](auto& doc) { doc["x"] = doc.value("x", 0) + 1; });
					}
				}));
	}

	for (uint32_t i = 0; i < READER_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.observe<int>([](const auto& doc) { return doc.value("x", 0); });
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	auto     info = nlohmann::json(envelope);
	uint64_t rwa  = info.value("readWriteActions", static_cast<uint64_t>(0));
	EXPECT_EQ(static_cast<uint64_t>(WRITER_COUNT) * ITERATIONS, rwa)
			<< "rwa counter (" << rwa << ") != expected (" << WRITER_COUNT * ITERATIONS << ")";

	int finalX = envelope.observe<int>([](const auto& doc) { return doc.value("x", 0); });
	EXPECT_EQ(static_cast<int>(WRITER_COUNT * ITERATIONS), finalX);
}


// Race: concurrent observe with non-void return under write contention
TEST(stress, ConcurrentObserveWithReturn)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"value", 0}}));

	const uint32_t            ITERATIONS   = 5000;
	const uint32_t            THREAD_COUNT = 8;
	std::atomic_uint          startSignal {0};
	std::atomic_bool          failure {false};
	std::vector<std::jthread> threads;

	// Writers
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						envelope.mutate<void>([](auto& doc) { doc["value"] = doc.value("value", 0) + 1; });
					}
				}));
	}

	// Readers that return values — verify return is always non-negative
	for (uint32_t i = 0; i < THREAD_COUNT / 2; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						std::string s = envelope.observe<std::string>([](const auto& doc) { return doc.dump(); });
						if (s.empty()) failure.store(true);

						int v = envelope.observe<int>([](const auto& doc) { return doc.value("value", -1); });
						if (v < 0) failure.store(true);

						bool has = envelope.observe<bool>([](const auto& doc) { return doc.contains("value"); });
						if (!has) failure.store(true);
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_FALSE(failure.load()) << "observe with return value produced invalid result under contention";
}


// Race: concurrent readLock readers should not block each other (shared lock)
TEST(stress, SharedReadLockConcurrency)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> envelope(nlohmann::json({{"data", "immutable"}}));

	const uint32_t            READER_COUNT = 16;
	const uint32_t            ITERATIONS   = 10000;
	std::atomic_uint          startSignal {0};
	std::atomic_uint64_t      totalReads {0};
	std::vector<std::jthread> threads;

	// All readers — no writers. All should run concurrently without blocking.
	for (uint32_t i = 0; i < READER_COUNT; i++)
	{
		threads.push_back(std::jthread(
				[&]()
				{
					startSignal.wait(0);
					for (uint32_t j = 0; j < ITERATIONS; j++)
					{
						if (auto const& [doc, rl] = envelope.readLock(); rl)
						{
							EXPECT_EQ("immutable", doc.value("data", ""));
							totalReads.fetch_add(1, std::memory_order_relaxed);
						}
					}
				}));
	}

	startSignal = 1;
	startSignal.notify_all();
	threads.clear();

	EXPECT_EQ(static_cast<uint64_t>(READER_COUNT) * ITERATIONS, totalReads.load());
}


TEST(tests, MoveConstructor)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> docl({{"foo", "bar"}, {"few", "lar"}});

	// Check we have pre-change value..
	EXPECT_EQ("bar", docl.observe<std::string>([](const auto& doc) { return doc.value("foo", ""); }));

	// Modify the item
	docl.mutate<void>([](auto& doc) { doc["foo"] = "bare"; });

	// Check we have pre-change value.. Note that here we return a boolean to avoid data copy
	EXPECT_TRUE(docl.observe<bool>([](const auto& doc) { return doc.value("foo", "").find("bare") == 0; }));

	// Check to make sure that the statistics match
	auto info = nlohmann::json(docl);
	EXPECT_EQ(1, info.value("readWriteActions", 0));

	// Now, we "move" docl --> docl2
	siddiqsoft::RWLEnvelope<nlohmann::json> docl2(std::move(docl));

	// Must be empty since we moved it into the envelope
	EXPECT_TRUE(docl.observe<bool>([](const auto& doc) { return doc.empty(); }));

	// Check we have pre-change value..
	EXPECT_EQ("lar", docl2.observe<std::string>([](const auto& doc) { return doc.value("few", ""); }));

	// Modify the item
	docl2.mutate<void>([](auto& doc) { doc["few"] = "lare"; });

	// Check we have pre-change value.. Note that here we return a boolean to avoid data copy
	EXPECT_TRUE(docl2.observe<bool>(
			[](const auto& doc) { return (doc.value("foo", "").find("bare") == 0) && (doc.value("few", "").find("lare") == 0); }));

	// Check to make sure that the statistics match
	auto info2 = nlohmann::json(docl2);
	// We performed two mutates.. one from the previous object and one in the new copy
	EXPECT_EQ(2, info2.value("readWriteActions", 0));
}
