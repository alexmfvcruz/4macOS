// WMI interface names used by TNICEventSink.h (C4-P). WMI does not exist on
// macOS: the interfaces are declared so that the original class declarations
// compile; the native CTNICEventSink stand-in reports that no network
// interface events are available.
#pragma once
#include <windows.h>
#include <objbase.h>
typedef WCHAR* BSTR;
struct IWbemClassObject; struct IWbemServices; struct IWbemLocator;
struct IWbemObjectSink : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Indicate(long count, IWbemClassObject** objects) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetStatus(long flags, HRESULT result, BSTR text, IWbemClassObject* object) = 0;
};
