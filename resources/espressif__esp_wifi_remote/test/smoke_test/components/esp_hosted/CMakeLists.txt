set(IDF_VER_DIR "idf_v${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

idf_component_register(SRCS "${IDF_VER_DIR}/esp_hosted_mock.c"
                    INCLUDE_DIRS "${IDF_VER_DIR}/include" "include"
                    REQUIRES esp_wifi esp_wifi_remote)
