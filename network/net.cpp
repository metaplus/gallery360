#include "stdafx.h"
#include "net.hpp"
#include <boost/property_tree/xml_parser.hpp>
#include <folly/executors/thread_factory/NamedThreadFactory.h>

std::vector<boost::thread*> net::create_asio_threads(boost::asio::io_context& context, boost::thread_group& thread_group, uint32_t num) {
	std::vector<boost::thread*> threads(num);
	std::generate(threads.begin(), threads.end(),
				  [&thread_group, &context] {
					  return thread_group.create_thread(
						  [&context] {
							  try {
								  context.run();
							  } catch (...) {
								  fmt::print(std::cerr, "Exception@{}: {}\n",
											 boost::this_thread::get_id(),
											 boost::diagnostic_information(boost::current_exception()));
								  return EXIT_FAILURE;
							  }
							  return EXIT_SUCCESS;
						  });
				  });
	return threads;
}

std::vector<std::thread> net::create_asio_named_threads(boost::asio::io_context& context, uint32_t num) {
	std::vector<std::thread> threads(num);
	auto thread_factory = std::make_unique<folly::NamedThreadFactory>("NetAsio");
	std::generate(threads.begin(), threads.end(),
				  [&thread_factory, &context] {
					  return thread_factory->newThread([&context] { context.run(); });
				  });
	return threads;
}

std::string net::config_path(core::as_view_t) noexcept {
	return config_path().generic_string();
}

std::filesystem::path net::config_path() noexcept {
	const auto config_path = std::filesystem::path{ _NET_DIR } / "config.xml";
	core::verify(std::filesystem::is_regular_file(config_path));
	return config_path;
}

boost::property_tree::ptree const& load_config() {
	static std::once_flag flag;
	static boost::property_tree::ptree config_ptree;
	std::call_once(flag,
				   [] {
					   const auto config_path = net::config_path(core::as_view);
					   boost::property_tree::read_xml(config_path, config_ptree);
				   });
	return config_ptree;
}

std::string net::config_entry(std::string_view entry_name) {
	return load_config().get<std::string>(entry_name.data());
}
