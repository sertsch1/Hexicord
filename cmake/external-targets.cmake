macro(define_boost_target component_name)
    add_library(__Boost_${component_name} INTERFACE)
    target_include_directories(__Boost_${component_name} INTERFACE ${Boost_INCLUDE_DIRS})
    target_link_libraries(__Boost_${component_name} INTERFACE ${Boost_${component_name}_LIBRARY})

    add_library(Boost::${component_name} ALIAS __Boost_${component_name})
endmacro()

macro(define_openssl_crypto_target)
    add_library(__OpenSSL_Crypto INTERFACE)
    target_include_directories(__OpenSSL_Crypto INTERFACE ${OPENSSL_INCLUDE_DIRS})
    if(${CMAKE_VERSION} VERSION_LESS 3.2)
        # FindOpenSSL prior to 3.2 doesn't provides separate variable libcrypto
        target_link_libraries(__OpenSSL_Crypto INTERFACE ${OPENSSL_LIBRARIES})
    else()
        target_link_libraries(__OpenSSL_Crypto INTERFACE ${OPENSSL_CRYPTO_LIBRARY})
    endif()

    add_library(OpenSSL::Crypto ALIAS __OpenSSL_Crypto)
endmacro()


macro(define_openssl_ssl_target)
    add_library(__OpenSSL_SSL INTERFACE)
    target_include_directories(__OpenSSL_SSL INTERFACE ${OPENSSL_INCLUDE_DIRS})
    if(${CMAKE_VERSION} VERSION_LESS 3.2)
        # FindOpenSSL prior to 3.2 doesn't provides separate variable libssl
        target_link_libraries(__OpenSSL_SSL INTERFACE ${OPENSSL_LIBRARIES})
    else()
        target_link_libraries(__OpenSSL_SSL INTERFACE ${OPENSSL_SSL_LIBRARY})
    endif()

    add_library(OpenSSL::SSL ALIAS __OpenSSL_SSL)
endmacro()

macro(define_threads_target)
    add_library(Threads::Threads INTERFACE IMPORTED)

    if(THREADS_HAVE_PTHREAD_ARG)
      set_property(TARGET Threads::Threads PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
    endif()

    if(CMAKE_THREAD_LIBS_INIT)
      set_property(TARGET Threads::Threads PROPERTY INTERFACE_LINK_LIBRARIES "${CMAKE_THREAD_LIBS_INIT}")
    endif()
endmacro()

macro(define_zlib_target)
    add_library(__Zlib INTERFACE)
    target_include_directories(__Zlib SYSTEM INTERFACE ${ZLIB_INCLUDE_DIRS})
    target_link_libraries(__Zlib INTERFACE ${ZLIB_LIBRARIES})

    add_library(ZLIB::ZLIB ALIAS __Zlib)
endmacro()
