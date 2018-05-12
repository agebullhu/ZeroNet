/**
 * ZMQ投票代理类
 *
 *
 */

#include "../stdafx.h"
#include "VoteStation.h"
#include <zeromq/zhelpers.h>

namespace agebull
{
	namespace zmq_net
	{


		bool vote_station::get_voters(const char* client_addr, const char* request_token)
		{
			acl::string strs("[");
			bool first = true;
			for (auto iter : workers_)
			{
				if (!first)
				{
					strs.append("\"");
					first = false;
				}
				else
					strs.append(",\"");
				strs.append(iter.second.flow_name.c_str());
				strs.append("\"");
			}
			strs.append("]");
			return send_state(client_addr, request_token, "&", strs.c_str());
		}

		void vote_station::launch(const shared_ptr<vote_station>& arg)
		{
			vote_station* station = arg.get();
			if (!station_warehouse::join(station))
			{
				return;
			}
			if (!station->do_initialize())
				return;
			zmq_threadstart(plan_poll, nullptr);
			const bool re_strart = station->poll();
			station_warehouse::left(station);
			station->destruct();
			if (re_strart)
			{
				vote_station* station2 = new vote_station(station->station_name_);
				station2->zmq_state_ = zmq_socket_state::Again;
				boost::thread thrds_s1(boost::bind(launch, shared_ptr<vote_station>(station2)));
			}
			else
			{
				log_msg3("Station:%s(%d | %d) closed", station->station_name_.c_str(), station->request_port_, station->response_port_);
			}
			sleep(1);
			delete station;
		}

		/**
		* \brief 执行一条命令
		*/
		sharp_char vote_station::command(const char* caller, vector<sharp_char> lines)
		{
			if (send_state(caller, *lines[0], *lines[1], *lines[2]))
				return sharp_char("vote start");
			return sharp_char(ZERO_STATUS_FAILED);
		}


		/**
		* \brief 当远程调用进入时的处理
		*/
		void vote_station::request(ZMQ_HANDLE socket, bool inner)
		{
			vector<sharp_char> list;
			//0 路由到的地址 1 空帧 2 请求标识 3 命令 4 参数
			zmq_state_ = recv(request_scoket_, list);

			if (list[2][0] == '@')//计划类型
			{
				save_plan(socket, list);
				zmq_state_ = send_addr(socket, *list[0]);
				zmq_state_ = send_late(socket, ZERO_STATUS_PLAN);
				return;
			}

			redis_live_scope redis_live_scope(REDIS_DB_ZERO_VOTE);
			char key[256];
			sprintf(key, "vote:%s:%s", station_name_.c_str(), *list[2]);
			trans_redis& redis = trans_redis::get_context();
			switch (list[3][0])
			{
			case '*': //请求投票者名单
				get_voters(*list[0], *list[2]);
				break;
			case '$': //取投票状态
			{
				std::map<acl::string, acl::string> values;
				if (redis->hgetall(key, values))
					send_state(*list[0], *list[2], values["*"].c_str(), values);
				else
					send_state(*list[0], *list[2], ZERO_STATUS_ERROR, values);
				break;
			}
			case '@': //开始投票
				if (start_vote(*list[0], *list[3], *list[4]))
				{
					std::map<acl::string, acl::string> values;
					redis->hgetall(key, values);
					send_state(*list[0], *list[2], values["*"].c_str(), values);
				}
				else
					send_state(*list[0], *list[2], "*", ZERO_STATUS_ERROR);
				break;
			case '%': //继续等待投票结束
				redis->hset(key, "*", ZERO_STATUS_VOTE_WAITING);
				send_state(*list[0], *list[2], "*", ZERO_STATUS_VOTE_WAITING);
				break;
			case 'v': //结束投票
				redis->hset(key, "*", "-end");
				send_state(*list[0], *list[2], "*", ZERO_STATUS_VOTE_END);
				break;
			case 'x': //删除投票
				redis->hset(key, "*", ZERO_STATUS_VOTE_CLOSED);
				redis->pexpire(key, 360000);//一小时后过期
				send_state(*list[0], *list[2], "*", ZERO_STATUS_VOTE_BYE);
				break;
			default:
				send_state(*list[0], *list[2], "*", ZERO_STATUS_ERROR);
				break;
			}
		}

		bool vote_station::start_vote(const char* client_addr, const char* request_token, const char* request_argument)
		{
			redis_live_scope redis_live_scope(REDIS_DB_ZERO_VOTE);
			trans_redis& redis = trans_redis::get_context();
			char key[256];
			sprintf(key, "vote:%s", request_token);
			redis->hset(key, "*", ZERO_STATUS_VOTE_START);
			redis->hset(key, "#", request_argument);
			redis->hset(key, "@", client_addr);

			//路由到所有工作对象
			for (auto voter : workers_)
			{
				zmq_state_ = send_addr(response_socket_, voter.second.net_name.c_str());
				zmq_state_ = send_more(response_socket_, client_addr);
				zmq_state_ = send_more(response_socket_, request_token);
				zmq_state_ = send_late(response_socket_, request_argument);
				if (zmq_state_ == zmq_socket_state::Succeed)
					redis->hset(key, voter.second.flow_name.c_str(), ZERO_STATUS_VOTE_SENDED);
				else
					redis->hset(key, voter.second.flow_name.c_str(), ZERO_STATUS_ERROR);
			}
			return zmq_state_ == zmq_socket_state::Succeed;
		}

		bool vote_station::re_push_vote(const char* client_addr, const char* request_token)
		{
			redis_live_scope redis_live_scope(REDIS_DB_ZERO_VOTE);
			trans_redis& redis = trans_redis::get_context();
			std::map<acl::string, acl::string> values;
			char key[256];
			sprintf(key, "vote:%s", request_token);
			if (!redis->hgetall(key, values))
			{
				send_state(client_addr, request_token, "*", ZERO_STATUS_ERROR);
				return false;
			}
			redis->hset(key, "@", client_addr);
			const auto request_argument = values["#"];
			//路由到所有未返回的工作对象
			for (auto kv : values)
			{
				switch (kv.first[0])
				{
				case '*':
				case '@':
				case '#':
					continue;
				default:
					switch (kv.second[0])
					{
					case '-':
					case '+':
						zmq_state_ = send_addr(response_socket_, kv.first.c_str());
						zmq_state_ = send_more(response_socket_, client_addr);
						zmq_state_ = send_more(response_socket_, request_token);
						zmq_state_ = send_late(response_socket_, request_argument);
						if (zmq_state_ == zmq_socket_state::Succeed)
							redis->hset(key, kv.first.c_str(), ZERO_STATUS_VOTE_SENDED);
						break;
					default:;
					}
				}
			}
			return zmq_state_ == zmq_socket_state::Succeed;
		}

		bool vote_station::send_state(const char* client_addr, const char* request_token, const char* voter, const char* state)
		{
			zmq_state_ = send_addr(request_scoket_, client_addr);
			zmq_state_ = send_more(request_scoket_, request_token);
			zmq_state_ = send_more(request_scoket_, voter);
			zmq_state_ = send_late(request_scoket_, state);
			return zmq_state_ == zmq_socket_state::Succeed;
		}

		bool vote_station::send_state(const char* client_addr, const char* request_token, const char* state, std::map<acl::string, acl::string>& values)
		{
			zmq_state_ = send_addr(request_scoket_, client_addr);
			zmq_state_ = send_more(request_scoket_, request_token);
			zmq_state_ = send_more(request_scoket_, "$");
			for (auto kv : values)
			{
				if (kv.first == "*" || kv.first == "#")
					continue;
				zmq_state_ = send_more(request_scoket_, kv.first.c_str());
				zmq_state_ = send_more(request_scoket_, kv.second.c_str());
			}
			zmq_state_ = send_more(request_scoket_, "*");
			zmq_state_ = send_late(request_scoket_, state);
			return zmq_state_ == zmq_socket_state::Succeed;
		}

		/**
		* \brief 当工作操作返回时的处理
		*/
		void vote_station::response()
		{
			vector<sharp_char> list;
			//0 路由到的地址 1 空帧 2 原始者请求地址 3 请求标识 4 结果
			zmq_state_ = recv(response_socket_, list);
			if (zmq_state_ == zmq_socket_state::TimedOut)
			{
				return;
			}
			if (zmq_state_ != zmq_socket_state::Succeed)
			{
				log_error3("接收结果失败%s(%d)%s", station_name_.c_str(), response_port_, state_str(zmq_state_));
				return;
			}

			switch (list[2][0])
			{
			case ZERO_WORKER_JOIN://加入
				worker_join(*list[0]/*, *list[3], true*/);
				send_addr(response_socket_, *list[0]);
				send_late(response_socket_, ZERO_STATUS_WECOME);
				return;
			case ZERO_WORKER_LISTEN://开始工作
				return;
			default: break;
			}
			char key[256];
			sprintf(key, "vote:%s", *list[3]);
			redis_live_scope redis_live_scope(REDIS_DB_ZERO_VOTE);
			trans_redis& redis = trans_redis::get_context();
			if (redis->exists(key))//如果投票还存在，返回结投票者
			{
				auto voter = workers_[*list[0]];
				redis->hset(key, voter.flow_name.c_str(), *list[4]);
				send_state(*list[2], *list[3], voter.flow_name.c_str(), *list[4]);
			}
		}
	}
}
