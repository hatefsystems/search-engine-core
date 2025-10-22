#pragma once

#include "SponsorProfile.h"
#include "../../infrastructure.h"
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <memory>
#include <vector>
#include <optional>

namespace search_engine {
namespace storage {

class SponsorStorage {
private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection sponsorCollection_;
    
    // Conversion methods between SponsorProfile and BSON
    bsoncxx::document::value sponsorProfileToBson(const SponsorProfile& page) const;
    SponsorProfile bsonToSponsorProfile(const bsoncxx::document::view& doc) const;
    
    // Helper methods for BSON conversion
    static std::string sponsorStatusToString(SponsorStatus status);
    static SponsorStatus stringToSponsorStatus(const std::string& status);
    
    // Ensure indexes are created
    void ensureIndexes();

public:
    // Constructor with connection string
    explicit SponsorStorage(const std::string& connectionString = "mongodb://localhost:27017",
                           const std::string& databaseName = "search-engine");
    
    // Destructor
    ~SponsorStorage() = default;
    
    // Move constructor and assignment (delete copy operations for RAII)
    SponsorStorage(SponsorStorage&& other) noexcept = default;
    SponsorStorage& operator=(SponsorStorage&& other) noexcept = default;
    SponsorStorage(const SponsorStorage&) = delete;
    SponsorStorage& operator=(const SponsorStorage&) = delete;
    
    // Core storage operations
    Result<std::string> store(const SponsorProfile& page);
    Result<SponsorProfile> findById(const std::string& id);
    Result<std::optional<SponsorProfile>> findByEmail(const std::string& email);
    Result<std::vector<SponsorProfile>> findByStatus(SponsorStatus status);
    Result<bool> updateStatus(const std::string& id, SponsorStatus status, const std::string& notes = "");
    Result<bool> updatePaymentInfo(const std::string& id, const std::string& paymentRef, const std::string& transactionId = "");
    
    // Admin operations
    Result<std::vector<SponsorProfile>> findAll(int limit = 100, int skip = 0);
    Result<long> countByStatus(SponsorStatus status);
    Result<double> getTotalAmountByStatus(SponsorStatus status, const std::string& currency = "");
    
    // Connection test
    Result<bool> testConnection();
};

} // namespace storage
} // namespace search_engine
