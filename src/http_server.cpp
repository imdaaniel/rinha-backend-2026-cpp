#include "http_server.h"
#include "index_loader.h"
#include "vectorizer.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <iostream>
#include <thread>

using json = nlohmann::json;

HttpServer::HttpServer(int port, IndexLoader* index_loader, Vectorizer* vectorizer)
    : port_(port), index_loader_(index_loader), vectorizer_(vectorizer) {}

HttpServer::~HttpServer() {
    stop();
}

struct ConnectionData {
    std::string post_data;
};

void request_completed(void* cls, struct MHD_Connection* connection,
                       void** con_cls, enum MHD_RequestTerminationCode toe) {
    (void)cls;
    (void)connection;
    (void)toe;
    if (*con_cls) {
        delete static_cast<ConnectionData*>(*con_cls);
        *con_cls = nullptr;
    }
}

bool HttpServer::start() {
    // Create HTTP daemon
    daemon_ = MHD_start_daemon(
        MHD_USE_THREAD_PER_CONNECTION,
        port_,
        nullptr, nullptr,
        &request_handler, this,
        MHD_OPTION_CONNECTION_LIMIT, 1000,
        MHD_OPTION_CONNECTION_TIMEOUT, 2,
        MHD_OPTION_NOTIFY_COMPLETED, &request_completed, nullptr,
        MHD_OPTION_END
    );
    
    if (!daemon_) {
        std::cerr << "Failed to start HTTP server on port " << port_ << std::endl;
        return false;
    }
    
    std::cout << "HTTP server started on port " << port_ << std::endl;
    return true;
}

void HttpServer::stop() {
    if (daemon_) {
        MHD_stop_daemon(daemon_);
        daemon_ = nullptr;
    }
}

MHD_Result HttpServer::request_handler(void* cls, struct MHD_Connection* connection,
                                         const char* url, const char* method,
                                         const char* version, const char* upload_data,
                                         size_t* upload_data_size, void** con_cls) {
    (void)version;
    
    if (*con_cls == nullptr) {
        *con_cls = new ConnectionData();
        return MHD_YES;
    }
    
    HttpServer* server = static_cast<HttpServer*>(cls);
    return server->handle_request(connection, url, method, upload_data, upload_data_size, con_cls);
}

MHD_Result HttpServer::handle_request(struct MHD_Connection* connection,
                                        const char* url, const char* method,
                                        const char* upload_data, size_t* upload_data_size,
                                        void** con_cls) {
    ConnectionData* cd = static_cast<ConnectionData*>(*con_cls);
    
    // Handle POST data
    if (strcmp(method, "POST") == 0 && *upload_data_size != 0) {
        cd->post_data.append(upload_data, *upload_data_size);
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    if (strcmp(url, "/ready") == 0) {
        return handle_ready(connection);
    } else if (strcmp(url, "/fraud-score") == 0) {
        return handle_fraud_score(connection, cd->post_data.c_str());
    }
    
    // 404
    const char* response = "Not Found";
    struct MHD_Response* resp = MHD_create_response_from_buffer(
        strlen(response), (void*)response, MHD_RESPMEM_PERSISTENT);
    MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, resp);
    MHD_destroy_response(resp);
    return ret;
}

MHD_Result HttpServer::handle_ready(struct MHD_Connection* connection) {
    const char* response;
    unsigned int status;
    
    if (index_loader_->is_ready()) {
        response = "ok";
        status = MHD_HTTP_OK;
    } else {
        response = "not ready";
        status = MHD_HTTP_SERVICE_UNAVAILABLE;
    }
    
    struct MHD_Response* resp = MHD_create_response_from_buffer(
        strlen(response), (void*)response, MHD_RESPMEM_PERSISTENT);
    MHD_add_response_header(resp, "Content-Type", "text/plain");
    MHD_Result ret = MHD_queue_response(connection, status, resp);
    MHD_destroy_response(resp);
    return ret;
}

MHD_Result HttpServer::handle_fraud_score(struct MHD_Connection* connection, const char* data) {
    try {
        // Parse JSON
        json j = json::parse(data);
        
        // Build payload
        Payload p;
        p.id = j["id"];
        p.transaction.amount = j["transaction"]["amount"];
        p.transaction.installments = j["transaction"]["installments"];
        p.transaction.requested_at = j["transaction"]["requested_at"];
        p.customer.avg_amount = j["customer"]["avg_amount"];
        p.customer.tx_count_24h = j["customer"]["tx_count_24h"];
        p.customer.known_merchants = j["customer"]["known_merchants"].get<std::vector<std::string>>();
        p.merchant.id = j["merchant"]["id"];
        p.merchant.mcc = j["merchant"]["mcc"];
        p.merchant.avg_amount = j["merchant"]["avg_amount"];
        p.terminal.is_online = j["terminal"]["is_online"];
        p.terminal.card_present = j["terminal"]["card_present"];
        p.terminal.km_from_home = j["terminal"]["km_from_home"];
        
        if (j.contains("last_transaction") && !j["last_transaction"].is_null()) {
            LastTransaction lt;
            lt.timestamp = j["last_transaction"]["timestamp"];
            lt.km_from_current = j["last_transaction"]["km_from_current"];
            p.last_transaction = lt;
        }
        
        // Vectorize
        auto vec = vectorizer_->vectorize(p);
        
        // Search
        float fraud_score = index_loader_->search_fraud_score(vec.data(), 5);
        bool approved = fraud_score < 0.6f;
        
        // Build response
        json response;
        response["approved"] = approved;
        response["fraud_score"] = fraud_score;
        
        std::string response_str = response.dump();
        
        return send_json_response(connection, MHD_HTTP_OK, response_str.c_str());
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling fraud-score: " << e.what() << std::endl;
        const char* error = "{\"approved\":true,\"fraud_score\":0.0}";
        return send_json_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, error);
    }
}

MHD_Result HttpServer::send_json_response(struct MHD_Connection* connection,
                                          unsigned int status_code,
                                          const char* json_str) {
    struct MHD_Response* resp = MHD_create_response_from_buffer(
        strlen(json_str), (void*)json_str, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(resp, "Content-Type", "application/json");
    MHD_Result ret = MHD_queue_response(connection, status_code, resp);
    MHD_destroy_response(resp);
    return ret;
}
