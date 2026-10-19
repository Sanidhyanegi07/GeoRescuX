# ==============================================================================
# Android Target Packaging Configuration
# ==============================================================================

function(georescux_configure_android_target TARGET_NAME)
    if(ANDROID)
        message(STATUS "[GeoRescuX Prototype] Configuring Android packaging properties for: ${TARGET_NAME}")
        set_target_properties(${TARGET_NAME} PROPERTIES
            QT_ANDROID_PACKAGE_NAME "com.example.georescux.qtprototype"
            QT_ANDROID_PACKAGE_VERSION_CODE "1"
            QT_ANDROID_PACKAGE_VERSION_NAME "1.0.0"
            QT_ANDROID_MIN_SDK_VERSION "28"
            QT_ANDROID_TARGET_SDK_VERSION "35"
            QT_ANDROID_COMPILE_SDK_VERSION "35"
            QT_ANDROID_SDK_BUILD_TOOLS_REVISION "35.0.0"
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/android"
        )
    endif()
endfunction()
