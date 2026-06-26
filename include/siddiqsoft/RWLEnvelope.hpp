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

#include <type_traits>
#ifndef RWLENVELOPE_HPP
#define RWLENVELOPE_HPP

#include <functional>
#include <shared_mutex>
#include <tuple>
#include <utility>
#include <concepts>

#if !__has_cpp_attribute(nodiscard)
#error "We really should have [[nodiscard]]"
#endif

#if !__cpp_lib_shared_mutex
#error "Must have <shared_mutex>"
#endif

namespace siddiqsoft
{
	/// @brief Concept to enforce that callbacks do not throw exceptions (for const/read-only access)
	/// Callbacks must be marked noexcept and accept const T& as first parameter
	/// @tparam Callback The callback type to check
	/// @tparam Args The argument types to pass to the callback
	template <typename T, typename Callback, typename... Args>
	concept ObserveCallbackNoexcept = requires(Callback f, const T& item, Args... args) {
		{ f(item, std::forward<Args>(args)...) } noexcept;
	};

	/// @brief Concept to enforce that callbacks do not throw exceptions (for mutable access)
	/// Callbacks must be marked noexcept and accept T& as first parameter
	/// @tparam Callback The callback type to check
	/// @tparam Args The argument types to pass to the callback
	template <typename T, typename Callback, typename... Args>
	concept MutateCallbackNoexcept = requires(Callback f, T& item, Args... args) {
		{ f(item, std::forward<Args>(args)...) } noexcept;
	};


	/// @brief Implements a simple envelope-access model to make it easy for clients to use the reader-writer lock model.
	/// @tparam T Type for your object which is to be "enveloped"
	template <typename T>
		requires std::copy_constructible<T>
	class RWLEnvelope
	{
		using RWLock = std::unique_lock<std::shared_mutex>;
		using RLock  = std::shared_lock<std::shared_mutex>;

	private:
		/// @brief Private helper to implement "run on exit"
		struct rone
		{
			/// @brief Hold reference
			uint64_t& m_Dest;

			/// @brief Constructor to hold reference to target
			/// @param dest
			rone(uint64_t& dest)
				: m_Dest(dest)
			{
			}

			/// @brief The destructor is where we'd like to increment
			~rone() { ++m_Dest; }
		};

	public:
		/// @brief Default constructor. Use the reassign() method to initialize the underlying storage later.
		explicit RWLEnvelope()
			: _item(T {})
		{
		}


		/// @brief Implement a move constructor
		/// NOTE: The underlying mutex are *not* shared/moved!
		/// The new object gets it own mutex whereas the source retains its mutex.
		/// @param src The source is the other envelope of the same type
		explicit RWLEnvelope(RWLEnvelope<T>&& src) noexcept
		{
			try
			{
				// We must execute within the lock of the source object
				if (auto [o, wl] = src.writeLock(); wl)
				{
					// Move the internal data
					_item = std::move(o);
					// Move the counter (transfer src's value to us and zero out src)
					_rwa = std::exchange(src._rwa, 0);
					// Cannot "move" the mutex.
					// as we're using it! Moreover, once the lock releases
					// we can clear the calling object
				}
			}
			catch (...)
			{
				// cannot throw
			}
		}


		/// @brief Move constructor
		/// @param src source/contained object
		explicit RWLEnvelope(T&& src)
		{
			// Delegate to the underlying item type move assignment operator
			_item = std::move(src);
		}


		/// @brief Move assignment replace previous value
		/// @param src New source/contained object; the src will be reset/null'd
		void reassign(T&& src)
		{
			RWLock myWriterLock(_sMutex);
			// Delegate to the underlying item type move assignment operator
			_item = std::move(src);
		}


		/// @brief Non-move constructor is not allowed
		/// @param source
		explicit RWLEnvelope(const T& arg)
		{
			RWLock myWriterLock(_sMutex);
			_item = arg;
		}


		/// @brief Non-move assignment is not allowed
		/// @param source
		/// @return self
		RWLEnvelope& operator=(RWLEnvelope const&) = delete;


		/// @brief Perform a read-only action where the object is not "written" to and the read operations are shared amongst other reader threads
		/// @tparam Callback The callback must accept const T& as the first argument along with any additional arguments
		/// @tparam Args Additional arguments to forward to the callback
		/// @param cbf The callback function (must be noexcept)
		/// @param args Additional arguments to pass to the callback
		/// @return Returns (forwards) the return from the callback
		/// @note The callback MUST be marked noexcept. Callbacks that may throw will not compile.
		template <typename Callback, typename... Args>
			requires ObserveCallbackNoexcept<T, Callback, Args...>
		auto observe(Callback cbf, Args&&... args) const
		{
			RLock myLock(_sMutex);
			return cbf(_item, std::forward<Args>(args)...);
		}


		/// @brief Perform an action where the object maybe "written" to by blocking all other threads.
		/// @tparam Callback The callback type (must be noexcept)
		/// @tparam Args Additional argument types
		/// @param cbf The callback function that may modify the contents (must be noexcept)
		/// @param args Additional arguments to pass to the callback
		/// @return Returns (forwards) the return from the callback
		/// @note The callback MUST be marked noexcept. Callbacks that may throw will not compile.
		template <typename Callback, typename... Args>
			requires MutateCallbackNoexcept<T, Callback, Args...>
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


		/// @brief Returns a copy of the underlying object
		/// @return Copy of the underlying object (must have copy-constructor)
		[[nodiscard]] T snapshot() const
		{
			RLock myLock(_sMutex);
			return _item;
		}


		/// @brief Aquires a readerlock and retruns the lock along with the item for the client to modify within an if() or statement block.
		///        The client MUST use this inside a statement block and excercise utmost care! Avoid using IO within the lock!
		///        Example: if (auto [o, rl] = var.readerLock(); rl) { .. }
		/// @return tuple of const T& and the readerlock.
		[[nodiscard]] std::tuple<const T&, RLock> readLock() { return {std::ref(_item), RLock(_sMutex)}; }


		/// @brief Acquires a writerlock and returns the lock along with the reference to the item for the client to play with within lock.
		///        The client MUST use this inside a statement block and excercise utmost care! Avoid using IO within the lock!
		///        Example: if (auto [o, rwl] = var.writerLock(); rwl) { .. }
		/// @return tuple of T& and writer lock.
		[[nodiscard]] std::tuple<T&, RWLock> writeLock() { return {std::ref(_item), RWLock(_sMutex)}; }


#ifdef INCLUDE_NLOHMANN_JSON_HPP_
		// If the JSON library is included in the current project, then make the serializer available.
	public:
		/// @brief If nlohmann json library is included, this operator returns a json representation of our class
		operator nlohmann::json() const
		{
			RLock myLock(_sMutex);
			return {{"storage", _item}, {"_typver", "RWLEnvelope/1.0.0"}, {"readWriteActions", _rwa}};
		}
#endif

	private:
		/// @brief The underlying type
		/// Avoid initializing (not all types may have default constructor)
		T _item;

		/// @brief The mutex against which we use reader/writer lock
		mutable std::shared_mutex _sMutex {};

		/// @brief Internal counter to track the callbacks during mutate
		uint64_t _rwa {0};
	};
} // namespace siddiqsoft

#endif // !RWLENVELOPE_HPP
