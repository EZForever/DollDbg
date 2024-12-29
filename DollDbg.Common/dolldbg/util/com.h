#pragma once
#include <dolldbg/dolldbg.h>

#include <type_traits>

#include <objbase.h>

namespace DollDbg
{
	namespace Util
	{
		template<class Tself, class Timpl> // where Tself : ComClass<Tself, Timpl>, Timpl : IUnknown
		class ComClass : public Timpl
		{
		private:
			ComClass(ComClass&) = delete;

			ULONG _ref;

		protected:
			inline ComClass()
				: _ref(0)
			{
				static_assert(std::is_base_of<ComClass<Tself, Timpl>, Tself>::value, "ComClass<Tself>");
				static_assert(std::is_base_of<IUnknown, Timpl>::value, "ComClass<Timpl>");
			}

		public:
			template<class... Targs>
			static Tself* Create(Targs&& ...args)
			{
				auto obj = new Tself(std::forward<Targs>(args)...);
				obj->AddRef();
				return obj;
			}

			// --- Implements Timpl->IUnknown

			STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject)
			{
				if (!ppvObject)
					return E_POINTER;

				if (riid != IID_IUnknown && riid != __uuidof(Timpl))
				{
					*ppvObject = nullptr;
					return E_NOINTERFACE;
				}

				this->AddRef();
				*ppvObject = this;
				return S_OK;
			}

			STDMETHODIMP_(ULONG) AddRef()
			{
				return _InterlockedIncrement(&_ref);
			}

			STDMETHODIMP_(ULONG) Release()
			{
				LONG lref = _InterlockedDecrement(&_ref);
				if (lref == 0)
					delete static_cast<Tself*>(this); // Virtual dtor is fine but better leave vtbl untouched
				return lref;
			}
		};
	}
}