workspace "OpenGL-Cookbook"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release"
	}

	startproject "OpenGL-Cookbook"

outputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include Direnctories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "OpenGL-Cookbook/vendor/GLFW/include"
IncludeDir["Glad"] = "OpenGL-Cookbook/vendor/Glad/include"
IncludeDir["Assimp"] = "OpenGL-Cookbook/vendor/assimp/include"
IncludeDir["ImGui"] = "OpenGL-Cookbook/vendor/imgui"
IncludeDir["GLM"] = "OpenGL-Cookbook/vendor/glm"
IncludeDir["stb_image"] = "OpenGL-Cookbook/vendor/stb_image"

--Include GLFW premake file
include "OpenGL-Cookbook/vendor/GLFW"
--Include Glad premake file
include "OpenGL-Cookbook/vendor/Glad"
--Include Imgui premake file
include "OpenGL-Cookbook/vendor/imgui"
--Include Assimp premake file
include "OpenGL-Cookbook/vendor/assimp"

project "OpenGL-Cookbook"
	location "OpenGL-Cookbook"
	kind "ConsoleApp"
	language "C++"
	staticruntime "off"

	targetdir ("bin/" .. outputDir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputDir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl",
		"%{prj.name}/vendor/stb_image/**.h",
		"%{prj.name}/vendor/stb_image/**.cpp"
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.Assimp}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.GLM}"
	}
	links
	{
		"glfw",
		"glad",
		"opengl32.lib",
		"assimp",
		"Imgui"
	}

	filter "system:windows"
	cppdialect "C++17"
	systemversion "latest"
	
	defines "EN_PLATFORM_WINDOWS"


	filter "configurations:Debug"
		defines "EN_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "EN_RELEASE"
		runtime "Release"
		optimize "on"

