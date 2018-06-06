#include "../stdinc.h"
#include "zero_config.h"
#include "../cfg/json_config.h"

namespace agebull
{
	namespace zmq_net
	{
		void zero_config::read_json(const char* val)
		{
			boost::lock_guard<boost::mutex> guard(mutex_);
			acl::json json;
			json.update(val);
			acl::json_node* iter = json.first_node();
			while (iter)
			{
				const char* tag = iter->tag_name();
				if (tag == nullptr || tag[0] == 0)
				{
					iter = json.next_node();
					continue;
				}
				const int idx = strmatchi(16, tag
				                          , "station_name"
				                          , "station_type"
				                          , "request_port"
				                          , "worker_out_port"
				                          , "worker_in_port"
				                          , "description"
				                          , "caption"
				                          , "station_alias"
				                          , "station_state"
				                          , "request_in"
				                          , "request_out"
				                          , "request_err"
				                          , "worker_in"
				                          , "worker_out"
				                          , "worker_err"
				                          , "short_name");
				switch (idx)
				{
				case 0:
					station_name_ = iter->get_string();
					break;
				case 1:
					station_type_ = static_cast<int>(*iter->get_int64());
					break;
				case 2:
					request_port_ = static_cast<int>(*iter->get_int64());
					break;
				case 3:
					worker_out_port_ = static_cast<int>(*iter->get_int64());
					break;
				case 4:
					worker_in_port_ = static_cast<int>(*iter->get_int64());
					break;
				case 5:
					station_description_ = iter->get_string();
					break;
				case 6:
					station_caption_ = iter->get_string();
					break;
				case 7:
					alias_.clear();
					{
						auto ajson = iter->get_obj();
						if (!ajson->is_array())
							break;
						auto it = ajson->first_child();
						while (it)
						{
							alias_.emplace_back(it->get_string());
							it = ajson->next_child();
						}
					}
					break;
				case 8:
					station_state_ = static_cast<station_state>(*iter->get_int64());
					break;
				case 9:
					request_in = *iter->get_int64();
					break;
				case 10:
					request_out = *iter->get_int64();
					break;
				case 11:
					worker_err = *iter->get_int64();
					break;
				case 12:
					worker_in = *iter->get_int64();
					break;
				case 13:
					worker_out = *iter->get_int64();
					break;
				case 14:
					worker_err = *iter->get_int64();
					break;
				case 15:
					short_name = *iter->get_string();
					break;
				default: break;
				}
				iter = json.next_node();
			}
			check_type_name();
		}

		acl::string zero_config::to_json(int type)
		{
			boost::lock_guard<boost::mutex> guard(mutex_);
			acl::json json;
			acl::json_node& node = json.create_node();
			if (type != 1)
			{
				if (!station_name_.empty())
					node.add_text("station_name", station_name_.c_str());
				if (!short_name.empty())
					node.add_text("short_name", short_name.c_str());
				if (!station_description_.empty())
					node.add_text("_description", station_description_.c_str());
				if (!station_caption_.empty())
					node.add_text("_caption", station_caption_.c_str());
				if (alias_.size() > 0)
				{
					acl::json_node& array = json.create_array();
					array.set_tag("station_alias");
					array.add_array_text(short_name.c_str());
					for (auto alia : alias_)
					{
						array.add_array_text(alia.c_str());
					}
				}
				if (station_type_ > 0)
					node.add_number("station_type", station_type_);
				if (request_port_ > 0)
					node.add_number("request_port", request_port_);
				if (worker_in_port_ > 0)
					node.add_number("worker_in_port", worker_in_port_);
				if (worker_out_port_ > 0)
					node.add_number("worker_out_port", worker_out_port_);
			}
			if (type < 2)
			{
				node.add_number("station_state", static_cast<int>(station_state_));
				node.add_number("request_in", request_in);
				node.add_number("request_out", request_out);
				node.add_number("request_err", request_err);
				node.add_number("worker_in", worker_in);
				node.add_number("worker_out", worker_out);
				node.add_number("worker_err", worker_err);
				if (workers.size() > 0)
				{
					acl::json_node& array = json.create_array();
					for (auto& worker : workers)
					{
						acl::json_node& work = json.create_node();
						worker.second.to_json(work);
						array.add_child(work);
					}
					acl::json_node& workers_n = node.add_child("workers", array);

				}
			}
			return node.to_string();
		}
	}
}