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
#define _WIN32_WINNT 0x0A00
#include <windows.h>

#include <thread>
#include "gtest/gtest.h"

#include "nlohmann/json.hpp"
#include "../src/RWLEnvelope.hpp"

#include <processthreadsapi.h>

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
		readerPool.push_back(std::jthread([&]() {
			startSignal.wait(0);
			for (uint32_t j = 0; j < READ_LIMIT; j++)
			{
				myContainer.observe<void>([&](auto const& o) { readCounter++; });
				std::this_thread::sleep_for(std::chrono::nanoseconds(rand() % READ_LIMIT));
			}
			readerFinished++;
			readerFinished.notify_all();
		}));
	}

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		readerPool.push_back(std::jthread([&]() {
			unsigned tid = GetCurrentThreadId();
			startSignal.wait(0);
			for (uint32_t j = 0; j < WRITE_LIMIT; j++)
			{
				myContainer.mutate<void>([&writeCounter, tid, j](auto& o) {
					o["lastThreadId"] = tid;
					o["j"]            = j;
					o["writeCount"]   = ++writeCounter;
				});
				std::this_thread::sleep_for(std::chrono::nanoseconds(rand() % WRITE_LIMIT));
			}
			writerFinished++;
			writerFinished.notify_all();
		}));
	}

	// Let's signal threads to start!
	startSignal = 1;
	startSignal.notify_all();

	// Wait until all of the threads exit
	std::this_thread::sleep_for(2s);

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
		readerPool.push_back(std::jthread([&]() {
			startSignal.wait(0);
			for (uint32_t j = 0; j < READ_LIMIT; j++)
			{
				if (auto [o, rl] = myContainer.readLock(); rl) { readCounter++; }

				std::this_thread::sleep_for(std::chrono::milliseconds(rand() % READ_LIMIT));
			}
			readerFinished++;
			readerFinished.notify_all();
		}));
	}

	for (uint32_t i = 0; i < THREAD_COUNT; i++)
	{
		readerPool.push_back(std::jthread([&]() {
			auto tid = GetCurrentThreadId();
			startSignal.wait(0);
			for (uint32_t j = 0; j < WRITE_LIMIT; j++)
			{
				if (auto [o, rwl] = myContainer.writeLock(); rwl)
				{
					o["lastThreadId"] = tid;
					o["j"]            = j;
					o["writeCount"]   = ++writeCounter;
				};
				std::this_thread::sleep_for(std::chrono::milliseconds(rand() % WRITE_LIMIT));
			}
			writerFinished++;
			writerFinished.notify_all();
		}));
	}

	// Let's signal threads to start!
	startSignal = 1;
	startSignal.notify_all();

	// Wait until all of the threads exit
	std::this_thread::sleep_for(6s);

	myContainer.observe<void>([&](auto const& o) { std::cerr << std::format("{} - {}\n", __func__, o.dump()) << std::endl; });
	std::cerr << std::format("{} - {}\n", __func__, myContainer.snapshot().dump()) << std::endl;

	EXPECT_EQ(WRITE_LIMIT / 10, writerFinished.load()) << myContainer.snapshot().dump();
	EXPECT_EQ(READ_LIMIT / 10, readerFinished.load()) << myContainer.snapshot().dump();

	EXPECT_EQ(READ_LIMIT * THREAD_COUNT, readCounter.load()) << myContainer.snapshot().dump();
	EXPECT_EQ(WRITE_LIMIT * THREAD_COUNT, writeCounter.load()) << myContainer.snapshot().dump();
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
