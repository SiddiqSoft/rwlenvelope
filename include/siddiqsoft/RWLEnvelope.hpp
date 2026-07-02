/*
	RWLEnvelope
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

#pragma once

/// @file RWLEnvelope.hpp
/// @brief Thread-safe envelope wrapper using reader-writer locks
/// @details
/// RWLEnvelope provides a simple, convenient envelope-access model for thread-safe access to objects
/// using reader-writer locks. It wraps a type T with std::shared_mutex to enable safe concurrent
/// read and exclusive write access patterns.
///
/// @section requirements Requirements
/// - C++ Standard: C++20 or later (requires C++20 concepts support)
/// - Required Headers: <shared_mutex>, <functional>, <tuple>, <utility>, <concepts>
/// - Compiler Support: Must support [[nodiscard]] attribute and C++20 concepts
/// - Optional: nlohmann/json library for JSON serialization support
///
/// @section usage Quick Start
/// @code
/// siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;
/// data.observe([](const auto& m) noexcept { return m.find("key") != m.end(); });
/// data.mutate([](auto& m) noexcept { m["key"] = 42; });
/// @endcode
///

#ifndef RWLENVELOPE_HPP
#define RWLENVELOPE_HPP

#include <functional>
#include <shared_mutex>
#include <tuple>
#include <utility>
#include <concepts>
#include <exception>
#include <type_traits>

#if !__has_cpp_attribute(nodiscard)
#error "We really should have [[nodiscard]]"
#endif

#if !__cpp_lib_shared_mutex
#error "Must have <shared_mutex>"
#endif

namespace siddiqsoft
{
	/// @brief Concept to enforce that callbacks do not throw exceptions (for const/read-only access)
	/// @details
	/// This concept ensures that callbacks used with observe() are:
	/// - Marked with noexcept specification
	/// - Accept const ContainerType& as the first parameter
	/// - Can accept additional arguments via Args...
	/// @tparam ContainerType The type of the container being accessed
	/// @tparam Callback The callback type to check
	/// @tparam Args The argument types to pass to the callback
	/// @see observe()
	template <typename ContainerType, typename Callback, typename... Args>
	concept ObserveCallbackNoexcept = requires(Callback f, const ContainerType& item, Args... args) {
		{ f(item, std::forward<Args>(args)...) } noexcept;
	};

	/// @brief Concept to enforce that callbacks do not throw exceptions (for mutable access)
	/// @details
	/// This concept ensures that callbacks used with mutate() are:
	/// - Marked with noexcept specification
	/// - Accept ContainerType& (mutable reference) as the first parameter
	/// - Can accept additional arguments via Args...
	/// @tparam ContainerType The type of the container being accessed
	/// @tparam Callback The callback type to check
	/// @tparam Args The argument types to pass to the callback
	/// @see mutate()
	template <typename ContainerType, typename Callback, typename... Args>
	concept MutateCallbackNoexcept = requires(Callback f, ContainerType& item, Args... args) {
		{ f(item, std::forward<Args>(args)...) } noexcept;
	};


	/// @class RWLEnvelope
	/// @brief Thread-safe envelope wrapper using reader-writer locks
	/// @tparam ContainerType Type for your object which is to be "enveloped"
	/// @details
	/// RWLEnvelope provides a simple, convenient envelope-access model for thread-safe access to objects
	/// using reader-writer locks. It wraps a type ContainerType with std::shared_mutex to enable safe
	/// concurrent read and exclusive write access patterns.
	///
	/// @section thread_safety Thread Safety Guarantees
	/// - Multiple Readers: Multiple threads can call observe() or readLock() concurrently
	/// - Exclusive Writer: Only one thread can call mutate() or writeLock() at a time
	/// - Reader-Writer Exclusion: No readers can access while a writer is active, and vice versa
	/// - Exception Safe: Move constructor is noexcept; mutate() includes exception handling
	///
	/// @section usage_patterns Usage Patterns
	/// @subsection pattern_callback Callback-based Access
	/// @code
	/// siddiqsoft::RWLEnvelope<std::map<std::string, int>> cache;
	/// bool exists = cache.observe([](const auto& m) noexcept {
	///     return m.count("key") > 0;
	/// });
	/// cache.mutate([](auto& m) noexcept {
	///     m["key"] = 42;
	/// });
	/// @endcode
	///
	/// @subsection pattern_lock Direct Lock Access
	/// @code
	/// if (auto const& [map, lock] = data.readLock(); lock) {
	///     auto it = map.find("key");
	///     if (it != map.end()) {
	///         std::cout << it->second << std::endl;
	///     }
	/// }
	/// @endcode
	///
	/// @subsection pattern_snapshot Snapshot for External Processing
	/// @code
	/// std::vector<int> copy = data.snapshot();
	/// std::sort(copy.begin(), copy.end());
	/// @endcode
	///
	/// @subsection pattern_args Callbacks with Additional Arguments
	/// @code
	/// std::string searchKey = "target";
	/// bool found = data.observe(
	///     [](const auto& m, const std::string& key) noexcept {
	///         return m.find(key) != m.end();
	///     },
	///     searchKey
	/// );
	/// @endcode
	///
	/// @section performance Performance Considerations
	/// - Keep callbacks and lock scopes as short as possible
	/// - Avoid I/O operations (file, network) within locks
	/// - Use snapshot() for expensive post-processing
	/// - Use observe() or readLock() to avoid copying
	/// - Direct callbacks eliminate std::function overhead
	///
	/// @section limitations Limitations
	/// - Copy assignment is deleted; use move semantics
	/// - Default constructor requires T to be default-constructible
	/// - Mutex is not shared when moving envelopes
	/// - No recursive locking (will deadlock)
	/// - All callbacks must be marked noexcept
	template <typename ContainerType>
		requires std::copy_constructible<ContainerType>
	class RWLEnvelope
	{
		using RWLock = std::unique_lock<std::shared_mutex>;
		using RLock  = std::shared_lock<std::shared_mutex>;

	private:
		/// @brief Private helper to implement "run on exit" semantics
		/// @details
		/// This RAII helper increments a counter when destroyed, used to track
		/// the number of mutate() callbacks executed.
		struct rone
		{
			/// @brief Reference to the counter to increment on destruction
			uint64_t& m_Dest;

			/// @brief Constructor to hold reference to target counter
			/// @param dest Reference to the counter that will be incremented on destruction
			/// @details
			/// Stores a reference to the counter that will be incremented when this
			/// object is destroyed, implementing the "run on exit" pattern.
			rone(uint64_t& dest)
				: m_Dest(dest)
			{
			}

			/// @brief The destructor increments the counter
			~rone() { ++m_Dest; }
		};

	public:
		/// @brief Default constructor
		/// @details
		/// Creates an envelope with a default-constructed instance of type ContainerType.
		/// Use reassign() to initialize the underlying storage later if the type doesn't
		/// have a default constructor.
		/// @note Requires ContainerType to be default-constructible
		/// @see reassign()
		/// @example
		/// @code
		/// siddiqsoft::RWLEnvelope<std::map<std::string, int>> myMap;
		/// @endcode
		explicit RWLEnvelope()
			: _item(ContainerType {})
		{
		}


		/// @brief Move constructor from another RWLEnvelope
		/// @param src The source envelope to move from
		/// @details
		/// Moves the contents of another envelope into a new envelope. The source envelope's
		/// data is transferred, but each envelope maintains its own mutex.
		/// @note The underlying mutexes are **not** shared. The new object gets its own mutex
		///       while the source retains its mutex.
		/// @note This operation is noexcept
		/// @see RWLEnvelope(ContainerType&&)
		/// @example
		/// @code
		/// siddiqsoft::RWLEnvelope<std::string> env1(std::string("hello"));
		/// siddiqsoft::RWLEnvelope<std::string> env2(std::move(env1));
		/// @endcode
		explicit RWLEnvelope(RWLEnvelope<ContainerType>&& src) noexcept
		{
			try
			{
				// Execute within the lock of the source object
				if (auto [o, wl] = src.writeLock(); wl)
				{
					// Move the internal data from source
					_item = std::move(o);
					// Transfer the counter from source to this object
					_rwa = std::exchange(src._rwa, 0);
				}
			}
			catch (...)
			{
				// cannot throw
			}
		}


		/// @brief Move constructor from ContainerType
		/// @param src The source object to move into the envelope
		/// @details
		/// Creates an envelope by moving the provided object into the envelope.
		/// @note The source object will be moved from and left in a valid but unspecified state
		/// @see RWLEnvelope(RWLEnvelope<ContainerType>&&)
		/// @example
		/// @code
		/// std::vector<int> data = {1, 2, 3};
		/// siddiqsoft::RWLEnvelope<std::vector<int>> envelope(std::move(data));
		/// // data is now empty
		/// @endcode
		explicit RWLEnvelope(ContainerType&& src)
		{
			_item = std::move(src);
		}


		/// @brief Replace the enclosed object with a new one
		/// @param src The new object to move into the envelope
		/// @details
		/// Replaces the enclosed object with a new one by moving the provided object into
		/// the envelope. Acquires an exclusive lock during the operation.
		/// @note Acquires an exclusive (writer) lock
		/// @see RWLEnvelope(ContainerType&&)
		/// @example
		/// @code
		/// siddiqsoft::RWLEnvelope<std::vector<int>> data;
		/// std::vector<int> newData = {1, 2, 3, 4, 5};
		/// data.reassign(std::move(newData));
		/// // newData is now empty
		/// @endcode
		void reassign(ContainerType&& src)
		{
			RWLock myWriterLock(_sMutex);
			_item = std::move(src);
		}


		/// @brief Copy constructor from ContainerType
		/// @param arg The source object to copy into the envelope
		/// @details
		/// Creates an envelope by copying the provided object into the envelope.
		/// Acquires an exclusive lock during the copy operation.
		/// @note Acquires an exclusive (writer) lock
		/// @note Requires ContainerType to be copy-constructible
		/// @see RWLEnvelope(ContainerType&&)
		/// @example
		/// @code
		/// std::map<std::string, int> original = {{"key", 42}};
		/// siddiqsoft::RWLEnvelope<std::map<std::string, int>> envelope(original);
		/// // original is unchanged
		/// @endcode
		explicit RWLEnvelope(const ContainerType& arg)
		{
			RWLock myWriterLock(_sMutex);
			_item = arg;
		}


		/// @brief Copy assignment operator (deleted)
		/// @details
		/// Copy assignment is explicitly deleted to enforce move semantics and prevent
		/// accidental expensive copies.
		/// @note Use move semantics or reassign() instead
		RWLEnvelope& operator=(RWLEnvelope const&) = delete;


		/// @brief Perform a read-only operation on the enclosed object
		/// @tparam Callback The callback type (must satisfy ObserveCallbackNoexcept concept)
		/// @tparam Args Additional arguments to forward to the callback
		/// @param cbf The callback function that accepts const ContainerType& as first parameter (must be noexcept)
		/// @param args Additional arguments to pass to the callback
		/// @return The return value from the callback
		/// @details
		/// Performs a read-only operation on the enclosed object using a shared (reader) lock.
		/// Multiple threads can execute observe() concurrently. The callback must be marked
		/// noexcept and accept const ContainerType& as the first parameter, followed by any
		/// additional arguments.
		/// @note The callback MUST be marked noexcept. Callbacks that may throw will not compile.
		/// @note Acquires a shared lock; multiple readers can access simultaneously
		/// @note Keep callbacks short to minimize lock duration
		/// @see readLock(), mutate()
		/// @example
		/// @code
		/// siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;
		/// bool found = data.observe([](const auto& map) noexcept {
		///     return map.find("key") != map.end();
		/// });
		/// @endcode
		template <typename Callback, typename... Args>
			requires ObserveCallbackNoexcept<ContainerType, Callback, Args...>
		auto observe(Callback cbf, Args&&... args) const
		{
			RLock myLock(_sMutex);
			return cbf(_item, std::forward<Args>(args)...);
		}


		/// @brief Perform a read-write operation on the enclosed object
		/// @tparam Callback The callback type (must satisfy MutateCallbackNoexcept concept)
		/// @tparam Args Additional argument types
		/// @param cbf The callback function that accepts ContainerType& as first parameter (must be noexcept)
		/// @param args Additional arguments to pass to the callback
		/// @return The return value from the callback
		/// @details
		/// Performs a read-write operation on the enclosed object using an exclusive (writer) lock.
		/// Only one thread can execute mutate() at a time, and no observe() calls can run concurrently.
		/// The callback must be marked noexcept and accept ContainerType& as the first parameter,
		/// followed by any additional arguments.
		/// @note The callback MUST be marked noexcept. Callbacks that may throw will not compile.
		/// @note Acquires an exclusive lock; blocks all other readers and writers
		/// @note Keep callbacks short to minimize lock contention
		/// @note Avoid I/O operations within the callback
		/// @note Exception handling is included for callbacks that may throw despite noexcept requirement
		/// @see writeLock(), observe()
		/// @example
		/// @code
		/// siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;
		/// data.mutate([](auto& map) noexcept {
		///     map["key"] = 42;
		/// });
		/// int newSize = data.mutate([](auto& map) noexcept {
		///     map["another"] = 100;
		///     return map.size();
		/// });
		/// @endcode
        // NOLINTBEGIN(clang-diagnostic-return-type)
		template <typename Callback, typename... Args>
			requires MutateCallbackNoexcept<ContainerType, Callback, Args...>
		auto mutate(Callback cbf, Args&&... args)
		{
			RWLock myWriterLock(_sMutex);
			rone   d(_rwa); // we increment the housekeeping counter on each callback

			try
			{
				return cbf(_item, std::forward<Args>(args)...);
			}
			catch (...)
			{
			}
		}
        // NOLINTEND(clang-diagnostic-return-type)


		/// @brief Returns a copy of the underlying object
		/// @return A copy of the enclosed object
		/// @details
		/// Returns a copy of the underlying object. Acquires a shared lock during the copy operation.
		/// This is useful when you need to work with the data outside of a lock scope.
		/// @note Acquires a shared lock
		/// @note Requires ContainerType to be copy-constructible
		/// @note The [[nodiscard]] attribute encourages proper use of the returned value
		/// @note Use this when you need to perform expensive operations without holding the lock
		/// @see observe(), readLock()
		/// @example
		/// @code
		/// siddiqsoft::RWLEnvelope<std::vector<int>> data;
		/// std::vector<int> copy = data.snapshot();
		/// std::sort(copy.begin(), copy.end());
		/// @endcode
		[[nodiscard]] ContainerType snapshot() const
		{
			RLock myLock(_sMutex);
			return _item;
		}


		/// @brief Acquires a shared (reader) lock and returns it with a const reference to the object
		/// @return A tuple containing:
		///         - const ContainerType&: A const reference to the enclosed object
		///         - RLock: A std::shared_lock<std::shared_mutex> that remains locked within the scope
		/// @details
		/// Acquires a shared (reader) lock and returns both the lock and a const reference to the
		/// enclosed object. Intended for use with structured bindings in an if-statement or scoped block.
		/// Multiple threads can execute readLock() concurrently.
		/// @note The lock is automatically released when the scope exits
		/// @note Use within a statement block; the lock is tied to the scope
		/// @note Avoid I/O operations within the lock
		/// @note Multiple readers can access simultaneously
		/// @note The [[nodiscard]] attribute encourages proper use of the returned value
		/// @see writeLock(), observe()
		/// @example
		/// @code
		/// siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;
		/// if (auto const& [map, lock] = data.readLock(); lock) {
		///     auto it = map.find("key");
		///     if (it != map.end()) {
		///         std::cout << it->second << std::endl;
		///     }
		/// } // lock is released here
		/// @endcode
		[[nodiscard]] std::tuple<const ContainerType&, RLock> readLock() { return {std::ref(_item), RLock(_sMutex)}; }


		/// @brief Acquires an exclusive (writer) lock and returns it with a mutable reference to the object
		/// @return A tuple containing:
		///         - ContainerType&: A mutable reference to the enclosed object
		///         - RWLock: A std::unique_lock<std::shared_mutex> that remains locked within the scope
		/// @details
		/// Acquires an exclusive (writer) lock and returns both the lock and a mutable reference to the
		/// enclosed object. Intended for use with structured bindings in an if-statement or scoped block.
		/// Only one thread can execute writeLock() at a time, and no readers can access concurrently.
		/// @note The lock is automatically released when the scope exits
		/// @note Use within a statement block; the lock is tied to the scope
		/// @note Avoid I/O operations within the lock
		/// @note Only one writer can access at a time; blocks all readers
		/// @note The [[nodiscard]] attribute encourages proper use of the returned value
		/// @see readLock(), mutate()
		/// @example
		/// @code
		/// siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;
		/// if (auto [map, lock] = data.writeLock(); lock) {
		///     map["key"] = 42;
		///     map.erase("old_key");
		/// } // lock is released here
		/// @endcode
		[[nodiscard]] std::tuple<ContainerType&, RWLock> writeLock() { return {std::ref(_item), RWLock(_sMutex)}; }


#ifdef INCLUDE_NLOHMANN_JSON_HPP_
		/// @brief JSON conversion operator (optional)
		/// @return A JSON object containing the envelope's state
		/// @details
		/// If the nlohmann/json library is included in the project, this operator provides
		/// a JSON representation of the envelope. The returned JSON object contains:
		/// - "storage": The enclosed object serialized to JSON
		/// - "_typver": Version string "RWLEnvelope/1.5.1"
		/// - "readWriteActions": Internal counter tracking the number of mutate() callbacks executed
		/// @note Only available if INCLUDE_NLOHMANN_JSON_HPP_ is defined
		/// @note Acquires a shared lock
		/// @see snapshot()
		/// @example
		/// @code
		/// #include "nlohmann/json.hpp"
		/// #include "siddiqsoft/RWLEnvelope.hpp"
		/// siddiqsoft::RWLEnvelope<nlohmann::json> doc({{"key", "value"}});
		/// nlohmann::json representation = doc;
		/// std::cout << representation.dump(2) << std::endl;
		/// @endcode
	public:
		operator nlohmann::json() const
		{
			RLock myLock(_sMutex);
			return {{"storage", _item}, {"_typver", "RWLEnvelope/1.5.1"}, {"readWriteActions", _rwa}};
		}
#endif

	private:
		/// @brief The underlying enclosed object
		/// @details
		/// Stores the actual object being managed by the envelope. Access to this member
		/// is protected by the _sMutex shared mutex.
		ContainerType _item;

		/// @brief The shared mutex protecting access to _item
		/// @details
		/// This std::shared_mutex provides the synchronization mechanism for reader-writer
		/// lock semantics. It allows multiple concurrent readers or a single exclusive writer.
		mutable std::shared_mutex _sMutex {};

		/// @brief Internal counter tracking mutate() callback executions
		/// @details
		/// This counter is incremented each time a mutate() callback completes successfully.
		/// It provides a way to track the number of write operations performed on the envelope.
		/// The counter is included in the JSON representation when using the JSON conversion operator.
		/// @note This counter is not thread-safe on its own; it's only accessed within
		///       the exclusive lock of mutate()
		/// @see mutate(), operator nlohmann::json()
		uint64_t _rwa {0};
	};
} // namespace siddiqsoft

#endif // !RWLENVELOPE_HPP
