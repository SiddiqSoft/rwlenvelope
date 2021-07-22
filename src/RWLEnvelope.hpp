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

#ifndef RWLENVELOPE_HPP
#define RWLENVELOPE_HPP

#include <functional>
#include <shared_mutex>
#include <tuple>

#if !__has_cpp_attribute(nodiscard)
#error "We really should have [[nodiscard]]"
#endif

#if !__cpp_lib_shared_mutex
#error "Must have <shared_mutex>"
#endif

namespace siddiqsoft
{
	/// @brief Implements a simple envelope-access model to make it easy for clients to use the reader-writer lock model.
	/// @tparam T Type for your object which is to be "enveloped"
	template <class T> class RWLEnvelope
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


		/// @brief Move constructor
		/// @param src source/contained object
		explicit RWLEnvelope(T&& src) { std::exchange(_item, src); }


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
		explicit RWLEnvelope(const T&) = delete;


		/// @brief Non-move assignment is not allowed
		/// @param source
		/// @return self
		RWLEnvelope& operator=(RWLEnvelope const&) = delete;


		/// @brief Perform a read-only action where the object is not "written" to and the read operations are shared amongst other reader threads
		/// @tparam R The type of the return from the callback
		/// @param callback The callback may not modify the contents and gets a readonly lock access to the stored data
		/// @return Returns (forwards) the return from the callback
		template <typename R = void> R observe(std::function<R(const T&)> callback) const
		{
			RLock myLock(_sMutex);
			return callback(_item);
		}


		/// @brief Perform an action where the object maybe "written" to by blocking all other threads.
		/// @tparam R The type of the return from the callback
		/// @param callback The callback may modify the contents and gets a readwrite lock access to the stored data
		/// @return Returns (forwards) the return from the callback
		template <typename R = void> R mutate(std::function<R(T&)> callback)
		{
			rone   d(_rwa); // we increment the housekeeping counter on each callback
			RWLock myWriterLock(_sMutex);
			return callback(_item);
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
