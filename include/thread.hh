/* This file is part of 3hs
 * Copyright (C) 2021-2022 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef inc_thread_hh
#define inc_thread_hh

#include <functional>
#include <3ds.h>
#include "panic.hh"


namespace ctr
{
	template <typename ... Ts>
	class thread
	{
	public:
		/* exit the current thread */
		static void exit()
		{
			threadExit(0);
		}

		/* create a new thread */
		thread(std::function<void(Ts...)> cb, Ts& ... args)
		{
			ThreadFuncParams *params = new ThreadFuncParams;
			params->func = [&]() -> void { cb(args...); };
			params->self = this;

			s32 prio = 0;
			svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);

			this->threadobj = threadCreate(&thread::_entrypoint, params, 64 * 1024, prio - 1,
				-2, false);
			panic_assert(this->threadobj != nullptr, "failed to create thread");
		}

		~thread()
		{
			this->join();
			threadFree(this->threadobj);
		}

		/* wait for the thread to finish */
		void join()
		{
			if(!this->finished())
				threadJoin(this->threadobj, U64_MAX);
		}

		/* returns if the thread is done */
		bool finished()
		{
			return this->isFinished;
		}


	private:
		typedef struct ThreadFuncParams
		{
			std::function<void()> func;
			thread *self;
		} ThreadFuncParams;

		static void _entrypoint(void *arg)
		{
			ThreadFuncParams *params = (ThreadFuncParams *) arg;
			params->func();
			params->self->isFinished = true;
			delete params;
		}

		Thread threadobj;
		bool isFinished;


	};

	template <typename ... Ts>
	class reuse_thread
	{
	public:
		/* will run a new thread, but first finished the previous one */
		thread<Ts...> *run(std::function<void(Ts...)> cb, Ts& ... args)
		{
			this->cleanup();
			return this->th = new thread<Ts...>(cb, args...);
		}

		/* returns the current active thread, and nullptr if there is none */
		thread<Ts...> *current_thread() { return this->th; }
		~reuse_thread() { this->cleanup(); }


	private:
		void cleanup()
		{
			if(this->th != nullptr)
			{
				this->th->join();
				delete this->th;
				this->th = nullptr;
			}
		}

		thread<Ts...> *th = nullptr;


	};
}

#endif

