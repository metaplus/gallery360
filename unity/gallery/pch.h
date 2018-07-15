#pragma once
//#include "Gallery/interface.h"     

//#include <cereal/cereal.hpp>
//#include <cereal/access.hpp>
//#include <cereal/archives/binary.hpp> 
//#include <cereal/archives/json.hpp>
//#include <cereal/types/memory.hpp>
//#include <cereal/types/chrono.hpp>
//#include <cereal/types/common.hpp>
//#include <cereal/types/tuple.hpp>
//#include <cereal/types/string.hpp>
//#include <cereal/types/utility.hpp>
//#include <cereal/types/map.hpp>

#pragma warning(push)
#pragma warning(disable:4267 4834)

//#include <cereal/types/variant.hpp>
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_USE_FUTURE_HPP
#include <boost/asio.hpp>
//#include <boost/interprocess/ipc/message_queue.hpp>

#pragma warning(pop)

//namespace interprocess = boost::interprocess;

#if defined UNITYAPI || defined DLLAPI
#error "macro naming collision"
#pragma push_marcro("UNITYAPI")
#pragma push_marcro("DLLAPI")
#endif

#ifdef GALLERY_EXPORTS
#define DLLAPI __declspec(dllexport)
#define UNITYAPI  __declspec(dllexport) __stdcall
#else
#define DLLAPI __declspec(dllimport)
#define UNITYAPI  __declspec(dllimport) __stdcall
#endif  // GALLERY_EXPORTS

#include <d3d11.h>
#include "gallery/openvr.h"
#include "unity/detail/PlatformBase.h"
#include "unity/detail/IUnityInterface.h"
#include "unity/detail/IUnityGraphics.h"
#include "unity/detail/IUnityGraphicsD3D11.h"
//#include "unity/gallery/interprocess.h"
#include "unity/gallery/graphic.h"
#include "unity/gallery/dll.h"
#include "unity/gallery/export.h"
