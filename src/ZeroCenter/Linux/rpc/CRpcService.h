#ifndef AGEBULL_RPCSERVICE_H
#pragma once
#include "../stdafx.h"
namespace agebull
{
	class rpc_service
	{
		/**
		 * \brief 取本机IP并显示在控制台
		 */
		static bool get_local_ips();
	public:
		/**
		* \brief 初始化
		*/
		static bool initialize()
		{
			// 在程序初始化时打开日志
			acl::acl_cpp_init();
			string path;
			GetProcessFilePath(path);
			cout << "Current folder:" << path << endl;
			auto log = path;
			log.append("/zero.log");
			logger_open(log.c_str(), "zero_center", DEBUG_CONFIG);
			{
				redis_live_scope scope; 
				if (!ping_redis())
				{
					std::cout << "Redis:failed" << endl;
					return false;
				}
				std::cout << "Redis:ready" << endl;
			}
			if (!get_local_ips())
			{
				std::cout << "Ip:empty" << endl;
				return false;
			}
			return true;
		}

		/**
		* \brief
		*/
		static void start()
		{
			config_zero_center();
			start_zero_center();

			log_msg("zero center in service");
		}

		/**
		* \brief 中止
		*/
		static void stop()
		{
			close_net_command();
			distory_net_command();
			thread_sleep(50);
			acl::log::close();
		}
	};

	/**
	* \brief 取本机IP并显示在控制台
	*/
	inline bool rpc_service::get_local_ips()
	{
		char hname[128];
		gethostname(hname, sizeof(hname));
		cout << "Host:" << hname << endl << "IPs:";
		struct addrinfo hint{};
		memset(&hint, 0, sizeof(hint));
		hint.ai_family = AF_INET;
		hint.ai_socktype = SOCK_STREAM;

		addrinfo* info = nullptr;
		char ipstr[16];
		bool first = true;
		if (getaddrinfo(hname, nullptr, &hint, &info) == 0 && info != nullptr)
		{
			addrinfo* now = info;
			do
			{
				inet_ntop(AF_INET, &(reinterpret_cast<struct sockaddr_in *>(now->ai_addr)->sin_addr), ipstr, 16);
				if (first)
					first = false;
				else
					cout << ",";
				cout << ipstr;
				now = now->ai_next;
			}
			while (now != nullptr);
			freeaddrinfo(info);
		}
		cout << endl;
		return !first;
	}
}
#endif AGEBULL_RPCSERVICE_H