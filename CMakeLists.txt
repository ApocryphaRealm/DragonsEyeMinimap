cmake_minimum_required(VERSION 3.21)

########################################################################################################################
## Define project
########################################################################################################################

project(
	DragonsEyeMinimap
	VERSION 0.1.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY
)

set(headers
	include/IUI/API.h
	include/IUI/GFxArray.h
	include/IUI/GFxDisplayObject.h
	include/IUI/GFxLoggers.h
	include/IUI/GFxObject.h
	include/RE/B/BSCullingProcess.h
	include/RE/B/BSShaderAccumulator.h
	include/RE/B/BSScaleformImageLoader.h
	include/RE/G/GFxImageLoader.h
	include/RE/G/GFxValue.h
	include/RE/H/HUDData.h
	include/RE/I/ImageData.h
	include/RE/I/ImageSpaceShaderParam.h
	include/RE/L/LoadedAreaBound.h
	include/RE/L/LocalMapCamera.h
	include/RE/L/LocalMapMenu.h
	include/RE/R/RendererShadowState.h
	include/RE/R/RenderPassCache.h
	include/RE/R/RenderTargetManager.h
	include/RE/S/ShadowSceneNode.h
	include/RE/T/TES.h
	include/RE/RTTI.h
	include/utils/Trampoline.h
	include/utils/INISettingCollection.h
	include/utils/Logger.h
	include/utils/Setting.h
	include/Hooks.h
	include/Minimap.h
	include/pch.h
	include/Settings.h
)

set(sources
	source/RE/G/GFxValue.cpp
	source/RE/L/LocalMapCamera.cpp
	source/utils/INISettingCollection.cpp
	source/utils/Trampoline.cpp
	source/Hooks.cpp
	source/main.cpp
	source/MessageListeners.cpp
	source/Minimap.cpp
	source/pch.cpp
	source/Settings.cpp
)

source_group(
	TREE
		${CMAKE_CURRENT_SOURCE_DIR}
	FILES
		${headers}
		${sources}
)

########################################################################################################################
## Configure target DLL
########################################################################################################################

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(spdlog REQUIRED CONFIG)

add_commonlibsse_plugin(${PROJECT_NAME} SOURCES ${headers} ${sources})
add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS "${PROJECT_NAME}")

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		CommonLibSSE::CommonLibSSE
		spdlog::spdlog_header_only
)

if (MSVC)
	target_link_options(
		${PROJECT_NAME}
		PRIVATE
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF>"
	)
endif()

target_precompile_headers(
	${PROJECT_NAME}
	PRIVATE
		include/PCH.h
)

########################################################################################################################
## Automatic plugin deployment
########################################################################################################################

set(SKYRIM_SE_DIR "C:/Program Files (x86)/Steam/steamapps/common/Skyrim Special Edition")
set(SKYRIM_VR_DIR "C:/Program Files (x86)/Steam/steamapps/common/Skyrim VR")
set(SKYRIM_AE_1_6_353_DIR "C:/Program Files (x86)/Steam/steamapps/common/Skyrim Anniversary Edition 1.6.353")
set(SKYRIM_AE_1_6_640_DIR "C:/Program Files (x86)/Steam/steamapps/common/Skyrim Anniversary Edition 1.6.640")

if ("$ENV{RUNTIME_DISABLE_FLAGS}" STREQUAL "")
	set(COPY_SE_AE YES)
	set(COPY_VR YES)
elseif ("$ENV{RUNTIME_DISABLE_FLAGS}" STREQUAL "-UENABLE_SKYRIM_VR")
	set(COPY_SE_AE YES)
	set(COPY_VR NO)
elseif ("$ENV{RUNTIME_DISABLE_FLAGS}" STREQUAL "-UENABLE_SKYRIM_SE -UENABLE_SKYRIM_AE")
	set(COPY_SE_AE NO)
	set(COPY_VR YES)
endif()

if (COPY_SE_AE)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}>
		"${SKYRIM_SE_DIR}/Data/SKSE/Plugins/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}>
		"${SKYRIM_AE_1_6_353_DIR}/Data/SKSE/Plugins/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}>
		"${SKYRIM_AE_1_6_640_DIR}/Data/SKSE/Plugins/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
	)
endif()
if (COPY_VR)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}>
		"${SKYRIM_VR_DIR}/Data/SKSE/Plugins/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
	)
endif()
