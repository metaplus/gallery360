#include "stdafx.h"
#include "CppUnitTest.h"
#include <fstream>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/ostream.hpp>
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace test
{
    TEST_CLASS(MediaTest)
    {
    public:
        std::filesystem::path dash_root{ "F:/Tile/test-many" };

        TEST_METHOD(ParserInit)
        {
            Assert::IsTrue(is_directory(dash_root));
            auto file_path = dash_root / "tile1-576p-5000kbps_dashinit.mp4";
            auto file_size = std::filesystem::file_size(file_path);
            std::ifstream file{ file_path };
            Assert::IsTrue(file.is_open());
            //boost::beast::file file;
            //boost::system::error_code errc;
            //file.open(file_path.generic_string().c_str(), boost::beast::file_mode::scan, errc);
            boost::beast::multi_buffer buffer;
            boost::beast::ostream(buffer) << file.rdbuf();
            Assert::AreEqual(boost::asio::buffer_size(buffer.data()), file_size);
            Logger::WriteMessage(fmt::format("file size {}", file_size).c_str());
        }
    };
}