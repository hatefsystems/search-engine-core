
#include<string>
using namespace std;
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
using namespace std;
using namespace bsoncxx::v_noabi::builder::stream;
#include "infrastructure.h"
#include <mutex>
#include <memory>

// Forward declaration
namespace mongocxx {
    class instance;
}

// Singleton class to manage MongoDB instance
class MongoDBInstance {
private:
    static std::unique_ptr<mongocxx::instance> instance;
    static std::mutex mutex;

public:
    static mongocxx::instance& getInstance();
};

class mongodb
{
public:
	Result<bool> subscribeEmail(const string& email, const string& ipAddress = "", const string& userAgent = "");
};