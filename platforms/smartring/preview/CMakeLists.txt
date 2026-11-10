add_executable(smartring_preview main.cpp)
target_link_libraries(smartring_preview PRIVATE moto_nav_ui)
target_compile_options(smartring_preview PRIVATE "-sUSE_SDL=2" -O2)
target_link_options(smartring_preview PRIVATE
    "-sUSE_SDL=2" "-sALLOW_MEMORY_GROWTH=1" "-sINITIAL_MEMORY=33554432"
    "-sEXPORTED_RUNTIME_METHODS=['ccall']"
    "-sEXPORTED_FUNCTIONS=['_main','_preview_page','_preview_track','_preview_ready','_preview_connecting']")
set_target_properties(smartring_preview PROPERTIES OUTPUT_NAME "smartring-preview" SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/runtime")
