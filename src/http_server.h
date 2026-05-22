#pragma once

#include <microhttpd.h>
#include <string>
#include <memory>

class IndexLoader;
class Vectorizer;

class HttpServer {
public:
    HttpServer(int port, IndexLoader* index_loader, Vectorizer* vectorizer);
    ~HttpServer();
    
    bool start();
    void stop();

private:
    static MHD_Result request_handler(void* cls, struct MHD_Connection* connection,
                                      const char* url, const char* method,
                                      const char* version, const char* upload_data,
                                      size_t* upload_data_size, void** con_cls);
    
    MHD_Result handle_request(struct MHD_Connection* connection,
                              const char* url, const char* method,
                              const char* upload_data, size_t* upload_data_size,
                              void** con_cls);
    
    MHD_Result handle_ready(struct MHD_Connection* connection);
    MHD_Result handle_fraud_score(struct MHD_Connection* connection, const char* data);
    
    static MHD_Result send_json_response(struct MHD_Connection* connection,
                                          unsigned int status_code,
                                          const char* json_str);

    int port_;
    struct MHD_Daemon* daemon_{nullptr};
    IndexLoader* index_loader_;
    Vectorizer* vectorizer_;
};
