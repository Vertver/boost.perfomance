#ifndef BOOST_MEMORY_COUNTER_WINDOWS_HPP
#define BOOST_MEMORY_COUNTER_WINDOWS_HPP

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#include <system_error>
#include <cstdlib>
#include <boost/cstdint.hpp>
#include <boost/winapi/get_last_error.hpp>
#include <boost/winapi/basic_types.hpp>
#include <boost/winapi/error_codes.hpp>
#include <boost/dll/import.hpp>
#include <boost/function.hpp>
#include <boost/container/vector.hpp>
#include <boost/thread.hpp>
#include <boost/process.hpp>

namespace boost { namespace perfomance { namespace detail { namespace windows {

class memory_counter {
private:
	typedef struct {
		boost::winapi::DWORD_		cb;
		boost::winapi::DWORD_		PageFaultCount;
		boost::winapi::SIZE_T_		PeakWorkingSetSize;
		boost::winapi::SIZE_T_		WorkingSetSize;
		boost::winapi::SIZE_T_		QuotaPeakPagedPoolUsage;
		boost::winapi::SIZE_T_		QuotaPagedPoolUsage;
		boost::winapi::SIZE_T_		QuotaPeakNonPagedPoolUsage;
		boost::winapi::SIZE_T_		QuotaNonPagedPoolUsage;
		boost::winapi::SIZE_T_		PagefileUsage;
		boost::winapi::SIZE_T_		PeakPagefileUsage;
	} PROCESS_MEMORY_COUNTERS;

	typedef struct {
		boost::winapi::DWORD_		dwLength;
		boost::winapi::DWORD_		dwMemoryLoad;
		boost::winapi::ULONGLONG_	ullTotalPhys;
		boost::winapi::ULONGLONG_	ullAvailPhys;
		boost::winapi::ULONGLONG_	ullTotalPageFile;
		boost::winapi::ULONGLONG_	ullAvailPageFile;
		boost::winapi::ULONGLONG_	ullTotalVirtual;
		boost::winapi::ULONGLONG_	ullAvailVirtual;
		boost::winapi::ULONGLONG_	ullAvailExtendedVirtual;
	} MEMORYSTATUSEX;

	typedef boost::winapi::BOOL_(__stdcall GetProcessMemoryInfo_t)(void*, void*, unsigned long);
	typedef boost::winapi::BOOL_(__stdcall GlobalMemoryStatusEx_t)(void*);

	GetProcessMemoryInfo_t* pGetProcessMemoryInfo;
	GlobalMemoryStatusEx_t* pGlobalMemoryStatusEx;
	PROCESS_MEMORY_COUNTERS memory_counters = {};
	MEMORYSTATUSEX memory_status = {};

	bool get_memory_system_info(boost::winapi::HANDLE_ hProcess) {
		if (!pGetProcessMemoryInfo) return false;
		return !!pGetProcessMemoryInfo(hProcess, &memory_counters, sizeof(PROCESS_MEMORY_COUNTERS));
	}

	bool global_memory_status() {
		memory_status.dwLength = sizeof(MEMORYSTATUSEX);
		if (!pGlobalMemoryStatusEx) return false;
		return !!pGlobalMemoryStatusEx(&memory_status);
	}

	bool load_global_memory_status_procs() {
		/*
			Так как данная функция не доступна в Boost.Winapi библиотеке, нам следует получать
			её адрес напрямую из библиотеки Kernel32.
		*/
		boost::dll::shared_library lib("kernel32.dll", boost::dll::load_mode::search_system_folders);
		pGlobalMemoryStatusEx = &lib.get<GlobalMemoryStatusEx_t>("GlobalMemoryStatusEx");
		return !!pGlobalMemoryStatusEx;
	}

	bool load_get_process_memory_info_procs() {
		const char* DllArray[] = { "kernel32.dll", "psapi.dll" };
		const char* FuncArray[] = { "K32GetProcessMemoryInfo", "GetProcessMemoryInfo" };

		/*
			Из-за того, что GetProcessMemoryInfo является устаревшей версией функции K32GetProcessMemoryInfo,
			нам в зависимости от операционной системы следует использовать разные версии функций. 

			GetProcessMemoryInfo является первой версией, и она была единственной подобной функцией до выхода
			Windows 7, которая добавила функцию K32GetProcessMemoryInfo прямо внутрь библиотеки Kernel32, 
			из-за чего теперь приходиться узнавать, доступна ли данная функция в этой библиотеке или нет.

			См. https://docs.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo
		*/
		for (size_t i = 0; i < 2; i++) {
			boost::dll::shared_library lib(DllArray[i], boost::dll::load_mode::search_system_folders);
			pGetProcessMemoryInfo = &lib.get<GetProcessMemoryInfo_t>(FuncArray[i]);
			if (pGetProcessMemoryInfo) {
				break;
			}
		}

		return !!pGetProcessMemoryInfo;
	}

	bool open_process_query_information(int process_id, boost::winapi::HANDLE_& hProcess) {
		hProcess = boost::winapi::OpenProcess(
			boost::winapi::PROCESS_QUERY_INFORMATION_ | boost::winapi::PROCESS_VM_READ_,
			FALSE,
			process_id
		);

		if (hProcess == 0 || hProcess == boost::winapi::INVALID_HANDLE_VALUE_) {
			if (boost::winapi::GetLastError() == boost::winapi::ERROR_ACCESS_DENIED_) {
				/*
					Начиная с Windows 7, появился флаг PROCESS_QUERY_LIMITED_INFORMATION,
					необходимый для получения лимитированной информации. На Windows XP
					достаточно лишь PROCESS_QUERY_INFORMATION опции
				*/
				hProcess = boost::winapi::OpenProcess(
					0x1000 /*boost::winapi::PROCESS_QUERY_LIMITED_INFORMATION_*/ | boost::winapi::PROCESS_VM_READ_,
					FALSE,
					process_id
				);

				if (hProcess == 0 || hProcess == boost::winapi::INVALID_HANDLE_VALUE_) {
					return false;
				}
			} else {
				return false;
			}
		}
	}

	bool get_system_memory_counter(int process_id) {
		boost::winapi::HANDLE_ hProcess = NULL;
		if (!open_process_query_information(process_id, hProcess)) return false;
		if (!get_memory_system_info(hProcess)) {
			boost::winapi::CloseHandle(hProcess);
			return false;
		}

		return !!boost::winapi::CloseHandle(hProcess);
	}

public:
	memory_counter() : pGetProcessMemoryInfo(NULL), pGlobalMemoryStatusEx(NULL) {
		/*
			Данные функции могут завершится с ошибкой, поэтому просто игнорируем это
		*/
		load_global_memory_status_procs();
		load_get_process_memory_info_procs();
	}

	/*
		Функции для получения глобального потребления памяти
	*/
	bool get_swap_load(unsigned long long& swap_load) {
		if (!global_memory_status()) return false;
		swap_load = memory_status.ullTotalPageFile - memory_status.ullAvailPageFile;
		return true;
	}

	bool get_vmemory_load(unsigned long long& mem_load) {
		if (!global_memory_status()) return false;
		mem_load = memory_status.ullTotalVirtual - memory_status.ullAvailVirtual;
		return true;
	}

	/*
		Функции для получения потребления памяти текущего процесса
	*/
	bool get_process_swap_load(unsigned long long& swap_load) {
		if (!get_memory_system_info(boost::winapi::GetCurrentProcess())) return false;
		swap_load = memory_counters.PagefileUsage;
		return true;
	}

	bool get_process_vmemory_load(unsigned long long& mem_load) {
		if (!get_memory_system_info(boost::winapi::GetCurrentProcess())) return false;
		mem_load = memory_counters.WorkingSetSize;
		return true;
	}

	/*
		Функции для получения потребления памяти по дескриптору процесса
	*/
	bool get_process_swap_load(void* hProcess, unsigned long long& swap_load) {
		if (!get_memory_system_info(hProcess)) return false;
		swap_load = memory_counters.PagefileUsage;
		return true;
	}

	bool get_process_vmemory_load(void* hProcess, unsigned long long& mem_load) {
		if (!get_memory_system_info(hProcess)) return false;
		mem_load = memory_counters.WorkingSetSize;
		return true;
	}

	/*
		Функции для получения потребления памяти по идентификатору процесса
	*/
	bool get_process_swap_load(int process_id, unsigned long long& swap_load) {
		if (!get_system_memory_counter(process_id)) return false;
		swap_load = memory_counters.PagefileUsage;
		return true;
	}

	bool get_process_vmemory_load(int process_id, unsigned long long& mem_load) {
		if (!get_system_memory_counter(process_id)) return false;
		mem_load = memory_counters.WorkingSetSize;
		return true;
	}
};

}}}}
#endif
