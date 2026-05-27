#include <gtest/gtest.h>
#include "../../include/search_engine/seo/SchemaValidator.h"
#include <nlohmann/json.hpp>

using namespace search_engine::seo;

class SchemaValidatorTest : public ::testing::Test {
protected:
    nlohmann::json createValidPersonSchema() {
        return {
            {"@context", "https://schema.org"},
            {"@type", "Person"},
            {"name", "John Doe"},
            {"url", "https://hatef.ir/john-doe"}
        };
    }

    nlohmann::json createValidOrganizationSchema() {
        return {
            {"@context", "https://schema.org"},
            {"@type", "Organization"},
            {"name", "Tech Company"},
            {"url", "https://hatef.ir/tech-company"}
        };
    }
};

TEST_F(SchemaValidatorTest, ValidPersonSchemaPassesValidation) {
    auto schema = createValidPersonSchema();
    auto result = SchemaValidator::validatePersonSchema(schema);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.errors.size(), 0);
}

TEST_F(SchemaValidatorTest, PersonSchemaMissingContextFails) {
    auto schema = createValidPersonSchema();
    schema.erase("@context");
    
    auto result = SchemaValidator::validatePersonSchema(schema);
    
    EXPECT_FALSE(result.success);
    EXPECT_GT(result.errors.size(), 0);
}

TEST_F(SchemaValidatorTest, PersonSchemaMissingTypeFails) {
    auto schema = createValidPersonSchema();
    schema.erase("@type");
    
    auto result = SchemaValidator::validatePersonSchema(schema);
    
    EXPECT_FALSE(result.success);
    EXPECT_GT(result.errors.size(), 0);
}

TEST_F(SchemaValidatorTest, PersonSchemaMissingNameFails) {
    auto schema = createValidPersonSchema();
    schema.erase("name");
    
    auto result = SchemaValidator::validatePersonSchema(schema);
    
    EXPECT_FALSE(result.success);
    EXPECT_GT(result.errors.size(), 0);
}

TEST_F(SchemaValidatorTest, PersonSchemaWrongTypeFails) {
    auto schema = createValidPersonSchema();
    schema["@type"] = "Organization";
    
    auto result = SchemaValidator::validatePersonSchema(schema);
    
    EXPECT_FALSE(result.success);
    EXPECT_GT(result.errors.size(), 0);
}

TEST_F(SchemaValidatorTest, PersonSchemaWithValidUrl) {
    auto schema = createValidPersonSchema();
    schema["url"] = "https://hatef.ir/john-doe";
    
    auto result = SchemaValidator::validatePersonSchema(schema);
    
    EXPECT_TRUE(result.success);
}

TEST_F(SchemaValidatorTest, PersonSchemaWithInvalidUrlWarning) {
    auto schema = createValidPersonSchema();
    schema["url"] = "not-a-valid-url";
    
    auto result = SchemaValidator::validatePersonSchema(schema);
    
    // Should still succeed but with warnings
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.warnings.size(), 0);
}

TEST_F(SchemaValidatorTest, PersonSchemaWithSameAsArray) {
    auto schema = createValidPersonSchema();
    schema["sameAs"] = nlohmann::json::array({
        "https://github.com/johndoe",
        "https://linkedin.com/in/johndoe"
    });
    
    auto result = SchemaValidator::validatePersonSchema(schema);
    
    EXPECT_TRUE(result.success);
}

TEST_F(SchemaValidatorTest, PersonSchemaWithInvalidSameAsUrl) {
    auto schema = createValidPersonSchema();
    schema["sameAs"] = nlohmann::json::array({
        "https://github.com/johndoe",
        "not-a-url"
    });
    
    auto result = SchemaValidator::validatePersonSchema(schema);
    
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.warnings.size(), 0);
}

TEST_F(SchemaValidatorTest, PersonSchemaWithOccupation) {
    auto schema = createValidPersonSchema();
    schema["hasOccupation"] = {
        {"@type", "Occupation"},
        {"name", "Software Engineer"}
    };
    
    auto result = SchemaValidator::validatePersonSchema(schema);
    
    EXPECT_TRUE(result.success);
}

TEST_F(SchemaValidatorTest, PersonSchemaWithInvalidOccupation) {
    auto schema = createValidPersonSchema();
    schema["hasOccupation"] = {
        {"@type", "Occupation"}
        // Missing required 'name' field
    };
    
    auto result = SchemaValidator::validatePersonSchema(schema);
    
    EXPECT_FALSE(result.success);
    EXPECT_GT(result.errors.size(), 0);
}

TEST_F(SchemaValidatorTest, ValidOrganizationSchemaPassesValidation) {
    auto schema = createValidOrganizationSchema();
    auto result = SchemaValidator::validateOrganizationSchema(schema);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.errors.size(), 0);
}

TEST_F(SchemaValidatorTest, OrganizationSchemaWithLocalBusinessType) {
    auto schema = createValidOrganizationSchema();
    schema["@type"] = "LocalBusiness";
    
    auto result = SchemaValidator::validateOrganizationSchema(schema);
    
    EXPECT_TRUE(result.success);
}

TEST_F(SchemaValidatorTest, OrganizationSchemaWithAddress) {
    auto schema = createValidOrganizationSchema();
    schema["address"] = {
        {"@type", "PostalAddress"},
        {"streetAddress", "123 Main St"},
        {"addressLocality", "San Francisco"},
        {"addressCountry", "USA"}
    };
    
    auto result = SchemaValidator::validateOrganizationSchema(schema);
    
    EXPECT_TRUE(result.success);
}

TEST_F(SchemaValidatorTest, OrganizationSchemaWithContactPoint) {
    auto schema = createValidOrganizationSchema();
    schema["contactPoint"] = {
        {"@type", "ContactPoint"},
        {"telephone", "+1-555-1234"},
        {"email", "contact@techcompany.com"}
    };
    
    auto result = SchemaValidator::validateOrganizationSchema(schema);
    
    EXPECT_TRUE(result.success);
}

TEST_F(SchemaValidatorTest, OrganizationSchemaWithInvalidContactPoint) {
    auto schema = createValidOrganizationSchema();
    schema["contactPoint"] = {
        {"@type", "ContactPoint"}
        // Missing telephone and email
    };
    
    auto result = SchemaValidator::validateOrganizationSchema(schema);
    
    EXPECT_TRUE(result.success); // Still succeeds but with warnings
    EXPECT_GT(result.warnings.size(), 0);
}

TEST_F(SchemaValidatorTest, OrganizationSchemaWithFoundingDate) {
    auto schema = createValidOrganizationSchema();
    schema["foundingDate"] = "2020";
    
    auto result = SchemaValidator::validateOrganizationSchema(schema);
    
    EXPECT_TRUE(result.success);
}

TEST_F(SchemaValidatorTest, OrganizationSchemaWithValidFoundingDateFormats) {
    auto schema = createValidOrganizationSchema();
    
    // Test year only
    schema["foundingDate"] = "2020";
    auto result1 = SchemaValidator::validateOrganizationSchema(schema);
    EXPECT_TRUE(result1.success);
    
    // Test year-month
    schema["foundingDate"] = "2020-01";
    auto result2 = SchemaValidator::validateOrganizationSchema(schema);
    EXPECT_TRUE(result2.success);
    
    // Test full date
    schema["foundingDate"] = "2020-01-15";
    auto result3 = SchemaValidator::validateOrganizationSchema(schema);
    EXPECT_TRUE(result3.success);
}

TEST_F(SchemaValidatorTest, OrganizationSchemaWithInvalidFoundingDate) {
    auto schema = createValidOrganizationSchema();
    schema["foundingDate"] = "not-a-date";
    
    auto result = SchemaValidator::validateOrganizationSchema(schema);
    
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.warnings.size(), 0);
}

TEST_F(SchemaValidatorTest, SchemaStructureNotObject) {
    nlohmann::json schema = "not an object";
    
    auto result = SchemaValidator::validateSchemaStructure(schema);
    
    EXPECT_FALSE(result.success);
    EXPECT_GT(result.errors.size(), 0);
}

TEST_F(SchemaValidatorTest, SchemaStructureWithNonSchemaOrgContext) {
    auto schema = createValidPersonSchema();
    schema["@context"] = "https://example.com";
    
    auto result = SchemaValidator::validateSchemaStructure(schema);
    
    EXPECT_TRUE(result.success); // Succeeds but with warning
    EXPECT_GT(result.warnings.size(), 0);
}

// Main function only for standalone test execution
#ifndef SEO_TESTS_COMBINED
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
