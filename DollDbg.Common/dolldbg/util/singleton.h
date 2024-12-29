#pragma once
#include <dolldbg/dolldbg.h>

#include <stdexcept>
#include <type_traits>

#include <intrin.h>

namespace DollDbg
{
	namespace Util
	{
		template<class Tself> // where Tself : Singleton<Tself>
		class Singleton
		{
		private:
			// Note on picking the value: https://devblogs.microsoft.com/oldnewthing/20170420-00/?p=96005
			static constexpr Tself* INITFINI_RUNNING = (Tself*)0xffff;

			static Tself* _instance;

		protected:
			inline Singleton()
				{ static_assert(std::is_base_of<Singleton<Tself>, Tself>::value, "Singleton<Tself>"); }

		public:
			template<class... Targs>
			static void init(Targs&& ...args)
			{
				auto instance = (Tself*)_InterlockedCompareExchangePointer((PVOID*)&_instance, INITFINI_RUNNING, nullptr);
				if (instance != nullptr)
					throw std::runtime_error("Singleton<Tself>::init(): Already initialized / Being initialized");

				instance = new Tself(std::forward<Targs>(args)...);
				_InterlockedExchangePointer((PVOID*)&_instance, instance);
			}

			static void fini()
			{
				auto instance = (Tself*)_InterlockedExchangePointer((PVOID*)&_instance, INITFINI_RUNNING);
				if (instance != INITFINI_RUNNING)
				{
					delete instance;
					_InterlockedExchangePointer((PVOID*)&_instance, nullptr);
				}
			}

			static Tself& get()
			{
				auto instance = (Tself*)_InterlockedCompareExchangePointer((PVOID*)&_instance, nullptr, nullptr);
				if (instance == nullptr || instance == INITFINI_RUNNING)
					throw std::runtime_error("Singleton<Tself>::get(): Not initialized / Being initialized");
				return *instance;
			}
		};
	}
}