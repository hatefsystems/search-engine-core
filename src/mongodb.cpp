#include "../include/mongodb.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include "../include/infrastructure.h"
#include <mutex>
using namespace bsoncxx::v_noabi::builder::stream;
using namespace std;



// Initialize static members
std::unique_ptr<mongocxx::instance> MongoDBInstance::instance;
std::mutex MongoDBInstance::mutex;

// Implementation of getInstance method
mongocxx::instance& MongoDBInstance::getInstance() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!instance) {
        instance = std::make_unique<mongocxx::instance>();
    }
    return *instance;
}

// Function to subscribe email to a mailing list
Result<bool> mongodb::subscribeEmail(const string& email, const string& ipAddress, const string& userAgent)
{
    // Logic to add the email to your mailing list
    try
    {
        // Use the singleton instance instead of creating a new one
        mongocxx::instance& instance = MongoDBInstance::getInstance();
        (void)instance; // Suppress unused variable warning
        
        // Get MongoDB connection string from environment or use default
        const char* mongoUri = std::getenv("MONGODB_URI");
        std::string mongoConnectionString = mongoUri ? mongoUri : "mongodb://admin:password123@mongodb:27017";
        
        mongocxx::uri uri(mongoConnectionString);
        mongocxx::client client(uri);
        auto database = client["search-engine"];
        auto collection = database["news-subscriber"];

        // Create filter to check if email already exists
        auto filter = document{} << "email" << email << finalize;
        auto count = collection.count_documents(filter.view());

        if (count == 0)
        {
            // Get current timestamp
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            
            // Create document with all fields
            auto doc = make_document(
                kvp("email", email),
                kvp("ip_address", ipAddress),
                kvp("user_agent", userAgent),
                kvp("created_at", bsoncxx::types::b_date{std::chrono::milliseconds{timestamp}})
            );
            
            // Insert new email subscription
            auto result = collection.insert_one(doc.view());
            if (result) {
                return Result<bool>::Success(true, "Successfully subscribed!");
            } else {
                return Result<bool>::Failure("Failed to insert email");
            }
        }
        else {
            return Result<bool>::Failure("duplicate");
        }
    }
    catch (const mongocxx::exception& e) {
        return Result<bool>::Failure(e.what());
    }
}

