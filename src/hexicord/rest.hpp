#ifndef HEXICORD_REST_HPP
#define HEXICORD_REST_HPP 

#include <string>         // std::string
#include <utility>        // std::move, std::forward
#include <vector>         // std::vector
#include <unordered_map>  // std::unordered_map
#include <functional>     // std::function

namespace Hexicord { namespace REST {
    using HeadersMap = std::unordered_map<std::string, std::string>;

    struct HTTPResponse {
        unsigned statusCode;
        
        HeadersMap headers;
        std::vector<uint8_t> body;
    };

    struct HTTPRequest {
        std::string method;
        std::string path;
        
        unsigned version;
        std::vector<uint8_t> body;
        HeadersMap headers;
    };

    /// Basic requests factories.
    /// @{ 

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

    /// @}

    /**
     *  Main class. Represents single transport-layer connection used for HTTP requests.
     *
     *  \tparam StreamProvider Implementation of underlaying connection.
     *      Should be constructable with servername and arbitrary other arguments.
     *      Should be movable.
     *      Should define following types:
     *          StreamProvider::ErrorType    type to be used for error reporting in async callback.
     *          StreamProvider::Exception    type thrown on any error.
     *          StreamProvider::RequestType  raw request type.
     *      Should define following constants:
     *          StreamProvider::DefaultPort  unsigned short. Default port used for connection.
     *      Should define following static methods:
     *          StreamProvider::setMethod(RequestType&, const std::string&) const
     *              Set method for request.
     *          StreamProvider::setPath(RequestType&, const std::string&) const 
     *              Set request path for request.
     *          StreamProvider::setVersion(RequestType&, const std::string&) const
     *              Set HTTP version for request.
     *          StreamProvider::setHeader(RequestType&, const std::string&, const std::string&) const 
     *              Set header for request.
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
     *              Should return true whatever connection is open and ready.
     */
    template<typename StreamProvider>
    class GenericHTTPConnection {
    public:
        using AsyncRequestCallback = std::function<void(HTTPResponse, typename StreamProvider::ErrorType)>;
        
        /*  
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

        /**
         *  Open connection and prepare for handling requests.
         */
        void open() {
            streamProvider.openConnection();
        }

        /**
         *  Close connection. Can be reopened using \ref open().
         */
        void close() {
            streamProvider.closeConnection();
        }

        bool isOpen() {
            return streamProvider.isOpen();
        }
    
        HTTPResponse request(const HTTPRequest& request) {
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

            return streamRequest;
        }
    };

}} // namespace Hexicord::REST

#endif // HEXICORD_REST_HPP 
