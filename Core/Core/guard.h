#pragma once

namespace core
{
	class time_guard
	{
		std::chrono::steady_clock::time_point time_mark_;
	public:
		time_guard();
		~time_guard();
	};

}
