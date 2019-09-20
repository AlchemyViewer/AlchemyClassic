/** 
 * @file llmachineid.cpp
 * @brief retrieves unique machine ids
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llmachineid.h"
#include <system_error>

#if	defined(LL_WINDOWS)
#  define _WIN32_DCOM
#  include <comdef.h>
#  include <Wbemidl.h>
#elif defined(LL_DARWIN)
#  include <CoreFoundation/CoreFoundation.h>
#  include <IOKit/IOKitLib.h>
#else
#  include "lluuid.h"
#endif

#ifdef LL_WINDOWS
class LLComInitialize
{
    HRESULT mHR;
public:
    LLComInitialize()
    {
        mHR = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(mHR)) {
            LL_DEBUGS("AppInit") << "Failed to initialize COM library. Error code = 0x" << std::hex << mHR << std::dec << LL_ENDL;
        }
    }

    ~LLComInitialize()
    {
        if (SUCCEEDED(mHR)) {
            CoUninitialize();
        }
    }
};

#endif //LL_WINDOWS

// get an unique machine id.
// NOT THREAD SAFE - do before setting up threads.
// Keying on MAC address for this is stupid. lol

LLMachineID::LLMachineID() : mIdLength(0)
{
    memset(mUniqueId, 0, sizeof(mUniqueId));
}

bool LLMachineID::init()
{
    mIdLength = 0;
    memset(mUniqueId, 0, sizeof(mUniqueId));
    
#ifdef LL_WINDOWS
# pragma comment(lib, "wbemuuid.lib")

        // algorithm to detect BIOS serial number found at:
        // http://msdn.microsoft.com/en-us/library/aa394077%28VS.85%29.aspx
        // we can't use the MAC address since on Windows 7, the first returned MAC address changes with every reboot.
        // we shouldn't have ever done it that way in the first place. lol. so unreasonably silly.

        HRESULT hres;

        // Step 1: --------------------------------------------------
        // Initialize COM. ------------------------------------------

        LLComInitialize comInit;

        // Step 2: --------------------------------------------------
        // Set general COM security levels --------------------------
        // Note: If you are using Windows 2000, you need to specify -
        // the default authentication credentials for a user by using
        // a SOLE_AUTHENTICATION_LIST structure in the pAuthList ----
        // parameter of CoInitializeSecurity ------------------------

        hres =  CoInitializeSecurity(
            nullptr, 
            -1,                          // COM authentication
            nullptr,                        // Authentication services
            nullptr,                        // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
            RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
            nullptr,                        // Authentication info
            EOAC_NONE,                   // Additional capabilities 
            nullptr                         // Reserved
            );

                          
        if (FAILED(hres))
        {
            LL_WARNS("AppInit") << "Failed to initialize security: " << std::system_category().message(hres) << LL_ENDL;
            return false;                    // Program has failed.
        }
        
        // Step 3: ---------------------------------------------------
        // Obtain the initial locator to WMI -------------------------

        IWbemLocator *pLoc = nullptr;

        hres = CoCreateInstance(
            CLSID_WbemLocator,             
            nullptr, 
            CLSCTX_INPROC_SERVER, 
            IID_IWbemLocator, reinterpret_cast<LPVOID*>(&pLoc));
     
        if (FAILED(hres))
        {
            LL_WARNS("AppInit") << "Failed to create IWbemLocator object: " << std::system_category().message(hres) << LL_ENDL;
            return false;                 // Program has failed.
        }

        // Step 4: -----------------------------------------------------
        // Connect to WMI through the IWbemLocator::ConnectServer method

        IWbemServices *pSvc = nullptr;
    	
        // Connect to the root\cimv2 namespace with
        // the current user and obtain pointer pSvc
        // to make IWbemServices calls.
        hres = pLoc->ConnectServer(
             _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
             nullptr,                    // User name. NULL = current user
             nullptr,                    // User password. NULL = current
             nullptr,                       // Locale. NULL indicates current
             NULL,                    // Security flags.
             nullptr,                       // Authority (e.g. Kerberos)
             nullptr,                       // Context object 
             &pSvc                    // pointer to IWbemServices proxy
             );
        
        if (FAILED(hres))
        {
            LL_WARNS("AppInit") << "Could not connect:" << std::system_category().message(hres) << LL_ENDL;
            pLoc->Release();     
            return false;                // Program has failed.
        }

        LL_DEBUGS("AppInit") << "Connected to ROOT\\CIMV2 WMI namespace" << LL_ENDL;


        // Step 5: --------------------------------------------------
        // Set security levels on the proxy -------------------------

        hres = CoSetProxyBlanket(
           pSvc,                        // Indicates the proxy to set
           RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
           RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
           nullptr,                        // Server principal name 
           RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
           RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
           nullptr,                        // client identity
           EOAC_NONE                    // proxy capabilities 
        );

        if (FAILED(hres))
        {
            LL_WARNS("AppInit") << "Could not set proxy blanket: " << std::system_category().message(hres) << LL_ENDL;
            pSvc->Release();
            pLoc->Release();     
            return false;               // Program has failed.
        }

        // Step 6: --------------------------------------------------
        // Use the IWbemServices pointer to make requests of WMI ----

        // For example, get the name of the operating system
        IEnumWbemClassObject* pEnumerator = nullptr;
        hres = pSvc->ExecQuery(
            bstr_t("WQL"), 
            bstr_t("SELECT * FROM Win32_OperatingSystem"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &pEnumerator);
        
        if (FAILED(hres))
        {
            LL_WARNS("AppInit") << "Query for operating system name failed: " << std::system_category().message(hres) << LL_ENDL;
            pSvc->Release();
            pLoc->Release();
            return false;               // Program has failed.
        }

        // Step 7: -------------------------------------------------
        // Get the data from the query in step 6 -------------------
     
        IWbemClassObject *pclsObj = nullptr;
        ULONG uReturn = 0;
       
        while (pEnumerator)
        {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, 
                &pclsObj, &uReturn);

            if(0 == uReturn)
            {
                break;
            }

            VARIANT vtProp;

            // Get the value of the Name property
            hr = pclsObj->Get(L"SerialNumber", 0, &vtProp, nullptr, nullptr);
            LL_INFOS("AppInit") << " Serial Number : " << vtProp.bstrVal << LL_ENDL;
            // use characters in the returned Serial Number to create a byte array
            BSTR serialNumber ( vtProp.bstrVal);
            for (; mIdLength < sizeof(mUniqueId) && vtProp.bstrVal[mIdLength] != 0; ++mIdLength)
            {
                mUniqueId[mIdLength] = serialNumber[mIdLength];
            }
            VariantClear(&vtProp);

            pclsObj->Release();
            pclsObj = nullptr;
            break;
        }

        // Cleanup
        // ========
        
        if (pSvc) { pSvc->Release(); }
        if (pLoc) { pLoc->Release(); }
        if (pEnumerator) { pEnumerator->Release(); }

#elif defined(LL_DARWIN)
    // Apple best practice is to key to the system's serial number
    // https://developer.apple.com/library/archive/technotes/tn1103/_index.html
    //
    // Apple use a 12 char serial number formatted as:
    // P - manufacturing location, Y - year manufactured, W - week manufactured,
    // S - unique id, C - model number
    // PPYWWSSSCCCC
    //
    
    io_service_t expert = IOServiceGetMatchingService(kIOMasterPortDefault,
                                        IOServiceMatching("IOPlatformExpertDevice"));
    if (expert != 0)
    {
        CFTypeRef cf_prop = IORegistryEntryCreateCFProperty(
                                        expert, CFSTR(kIOPlatformSerialNumberKey),
                                        kCFAllocatorDefault, 0);
        if (cf_prop) {
            char serialNumber[sizeof(mUniqueId)] = {0};
            CFStringRef serial = (CFStringRef)cf_prop;
            if (CFStringGetCString(serial, serialNumber, sizeof(serialNumber), kCFStringEncodingUTF8)) {
                for (; mIdLength < sizeof(mUniqueId) && serialNumber[mIdLength] != '\0'; ++mIdLength)
                {
                    mUniqueId[mIdLength] = serialNumber[mIdLength];
                }
            }
        }
        IOObjectRelease(expert);
    }
#else
    if (LLUUID::getNodeID(mUniqueId) == 1)
    {
        mIdLength = 6; // getNodeID output is always 6 bytes
    }
#endif
    return (mIdLength != 0);
}


U32 LLMachineID::getUniqueID(U8 unique_id[32], size_t len) const
{
    size_t length = 0;
    if (mIdLength)
    {
        memset(unique_id, 0, len);
        for (; length < len && length < mIdLength; ++length)
        {
            unique_id[length] = mUniqueId[length];
        }

        LL_INFOS_ONCE("AppInit") << "UniqueID: 0x";
        // Code between here and LL_ENDL is not executed unless the LL_DEBUGS
        // actually produces output
        for (size_t i = 0; i < length; ++i)
        {
            // Copy each char to unsigned int to hexify. Sending an unsigned
            // char to a std::ostream tries to represent it as a char, not
            // what we want here.
            U32 byte = static_cast<U32>(unique_id[i]);
            LL_CONT << std::hex << std::setw(2) << std::setfill('0') << byte;
        }
        // Reset default output formatting to avoid nasty surprises!
        LL_CONT << std::dec << std::setw(0) << std::setfill(' ') << LL_ENDL;
    }
    return length;
}
