RWLEnvelope : A simple read-writer lock wrapper for modern C++
-------------------------------------------

[![CodeQL](https://github.com/SiddiqSoft/RWLEnvelope/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/SiddiqSoft/RWLEnvelope/actions/workflows/codeql-analysis.yml)
[![Build Status](https://dev.azure.com/siddiqsoft/siddiqsoft/_apis/build/status/SiddiqSoft.RWLEnvelope?branchName=main)](https://dev.azure.com/siddiqsoft/siddiqsoft/_build/latest?definitionId=6&branchName=main)
![](https://img.shields.io/nuget/v/SiddiqSoft.RWLEnvelope)
![](https://img.shields.io/github/v/tag/SiddiqSoft/RWLEnvelope)
![](https://img.shields.io/azure-devops/tests/siddiqsoft/siddiqsoft/6)
![](https://img.shields.io/azure-devops/coverage/siddiqsoft/siddiqsoft/6)

# Objective
- Avoid re-implementing the rw-lock; standard C++ (since C++14) has a good reader-writer lock implementation.
- Provide a simple, convenience layer atop the underlying [`std::unique_lock`](https://en.cppreference.com/w/cpp/thread/unique_lock) and [`std::shared_lock`](https://en.cppreference.com/w/cpp/thread/shared_lock) access to some type.

> **WE DO NOT IMPLEMENT** a read-writer lock, rather we provide a simple pattern
> where the most frequent use of the underlying facility is in a neat package.


# Requirements
You must be able to use `<shared_mutex>` and `<mutex>`

The build and tests are for Visual Studio 2019 under x64.

# Usage

- Use the nuget [SiddiqSoft.RWLEnvelope](https://www.nuget.org/packages/SiddiqSoft.RWLEnvelope/)
- Copy paste..whatever works.
- Two methods:
  -- Observer/mutator model with callback and custom return
- 

```cpp
#include "gtest/gtest.h"

#include "nlohmann/json.hpp"
#include "../src/RWLEnvelope.hpp"


TEST(examples, WithCallbacks)
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
}


TEST(examples, WithDirectLocks)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> docl({{"foo", "bar"}, {"few", "lar"}});

	// Check we have pre-change value..
	if (const auto& [doc, rl] = docl.readLock(); rl) { EXPECT_EQ("bar", doc.value("foo", "")); }

	// Modify the item
	if (auto& [doc, wl] = docl.writeLock(); wl) { doc["foo"] = "bare"; };

	// Check we have pre-change value.. Note that here we return a boolean to avoid data copy
	if (const auto& [doc, rl] = docl.readLock(); rl) { EXPECT_TRUE(doc.value("foo", "").find("bare") == 0); }
}

```



<small align="right">

&copy; 2021 Siddiq Software LLC. All rights reserved.

</small>
