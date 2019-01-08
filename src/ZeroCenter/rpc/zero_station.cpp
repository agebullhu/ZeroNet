#include "../stdafx.h"
#include "zero_station.h"
#include <netinet/in.h>
#include <arpa/inet.h>



namespace agebull
{
	namespace zero_net
	{

		//map<int64, vector<shared_char>> zero_station::results;
		//boost::mutex zero_station::results_mutex_;

		zero_station::zero_station(const string name, int type, int req_zmq_type, int res_zmq_type)
			: req_zmq_type_(req_zmq_type)
			, res_zmq_type_(res_zmq_type)
			, station_type_(type)
			, poll_items_(nullptr)
			, poll_count_(0)
			, task_semaphore_(0)
			, station_name_(name)
			, config_(station_warehouse::get_config(name.c_str()))
			, request_scoket_tcp_(nullptr)
			, request_socket_inproc_(nullptr)
			, trace_socket_inproc_(nullptr)
			, plan_socket_inproc_(nullptr)
			, proxy_socket_inproc_(nullptr)
			, worker_in_socket_tcp_(nullptr)
			, worker_out_socket_tcp_(nullptr)
			//, worker_out_socket_ipc_(nullptr)
			, zmq_state_(zmq_socket_state::succeed)
		{
			assert(req_zmq_type_ != ZMQ_PUB);
		}

		zero_station::zero_station(shared_ptr<zero_config>& config, int type, int req_zmq_type, int res_zmq_type)
			: req_zmq_type_(req_zmq_type)
			, res_zmq_type_(res_zmq_type)
			, station_type_(type)
			, poll_items_(nullptr)
			, poll_count_(0)
			, task_semaphore_(0)
			, station_name_(config->station_name)
			, config_(config)
			, request_scoket_tcp_(nullptr)
			, request_socket_inproc_(nullptr)
			, trace_socket_inproc_(nullptr)
			, plan_socket_inproc_(nullptr)
			, proxy_socket_inproc_(nullptr), worker_in_socket_tcp_(nullptr)
			, worker_out_socket_tcp_(nullptr)
			//, worker_out_socket_ipc_(nullptr)
			, zmq_state_(zmq_socket_state::succeed)
		{
			assert(req_zmq_type_ != ZMQ_PUB);
		}

		/**
		* \brief 初始化
		*/
		bool zero_station::initialize()
		{
			if (config_->is_state(station_state::stop))
				return false;
			//boost::lock_guard<boost::mutex> guard(mutex_);
			config_->start();
			zmq_state_ = zmq_socket_state::succeed;

			const char* station_name = get_station_name();
			poll_items_ = new zmq_pollitem_t[8];
			memset(poll_items_, 0, sizeof(zmq_pollitem_t) * 8);

			poll_count_ = 0;
			request_scoket_tcp_ = socket_ex::create_res_socket_tcp(station_name, req_zmq_type_, config_->request_port);
			if (request_scoket_tcp_ == nullptr)
			{
				config_->runtime_state(station_state::failed);
				config_->error("initialize request tpc", zmq_strerror(zmq_errno()));
				return false;
			}
			if (req_zmq_type_ == ZMQ_ROUTER)
			{
				socket_ex::setsockopt(request_scoket_tcp_, ZMQ_ROUTER_MANDATORY, 1);
			}
			poll_items_[poll_count_++] = { request_scoket_tcp_, 0, ZMQ_POLLIN, 0 };
			//if (json_config::use_ipc_protocol)
			//{
			//	request_socket_ipc_ = socket_ex::create_res_socket_ipc(station_name, "req", req_zmq_type_);
			//	if (request_socket_ipc_ == nullptr)
			//	{
			//		config_->runtime_state(station_state::Failed);
			//		config_->error("initialize request ipc", zmq_strerror(zmq_errno()));
			//		return false;
			//	}
			//	poll_items_[poll_count_++] = { request_socket_ipc_, 0, ZMQ_POLLIN, 0 };
			//}

			request_socket_inproc_ = socket_ex::create_res_socket_inproc(station_name, req_zmq_type_);
			if (request_socket_inproc_ == nullptr)
			{
				config_->runtime_state(station_state::failed);
				config_->error("initialize request inproc", zmq_strerror(zmq_errno()));
				return false;
			}
			poll_items_[poll_count_++] = { request_socket_inproc_, 0, ZMQ_POLLIN, 0 };


			if (zero_def::station_type::is_general_station(station_type_))
			{
				proxy_socket_inproc_ = socket_ex::create_req_socket_inproc(zero_def::name::proxy_dispatcher, station_name);
				plan_socket_inproc_ = socket_ex::create_req_socket_inproc(zero_def::name::plan_dispatcher, station_name);
				trace_socket_inproc_ = socket_ex::create_req_socket_inproc(zero_def::name::trace_dispatcher, station_name);
			}

			if (zero_def::station_type::is_pub_station(station_type_))
			{
				worker_out_socket_tcp_ = socket_ex::create_res_socket_tcp(station_name, ZMQ_PUB, config_->worker_out_port);
				if (worker_out_socket_tcp_ == nullptr)
				{
					config_->runtime_state(station_state::failed);
					config_->error("initialize worker out", zmq_strerror(zmq_errno()));
					return false;
				}
				if (station_type_ == zero_def::station_type::queue)
				{
					worker_in_socket_tcp_ = socket_ex::create_res_socket_tcp(station_name, ZMQ_PUB, config_->worker_in_port);
					if (worker_in_socket_tcp_ == nullptr)
					{
						config_->runtime_state(station_state::failed);
						config_->error("initialize worker in", zmq_strerror(zmq_errno()));
						return false;
					}
				}
				//if (json_config::use_ipc_protocol)
				//{
				//	worker_out_socket_ipc_ = socket_ex::create_res_socket_ipc(station_name, "sub", ZMQ_PUB);
				//	if (worker_out_socket_ipc_ == nullptr)
				//	{
				//		config_->runtime_state(station_state::Failed);
				//		config_->error("initialize worker out", zmq_strerror(zmq_errno()));
				//		return false;
				//	}
				//}
			}
			else if (zero_def::station_type::is_api_station(station_type_))
			{
				worker_out_socket_tcp_ = socket_ex::create_res_socket_tcp(station_name, res_zmq_type_, config_->worker_out_port);

				if (worker_out_socket_tcp_ == nullptr)
				{
					config_->runtime_state(station_state::failed);
					config_->error("initialize worker out", zmq_strerror(zmq_errno()));
					return false;
				}
				if (res_zmq_type_ == ZMQ_ROUTER)
				{
					socket_ex::setsockopt(worker_out_socket_tcp_, ZMQ_ROUTER_MANDATORY, 1);
				}
				poll_items_[poll_count_++] = { worker_out_socket_tcp_, 0, ZMQ_POLLIN, 0 };

				worker_in_socket_tcp_ = socket_ex::create_res_socket_tcp(station_name, ZMQ_ROUTER, config_->worker_in_port);
				if (worker_in_socket_tcp_ == nullptr)
				{
					config_->runtime_state(station_state::failed);
					config_->error("initialize worker in", zmq_strerror(zmq_errno()));
					return false;
				}
				socket_ex::setsockopt(worker_in_socket_tcp_, ZMQ_ROUTER_MANDATORY, 1);
				poll_items_[poll_count_++] = { worker_in_socket_tcp_, 0, ZMQ_POLLIN, 0 };
			}
			initialize_ext();
			if (global_config::monitor_socket)
			{
				switch (station_type_)
				{
				case zero_def::station_type::dispatcher:
					zmq_monitor::set_monitor(station_name, &worker_out_socket_tcp_, "dis");
					break;
				case zero_def::station_type::queue:
					zmq_monitor::set_monitor(station_name, &worker_out_socket_tcp_, "que");
					break;
				case zero_def::station_type::trace:
					break;
				case zero_def::station_type::notify:
					zmq_monitor::set_monitor(station_name, &worker_out_socket_tcp_, "pub");
					break;
				case zero_def::station_type::api:
				case zero_def::station_type::vote:
				case zero_def::station_type::route_api:
					zmq_monitor::set_monitor(station_name, &worker_in_socket_tcp_, "api");
					break;
				}
			}
			return true;
		}

		/**
		* \brief 析构
		*/
		bool zero_station::destruct()
		{
			//boost::lock_guard<boost::mutex> guard(mutex_);
			if (poll_items_ == nullptr)
				return true;
			if (request_scoket_tcp_ != nullptr)
			{
				socket_ex::close_res_socket(request_scoket_tcp_, config_->get_request_address().c_str());
			}
			if (worker_in_socket_tcp_ != nullptr)
			{
				socket_ex::close_res_socket(worker_in_socket_tcp_, config_->get_work_in_address().c_str());
			}
			if (worker_out_socket_tcp_ != nullptr)
			{
				socket_ex::close_res_socket(worker_out_socket_tcp_, config_->get_work_out_address().c_str());
			}
			if (request_socket_inproc_ != nullptr)
			{
				make_inproc_address(address, get_station_name());
				socket_ex::close_res_socket(request_socket_inproc_, address);
			}
			if (plan_socket_inproc_ != nullptr)
			{
				make_inproc_address(address, zero_def::name::plan_dispatcher);
				socket_ex::close_req_socket(plan_socket_inproc_, address);
			}
			if (proxy_socket_inproc_ != nullptr)
			{
				make_inproc_address(address, zero_def::name::proxy_dispatcher);
				socket_ex::close_req_socket(proxy_socket_inproc_, address);
			}
			if (trace_socket_inproc_ != nullptr)
			{
				make_inproc_address(address, zero_def::name::trace_dispatcher);
				socket_ex::close_req_socket(trace_socket_inproc_, address);
			}
			//if (request_socket_ipc_ != nullptr)
			//{
			//	make_ipc_address(address, get_station_name(), "req");
			//	socket_ex::close_res_socket(request_socket_ipc_, address);
			//}
			//if (worker_out_socket_ipc_ != nullptr)
			//{
			//	make_ipc_address(address, get_station_name(), "sub");
			//	socket_ex::close_res_socket(worker_out_socket_ipc_, address);
			//}
			delete[]poll_items_;
			poll_items_ = nullptr;
			destruct_ext();
			return true;
		}

		/**
		* \brief 消息泵
		* \return
		*/
		bool zero_station::poll()
		{
			config_->runing();
			//登记线程开始
			set_command_thread_run(get_station_name());

			while (true)
			{
				if (!can_do())
				{
					zmq_state_ = zmq_socket_state::intr;
					break;
				}
				const int state = zmq_poll(poll_items_, poll_count_, global_config::pool_timeout);
				if (state == 0)//超时或需要关闭
				{
					continue;
				}
				if (state < 0)
				{
					zmq_state_ = socket_ex::check_zmq_error();
					config_->log("pool error", socket_ex::state_str(zmq_state_));
					if (zmq_state_ < zmq_socket_state::again)
						continue;
					break;
				}
				zmq_state_ = zmq_socket_state::succeed;
				//#pragma omp parallel  for schedule(dynamic,3)
				for (int idx = 0; idx < poll_count_; idx++)
				{
					if (poll_items_[idx].revents & ZMQ_POLLIN)
					{
						poll_items_[idx].revents = 0;
						if (poll_items_[idx].socket == request_scoket_tcp_)
						{
							config_->request_in++;
							on_request(poll_items_[idx], false);
						}
						else if (poll_items_[idx].socket == request_socket_inproc_)
						{
							config_->request_in++;
							on_request(poll_items_[idx], true);
						}
						else if (poll_items_[idx].socket == worker_in_socket_tcp_)
						{
							on_response(poll_items_[idx]);
						}
						else if (global_config::api_route_mode && poll_items_[idx].socket == worker_out_socket_tcp_)
						{
							on_request(poll_items_[idx], false);
						}
						else
						{
							vector<shared_char> list;
							zmq_state_ = socket_ex::recv(poll_items_[idx].socket, list);
						}
						//else if (poll_items_[idx].socket == request_socket_ipc_)
						//{
						//	config_->request_in++;
						//	request(poll_items_[idx].socket, false);
						//}
					}
					/*if (poll_items_[idx].revents & ZMQ_POLLOUT)
					{
						if (poll_items_[idx].socket == request_scoket_tcp_)
						{
							config_->request_out++;
						}
						else if (poll_items_[idx].socket == request_socket_ipc_)
						{
							config_->request_out++;
						}
						else if (poll_items_[idx].socket == worker_out_socket_tcp_)
						{
							config_->worker_out++;
						}
					}
					if (poll_items_[idx].revents & ZMQ_POLLERR)
					{
						const zmq_socket_state err_state = check_zmq_error();
						config_->error("ZMQ_POLLERR", state_str(err_state));
					}*/
				}
			}
			const zmq_socket_state state = zmq_state_;
			config_->closing();
			return state < zmq_socket_state::term && state > zmq_socket_state::empty;
		}

		/**
		* \brief 调用集合的响应
		*/
		void zero_station::on_response(zmq_pollitem_t& socket)
		{
			vector<shared_char> list;
			zmq_state_ = socket_ex::recv(socket.socket, list);
			if (zmq_state_ == zmq_socket_state::timed_out)
			{
				config_->error("on_response", "timed_out");
				config_->worker_in++;
				config_->worker_err++;
				return;
			}
			if (zmq_state_ != zmq_socket_state::succeed)
			{
				config_->worker_in++;
				config_->worker_err++;
				config_->error("read work result", socket_ex::state_str(zmq_state_));
				return;
			}
			if (list.size() == 2 && strcmp(*list[1], "\004PING") == 0)
			{
				ping(socket.socket, list);
				return;
			}
			config_->worker_in++;
			if (list.size() < 3)
			{
				config_->worker_err++;
				config_->error("work result layout error", "size < 2");
				return;
			}
			if (strcmp(*list[list.size() - 1], global_config::service_key) != 0)
			{
				config_->worker_err++;
				config_->error("on_response", "service key error");
				send_request_status(socket.socket, *list[0], zero_def::status::deny_error, false);
				return;
			}
			if (list[1].tag() == zero_def::frame::extend_end)
			{
				config_->worker_in--;
				heartbeat(socket.socket, list[1][1], list);
				return;
			}
			ping(socket.socket, list);
			job_end(list);
		}

		/**
		* \brief 调用集合的响应
		*/
		void zero_station::on_request(zmq_pollitem_t& socket, bool inner)
		{
			vector<shared_char> list;
			zmq_state_ = socket_ex::recv(socket.socket, list);

			if (zmq_state_ != zmq_socket_state::succeed)
			{
				config_->request_err++;
				config_->error("on_request", socket_ex::state_str(zmq_state_));
				return;
			}
			const size_t list_size = int(inner ? list.size() - 1 : list.size());
			if (list_size < 2)
			{
				config_->request_err++;
				config_->debug("on_request", "frame_invalid");
				return;
			}
			if (strcmp(*list[1], "\004PING") == 0)
			{
				config_->request_in--;
				ping(socket.socket, list);
				return;
			}
			if (simple_command(socket.socket, list, list[1], inner))
			{
				return;
			}
			shared_char& description = list[inner ? 2 : 1];
			if (description.command() < zero_def::command::none)
			{
				send_request_status(socket.socket, *list[0], zero_def::status::frame_invalid, false);
				return;
			}
			var frame_size = description.frame_size();
			var descr_size = description.size();
			if ((frame_size + 1) > descr_size || (frame_size + 2) != list_size)
			{
				config_->request_err++;
				config_->error("on_request", "frame_invalid");
				send_request_status(socket.socket, *list[0], zero_def::status::frame_invalid, false);
				return;
			}
			if (!inner)
			{
				if (description[description.frame_size() + 1] != zero_def::frame::service_key)
				{
					config_->request_err++;
					config_->error("on_request", "service key error");
					send_request_status(socket.socket, *list[0], zero_def::status::deny_error, false);
					return;
				}
				if (strcmp(*list[list.size() - 1], global_config::service_key) != 0)
				{
					config_->request_err++;
					config_->error("on_request", "service key error");
					send_request_status(socket.socket, *list[0], zero_def::status::deny_error, false);
					return;
				}
			}
			if (!description.hase_frame(zero_def::frame::global_id))
			{
				description.append_frame(zero_def::frame::global_id);
				shared_char global_id;
				global_id.set_int64(station_warehouse::get_glogal_id());
				list.push_back(global_id);
				job_start(socket.socket, list, inner, false);
			}
			else
			{
				job_start(socket.socket, list, inner, true);
			}
		}

		/**
		* \brief 内部命令
		*/
		bool zero_station::simple_command(zmq_handler socket, vector<shared_char>& list, shared_char& description, bool inner)
		{
			const uchar cmd = description.command();
			switch (cmd)
			{
			case zero_def::command::ping:
				send_request_status(socket, *list[0], zero_def::status::ok, false);
				return true;
			case zero_def::command::plan:
				if (station_type_ == zero_def::station_type::plan)
					return false;
				job_plan(socket, list);
				return true;
			case zero_def::command::global_id:
				global_id_req(socket, list, description);
				return true;
			case zero_def::command::heart_join:
			case zero_def::command::heart_ready:
			case zero_def::command::heart_pitpat:
			case zero_def::command::heart_left:
				return heartbeat(socket, cmd, list);
			default:
				return simple_command_ex(socket, list, description, inner);
			}
		}

		/**
		* \brief 请求取得全局标识
		*/
		void zero_station::global_id_req(zmq_handler socket, vector<shared_char>& list, shared_char& description)
		{
			assert(list.size() > 0);
			char global_id[32];
			sprintf(global_id, "%llx", station_warehouse::get_glogal_id());

			vector<shared_char> ls;
			ls.push_back(list[0]);
			shared_char descirpt;
			descirpt.alloc_desc(8, zero_def::status::ok);
			ls.push_back(descirpt);
			descirpt.append_frame(zero_def::frame::global_id);
			ls.push_back(global_id);

			for (size_t i = 2; i <= description.frame_size() + 2; i++)
			{
				switch (description[i])
				{
				case zero_def::frame::requester:
					descirpt.append_frame(zero_def::frame::requester);
					ls.push_back(list[i]);
					break;
				case zero_def::frame::request_id:
					descirpt.append_frame(zero_def::frame::requester);
					ls.push_back(list[i]);
					break;
				}
			}
			descirpt.tag(zero_def::frame::result_end);
			send_request_result(socket, ls, false);
		}

		/**
		* \brief 工作进入计划
		*/
		void zero_station::job_plan(zmq_handler socket, vector<shared_char>& list)
		{
			assert(list.size() > 1);
			list.emplace_back(station_name_);
			list[1].append_frame(zero_def::frame::station_id);
			if (socket_ex::send(plan_socket_inproc_, list) != zmq_socket_state::succeed)
			{
				send_request_status(socket, *list[0], zero_def::status::send_error, true);
				return;
			}

			vector<shared_char> result;
			if (socket_ex::recv(plan_socket_inproc_, result) == zmq_socket_state::succeed)
			{
				result.insert(result.begin(), list[0]);
				send_request_result(socket, result, true);
			}
			else
			{
				send_request_status(socket, *list[0], zero_def::status::recv_error, true);
			}
		}

		/**
		* \brief 网络数据跟踪
		*/
		void zero_station::trace(uchar io, vector<shared_char> list, const char* err_msg)
		{
			if (!global_config::trace_net)
				return;
			shared_char description(list[1].get_buffer(), list[1].size());
			description.append_frame(zero_def::frame::station_id);
			list.push_back(config_->station_name);
			description.append_frame(zero_def::frame::station_type);
			list.push_back(config_->type_name_);
			if (err_msg != nullptr)
			{
				description.append_frame(zero_def::frame::status);
				list.push_back(err_msg);
			}
			description.tag(io);
			list[1] = description;
			zmq_socket_state state;
			{
				//boost::lock_guard<boost::mutex> guard(send_mutex_);
				state = socket_ex::send(trace_socket_inproc_, list);
			}
			if (state != zmq_socket_state::succeed)
			{
				config_->error("send_trace", err_msg, socket_ex::state_str(state));
			}
		}

		/**
		* \brief 反向代理执行完成
		*/
		void zero_station::proxy_end(vector<shared_char>& list)
		{
			if (socket_ex::send(proxy_socket_inproc_, list) != zmq_socket_state::succeed)
			{
				config_->error("send to proxy dispatcher failed", zero_def::desc_str(false, list[1].get_buffer(), list.size()));
				trace(3, list, "send to proxy dispatcher failed");
			}
			else
			{
				trace(3, list, nullptr);
			}
		}
		/**
		* \brief 计划执行完成
		*/
		void zero_station::plan_end(vector<shared_char>& list)
		{
			shared_char& description = list[1];
			list.emplace_back(station_name_);
			description.append_frame(zero_def::frame::station_id);
			
			if (socket_ex::send(plan_socket_inproc_, list) != zmq_socket_state::succeed)
			{
				config_->error("send to plan dispatcher failed", zero_def::desc_str(false, list[1].get_buffer(), list.size()));
				trace(3, list, "send to plan dispatcher failed");
			}
			else
			{
				trace(3, list, nullptr);
			}
		}
		/**
		* \brief 暂停
		*/
		bool zero_station::pause()
		{
			if (zero_def::station_type::is_sys_station(config_->station_type) || !config_->is_state(station_state::run))
				return false;
			config_->set_state(station_state::pause);
			zero_event(zero_net_event::event_station_pause, "station", config_->station_name.c_str(), nullptr);
			return true;
		}

		/**
		* \brief 继续
		*/
		bool zero_station::resume()
		{
			if (zero_def::station_type::is_sys_station(config_->station_type) || !config_->is_state(station_state::pause))
				return false;
			config_->set_state(station_state::run);
			zero_event(zero_net_event::event_station_resume, "station", config_->station_name.c_str(), nullptr);
			return true;
		}

		/**
		* \brief 关闭
		*/

		bool zero_station::close(bool waiting)
		{
			if (zero_def::station_type::is_sys_station(config_->station_type))
				return false;
			switch (config_->get_state())
			{
			case station_state::run:
			case station_state::pause:
			case station_state::stop:
				break;
			default:
				return false;
			}
			config_->log("close");
			config_->runtime_state(station_state::closing);
			zero_event(zero_net_event::event_station_closing, "station", config_->station_name.c_str(), nullptr);
			while (waiting && config_->is_state(station_state::closing))
				THREAD_SLEEP(50);
			return true;
		}
	}
}