#pragma once
//#include "Gallery/interface.h"
#include <cereal/cereal.hpp>
#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp> 
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/chrono.hpp>
#include <cereal/types/common.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/map.hpp>

#pragma warning(push)
#pragma warning(disable:4267 4834)

#include <cereal/types/variant.hpp>
#include <boost/asio.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

#pragma warning(pop)

#include "gallery/openvr.h"

namespace interprocess = boost::interprocess;
namespace asio = boost::asio;

#if defined UNITYAPI || defined DLLAPI || defined EXTERN
#error "macro naming collision"
#pragma push_marcro("UNITYAPI")
#pragma push_marcro("EXTERN")
#endif
#ifdef GALLERY_EXPORTS
#define DLLAPI __declspec(dllexport)
#define UNITYAPI  __declspec(dllexport) __stdcall
#else
#define DLLAPI __declspec(dllimport)
#define UNITYAPI  __declspec(dllimport) __stdcall
#endif  // GALLERY_EXPORTS

#ifdef __cplusplus
#define EXTERN extern "C"
#else 
#define EXTERN extern 
#endif