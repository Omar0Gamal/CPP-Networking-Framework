workspace "DarkFireNetworking"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "darkfire_tests"

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

    flags { "MultiProcessorCompile" }
    cppdialect "C++20"

    defines {
        "ASIO_STANDALONE"
    }

    filter "system:windows"
        systemversion "latest"
        defines {
            "_WIN32_WINNT=0x0A00",
            "WIN32_LEAN_AND_MEAN",
            "NOMINMAX"
        }
        links {
            "ws2_32"
        }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        optimize "Off"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        symbols "Off"

-- -----------------------------------------------------------------------------
-- DarkFire Core Static Library
-- -----------------------------------------------------------------------------
project "darkfire_net"
    kind "StaticLib"
    language "C++"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs {
        "include",
        "Libs/asio-1.28.1/include"
    }

    files {
        "include/**.hpp",
        "include/**.h"
    }

-- -----------------------------------------------------------------------------
-- Test Suite
-- -----------------------------------------------------------------------------
project "darkfire_tests"
    kind "ConsoleApp"
    language "C++"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs {
        "include",
        "Libs/asio-1.28.1/include"
    }

    files {
        "tests/**.cpp"
    }

    links {
        "darkfire_net"
    }

-- -----------------------------------------------------------------------------
-- Example: EchoBench
-- -----------------------------------------------------------------------------
project "EchoBench"
    kind "ConsoleApp"
    language "C++"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs {
        "include",
        "Libs/asio-1.28.1/include"
    }

    files {
        "examples/EchoBench/**.cpp"
    }

    links {
        "darkfire_net"
    }

-- -----------------------------------------------------------------------------
-- Example: ChatServer & ChatClient
-- -----------------------------------------------------------------------------
project "ChatServer"
    kind "ConsoleApp"
    language "C++"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs {
        "include",
        "Libs/asio-1.28.1/include"
    }

    files {
        "examples/ChatRoom/ChatServer.cpp",
        "examples/ChatRoom/ChatProtocol.hpp"
    }

    links {
        "darkfire_net"
    }

project "ChatClient"
    kind "ConsoleApp"
    language "C++"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs {
        "include",
        "Libs/asio-1.28.1/include"
    }

    files {
        "examples/ChatRoom/ChatClient.cpp",
        "examples/ChatRoom/ChatProtocol.hpp"
    }

    links {
        "darkfire_net"
    }

-- -----------------------------------------------------------------------------
-- Example: SpatialServer & SpatialClient
-- -----------------------------------------------------------------------------
project "SpatialServer"
    kind "ConsoleApp"
    language "C++"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs {
        "include",
        "Libs/asio-1.28.1/include"
    }

    files {
        "examples/SpatialReplication/SpatialServer.cpp",
        "examples/SpatialReplication/SpatialProtocol.hpp"
    }

    links {
        "darkfire_net"
    }

project "SpatialClient"
    kind "ConsoleApp"
    language "C++"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs {
        "include",
        "Libs/asio-1.28.1/include"
    }

    files {
        "examples/SpatialReplication/SpatialClient.cpp",
        "examples/SpatialReplication/SpatialProtocol.hpp"
    }

    links {
        "darkfire_net"
    }
