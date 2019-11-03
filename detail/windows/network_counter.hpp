#ifndef BOOST_NETWORK_COUNTER_WINDOWS_HPP
#define BOOST_NETWORK_COUNTER_WINDOWS_HPP

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
#include <boost/asio.hpp>

namespace boost { namespace perfomance { namespace detail { namespace windows {

typedef std::basic_string<char> network_string;
typedef std::ostringstream network_format;

enum ESocketStatus : unsigned long {
	eNone = 0,
	eDisabled,
	eClosed,
	eWaitForClose,
	eClosing,
	eListening,
	eSynReceived,
	eSynSent
};

typedef struct {
	short LocalPortNumber;
	short RemotePortNumber;
	unsigned long SocketStatus;
	unsigned long ProcessID;
	unsigned long long InBandwidthBytes;
	unsigned long long OutBandwidthBytes;
	network_string ProcessNameString;
	network_string LocalIPAddressString;
	network_string RemoteIPAddressString;
} NetworkTCPProcessStatusStruct;

typedef struct {
	unsigned long CountOfUDPListeners;
	unsigned long CountOfTCPErrors;
	unsigned long CountOfTCPConnections;
	unsigned long MaxCountOfTCPConnections;	
} NetworkGlobalStatus;

typedef boost::container::vector<NetworkTCPProcessStatusStruct> process_tcps_network_vector;

class netword_counter {
private:
	typedef enum _TCP_TABLE_CLASS {
		TCP_TABLE_BASIC_LISTENER,
		TCP_TABLE_BASIC_CONNECTIONS,
		TCP_TABLE_BASIC_ALL,
		TCP_TABLE_OWNER_PID_LISTENER,
		TCP_TABLE_OWNER_PID_CONNECTIONS,
		TCP_TABLE_OWNER_PID_ALL,
		TCP_TABLE_OWNER_MODULE_LISTENER,
		TCP_TABLE_OWNER_MODULE_CONNECTIONS,
		TCP_TABLE_OWNER_MODULE_ALL
	};

	typedef enum {
		MIB_TCP_STATE_CLOSED = 1,
		MIB_TCP_STATE_LISTEN = 2,
		MIB_TCP_STATE_SYN_SENT = 3,
		MIB_TCP_STATE_SYN_RCVD = 4,
		MIB_TCP_STATE_ESTAB = 5,
		MIB_TCP_STATE_FIN_WAIT1 = 6,
		MIB_TCP_STATE_FIN_WAIT2 = 7,
		MIB_TCP_STATE_CLOSE_WAIT = 8,
		MIB_TCP_STATE_CLOSING = 9,
		MIB_TCP_STATE_LAST_ACK = 10,
		MIB_TCP_STATE_TIME_WAIT = 11,
		MIB_TCP_STATE_DELETE_TCB = 12,
		MIB_TCP_STATE_RESERVED = 100
	} MIB_TCP_STATE;

	typedef enum {
		TcpConnectionEstatsSynOpts,
		TcpConnectionEstatsData,
		TcpConnectionEstatsSndCong,
		TcpConnectionEstatsPath,
		TcpConnectionEstatsSendBuff,
		TcpConnectionEstatsRec,
		TcpConnectionEstatsObsRec,
		TcpConnectionEstatsBandwidth,
		TcpConnectionEstatsFineRtt,
		TcpConnectionEstatsMaximum,
	} TCP_ESTATS_TYPE;

	typedef enum {
		TcpRtoAlgorithmOther = 1,
		TcpRtoAlgorithmConstant,
		TcpRtoAlgorithmRsre,
		TcpRtoAlgorithmVanj,

		MIB_TCP_RTO_OTHER = 1,
		MIB_TCP_RTO_CONSTANT = 2,
		MIB_TCP_RTO_RSRE = 3,
		MIB_TCP_RTO_VANJ = 4,
	} TCP_RTO_ALGORITHM;

	typedef struct _MIB_TCPSTATS_LH {
		union {
			boost::winapi::DWORD_ dwRtoAlgorithm;
			TCP_RTO_ALGORITHM RtoAlgorithm;
		};
		boost::winapi::DWORD_ dwRtoMin;
		boost::winapi::DWORD_ dwRtoMax;
		boost::winapi::DWORD_ dwMaxConn;
		boost::winapi::DWORD_ dwActiveOpens;
		boost::winapi::DWORD_ dwPassiveOpens;
		boost::winapi::DWORD_ dwAttemptFails;
		boost::winapi::DWORD_ dwEstabResets;
		boost::winapi::DWORD_ dwCurrEstab;
		boost::winapi::DWORD_ dwInSegs;
		boost::winapi::DWORD_ dwOutSegs;
		boost::winapi::DWORD_ dwRetransSegs;
		boost::winapi::DWORD_ dwInErrs;
		boost::winapi::DWORD_ dwOutRsts;
		boost::winapi::DWORD_ dwNumConns;
	};

	typedef struct _MIB_UDPSTATS {
		boost::winapi::DWORD_ dwInDatagrams;
		boost::winapi::DWORD_ dwNoPorts;
		boost::winapi::DWORD_ dwInErrors;
		boost::winapi::DWORD_ dwOutDatagrams;
		boost::winapi::DWORD_ dwNumAddrs;
	};

	typedef struct _MIB_TCPROW_LH {
		union {
			boost::winapi::DWORD_ dwState;	// <-- Использовалось вплоть до Windows Server 2003 включительно
			MIB_TCP_STATE State;			// <-- Испольуется начиная с Windows Vista+
		};
		boost::winapi::DWORD_ dwLocalAddr;
		boost::winapi::DWORD_ dwLocalPort;
		boost::winapi::DWORD_ dwRemoteAddr;
		boost::winapi::DWORD_ dwRemotePort;
	};

	typedef struct _MIB_TCP6ROW {
		MIB_TCP_STATE State;
		IN6_ADDR LocalAddr;
		DWORD dwLocalScopeId;
		DWORD dwLocalPort;
		IN6_ADDR RemoteAddr;
		DWORD dwRemoteScopeId;
		DWORD dwRemotePort;
	};

	typedef struct _MIB_TCP6TABLE {
		DWORD dwNumEntries;
		_MIB_TCP6ROW table[1];
	};

	typedef struct _MIB_TCPROW_W2K {
		boost::winapi::DWORD_ dwState;
		boost::winapi::DWORD_ dwLocalAddr;
		boost::winapi::DWORD_ dwLocalPort;
		boost::winapi::DWORD_ dwRemoteAddr;
		boost::winapi::DWORD_ dwRemotePort;
	};

	typedef struct _MIB_TCPROW_OWNER_PID {
		boost::winapi::DWORD_ dwState;
		boost::winapi::DWORD_ dwLocalAddr;
		boost::winapi::DWORD_ dwLocalPort;
		boost::winapi::DWORD_ dwRemoteAddr;
		boost::winapi::DWORD_ dwRemotePort;
		boost::winapi::DWORD_ dwOwningPid;
	};

	typedef struct _MIB_TCPTABLE_OWNER_PID {
		boost::winapi::DWORD_ dwNumEntries;
		_MIB_TCPROW_OWNER_PID table[1];
	};

	typedef boost::winapi::DWORD_(__stdcall GetExtendedTcpTable_t)(void*, unsigned long*, int, unsigned long, _TCP_TABLE_CLASS, unsigned long);
	typedef boost::winapi::DWORD_(__stdcall GetPerTcpConnectionEStats_t)(_MIB_TCPROW_LH*, TCP_ESTATS_TYPE, unsigned char*, unsigned long, unsigned long, unsigned char*, unsigned long, unsigned long, unsigned char*, unsigned long, unsigned long);
	typedef boost::winapi::DWORD_(__stdcall GetPerTcp6ConnectionEStats_t)(_MIB_TCP6ROW*, TCP_ESTATS_TYPE, unsigned char*, unsigned long, unsigned long, unsigned char*, unsigned long, unsigned long, unsigned char*, unsigned long, unsigned long);
	typedef boost::winapi::DWORD_(__stdcall GetTcpStatisticsEx_t)(_MIB_TCPSTATS_LH*, unsigned long);
	typedef boost::winapi::DWORD_(__stdcall GetUdpStatisticsEx_t)(_MIB_UDPSTATS*, unsigned long);

	GetExtendedTcpTable_t* pGetExtendedTcpTable = NULL;
	GetPerTcpConnectionEStats_t* pGetPerTcpConnectionEStats = NULL;
	GetPerTcp6ConnectionEStats_t* pGetPerTcp6ConnectionEStats = NULL;
	GetTcpStatisticsEx_t* pGetTcpStatistics = NULL;
	GetUdpStatisticsEx_t* pGetUdpStatistics = NULL;

	bool load_proc_for_vista_functions() {
		boost::dll::shared_library lib("Iphlpapi.dll", boost::dll::load_mode::search_system_folders);
		if (!lib.is_loaded()) return false;

		pGetExtendedTcpTable = &lib.get<GetExtendedTcpTable_t>("GetExtendedTcpTable");
		pGetPerTcpConnectionEStats = &lib.get<GetPerTcpConnectionEStats_t>("GetPerTcpConnectionEStats");
		pGetPerTcp6ConnectionEStats = &lib.get<GetPerTcp6ConnectionEStats_t>("GetPerTcp6ConnectionEStats");
		pGetTcpStatistics = &lib.get<GetTcpStatisticsEx_t>("GetTcpStatisticsEx");
		pGetUdpStatistics = &lib.get<GetUdpStatisticsEx_t>("GetUdpStatisticsEx");

		if (!pGetExtendedTcpTable || !pGetPerTcpConnectionEStats || !pGetPerTcp6ConnectionEStats || !pGetTcpStatistics || !pGetUdpStatistics) return false;
		return true;
	}

	bool get_per_tcp_connection_stats(_MIB_TCPROW_LH* pRow, TCP_ESTATS_TYPE StatsType, unsigned char* pRosData, 
		unsigned long RosDataSize, unsigned char* pRodData, unsigned long RodDataSize) {
		if (!pGetPerTcpConnectionEStats) return false;

		unsigned long retValue = pGetPerTcpConnectionEStats(pRow, StatsType, NULL, 0, 0, pRosData, 0, RosDataSize, pRodData, 0, RodDataSize);
		return retValue == boost::winapi::NO_ERROR_;
	}

	bool get_per_tcpv6_connection_stats(_MIB_TCP6ROW* pRow, TCP_ESTATS_TYPE StatsType, unsigned char* pRosData,
		unsigned long RosDataSize, unsigned char* pRodData, unsigned long RodDataSize) {
		if (!pGetPerTcp6ConnectionEStats) return false;

		unsigned long retValue = pGetPerTcp6ConnectionEStats(pRow, StatsType, NULL, 0, 0, pRosData, 0, RosDataSize, pRodData, 0, RodDataSize);
		return retValue == boost::winapi::NO_ERROR_;
	}

	bool get_extended_tcp_table(void* pBuffer, unsigned long* BufferSize, unsigned long ConnectionType) {
		if (!pGetExtendedTcpTable) return false;

		unsigned long retValue = pGetExtendedTcpTable(pBuffer, BufferSize, TRUE, ConnectionType, TCP_TABLE_OWNER_PID_ALL, 0);
		return retValue == boost::winapi::NO_ERROR_;
	}

	bool get_global_network_info(NetworkGlobalStatus& global_status, unsigned long ConnectionType) {
		_MIB_TCPSTATS_LH TcpStats = {};
		_MIB_UDPSTATS UdpStats = {};

		if (!pGetTcpStatistics || !pGetUdpStatistics) return false;
		if (pGetTcpStatistics(&TcpStats, ConnectionType) != boost::winapi::NO_ERROR_ || pGetUdpStatistics(&UdpStats, ConnectionType) != boost::winapi::NO_ERROR_) return false;

		global_status.CountOfTCPConnections = TcpStats.dwNumConns;
		global_status.CountOfTCPErrors = TcpStats.dwInErrs;
		global_status.MaxCountOfTCPConnections = TcpStats.dwMaxConn;
		global_status.CountOfUDPListeners = UdpStats.dwNumAddrs;
		return true;
	}


};

}}}}
#endif
