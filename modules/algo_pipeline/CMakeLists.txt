add_library(algo_pipeline STATIC pipeline.cpp)
target_include_directories(algo_pipeline PUBLIC ${COMMON_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
)
