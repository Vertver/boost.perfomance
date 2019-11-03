#ifndef BOOST_CPU_COUNTER_WINDOWS_HPP
#define BOOST_CPU_COUNTER_WINDOWS_HPP

#include <system_error>
#include <cstdlib>
#include <boost/cstdint.hpp>
#include <boost/winapi/get_last_error.hpp>
#include <boost/winapi/basic_types.hpp>
#include <boost/dll/import.hpp>
#include <boost/function.hpp>
#include <boost/container/vector.hpp>
#include <boost/thread.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost { namespace perfomance { namespace detail { namespace windows {

class cpu_counter {
private:
	typedef struct
	{
		boost::detail::winapi::LARGE_INTEGER_ IdleTime;
		boost::detail::winapi::LARGE_INTEGER_ KernelTime;
		boost::detail::winapi::LARGE_INTEGER_ UserTime;
		boost::detail::winapi::LARGE_INTEGER_ DpcTime;
		boost::detail::winapi::LARGE_INTEGER_ InterruptTime;
		boost::winapi::DWORD_ InterruptCount;
	} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

	enum system_information_class {
		system_basic_information = 0,
		system_performance_information = 2,
		system_time_of_day_information = 3,
		system_process_information = 5,
		system_processor_performance_information = 8,	// <-- нам нужно это
		system_interrupt_information = 23,
		system_exception_information = 33,
		system_registry_quota_information = 37,
		system_lookaside_information = 45
	};

	unsigned long cpu_count;

	/*
		Так как на MSDN написано о том, что функция NtQuerySystemInformation может в любой
		момент исчезнуть из WinAPI, то крайне рекомендуется её получать через GetProcAddress,
		чтобы не было никаких проблем с импортами в будущем.
	*/
	typedef long(__stdcall NtQuerySystemInformation_t)(int, void*, unsigned long, unsigned long*);
	NtQuerySystemInformation_t* pNtQuerySystemInformation;

	boost::container::vector<long long> prev_idle_time;
	boost::container::vector<long long> prev_user_time;
	boost::container::vector<long long> prev_kernel_time;
	boost::container::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> perf_info;

	inline float calculate_load(unsigned long current_index, long long idle_time, long long kernel_time, long long user_time) {
		float ret_value = (
			(float)(idle_time - prev_idle_time[current_index]) / 
			(float)(
			(kernel_time + user_time) - 
			(prev_kernel_time[current_index] + prev_user_time[current_index])
			)
		);

		prev_idle_time[current_index] = idle_time;
		prev_user_time[current_index] = user_time;
		prev_kernel_time[current_index] = kernel_time;

		/*
			Мы должны инвертирвать переменнную так как мы после деления дельт
			получаем обратный результат
		*/
		return 1.f - ret_value;
	}

	inline bool is_nt_failed(long result) {
		return (((unsigned long)(result)) >> 30) == 3;
	}

	bool nt_query_system_information()
	{
		long retValue = 0;
		if (!pNtQuerySystemInformation) return false;

		/*
			NtQuerySystemInformation не во всех случаях может быть выполнена успешна,
			поэтому обязательно проверим значение
		*/
		retValue = pNtQuerySystemInformation(8, &perf_info[0], sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * cpu_count, NULL);
		return !is_nt_failed(retValue);
	}

public:
	cpu_counter() : pNtQuerySystemInformation(NULL) {
		cpu_count = boost::thread::hardware_concurrency();

		/*
			Как оговаривалось раннее, данная функция может отсутствовать, поэтому используем
			библиотеку Boost.dll для того, чтобы получить адрес это функции.
		*/
		if (!pNtQuerySystemInformation) {
			boost::dll::shared_library lib("ntdll.dll", boost::dll::load_mode::search_system_folders);

			/*
				Если этой функции нету, то скорее всего появилась новая функция для
				определения загрузки, либо текущий системный модуль не доступен
				(такое случается крайне редко, но лучше быть меткими и знать о каждом
				шаге наперед)
			*/
			pNtQuerySystemInformation = &lib.get<NtQuerySystemInformation_t>("NtQuerySystemInformation");
			if (pNtQuerySystemInformation) {
				/*
					Резервируем место для нашего счетчика, чтобы в будущем передать указатель
					нашей функции и работать с нужным кол-во потоков
				*/
				perf_info.resize(cpu_count);
				prev_idle_time.resize(cpu_count);
				prev_kernel_time.resize(cpu_count);
				prev_user_time.resize(cpu_count);

				/*
					Для того, чтобы значения были правильны с самого первого использования функции,
					мы должны обновить значения в буферах
				*/
				nt_query_system_information();		
			}
		}
	}

	bool get_load(float& base_load) {
		if (!nt_query_system_information()) return false;
		base_load = 0.f;

		for (size_t i = 0; i < cpu_count; i++) {
			base_load += calculate_load(i, perf_info[i].IdleTime.QuadPart, perf_info[i].KernelTime.QuadPart, perf_info[i].UserTime.QuadPart);
		}	

		base_load /= cpu_count;
		return true;
	}

	bool get_load_per_core(boost::container::vector<float>& vector_load) {
		if (!nt_query_system_information()) return false;
		vector_load.resize(cpu_count);

		for (size_t i = 0; i < cpu_count; i++) {
			vector_load[i] = calculate_load(i, perf_info[i].IdleTime.QuadPart, perf_info[i].KernelTime.QuadPart, perf_info[i].UserTime.QuadPart);
		}

		return true;
	}
};

}}}}
#endif