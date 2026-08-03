if(EXISTS "${SOURCE_ROOT}/bin")
    file(COPY "${SOURCE_ROOT}/bin/" DESTINATION "${DESTINATION}")
endif()

if(EXISTS "${SOURCE_ROOT}/lib/usd")
    file(COPY "${SOURCE_ROOT}/lib/usd/"
        DESTINATION "${DESTINATION}/openusd/lib/usd")
endif()

if(EXISTS "${SOURCE_ROOT}/plugin/usd")
    file(COPY "${SOURCE_ROOT}/plugin/usd/"
        DESTINATION "${DESTINATION}/openusd/plugin/usd")
endif()
