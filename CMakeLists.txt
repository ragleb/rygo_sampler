set(target rygo_sampler)

# Ensure OBJCXX is enabled for the AU wrapper .mm files
if(SMTG_MAC)
    enable_language(OBJCXX)
endif()

smtg_add_vst3plugin(${target}
    source/rygo_sampler_entry.cpp
    source/rygo_sampler_processor.cpp
    source/rygo_sampler_processor.h
    source/rygo_sampler_controller.cpp
    source/rygo_sampler_controller.h
    source/rygo_sampler_ids.h
    source/version.h
)

smtg_target_setup_as_vst3_example(${target})

target_link_libraries(${target}
    PRIVATE
        sdk
        vstgui_support
)

smtg_target_add_plugin_resources(${target}
    RESOURCES
        resource/rygo_editor.uidesc
        resource/background.png
        resource/background.jpg
        resource/knob_strip.png
        resource/gate_oneshot_1x.png
)

if(SMTG_MAC)
    if(XCODE AND SMTG_ENABLE_AUV2_BUILDS)
        include(SMTG_AddVST3AuV2)
        smtg_target_add_auv2(rygo_sampler_au
            BUNDLE_NAME rygo_sampler
            BUNDLE_IDENTIFIER com.zhri.rygosampler.audiounit.v101
            INFO_PLIST_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/resource/au-info.plist
            VST3_PLUGIN_TARGET ${target}
        )

        add_dependencies(rygo_sampler_au ${target})

        set(rygo_component_resources "$<TARGET_BUNDLE_CONTENT_DIR:rygo_sampler_au>/Resources")
        set(rygo_vst3_bundle "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/rygo_sampler.vst3")

        add_custom_command(TARGET rygo_sampler_au POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${rygo_component_resources}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${rygo_component_resources}/plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${rygo_vst3_bundle}" "${rygo_component_resources}/plugin.vst3"
            COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:rygo_sampler_au>"
        )
    elseif(SMTG_ENABLE_AUV2_BUILDS)
        set(rygo_au_sources
            ${public_sdk_SOURCE_DIR}/source/vst/auwrapper/aucocoaview.mm
            ${public_sdk_SOURCE_DIR}/source/vst/auwrapper/aucocoaview.h
            ${public_sdk_SOURCE_DIR}/source/vst/auwrapper/auwrapper.mm
            ${public_sdk_SOURCE_DIR}/source/vst/auwrapper/auwrapper.h
            ${public_sdk_SOURCE_DIR}/source/vst/auwrapper/NSDataIBStream.mm
            ${public_sdk_SOURCE_DIR}/source/vst/auwrapper/NSDataIBStream.h
        )

        string(RANDOM LENGTH 16 ALPHABET "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" RYGO_COCOA_ID)
        set(rygo_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/au")
        file(MAKE_DIRECTORY "${rygo_generated_dir}")
        file(WRITE "${rygo_generated_dir}/aucocoaclassprefix.h"
            "#define SMTG_AUCocoaUIBase_CLASS_NAME SMTG_AUCocoaUIBase_${RYGO_COCOA_ID}\n")

        add_library(rygo_sampler_au MODULE
            ${rygo_au_sources}
        )

        target_compile_features(rygo_sampler_au
            PUBLIC
                cxx_std_20
        )

        target_compile_definitions(rygo_sampler_au
            PRIVATE
                SMTG_AUCocoaUIBase_CLASS_NAME=SMTG_AUCocoaUIBase_${RYGO_COCOA_ID}
                CA_USE_AUDIO_PLUGIN_ONLY=0
                SMTG_AUWRAPPER_USES_AUSDK=1
        )

        target_include_directories(rygo_sampler_au
            PRIVATE
                "${rygo_generated_dir}"
                "${public_sdk_SOURCE_DIR}/source/vst/auwrapper"
        )

        target_link_libraries(rygo_sampler_au
            PRIVATE
                sdk_hosting
                "-framework AudioUnit"
                "-framework CoreMIDI"
                "-framework AudioToolbox"
                "-framework CoreFoundation"
                "-framework Carbon"
                "-framework Cocoa"
                "-framework CoreAudio"
        )

        if(NOT "${SMTG_COREAUDIO_SDK_PATH}" STREQUAL "")
            if(EXISTS "${SMTG_COREAUDIO_SDK_PATH}/AudioUnits")
                target_sources(rygo_sampler_au
                    PRIVATE
                        ${public_sdk_SOURCE_DIR}/source/vst/auwrapper/ausdk.mm
                )
                target_include_directories(rygo_sampler_au
                    PRIVATE
                        "${SMTG_COREAUDIO_SDK_PATH}"
                        "${SMTG_COREAUDIO_SDK_PATH}/AudioUnits"
                        "${SMTG_COREAUDIO_SDK_PATH}/AudioUnits/AUPublic"
                        "${SMTG_COREAUDIO_SDK_PATH}/AudioUnits/AUPublic/OtherBases"
                        "${SMTG_COREAUDIO_SDK_PATH}/AudioUnits/AUPublic/Utility"
                )
            elseif(EXISTS "${SMTG_COREAUDIO_SDK_PATH}/src/AudioUnitSDK")
                file(GLOB RYGO_AU_SDK_SOURCES "${SMTG_COREAUDIO_SDK_PATH}/src/AudioUnitSDK/*.cpp")
                target_sources(rygo_sampler_au
                    PRIVATE
                        ${RYGO_AU_SDK_SOURCES}
                )
                target_include_directories(rygo_sampler_au
                    PRIVATE
                        "${SMTG_COREAUDIO_SDK_PATH}/include"
                )
            endif()
        endif()

        set(rygo_component_output_dir "${CMAKE_BINARY_DIR}/AudioUnit/$<CONFIG>")
        set(rygo_component_resources "$<TARGET_BUNDLE_CONTENT_DIR:rygo_sampler_au>/Resources")
        set(rygo_vst3_bundle "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/rygo_sampler.vst3")

        set_target_properties(rygo_sampler_au
            PROPERTIES
                PREFIX ""
                OUTPUT_NAME rygo_sampler
                BUNDLE TRUE
                MACOSX_BUNDLE TRUE
                BUNDLE_EXTENSION component
                MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resource/au-info.plist
                LIBRARY_OUTPUT_DIRECTORY "${rygo_component_output_dir}"
        )

        add_dependencies(rygo_sampler_au ${target})

        add_custom_command(TARGET rygo_sampler_au POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${rygo_component_resources}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${rygo_component_resources}/plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${rygo_vst3_bundle}" "${rygo_component_resources}/plugin.vst3"
            COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:rygo_sampler_au>"
        )
    endif()

    if(SMTG_ENABLE_AUV2_BUILDS)
        add_custom_target(install_rygo_sampler_au_user
            # Always re-embed the latest VST3 so changing only the plugin sources
            # (processor/controller) doesn't require touching auwrapper.mm to trigger a copy.
            COMMAND ${CMAKE_COMMAND} -E make_directory "${rygo_component_resources}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${rygo_component_resources}/plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${rygo_vst3_bundle}" "${rygo_component_resources}/plugin.vst3"
            COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:rygo_sampler_au>"
            # Install
            COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/Library/Audio/Plug-Ins/Components"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "$ENV{HOME}/Library/Audio/Plug-Ins/Components/rygo_sampler.component"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:rygo_sampler_au>" "$ENV{HOME}/Library/Audio/Plug-Ins/Components/rygo_sampler.component"
            DEPENDS rygo_sampler_au
        )
    endif()
endif()
