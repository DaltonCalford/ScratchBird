/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Claudio Valderrama on 25-Dec-2003
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2003 Claudio Valderrama
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *  Nickolay Samofatov <nickolay@broadviewsoftware.com>
 */


// =====================================
// Utility functions

#include "firebird.h"
#include "../common/os/guid.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#else
#define __need_size_t
#include <stddef.h>
#undef __need_size_t
#endif

#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "../common/gdsassert.h"
#include "../common/utils_proto.h"
#include "../common/classes/auto.h"
#include "../common/classes/locks.h"
#include "../common/classes/init.h"
#include "../common/isc_proto.h"
#include "../jrd/constants.h"
#include "firebird/impl/inf_pub.h"
#include "../jrd/align.h"
#include "../common/os/path_utils.h"
#include "../common/os/fbsyslog.h"
#include "../common/StatusArg.h"
#include "../common/os/os_utils.h"
#include "firebird/impl/sqlda_pub.h"
#include "../common/classes/ClumpletReader.h"
#include "../common/StatusArg.h"
#include "../common/TimeZoneUtil.h"
#include "../common/config/config.h"
#include "../common/ThreadStart.h"

#ifdef WIN_NT
#include <direct.h>
#include <io.h> // isatty()
#include <sddl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef HAVE_TIMES
#include <sys/times.h>
#endif


namespace fb_utils
{

bool implicit_name(const char* name, const char* prefix, int prefix_len);


char* copy_terminate(char* dest, const char* src, size_t bufsize)
{
/**************************************
 *
 * c o p y _ t e r m i n a t e
 *
 **************************************
 *
 * Functional description
 *	Do the same as strncpy but ensure the null terminator is written.
 *
 **************************************/
	if (!bufsize) // Was it a joke?
		return dest;

	--bufsize;
	strncpy(dest, src, bufsize);
	dest[bufsize] = 0;
	return dest;
}


char* exact_name(char* const name)
{
/**************************************
 *
 *	e x a c t _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Trim off trailing spaces from a metadata name.
 *	eg: insert a null after the last non-blank character.
 *
 *	SQL delimited identifier may have blank as part of the name
 *
 *	Parameters:  str - the string to terminate
 *	Returns:     str
 *
 **************************************/
	char* p = name;
	while (*p)
	    ++p;
	// Now, let's go back
	--p;
	while (p >= name && *p == '\x20') // blank character, ASCII(32)
		--p;
	*(p + 1) = '\0';
	return name;
}


char* exact_name_limit(char* const name, size_t bufsize)
{
/**************************************
 *
 *	e x a c t _ n a m e _ l i m i t
 *
 **************************************
 *
 * Functional description
 *	Trim off trailing spaces from a metadata name.
 *	eg: insert a null after the last non-blank character.
 *	It has a maximum length to ensure working between bounds.
 *
 *	SQL delimited identifier may have blank as part of the name
 *
 *	Parameters:  str - the string to terminate
 *               bufsize - the size of the variable containing the string.
 *	Returns:     str
 *
 **************************************/
	const char* const end = name + bufsize - 1;
	char* p = name;
	while (*p && p < end)
	    ++p;
	// Now, let's go back
	--p;
	while (p >= name && *p == '\x20') // blank character, ASCII(32)
		--p;
	*(p + 1) = '\0';
	return name;
}


// *****************************
// i m p l i c i t _ d o m a i n
// *****************************
// Determines if a domain or index is of the form RDB$<n[...n]>[<spaces>]
// This may be true for implicit domains and for unique and non-unique indices except PKs.
bool implicit_domain(const char* domain_name)
{
	return implicit_name(domain_name, IMPLICIT_DOMAIN_PREFIX, IMPLICIT_DOMAIN_PREFIX_LEN);
}


// ***********************************
// i m p l i c i t _ i n t e g r i t y
// ***********************************
// Determines if a table integrity constraint domain is of the form INTEG_<n[...n]>[<spaces>]
bool implicit_integrity(const char* integ_name)
{
	return implicit_name(integ_name, IMPLICIT_INTEGRITY_PREFIX, IMPLICIT_INTEGRITY_PREFIX_LEN);
}


// ***********************************
// i m p l i c i t _ p k
// ***********************************
// Determines if an index is of the form RDB$PRIMARY<n[...n]>[<spaces>]
bool implicit_pk(const char* pk_name)
{
	return implicit_name(pk_name, IMPLICIT_PK_PREFIX, IMPLICIT_PK_PREFIX_LEN);
}


// ***********************************
// i m p l i c i t _ n a m e
// ***********************************
// Determines if a name is of the form prefix<n[...n]>[<spaces>]
// where prefix has a fixed known length.
bool implicit_name(const char* name, const char* prefix, int prefix_len)
{
	if (strncmp(name, prefix, prefix_len) != 0)
		return false;

	int i = prefix_len;
	while (name[i] >= '0' && name[i] <= '9')
		++i;

	if (i == prefix_len) // 'prefix' alone isn't valid
		return false;

	while (name[i] == ' ')
		++i;

	return !name[i]; // we reached null term
}


int name_length(const TEXT* const name)
{
/**************************************
 *
 *	n a m e _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *	Compute effective length of system relation name and others.
 *	SQL delimited identifier may contain blanks. Trailing blanks are ignored.
 *  Assumes input is null terminated.
 *
 **************************************/
	const TEXT* q = name - 1;
	for (const TEXT* p = name; *p; p++)
	{
		if (*p != ' ') {
			q = p;
		}
	}

	return (q + 1) - name;
}


// *********************************
// n a m e _ l e n g t h _ l i m i t
// *********************************
// Compute length without trailing blanks. The second parameter is maximum length.
int name_length_limit(const TEXT* const name, size_t bufsize)
{
	const char* p = name + bufsize - 1;
	// Now, let's go back
	while (p >= name && *p == ' ') // blank character, ASCII(32)
		--p;
	return (p + 1) - name;
}


//***************
// r e a d e n v
//***************
// Goes to read directly the environment variables from the operating system on Windows
// and provides a stub for UNIX.
bool readenv(const char* env_name, ScratchBird::string& env_value)
{
#ifdef WIN_NT
	const DWORD rc = GetEnvironmentVariable(env_name, NULL, 0);
	if (rc)
	{
		env_value.reserve(rc - 1);
		DWORD rc2 = GetEnvironmentVariable(env_name, env_value.begin(), rc);
		if (rc2 < rc && rc2 != 0)
		{
			env_value.recalculate_length();
			return true;
		}
	}
#else
	const char* p = getenv(env_name);
	if (p)
		return env_value.assign(p).length() != 0;
#endif
	// Not found, clear the output var.
	env_value.begin()[0] = 0;
	env_value.recalculate_length();
	return false;
}


bool readenv(const char* env_name, ScratchBird::PathName& env_value)
{
	ScratchBird::string result;
	bool rc = readenv(env_name, result);
	env_value.assign(result.c_str(), result.length());
	return rc;
}


// Set environment variable.
// If overwrite == false and variable already exist, return true.
bool setenv(const char* name, const char* value, bool overwrite)
{
#ifdef WIN_NT
	int errcode = 0;

	if (!overwrite)
	{
		size_t envsize = 0;
		errcode = getenv_s(&envsize, NULL, 0, name);
		if (errcode || envsize)
			return errcode ? false : true;
	}

	// In Windows, _putenv_s sets only the environment data in the CRT.
	// Each DLL (for example ICU) may use a different CRT which different data
	// or use Win32's GetEnvironmentVariable, so we also use SetEnvironmentVariable.
	// This is a mess and is not guarenteed to work correctly in all situations.
	if (SetEnvironmentVariable(name, value))
	{
		_putenv_s(name, value);
		return true;
	}
	else
		return false;
#else
	return ::setenv(name, value, (int) overwrite) == 0;
#endif
}

// ***************
// s n p r i n t f
// ***************
// Provide a single place to deal with vsnprintf and error detection.
int snprintf(char* buffer, size_t count, const char* format...)
{
	va_list args;
	va_start(args, format);
	const int rc = VSNPRINTF(buffer, count, format, args);
	buffer[count - 1] = 0;
	va_end(args);
#if defined(DEV_BUILD) && !defined(HAVE_VSNPRINTF)
	// We don't have the safe functions, then check if we overflowed the buffer.
	// I would prefer to make this functionality available in prod build, too.
	// If the docs are right, the null terminator is not counted => rc < count.
#if defined(fb_assert_continue)
	fb_assert_continue(rc >= 0 && rc < count);
#else
	fb_assert(rc >= 0 && rc < count);
#endif
#endif
	return rc;
}

// *******************
// c l e a n u p _ p a s s w d
// *******************
// Copy password to newly allocated place and replace existing one in argv with spaces.
// Allocated space is released upon exit from utility.
// This is planned leak of a few bytes of memory in utilities.
// This function is deprecated. Use UtilSvc::hidePasswd(ArgvType&, int) whenever possible.
// However, there are several usages through fb_utils::get_passwd(char* arg);
char* cleanup_passwd(char* arg)
{
	if (! arg)
	{
		return arg;
	}

	const int lpass = static_cast<int>(strlen(arg));
	char* savePass = (char*) gds__alloc(lpass + 1);
	if (! savePass)
	{
		// No clear idea, how will it work if there is no memory
		// for password, but let others think. As a minimum avoid AV.
		return arg;
	}
	memcpy(savePass, arg, lpass + 1);
	memset(arg, ' ', lpass);
	return savePass;
}


#ifdef WIN_NT

static bool validateProductSuite (LPCSTR lpszSuiteToValidate);

// hvlad: begins from Windows 2000 we can safely add 'Global\' prefix for
// names of all kernel objects we use. For Win9x we must not add this prefix.
// Win NT will accept such names only if Terminal Server is installed.
// Check OS version carefully and add prefix if we can add it

bool prefix_kernel_object_name(char* name, size_t bufsize)
{
	static bool bGlobalPrefix = false;
	static bool bInitDone = false;

	if (!bInitDone)
	{
		bGlobalPrefix = isGlobalKernelPrefix();
		bInitDone = true;
	}

	// Backwards compatibility feature with ScratchBird 2.0.3 and earlier.
	// If the name already contains some prefix (specified by the user, as was
	// recommended in firebird.conf) additional prefix is not added
	if (bGlobalPrefix && !strchr(name, '\\'))
	{
		const char* prefix = "Global\\";
		const size_t len_prefix = strlen(prefix);
		const size_t len_name = strlen(name) + 1;

		// if name and prefix can't fit in name's buffer than we must
		// not overwrite end of name because it contains object type
		const size_t move_prefix = (len_name + len_prefix > bufsize) ?
			(bufsize - len_name) : len_prefix;

		memmove(name + move_prefix, name, len_name);
		memcpy(name, prefix, move_prefix);
		// CVC: Unfortunately, things like Glob instead of Global\\ do not achieve the objective
		// of telling the NT kernel the object is global and hence I consider them failures.
		//return move_prefix > 0; // Soft version of the check
		return move_prefix == len_prefix; // Strict version of the check.
	}
	return true;
}


// Simply handle guardian.
class DynLibHandle
{
public:
	explicit DynLibHandle(HMODULE mod)
		: m_handle(mod)
	{}
	~DynLibHandle()
	{
		if (m_handle)
			FreeLibrary(m_handle);
	}
	operator HMODULE() const
	{
		return m_handle;
	}
	/* The previous conversion is invoked with !object so this is enough.
	bool operator!() const
	{
		return !m_handle;
	}
	*/
private:
	HMODULE m_handle;
};


// hvlad: two functions below got from
// http://msdn2.microsoft.com/en-us/library/aa380797.aspx
// and slightly adapted for our coding style

// -------------------------------------------------------------
//   Note that the validateProductSuite and isTerminalServices
//   functions use ANSI versions of the functions to maintain
//   compatibility with Windows Me/98/95.
//   -------------------------------------------------------------

bool isGlobalKernelPrefix()
{
	// The strategy of this function is as follows: use Global\ kernel namespace
	// for engine objects if we can. This can be prevented by either lack of OS support
	// for the feature (Win9X) or lack of privileges (Vista, Windows 2000/XP restricted accounts)

	const DWORD dwVersion = GetVersion();

	// Is Windows NT running?
	if (!(dwVersion & 0x80000000))
	{
		if (LOBYTE(LOWORD(dwVersion)) <= 4) // This is Windows NT 4.0 or earlier.
			return validateProductSuite("Terminal Server");

		// Is it Windows 2000 or greater? It is possible to use Global\ prefix on any
		// version of Windows from Windows 2000 and up
		// Check if we have enough privileges to create global handles.
		// If not fall back to creating local ones.
		// The API for that is the NT thing, so we have to get addresses of the
		// functions dynamically to avoid troubles on Windows 9X platforms

		DynLibHandle hmodAdvApi(LoadLibrary("advapi32.dll"));

		if (!hmodAdvApi)
		{
			gds__log("LoadLibrary failed for advapi32.dll. Error code: %lu", GetLastError());
			return false;
		}

		typedef BOOL (WINAPI *PFnOpenProcessToken) (HANDLE, DWORD, PHANDLE);
		typedef BOOL (WINAPI *PFnLookupPrivilegeValue) (LPCSTR, LPCSTR, PLUID);
		typedef BOOL (WINAPI *PFnPrivilegeCheck) (HANDLE, PPRIVILEGE_SET, LPBOOL);

		PFnOpenProcessToken pfnOpenProcessToken =
			(PFnOpenProcessToken) GetProcAddress(hmodAdvApi, "OpenProcessToken");
		PFnLookupPrivilegeValue pfnLookupPrivilegeValue =
			(PFnLookupPrivilegeValue) GetProcAddress(hmodAdvApi, "LookupPrivilegeValueA");
		PFnPrivilegeCheck pfnPrivilegeCheck =
			(PFnPrivilegeCheck) GetProcAddress(hmodAdvApi, "PrivilegeCheck");

		if (!pfnOpenProcessToken || !pfnLookupPrivilegeValue || !pfnPrivilegeCheck)
		{
			// Should never happen, really
			gds__log("Cannot access privilege management API");
			return false;
		}

		HANDLE hProcess = GetCurrentProcess();
		HANDLE hToken;
		if (pfnOpenProcessToken(hProcess, TOKEN_QUERY, &hToken) == 0)
		{
			gds__log("OpenProcessToken failed. Error code: %lu", GetLastError());
			return false;
		}

		PRIVILEGE_SET ps;
		memset(&ps, 0, sizeof(ps));
		ps.Control = PRIVILEGE_SET_ALL_NECESSARY;
		ps.PrivilegeCount = 1;
		if (pfnLookupPrivilegeValue(NULL, TEXT("SeCreateGlobalPrivilege"), &ps.Privilege[0].Luid) == 0)
		{
			// Failure here means we're running on old version of Windows 2000 or XP
			// which always allow creating global handles
			CloseHandle(hToken);
			return true;
		}

		BOOL checkResult;
		if (pfnPrivilegeCheck(hToken, &ps, &checkResult) == 0)
		{
			gds__log("PrivilegeCheck failed. Error code: %lu", GetLastError());
			CloseHandle(hToken);
			return false;
		}

		CloseHandle(hToken);

		return checkResult;
	}

	return false;
}


// Incapsulates Windows private namespace
class PrivateNamespace
{
public:
	PrivateNamespace(MemoryPool& pool) :
		m_hNamespace(NULL),
		m_hTestEvent(NULL)
	{
		try
		{
			init();
		}
		catch (const ScratchBird::Exception& ex)
		{
			iscLogException("Error creating private namespace", ex);
		}
	}

	~PrivateNamespace()
	{
		if (m_hNamespace != NULL)
			ClosePrivateNamespace(m_hNamespace, 0);
		if (m_hTestEvent != NULL)
			CloseHandle(m_hTestEvent);
	}

	// Add namespace prefix to the name, returns true on success.
	bool addPrefix(char* name, size_t bufsize)
	{
		if (!isReady())
			return false;

		if (strchr(name, '\\') != 0)
			return false;

		const size_t prefixLen = strlen(sPrivateNameSpace) + 1;
		const size_t nameLen = strlen(name) + 1;
		if (prefixLen + nameLen > bufsize)
			return false;

		memmove(name + prefixLen, name, nameLen + 1);
		memcpy(name, sPrivateNameSpace, prefixLen - 1);
		name[prefixLen - 1] = '\\';
		return true;
	}

	bool isReady() const
	{
		return (m_hNamespace != NULL) || (m_hTestEvent != NULL);
	}

private:
	const char* sPrivateNameSpace = "ScratchBirdCommon";
	const char* sBoundaryName = "ScratchBirdCommonBoundary";

	void raiseError(const char* apiRoutine)
	{
		(ScratchBird::Arg::Gds(isc_sys_request) << apiRoutine << ScratchBird::Arg::OsError()).raise();
	}

	void init()
	{
		alignas(SID) char sid[SECURITY_MAX_SID_SIZE];
		DWORD cbSid = sizeof(sid);

		// For now use EVERYONE, could be changed later
		cbSid = sizeof(sid);
		if (!CreateWellKnownSid(WinWorldSid, NULL, &sid, &cbSid))
			raiseError("CreateWellKnownSid");

		// Create security descriptor which allows generic access to the just created SID

		SECURITY_ATTRIBUTES sa;
		RtlSecureZeroMemory(&sa, sizeof(sa));
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = FALSE;

		char strSecDesc[255];
		LPSTR strSid = NULL;
		if (ConvertSidToStringSid(&sid, &strSid))
		{
			snprintf(strSecDesc, sizeof(strSecDesc), "D:(A;;GA;;;%s)", strSid);
			LocalFree(strSid);
		}
		else
			strncpy(strSecDesc, "D:(A;;GA;;;WD)", sizeof(strSecDesc));

		if (!ConvertStringSecurityDescriptorToSecurityDescriptor(strSecDesc, SDDL_REVISION_1,
			&sa.lpSecurityDescriptor, NULL))
		{
			raiseError("ConvertStringSecurityDescriptorToSecurityDescriptor");
		}

		ScratchBird::Cleanup cleanSecDesc( [&sa] {
				LocalFree(sa.lpSecurityDescriptor);
			});

		HANDLE hBoundaryDesc = CreateBoundaryDescriptor(sBoundaryName, 0);
		if (hBoundaryDesc == NULL)
			raiseError("CreateBoundaryDescriptor");

		ScratchBird::Cleanup cleanBndDesc( [&hBoundaryDesc] {
				DeleteBoundaryDescriptor(hBoundaryDesc);
			});

		if (!AddSIDToBoundaryDescriptor(&hBoundaryDesc, &sid))
			raiseError("AddSIDToBoundaryDescriptor");

		int retry = 0;
		while (true)
		{
			m_hNamespace = CreatePrivateNamespace(&sa, hBoundaryDesc, sPrivateNameSpace);

			if (m_hNamespace == NULL)
			{
				DWORD err = GetLastError();
				if (err != ERROR_ALREADY_EXISTS)
					raiseError("CreatePrivateNamespace");

				m_hNamespace = OpenPrivateNamespace(hBoundaryDesc, sPrivateNameSpace);
				if (m_hNamespace == NULL)
				{
					err = GetLastError();
					if ((err == ERROR_PATH_NOT_FOUND) && (retry++ < 100))
					{
						// Namespace was closed by its last holder, wait a bit and try again
						Thread::sleep(10);
						continue;
					}

					if (err != ERROR_DUP_NAME)
						raiseError("OpenPrivateNamespace");

					ScratchBird::string name(sPrivateNameSpace);
					name.append("\\test");

					m_hTestEvent = CreateEvent(ISC_get_security_desc(), TRUE, TRUE, name.c_str());
					if (m_hTestEvent == NULL)
						raiseError("CreateEvent");
				}
			}

			break;
		}
	}

	HANDLE m_hNamespace;
	HANDLE m_hTestEvent;
};

static ScratchBird::InitInstance<PrivateNamespace> privateNamespace;


bool private_kernel_object_name(char* name, size_t bufsize)
{
	if (!privateNamespace().addPrefix(name, bufsize))
		return prefix_kernel_object_name(name, bufsize);

	return true;
}

bool privateNameSpaceReady()
{
	return privateNamespace().isReady();
}


// This is a very basic registry querying class. Not much validation, but avoids
// leaving the registry open by mistake.

class NTRegQuery
{
public:
	NTRegQuery();
	~NTRegQuery();
	bool openForRead(const char* key);
	bool readValueSize(const char* value);
	// Assumes previous call to readValueSize.
	bool readValueData(LPSTR data);
	void close();
	DWORD getDataType() const;
	DWORD getDataSize() const;
private:
	HKEY m_hKey;
	DWORD m_dwType;
	DWORD m_dwSize;
	const char* m_value;
};

inline NTRegQuery::NTRegQuery()
	: m_hKey(NULL), m_dwType(0), m_dwSize(0)
{
}

inline NTRegQuery::~NTRegQuery()
{
	close();
}

bool NTRegQuery::openForRead(const char* key)
{
	return RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_QUERY_VALUE, &m_hKey) == ERROR_SUCCESS;
}

bool NTRegQuery::readValueSize(const char* value)
{
	m_value = value;
	return RegQueryValueExA(m_hKey, value, NULL, &m_dwType, NULL, &m_dwSize) == ERROR_SUCCESS;
}

bool NTRegQuery::readValueData(LPSTR data)
{
	return RegQueryValueExA(m_hKey, m_value, NULL, &m_dwType, (LPBYTE) data, &m_dwSize) == ERROR_SUCCESS;
}

void NTRegQuery::close()
{
	if (m_hKey)
		RegCloseKey(m_hKey);

	m_hKey = NULL;
}

inline DWORD NTRegQuery::getDataType() const
{
	return m_dwType;
}

inline DWORD NTRegQuery::getDataSize() const
{
	return m_dwSize;
}


// This class represents the local allocation of dynamic memory in Windows.

class NTLocalString
{
public:
	explicit NTLocalString(DWORD dwSize);
	LPCSTR c_str() const;
	LPSTR getString();
	bool allocated() const;
	~NTLocalString();
private:
	LPSTR m_string;
};

NTLocalString::NTLocalString(DWORD dwSize)
{
	m_string = (LPSTR) LocalAlloc(LPTR, dwSize);
}

NTLocalString::~NTLocalString()
{
	if (m_string)
		LocalFree(m_string);
}

inline LPCSTR NTLocalString::c_str() const
{
	return m_string;
}

inline LPSTR NTLocalString::getString()
{
	return m_string;
}

inline bool NTLocalString::allocated() const
{
	return m_string != 0;
}


////////////////////////////////////////////////////////////
// validateProductSuite function
//
// Terminal Services detection code for systems running
// Windows NT 4.0 and earlier.
//
////////////////////////////////////////////////////////////

bool validateProductSuite (LPCSTR lpszSuiteToValidate)
{
	NTRegQuery query;

	// Open the ProductOptions key.
	if (!query.openForRead("System\\CurrentControlSet\\Control\\ProductOptions"))
		return false;

	// Determine required size of ProductSuite buffer.
	// If we get size == 1 it means multi string data with only a terminator.
	if (!query.readValueSize("ProductSuite") || query.getDataSize() < 2)
		return false;

	// Allocate buffer.
	NTLocalString lpszProductSuites(query.getDataSize());
	if (!lpszProductSuites.allocated())
		return false;

	// Retrieve array of product suite strings.
	if (!query.readValueData(lpszProductSuites.getString()) || query.getDataType() != REG_MULTI_SZ)
		return false;

	query.close();  // explicit but redundant.

	// Search for suite name in array of strings.
	bool fValidated = false;
	LPCSTR lpszSuite = lpszProductSuites.c_str();
	LPCSTR end = lpszSuite + query.getDataSize(); // paranoid check
	while (*lpszSuite && lpszSuite < end)
	{
		if (lstrcmpA(lpszSuite, lpszSuiteToValidate) == 0)
		{
			fValidated = true;
			break;
		}
		lpszSuite += (lstrlenA(lpszSuite) + 1);
	}

	return fValidated;
}

#endif // WIN_NT

// *******************************
// g e t _ p r o c e s s _ n a m e
// *******************************
// Return the name of the current process

ScratchBird::PathName get_process_name()
{
	char buffer[MAXPATHLEN];

#if defined(WIN_NT)
	const int len = GetModuleFileName(NULL, buffer, sizeof(buffer));
#elif defined(HAVE__PROC_SELF_EXE)
    const int len = readlink("/proc/self/exe", buffer, sizeof(buffer));
#else
	const int len = 0;
#endif

	if (len <= 0)
		buffer[0] = 0;
	else if (size_t(len) < sizeof(buffer))
		buffer[len] = 0;
	else
		buffer[len - 1] = 0;

	return buffer;
}

SLONG genUniqueId()
{
	static ScratchBird::AtomicCounter cnt;
	return ++cnt;
}

void getCwd(ScratchBird::PathName& pn)
{
	char* buffer = pn.getBuffer(MAXPATHLEN);
#if defined(WIN_NT)
	_getcwd(buffer, MAXPATHLEN);
#elif defined(HAVE_GETCWD)
	FB_UNUSED(getcwd(buffer, MAXPATHLEN));
#else
	FB_UNUSED(getwd(buffer));
#endif
	pn.recalculate_length();
}

namespace {
	class InputFile
	{
	public:
		explicit InputFile(const ScratchBird::PathName& name)
		  : flagEcho(false)
		{
			if (name == "stdin") {
				f = stdin;
			}
			else {
				f = os_utils::fopen(name.c_str(), "rt");
			}
			if (f && isatty(fileno(f)))
			{
				fprintf(stderr, "Enter password: ");
				fflush(stderr);
#ifdef HAVE_TERMIOS_H
				flagEcho = tcgetattr(fileno(f), &oldState) == 0;
				if (flagEcho)
				{
					flagEcho = oldState.c_lflag & ECHO;
				}
				if (flagEcho)
				{
					struct termios newState(oldState);
					newState.c_lflag &= ~ECHO;
					tcsetattr(fileno(f), TCSANOW, &newState);
				}
#elif defined(WIN_NT)
				HANDLE handle = (HANDLE) _get_osfhandle(fileno(f));
				DWORD dwMode;
				flagEcho = GetConsoleMode(handle, &dwMode) && (dwMode & ENABLE_ECHO_INPUT);
				if (flagEcho)
					SetConsoleMode(handle, dwMode & ~ENABLE_ECHO_INPUT);
#endif
			}
		}
		~InputFile()
		{
			if (flagEcho)
			{
				fprintf(stderr, "\n");
				fflush(stderr);
#ifdef HAVE_TERMIOS_H
				tcsetattr(fileno(f), TCSANOW, &oldState);
#elif defined(WIN_NT)
				HANDLE handle = (HANDLE) _get_osfhandle(fileno(f));
				DWORD dwMode;
				if (GetConsoleMode(handle, &dwMode))
					SetConsoleMode(handle, dwMode | ENABLE_ECHO_INPUT);
#endif
			}
			if (f && f != stdin) {
				fclose(f);
			}
		}

		FILE* getStdioFile() { return f; }
		bool operator!() { return !f; }

	private:
		FILE* f;
#ifdef HAVE_TERMIOS_H
		struct termios oldState;
#endif
		bool flagEcho;
	};
} // namespace

// fetch password from file
FetchPassResult fetchPassword(const ScratchBird::PathName& name, const char*& password)
{
	InputFile file(name);
	if (!file)
	{
		return FETCH_PASS_FILE_OPEN_ERROR;
	}

	ScratchBird::string pwd;
	if (! pwd.LoadFromFile(file.getStdioFile()))
	{
		return ferror(file.getStdioFile()) ? FETCH_PASS_FILE_READ_ERROR : FETCH_PASS_FILE_EMPTY;
	}

	// this is planned leak of a few bytes of memory in utilities
	char* pass = FB_NEW_POOL(*getDefaultMemoryPool()) char[pwd.length() + 1];
	pwd.copyTo(pass, pwd.length() + 1);
	password = pass;
	return FETCH_PASS_OK;
}



#ifdef WIN_NT
static SINT64 saved_frequency = 0;
#elif defined(HAVE_CLOCK_GETTIME)
constexpr SINT64 BILLION = 1'000'000'000;
#endif

// Returns current value of performance counter
SINT64 query_performance_counter()
{
#if defined(WIN_NT)

	// Use Windows performance counters
	LARGE_INTEGER counter;
	if (QueryPerformanceCounter(&counter) == 0)
		return 0;

	return counter.QuadPart;
#elif defined(HAVE_CLOCK_GETTIME)

	// Use high-resolution clock
	struct timespec tp;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &tp) != 0)
		return 0;

	return static_cast<SINT64>(tp.tv_sec) * BILLION + tp.tv_nsec;
#else

	// This is not safe because of possible wrapping and very imprecise
	return clock();
#endif
}


// Returns frequency of performance counter in Hz
SINT64 query_performance_frequency()
{
#if defined(WIN_NT)
	if (saved_frequency)
		return saved_frequency;

	LARGE_INTEGER frequency;
	if (QueryPerformanceFrequency(&frequency) == 0)
		return 1;

	saved_frequency = frequency.QuadPart;
	return frequency.QuadPart;
#elif defined(HAVE_CLOCK_GETTIME)

	return BILLION;
#else

	// This is not safe because of possible wrapping and very imprecise
	return CLOCKS_PER_SEC;
#endif
}


// returns system and user time in milliseconds that process runs
void get_process_times(SINT64 &userTime, SINT64 &sysTime)
{
#if defined(WIN_NT)
	FILETIME utime, stime, dummy;
	if (GetProcessTimes(GetCurrentProcess(), &dummy, &dummy, &stime, &utime))
	{
		LARGE_INTEGER bigint;

		bigint.HighPart = stime.dwHighDateTime;
		bigint.LowPart = stime.dwLowDateTime;
		sysTime = bigint.QuadPart / 10000;

		bigint.HighPart = utime.dwHighDateTime;
		bigint.LowPart = utime.dwLowDateTime;
		userTime = bigint.QuadPart / 10000;
	}
	else
	{
		sysTime = userTime = 0;
	}
#else
	::tms tus;
	if (times(&tus) == (clock_t)(-1))
	{
		sysTime = userTime = 0;
		return;
	}

	const int TICK = sysconf(_SC_CLK_TCK);
	sysTime = SINT64(tus.tms_stime) * 1000 / TICK;
	userTime = SINT64(tus.tms_utime) * 1000 / TICK;
#endif
}


void exactNumericToStr(SINT64 value, int scale, ScratchBird::string& target, bool append)
{
	if (value == 0)
	{
		if (append)
			target.append("0", 1);
		else
			target.assign("0", 1);
		return;
	}

	const int MAX_SCALE = 25;
	const int MAX_BUFFER = 50;

	if (scale < -MAX_SCALE || scale > MAX_SCALE)
	{
		fb_assert(false);
		return; // throw exception here?
	}

	const bool neg = value < 0;
	const bool dot = scale < 0; // Need the decimal separator or not?
	char buffer[MAX_BUFFER];
	int iter = MAX_BUFFER;

	buffer[--iter] = '\0';

	if (scale > 0)
	{
		while (scale-- > 0)
			buffer[--iter] = '0';
	}

	bool dot_used = false;
	FB_UINT64 uval = neg ? FB_UINT64(-(value + 1)) + 1 : value; // avoid problems with MIN_SINT64

	while (uval != 0)
	{
		buffer[--iter] = static_cast<char>(uval % 10) + '0';
		uval /= 10;

		if (dot && !++scale)
		{
			buffer[--iter] = '.';
			dot_used = true;
		}
	}

	if (dot)
	{
		// if scale > 0 we have N.M
		// if scale == 0 we have .M and we need 0.M
		// if scale < 0 we have pending zeroes and need 0.{0+}M
		if (!dot_used)
		{
			while (scale++ < 0)
				buffer[--iter] = '0';

			buffer[--iter] = '.';
			buffer[--iter] = '0';
		}
		else if (!scale)
			buffer[--iter] = '0';
	}

	if (neg)
		buffer[--iter] = '-';

	const FB_SIZE_T len = MAX_BUFFER - iter - 1;

	if (append)
		target.append(buffer + iter, len);
	else
		target.assign(buffer + iter, len);
}


// returns true if environment variable FIREBIRD_BOOT_BUILD is set
bool bootBuild()
{
	static enum {FB_BOOT_UNKNOWN, FB_BOOT_NORMAL, FB_BOOT_SET} state = FB_BOOT_UNKNOWN;

	if (state == FB_BOOT_UNKNOWN)
	{
		// not care much about protecting state with mutex - each thread will assign it same value
		ScratchBird::string dummy;
		state = readenv("FIREBIRD_BOOT_BUILD", dummy) ? FB_BOOT_SET : FB_BOOT_NORMAL;
	}

	return state == FB_BOOT_SET;
}

// Build full file name in specified directory
ScratchBird::PathName getPrefix(unsigned int prefType, const char* name)
{
	ScratchBird::PathName s;

#ifdef ANDROID
	const bool useInstallDir =
		prefType == ScratchBird::IConfigManager::DIR_BIN ||
		prefType == ScratchBird::IConfigManager::DIR_SBIN ||
		prefType == ScratchBird::IConfigManager::DIR_LIB ||
		prefType == ScratchBird::IConfigManager::DIR_GUARD ||
		prefType == ScratchBird::IConfigManager::DIR_PLUGINS;

	if (useInstallDir)
		s = name;
	else
		PathUtils::concatPath(s, ScratchBird::Config::getRootDirectory(), name);

	return s;
#else
	char tmp[MAXPATHLEN];

	const char* configDir[] = {
		FB_BINDIR, FB_SBINDIR, FB_CONFDIR, FB_LIBDIR, FB_INCDIR, FB_DOCDIR, "", FB_SAMPLEDIR,
		FB_SAMPLEDBDIR, "", FB_INTLDIR, FB_MISCDIR, FB_SECDBDIR, FB_MSGDIR, FB_LOGDIR,
		FB_GUARDDIR, FB_PLUGDIR, FB_TZDATADIR
	};

	fb_assert(FB_NELEM(configDir) == ScratchBird::IConfigManager::DIR_COUNT);
	fb_assert(prefType < ScratchBird::IConfigManager::DIR_COUNT);

	if (! bootBuild())
	{
		if (prefType != ScratchBird::IConfigManager::DIR_CONF &&
			prefType != ScratchBird::IConfigManager::DIR_MSG &&
			prefType != ScratchBird::IConfigManager::DIR_TZDATA &&
			configDir[prefType][0])
		{
			// Value is set explicitly and is not environment overridable
			PathUtils::concatPath(s, configDir[prefType], name);

			if (PathUtils::isRelative(s))
			{
				gds__prefix(tmp, s.c_str());
				return tmp;
			}
			else
				return s;
		}
	}

	switch (prefType)
	{
		case ScratchBird::IConfigManager::DIR_BIN:
		case ScratchBird::IConfigManager::DIR_SBIN:
#ifdef WIN_NT
			s = "";
#else
			s = "bin";
#endif
			break;

		case ScratchBird::IConfigManager::DIR_CONF:
		case ScratchBird::IConfigManager::DIR_LOG:
		case ScratchBird::IConfigManager::DIR_GUARD:
		case ScratchBird::IConfigManager::DIR_SECDB:
			s = "";
			break;

		case ScratchBird::IConfigManager::DIR_LIB:
#ifdef WIN_NT
			s = "";
#else
			s = "lib";
#endif
			break;

		case ScratchBird::IConfigManager::DIR_PLUGINS:
			s = "plugins";
			break;

		case ScratchBird::IConfigManager::DIR_TZDATA:
			PathUtils::concatPath(s, ScratchBird::TimeZoneUtil::getTzDataPath(), name);
			return s;

		case ScratchBird::IConfigManager::DIR_INC:
			s = "include";
			break;

		case ScratchBird::IConfigManager::DIR_DOC:
			s = "doc";
			break;

		case ScratchBird::IConfigManager::DIR_UDF:
			s = "UDF";
			break;

		case ScratchBird::IConfigManager::DIR_SAMPLE:
			s = "examples";
			break;

		case ScratchBird::IConfigManager::DIR_SAMPLEDB:
			s = "examples/empbuild";
			break;

		case ScratchBird::IConfigManager::DIR_HELP:
			s = "help";
			break;

		case ScratchBird::IConfigManager::DIR_INTL:
			s = "intl";
			break;

		case ScratchBird::IConfigManager::DIR_MISC:
			s = "misc";
			break;

		case ScratchBird::IConfigManager::DIR_MSG:
			gds__prefix_msg(tmp, name);
			return tmp;

		default:
			fb_assert(false);
			break;
	}

	if (s.hasData() && name[0])
		s += PathUtils::dir_sep;

	s += name;
	gds__prefix(tmp, s.c_str());

	return tmp;
#endif
}

unsigned int copyStatus(ISC_STATUS* const to, const unsigned int space,
						const ISC_STATUS* const from, const unsigned int count) noexcept
{
	unsigned int copied = 0;

	for (unsigned int i = 0; i < count; )
	{
		if (from[i] == isc_arg_end)
		{
			break;
		}
		i += nextArg(from[i]);
		if (i > space - 1)
		{
			break;
		}
		copied = i;
	}

	memcpy(to, from, copied * sizeof(to[0]));
	to[copied] = isc_arg_end;

	return copied;
}

unsigned int mergeStatus(ISC_STATUS* const dest, unsigned int space,
						 const ScratchBird::IStatus* from) noexcept
{
	const ISC_STATUS* s;
	unsigned int copied = 0;
	const int state = from->getState();
	ISC_STATUS* to = dest;

	if (state & ScratchBird::IStatus::STATE_ERRORS)
	{
		s = from->getErrors();
		copied = copyStatus(to, space, s, statusLength(s));

		to += copied;
		space -= copied;
	}

	if (state & ScratchBird::IStatus::STATE_WARNINGS)
	{
		if (!copied)
		{
			init_status(to);
			to += 2;
			space -= 2;
			copied += 2;
		}
		s = from->getWarnings();
		copied += copyStatus(to, space, s, statusLength(s));
	}

	if (!copied)
		init_status(dest);

	return copied;
}

void copyStatus(ScratchBird::CheckStatusWrapper* to, const ScratchBird::IStatus* from) noexcept
{
	to->init();

	unsigned flags = from->getState();
	if (flags & ScratchBird::IStatus::STATE_ERRORS)
		to->setErrors(from->getErrors());
	if (flags & ScratchBird::IStatus::STATE_WARNINGS)
		to->setWarnings(from->getWarnings());
}

void setIStatus(ScratchBird::IStatus* to, const ISC_STATUS* from) noexcept
{
	try
	{
		const ISC_STATUS* w = from;
		while (*w != isc_arg_end)
		{
			if (*w == isc_arg_warning)
			{
				to->setWarnings(w);
				break;
			}
			w += nextArg(*w);
		}
		to->setErrors2(w - from, from);
	}
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(to);
	}
}

unsigned int statusLength(const ISC_STATUS* const status) noexcept
{
	unsigned int l = 0;
	for(;;)
	{
		if (status[l] == isc_arg_end)
		{
			return l;
		}
		l += nextArg(status[l]);
	}
}

bool cmpStatus(unsigned int len, const ISC_STATUS* a, const ISC_STATUS* b) noexcept
{
	for (unsigned i = 0; i < len; )
	{
		const ISC_STATUS* op1 = &a[i];
		const ISC_STATUS* op2 = &b[i];
		if (*op1 != *op2)
			return false;

		if (i == len - 1 && *op1 == isc_arg_end)
			break;

		i += nextArg(*op1);
		if (i > len)		// arg does not fit
			return false;

		unsigned l1, l2;
		const char *s1, *s2;
		if (isStr(*op1))
		{
			if (*op1 == isc_arg_cstring)
			{
				l1 = op1[1];
				l2 = op2[1];
				s1 = (const char*)(op1[2]);
				s2 = (const char*)(op2[2]);
			}
			else
			{
				s1 = (const char*)(op1[1]);
				s2 = (const char*)(op2[1]);
				l1 = strlen(s1);
				l2 = strlen(s2);
			}

			if (l1 != l2)
				return false;
			if (memcmp(s1, s2, l1) != 0)
				return false;
		}
		else if (op1[1] != op2[1])
			return false;
	}

	return true;
}

unsigned int subStatus(const ISC_STATUS* in, unsigned int cin,
					   const ISC_STATUS* sub, unsigned int csub) noexcept
{
	for (unsigned pos = 0; csub <= cin - pos; )
	{
		for (unsigned i = 0; i < csub; )
		{
			const ISC_STATUS* op1 = &in[pos + i];
			const ISC_STATUS* op2 = &sub[i];
			if (*op1 != *op2)
				goto miss;

			i += nextArg(*op1);
			if (i > csub)		// arg does not fit
				goto miss;


			if (isStr(*op1))
			{
				unsigned l1, l2;
				const char *s1, *s2;
				if (*op1 == isc_arg_cstring)
				{
					l1 = op1[1];
					l2 = op2[1];
					s1 = (const char*) (op1[2]);
					s2 = (const char*) (op2[2]);
				}
				else
				{
					s1 = (const char*) (op1[1]);
					s2 = (const char*) (op2[1]);
					l1 = strlen(s1);
					l2 = strlen(s2);
				}

				if (l1 != l2)
					goto miss;
				if (memcmp(s1, s2, l1) != 0)
					goto miss;
			}
			else if (op1[1] != op2[1])
				goto miss;
		}

		return pos;

miss:	pos += nextArg(in[pos]);
	}

	return ~0u;
}

// moves DB path information (from limbo transaction) to another buffer
void getDbPathInfo(unsigned int& itemsLength, const unsigned char*& items,
	unsigned int& bufferLength, unsigned char*& buffer,
	ScratchBird::Array<unsigned char>& newItemsBuffer, const ScratchBird::PathName& dbpath)
{
	if (itemsLength && items)
	{
		const unsigned char* ptr = (const unsigned char*) memchr(items, fb_info_tra_dbpath, itemsLength);
		if (ptr)
		{
			newItemsBuffer.add(items, itemsLength);
			newItemsBuffer.remove(ptr - items);
			items = newItemsBuffer.begin();
			--itemsLength;

			unsigned int len = dbpath.length();
			if (len + 4 > bufferLength)
			{
				len = bufferLength - 4;
			}
			bufferLength -= (len + 3);
			*buffer++ = fb_info_tra_dbpath;
			*buffer++ = len;
			*buffer++ = len >> 8;
			memcpy(buffer, dbpath.c_str(), len);
			buffer += len;
			*buffer = isc_info_end;
		}
	}
}

// returns true if passed info items work with running svc thread
bool isRunningCheck(const UCHAR* items, unsigned int length)
{
	enum {S_NEU, S_RUN, S_INF} state = S_NEU;

	while (length--)
	{
		if (!items)
		{
			ScratchBird::Arg::Gds(isc_null_block).raise();
		}

		switch (*items++)
		{
		case isc_info_end:
		case isc_info_truncated:
		case isc_info_error:
		case isc_info_data_not_ready:
		case isc_info_length:
		case isc_info_flag_end:
		case isc_info_svc_running:
			break;

		case isc_info_svc_line:
		case isc_info_svc_to_eof:
		case isc_info_svc_timeout:
		case isc_info_svc_limbo_trans:
		case isc_info_svc_get_users:
		case isc_info_svc_stdin:
			if (state == S_INF)
			{
				ScratchBird::Arg::Gds(isc_mixed_info).raise();
			}
			state = S_RUN;
			break;

		case isc_info_svc_svr_db_info:
		case isc_info_svc_get_license:
		case isc_info_svc_get_license_mask:
		case isc_info_svc_get_config:
		case isc_info_svc_version:
		case isc_info_svc_server_version:
		case isc_info_svc_implementation:
		case isc_info_svc_capabilities:
		case isc_info_svc_user_dbpath:
		case isc_info_svc_get_env:
		case isc_info_svc_get_env_lock:
		case isc_info_svc_get_env_msg:
		case isc_info_svc_get_licensed_users:
			if (state == S_RUN)
			{
				ScratchBird::Arg::Gds(isc_mixed_info).raise();
			}
			state = S_INF;
			break;

		default:
			(ScratchBird::Arg::Gds(isc_unknown_info) << ScratchBird::Arg::Num(ULONG(items[-1]))).raise();
			break;
		}
	}

	return state == S_RUN;
}

static inline char conv_bin2ascii(ULONG l)
{
	return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[l & 0x3f];
}

// converts bytes to BASE64 representation
void base64(ScratchBird::string& b64, const ScratchBird::UCharBuffer& bin)
{
	b64.erase();
	const unsigned char* f = bin.begin();
	for (int i = bin.getCount(); i > 0; i -= 3, f += 3)
	{
		if (i >= 3)
		{
			const ULONG l = (ULONG(f[0]) << 16) | (ULONG(f[1]) <<  8) | f[2];
			b64 += conv_bin2ascii(l >> 18);
			b64 += conv_bin2ascii(l >> 12);
			b64 += conv_bin2ascii(l >> 6);
			b64 += conv_bin2ascii(l);
		}
		else
		{
			ULONG l = ULONG(f[0]) << 16;
			if (i == 2)
				l |= (ULONG(f[1]) << 8);
			b64 += conv_bin2ascii(l >> 18);
			b64 += conv_bin2ascii(l >> 12);
			b64 += (i == 1 ? '=' : conv_bin2ascii(l >> 6));
			b64 += '=';
		}
	}
}

void random64(ScratchBird::string& randomValue, FB_SIZE_T length)
{
	ScratchBird::UCharBuffer binRand;
	ScratchBird::GenerateRandomBytes(binRand.getBuffer(length), length);
	base64(randomValue, binRand);
	randomValue.resize(length, '$');
}

void logAndDie(const char* text)
{
	gds__log(text);
	ScratchBird::Syslog::Record(ScratchBird::Syslog::Error, text);
	abort();
}

UCHAR sqlTypeToDscType(SSHORT sqlType)
{
	switch (sqlType)
	{
	case SQL_VARYING:
		return dtype_varying;
	case SQL_TEXT:
		return dtype_text;
	case SQL_NULL:
		return dtype_text;
	case SQL_DOUBLE:
		return dtype_double;
	case SQL_FLOAT:
		return dtype_real;
	case SQL_D_FLOAT:
		return dtype_d_float;
	case SQL_TYPE_DATE:
		return dtype_sql_date;
	case SQL_TYPE_TIME:
		return dtype_sql_time;
	case SQL_TIMESTAMP:
		return dtype_timestamp;
	case SQL_BLOB:
		return dtype_blob;
	case SQL_ARRAY:
		return dtype_array;
	case SQL_LONG:
		return dtype_long;
	case SQL_SHORT:
		return dtype_short;
	case SQL_INT64:
		return dtype_int64;
	case SQL_QUAD:
		return dtype_quad;
	case SQL_BOOLEAN:
		return dtype_boolean;
	case SQL_DEC16:
		return dtype_dec64;
	case SQL_DEC34:
		return dtype_dec128;
	case SQL_INT128:
		return dtype_int128;
	case SQL_TIME_TZ:
		return dtype_sql_time_tz;
	case SQL_TIMESTAMP_TZ:
		return dtype_timestamp_tz;
	case SQL_TIME_TZ_EX:
		return dtype_ex_time_tz;
	case SQL_TIMESTAMP_TZ_EX:
		return dtype_ex_timestamp_tz;
	default:
		return dtype_unknown;
	}
}

unsigned sqlTypeToDsc(unsigned runOffset, unsigned sqlType, unsigned sqlLength,
	unsigned* dtype, unsigned* len, unsigned* offset, unsigned* nullOffset)
{
	sqlType &= ~1;
	unsigned dscType = sqlTypeToDscType(sqlType);

	if (dscType == dtype_unknown)
	{
		fb_assert(false);
		ScratchBird::Arg::Gds(isc_dsql_datatype_err).raise();
	}

	if (dtype)
		*dtype = dscType;

	if (sqlType == SQL_VARYING)
		sqlLength += sizeof(USHORT);
	if (len)
		*len = sqlLength;

	unsigned align = type_alignments[dscType % FB_NELEM(type_alignments)];
	if (align)
		runOffset = FB_ALIGN(runOffset, align);
	if (offset)
		*offset = runOffset;

	runOffset += sqlLength;
	align = type_alignments[dtype_short];
	if (align)
		runOffset = FB_ALIGN(runOffset, align);
	if (nullOffset)
		*nullOffset = runOffset;

	return runOffset + sizeof(SSHORT);
}

const ISC_STATUS* nextCode(const ISC_STATUS* v) noexcept
{
	do
	{
		v += nextArg(v[0]);
	} while (v[0] != isc_arg_warning && v[0] != isc_arg_gds && v[0] != isc_arg_end);

	return v;
}

bool containsErrorCode(const ISC_STATUS* v, ISC_STATUS code)
{
	for (; v[0] == isc_arg_gds; v = nextCode(v))
	{
		if (v[1] == code)
			return true;
	}

	return false;
}

inline bool sqlSymbolChar(char c, bool first)
{
	if (c & 0x80)
		return false;
	return (isdigit(c) && !first) || isalpha(c) || c == '_' || c == '$';
}

const char* dpbItemUpper(const char* s, FB_SIZE_T l, ScratchBird::string& buf)
{
	if (l && (s[0] == '"' || s[0] == '\''))
	{
		const char end_quote = s[0];
		bool ascii = true;

		// quoted string - strip quotes
		for (FB_SIZE_T i = 1; i < l; ++i)
		{
			if (s[i] == end_quote)
			{
				if (++i >= l)
				{
					if (ascii && s[0] == '\'')
						buf.upper();

					return buf.c_str();
				}

				if (s[i] != end_quote)
				{
					buf.assign(&s[i], l - i);
					(ScratchBird::Arg::Gds(isc_quoted_str_bad) << buf).raise();
				}

				// skipped the escape quote, continue processing
			}
			else if (!sqlSymbolChar(s[i], i == 1))
				ascii = false;

			buf += s[i];
		}

		buf.assign(1, s[0]);
		(ScratchBird::Arg::Gds(isc_quoted_str_miss) << buf).raise();
	}

	// non-quoted string - try to uppercase
	for (FB_SIZE_T i = 0; i < l; ++i)
	{
		if (!sqlSymbolChar(s[i], i == 0))
			return NULL;				// contains non-ascii data
		buf += toupper(s[i]);
	}

	return buf.c_str();
}

bool isBpbSegmented(unsigned parLength, const unsigned char* par)
{
	if (parLength && !par)
		ScratchBird::Arg::Gds(isc_null_block).raise();

	ScratchBird::ClumpletReader bpb(ScratchBird::ClumpletReader::Tagged, par, parLength);
	if (bpb.getBufferTag() != isc_bpb_version1)
	{
		(ScratchBird::Arg::Gds(isc_bpb_version) << ScratchBird::Arg::Num(bpb.getBufferTag()) <<
			ScratchBird::Arg::Num(isc_bpb_version1)).raise();
	}

	if (!bpb.find(isc_bpb_type))
		return true;

	int type = bpb.getInt();

	return type & isc_bpb_type_stream ? false : true;
}

FbShutdown::~FbShutdown()
{
	fb_shutdown(0, reason);
}

} // namespace fb_utils
