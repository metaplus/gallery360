#pragma once

namespace core
{
    inline bool exist_process(std::string_view name)
    {
        namespace process=boost::process;
        constexpr auto extension=".exe"sv;
        process::ipstream proclist;
        auto cmd=fmt::format("tasklist /FI \"IMAGENAME eq {}\" /FO table",
            name.rfind(extension)!=name.size()-extension.size()?
                fmt::format("{}.exe",name):name);
        std::string line;
        process::spawn(cmd,process::std_out>proclist);
        //std::getline(proclist,line,'\n');
        proclist>>std::ws;
        std::getline(proclist,line,'\n');
        return std::getline(proclist,line,'\r').good();
    }    
}