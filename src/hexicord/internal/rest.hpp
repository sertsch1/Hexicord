// Hexicord - Discord API library for C++11 using boost libraries.
// Copyright © 2017 Maks Mazurov (fox.cpp) <foxcpp@yandex.ru>
// 
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
// OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef HEXICORD_REST_HPP
#define HEXICORD_REST_HPP 

#include <string>         // std::string
#include <utility>        // std::move, std::forward
#include <vector>         // std::vector
#include <unordered_map>  // std::unordered_map
#include <functional>     // std::function
#include <mutex>          // std::mutex, std::lock_guard
#include <locale>         // std::tolower, std::locale

/**
 *  \file rest.hpp
 *  \internal
 *
 *  Defines implementation-independent thread-safe interface for HTTP requests.
 */

namespace Hexicord { namespace REST {
    namespace _detail {
        inline std::string stringToLower(const std::string& input) {
            std::string result;
            result.reserve(input.size());

            for (char ch : input) {
                result.push_back(std::tolower(ch, std::locale("C")));
            }
            return result;
        }

        struct CaseInsensibleStringEqual {
            bool operator()(const std::string& lhs, const std::string& rhs) const {
                return stringToLower(lhs) == stringToLower(rhs);
            }
        };
    }

    /// \internal
    /// Hash-map with case-insensible string keys.
    using HeadersMap = std::unordered_map<std::string, std::string, std::hash<std::string>, _detail::CaseInsensibleStringEqual>;

    /// \internal
    struct HTTPResponse {
        unsigned statusCode;
        
        HeadersMap headers;
        std::vector<uint8_t> body;
    };

    /// \internal
    struct HTTPRequest {
        std::string method;
        std::string path;
        
        unsigned version;
        std::vector<uint8_t> body;
        HeadersMap headers;
    };

    inline HTTPRequest Get(const std::string& path, const HeadersMap& additionalHeaders = {}) {
        return { "GET", path, 11, {}, additionalHeaders }; 
    }

    inline HTTPRequest Post(const std::string& path, const std::vector<uint8_t>& body = {}, const HeadersMap& additionalHeaders = {}) {
        return { "POST", path, 11, body, additionalHeaders }; 
    }

    inline HTTPRequest Put(const std::string& path, const std::vector<uint8_t>& body = {}, const HeadersMap& additionalHeaders = {}) {
        return { "PUT", path, 11, body, additionalHeaders }; 
    }

    inline HTTPRequest Patch(const std::string& path, const std::vector<uint8_t>& body = {}, const HeadersMap& additionalHeaders = {}) {
        return { "PATCH", path, 11, body, additionalHeaders };        
    }

    inline HTTPRequest Delete(const std::string& path, const std::vector<uint8_t>& body = {}, const HeadersMap& additionalHeaders = {}) {
        return { "DELETE", path, 11, body, additionalHeaders };
    }

    /**
     *  \internal
     *
     *  Main class. Represents single transport-layer connection used for HTTP requests.
     *
     *  \tparam StreamProvider Implementation of underlaying connection.
     *      Should be constructable with servername and arbitrary other arguments.
     *      Should be movable.
     *      Should define following types:
     *          StreamProvider::ErrorType    type to be used for error reporting in async callback.
     *          StreamProvider::Exception    type thrown on any error.
     *          StreamProvider::RequestType  raw request type.
     *      Should define following static methods:
     *          StreamProvider::setMethod(RequestType&, const std::string&)
     *              Set method for request.
     *          StreamProvider::setPath(RequestType&, const std::string&)
     *              Set request path for request.
     *          StreamProvider::setVersion(RequestType&, const std::string&)
     *              Set HTTP version for request.
     *          StreamProvider::setHeader(RequestType&, const std::string&, const std::string&)
     *              Set header for request.
     *          StreamProvider::setBody(RequestType&, const std::vector<uint8_t>&)
     *              Set body for request.
     *      Should define following non-static methods:
     *          StreamProvider::request(RequestType)
     *              Perform sync request. Should return HTTPResponse object.
     *          StreamProvider::asyncRequest(RequestType&&, AsyncRequestCallback)
     *              Perform async request.
     *          StreamProvider::openConnection()
     *              (Re)Open connection and get ready to accept requests.
     *          StreamProvider::closeConnection()
     *              Close connection.
     *          StreamProvider::isOpen() const
     *              Should return true whatever socket is open. 
     */
    template<typename StreamProvider>
    class GenericHTTPConnection {
    public:
        /**
         *  \internal
         *
         *  Async request callback. Not used now because async requests is not implemented.
         */
        using AsyncRequestCallback = std::function<void(HTTPResponse, typename StreamProvider::ErrorType)>;
        
        /** 
         *  \internal
         *
         *  Prepare connection context.
         *
         *  \param servername   domain name or network address of target server.
         *  \param port         connection port. defaults to StreamProvider::DefaultPort
         *  \param streamArgs   passed to StreamProvider constructor
         */
        template<typename... StreamArgs>
        GenericHTTPConnection(const std::string& servername, StreamArgs&&... streamArgs) 
            : streamProvider(servername, std::forward<StreamArgs>(streamArgs)...)
            , servername(servername) {
        }

        ~GenericHTTPConnection() {
            if (streamProvider.isOpen()) streamProvider.closeConnection();
        }

        GenericHTTPConnection(const GenericHTTPConnection&) = delete;
        GenericHTTPConnection(GenericHTTPConnection&&) = default;
        GenericHTTPConnection& operator=(const GenericHTTPConnection&) = delete;
        GenericHTTPConnection& operator=(GenericHTTPConnection&&) = default;

        /**
         *  \internal
         *
         *  Open connection and prepare for handling requests.
         *
         *  This method is thread-safe.
         */
        void open() {
            std::lock_guard<std::mutex> lock(connectionMutex);
            streamProvider.openConnection();
        }

        /**
         *  \internal
         *
         *  Close connection. Can be reopened using \ref open().
         *
         *  This method is thread-safe.
         */
        void close() {
            std::lock_guard<std::mutex> lock(connectionMutex);
            streamProvider.closeConnection();
        }

        /** 
         *  \internal
         *
         *  Whatever socket is open.
         *
         *  \warning Socket is one side of connection, it may be closed on other
         *           side and this can't be detected without attempt to send request.
         */
        bool isOpen() {
            return streamProvider.isOpen();
        }
   
        /**
         *  \internal
         *
         *  Perform HTTP request and read response.
         *
         *  This method is thread-safe.
         */
        HTTPResponse request(const HTTPRequest& request) {
            std::lock_guard<std::mutex> lock(connectionMutex);
            return streamProvider.request(this->prepareRequest(request));
        }
        
        //void asyncRequest(const HTTPRequest& request, AsyncRequestCallback callback) {
        //    streamProvider.asyncRequest(std::move(this->prepareRequest(request)), callback);
        //}

        HeadersMap connectionHeaders;
        StreamProvider streamProvider;
        const std::string servername;
    private:
        typename StreamProvider::RequestType prepareRequest(const HTTPRequest& request) {
            typename StreamProvider::RequestType streamRequest;

            StreamProvider::setMethod(streamRequest, request.method);
            StreamProvider::setPath(streamRequest, request.path);
            StreamProvider::setVersion(streamRequest, request.version);

            // Set default headers. 
            StreamProvider::setHeader(streamRequest, "User-Agent", "Generic HTTP 1.1 Client");
            StreamProvider::setHeader(streamRequest, "Connection", "keep-alive");
            StreamProvider::setHeader(streamRequest, "Accept",     "*/*");
            StreamProvider::setHeader(streamRequest, "Host",       servername);
            if (!request.body.empty()) {
                StreamProvider::setHeader(streamRequest, "Content-Length", std::to_string(request.body.size()));
                StreamProvider::setHeader(streamRequest, "Content-Type",   "application/octet-stream");
            }

            // Set per-connection headers.
            for (const auto& header : connectionHeaders) {
                StreamProvider::setHeader(streamRequest, header.first, header.second);
            }

            // Set per-request
            for (const auto& header : request.headers) {
                StreamProvider::setHeader(streamRequest, header.first, header.second);
            }

            if (!request.body.empty()) {
                StreamProvider::setBody(streamRequest, request.body);
            }

            return streamRequest;
        }

        std::mutex connectionMutex;
    };

}} // namespace Hexicord::REST

#endif // HEXICORD_REST_HPP 
