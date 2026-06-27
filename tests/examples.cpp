#include "gtest/gtest.h"
#include <atomic>
#include <format>
#include "nlohmann/json.hpp"
#include "../include/siddiqsoft/RWLEnvelope.hpp"

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
TEST(examples, WithCallbacks)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> docl({{"foo", "bar"}, {"few", "lar"}});

	// Check we have pre-change value..
	EXPECT_EQ("bar", docl.observe([](const auto& doc) noexcept { return doc.value("foo", ""); }));

	// Modify the item
	docl.mutate([](auto& doc) noexcept { doc["foo"] = "bare"; });

	// Check we have pre-change value.. Note that here we return a boolean to avoid data copy
	EXPECT_TRUE(docl.observe([](const auto& doc) noexcept { return doc.value("foo", "").find("bare") == 0; }));

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
	if (auto [doc, wl] = docl.writeLock(); wl) { doc["foo"] = "bare"; };

	// Check we have post-change value.. Note that here we return a boolean to minimize data copy
	if (const auto& [doc, rl] = docl.readLock(); rl) { EXPECT_TRUE(doc.value("foo", "").find("bare") == 0); }
}


TEST(examples, AssignWithCallbacks)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> docl; // we will assign later
	nlohmann::json                          doc2 {{"baa", 0x0baa}, {"fee", 0x0fee}, {"bee", 0x0bee}};

	// Move assign here post init
	docl.reassign(std::move(doc2));
	// Must be empty since we moved it into the envelope
	EXPECT_TRUE(doc2.empty());

	// Check we have pre-change value.. Note that here we return a boolean to avoid data copy
	EXPECT_TRUE(docl.observe(
			[](const auto& doc) noexcept -> bool
			{
				return (doc.value("fee", 0xfa17) == 0x0fee) && (doc.value("baa", 0xfa17) == 0x0baa) &&
		               (doc.value("bee", 0xfa17) == 0x0bee);
			}));

	EXPECT_EQ(3, docl.observe([](const auto& doc) noexcept { return doc.size(); }));
}


TEST(examples, AssignWithDirectLocks)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> docl({{"foo", "bar"}, {"few", "lar"}});
	nlohmann::json                          doc2 {{"baa", 0x0baa}, {"fee", 0x0fee}, {"bee", 0x0bee}};

	// Previous document has two items..
	if (auto const& [doc, rl] = docl.readLock(); rl) { EXPECT_EQ(2, doc.size()); }

	// Modify the item (replace the initial with new)
	if (auto [doc, wl] = docl.writeLock(); wl) { doc = std::move(doc2); };

	//doc2 -> Must be empty since we moved it into the envelope
	EXPECT_TRUE(doc2.empty());

	// Check we have post-change value..
	if (const auto& [doc, rl] = docl.readLock(); rl) { EXPECT_EQ(3, doc.size()); }
}

TEST(examples, CallbacksWithGlobalArgs)
{
	struct Demo
	{
		int val {0};
	};

	std::atomic_uint              counter {0};
	siddiqsoft::RWLEnvelope<Demo> safeDemo;

	auto localValue = safeDemo.mutate(
			[](auto& dd, auto& theCounter) noexcept
			{
				// Store the current value..
				dd.val = theCounter.load();
				// Increment the global counter..
				theCounter++;

				return dd.val;
			},
			counter);
	// The global counter must be 1..
	EXPECT_EQ(1, counter.load());
	// The local internal should be 0..
	EXPECT_EQ(0, localValue);

	auto localValue2nd = safeDemo.observe(
			[](const auto& dd, auto& theCounter) noexcept
			{
				theCounter++;
				return dd.val;
			},
			counter);
	// The global counter must be 1..
	EXPECT_EQ(2, counter.load());
	// The local internal should be 0..
	EXPECT_EQ(0, localValue2nd);
}
// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
