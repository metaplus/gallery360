#include "stdafx.h"
#include "net.hpp"
#include <boost/property_tree/xml_parser.hpp>
#include <folly/executors/thread_factory/NamedThreadFactory.h>
#include <regex>
#include <tinyxml2.h>
#include <execution>
#include <boost/multi_array.hpp>

namespace net
{
	namespace protocal
	{
		struct dash::parser::impl
		{
			std::vector<video_adaptation_set> video_adaptation_sets;
			audio_adaptation_set audio_adaptation_set;
			std::string title;
			std::chrono::milliseconds min_buffer_time;
			std::chrono::milliseconds max_segment_duration;
			std::chrono::milliseconds presentation_time;
			int16_t width = 0;
			int16_t height = 0;

			struct duration_parse_mismatch : std::runtime_error
			{
				using runtime_error::runtime_error;
				using runtime_error::operator=;
			};

			static std::vector<tinyxml2::XMLElement*> list_next_sibling(const char* name,
																		tinyxml2::XMLElement* first_element) {
				std::vector<tinyxml2::XMLElement*> sibling{ first_element };
				auto* next = first_element->NextSiblingElement();
				while (next != nullptr) {
					sibling.push_back(next);
					next = next->NextSiblingElement(name);
				}
				return sibling;
			}

			static std::array<int, 6> split_spatial_description(std::string_view srd) {
				std::array<int, 6> spatial{};
				srd.remove_prefix(srd.find(',') + 1);
				folly::splitTo<int>(',', srd, spatial.begin(), false);
				return spatial;
			}

			static void initialize(std::vector<video_adaptation_set>& video_adaptation_sets,
								   std::vector<tinyxml2::XMLElement*>::iterator element_begin,
								   std::vector<tinyxml2::XMLElement*>::iterator element_end) {
				video_adaptation_sets.resize(std::distance(element_begin, element_end));
				std::transform(std::execution::par, element_begin, element_end, video_adaptation_sets.begin(),
							   [](tinyxml2::XMLElement* element) {
								   video_adaptation_set adaptation_set;
								   auto represents = list_next_sibling("Representation", element->FirstChildElement("Representation"));
								   adaptation_set.codecs = represents.front()->Attribute("codecs");
								   adaptation_set.mime_type = represents.front()->Attribute("mimeType");
								   adaptation_set.width = boost::numeric_cast<int16_t>(std::stoi(element->Attribute("maxWidth")));
								   adaptation_set.height = boost::numeric_cast<int16_t>(std::stoi(element->Attribute("maxHeight")));
								   auto[x, y, w, h, total_w, total_h] = split_spatial_description(
									   element->FirstChildElement("SupplementalProperty")->Attribute("value"));
								   adaptation_set.x = boost::numeric_cast<int16_t>(x);
								   adaptation_set.y = boost::numeric_cast<int16_t>(y);
								   adaptation_set.represents.resize(represents.size());
								   std::transform(std::execution::par, represents.begin(), represents.end(), adaptation_set.represents.begin(),
												  [](tinyxml2::XMLElement* element) {
													  represent represent;
													  represent.id = boost::numeric_cast<int16_t>(std::stoi(element->Attribute("id")));
													  represent.bandwidth = boost::numeric_cast<int>(std::stoi(element->Attribute("bandwidth")));
													  represent.media = element->FirstChildElement("SegmentTemplate")->Attribute("media");
													  represent.initialization = element->FirstChildElement("SegmentTemplate")->Attribute("initialization");
													  return represent;
												  });
								   return adaptation_set;
							   });
			}

			static void initialize(struct audio_adaptation_set& audio_adaptation_set,
								   tinyxml2::XMLElement* element) {
				element = element->FirstChildElement("Representation");
				audio_adaptation_set.codecs = element->Attribute("codecs");
				audio_adaptation_set.mime_type = element->Attribute("mimeType");
				represent represent;
				represent.id = boost::numeric_cast<int16_t>(std::stoi(element->Attribute("id")));
				represent.bandwidth = boost::numeric_cast<int>(std::stoi(element->Attribute("bandwidth")));
				represent.media = element->FirstChildElement("SegmentTemplate")->Attribute("media");
				represent.initialization = element->FirstChildElement("SegmentTemplate")->Attribute("initialization");
				audio_adaptation_set.represents.push_back(std::move(represent));
				audio_adaptation_set.sample_rate = std::stoi(element->Attribute("audioSamplingRate"));
			}

			static void initialize(int16_t& width, int16_t& height, tinyxml2::XMLElement* element) {
				auto[x, y, w, h, total_w, total_h] = split_spatial_description(
					element->FirstChildElement("SupplementalProperty")->Attribute("value"));
				width = total_w;
				height = total_h;
			}
		};

		dash::parser::parser(std::string_view xml_text) {
			tinyxml2::XMLDocument document;
			core::check[tinyxml2::XMLError::XML_SUCCESS] << document.Parse(xml_text.data());
			auto* xml_root = document.RootElement();
			impl_->presentation_time = parse_duration(xml_root->Attribute("mediaPresentationDuration"));
			impl_->min_buffer_time = parse_duration(xml_root->Attribute("minBufferTime"));
			impl_->max_segment_duration = parse_duration(xml_root->Attribute("maxSegmentDuration"));
			impl_->title = xml_root->FirstChildElement("ProgramInformation")->FirstChildElement("Title")->GetText();
			auto adaptation_sets = impl::list_next_sibling("AdaptationSet",
														   xml_root->FirstChildElement("Period")->FirstChildElement("AdaptationSet"));
			auto audio_set_iter = std::remove_if(
				std::execution::par, adaptation_sets.begin(), adaptation_sets.end(),
				[](tinyxml2::XMLElement* element) {
					std::string_view mime = element->FirstChildElement("Representation")->Attribute("mimeType");
					return mime.find("audio") != std::string_view::npos;
				});
			auto futures = {
				boost::async([&] { impl::initialize(impl_->video_adaptation_sets, adaptation_sets.begin(), audio_set_iter); }),
				boost::async([&] { impl::initialize(impl_->audio_adaptation_set, *audio_set_iter); }),
				boost::async([&] { impl::initialize(impl_->width, impl_->height, adaptation_sets.front()); })
			};
			boost::wait_for_all(futures.begin(), futures.end());
		}

		std::string_view dash::parser::title() const {
			return impl_->title;
		}
		std::pair<int16_t, int16_t> dash::parser::scale() const {
			return std::make_pair(impl_->width, impl_->height);
		}

		dash::video_adaptation_set& dash::parser::video_set(int column, int row) const {
			boost::multi_array_ref<video_adaptation_set, 2>
				matrix_view{ impl_->video_adaptation_sets.data(), boost::extents[impl_->width][impl_->height] };
			using matrix_index = decltype(matrix_view)::index;
			return matrix_view(boost::array<matrix_index, 2>{ column, row });		//!dimension order
		}

		dash::audio_adaptation_set& dash::parser::audio_set() const {
			return impl_->audio_adaptation_set;
		}
	}

	std::chrono::milliseconds protocal::dash::parser::parse_duration(std::string_view duration) {
		static const std::regex duration_pattern{ R"(PT((\d+)H)?((\d+)M)?(\d+\.\d+)S)" };
		std::cmatch matches;
		if (std::regex_match(duration.data(), matches, duration_pattern)) {
			using namespace std::chrono;
			const hours t1{ matches[2].matched ? std::stoi(matches[2].str()) : 0 };
			const minutes t2{ matches[4].matched ? std::stoi(matches[4].str()) : 0 };
			const milliseconds t3{ boost::numeric_cast<int64_t>(
			   matches[5].matched ? 1000 * std::stod(matches[5].str()) : 0) };
			return t1 + t2 + t3;
		}
		throw impl::duration_parse_mismatch{ __PRETTY_FUNCTION__ };
	}

	std::vector<boost::thread*> create_asio_threads(boost::asio::io_context& context, boost::thread_group& thread_group, uint32_t num) {
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

	std::vector<std::thread> create_asio_named_threads(boost::asio::io_context& context, uint32_t num) {
		std::vector<std::thread> threads(num);
		auto thread_factory = std::make_unique<folly::NamedThreadFactory>("NetAsio");
		std::generate(threads.begin(), threads.end(),
					  [&thread_factory, &context] {
						  return thread_factory->newThread([&context] { context.run(); });
					  });
		return threads;
	}

	std::filesystem::path config_path() noexcept {
		const auto config_path = std::filesystem::path{ _NET_DIR } / "config.xml";
		core::verify(std::filesystem::is_regular_file(config_path));
		return config_path;
	}

	boost::property_tree::ptree const& load_config() {
		static std::once_flag flag;
		static boost::property_tree::ptree config_ptree;
		std::call_once(flag,
					   [] {
						   const auto config_path = net::config_path().generic_string();
						   boost::property_tree::read_xml(config_path, config_ptree);
					   });
		return config_ptree;
	}

	std::string config_entry(std::string_view entry_name) {
		return load_config().get<std::string>(entry_name.data());
	}
}