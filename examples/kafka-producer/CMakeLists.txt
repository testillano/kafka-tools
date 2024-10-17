add_executable (kafka-producer main.cpp)
add_library(ert_logger STATIC IMPORTED)
add_library(cppkafka STATIC IMPORTED)
add_library(rdkafka STATIC IMPORTED)
set_property(TARGET ert_logger PROPERTY IMPORTED_LOCATION /usr/local/lib/ert/libert_logger.a)
set_property(TARGET cppkafka PROPERTY IMPORTED_LOCATION /usr/local/lib/libcppkafka.a)
set_property(TARGET rdkafka PROPERTY IMPORTED_LOCATION /usr/local/lib/librdkafka.a)
target_link_libraries( kafka-producer
PRIVATE
${CMAKE_EXE_LINKER_FLAGS}
        ert_logger

        cppkafka
        rdkafka
        curl

        z
        zstd

        boost_system
        ssl            #Needed by boost_system
        crypto         #Needed by ssl, and need to be appended after ssl
        pthread        #Needed by boost::asio

        ) # target_link_libraries

