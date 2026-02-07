# Tier 3: Legal Vault Protocol

## Overview

The Legal Vault is the highest security tier in our three-tier privacy architecture. It stores **unencrypted** compliance data that can only be accessed with a valid court order and dual authorization.

## Purpose

- **Emergency legal compliance** - Response to valid court orders only
- **Air-gapped storage** - Physically separated from application servers
- **Dual authorization** - Requires CEO + Legal Officer signatures
- **Complete audit trail** - Every access is logged and cannot be deleted

## Data Schema

```cpp
struct EmergencyLegalData {
    // Case identification
    std::string caseNumber;              // Court case number
    std::string courtOrder;              // Document reference number
    std::chrono::system_clock::time_point orderDate;
    
    // Original unencrypted data (from Tier 2 compliance logs)
    std::string ipAddress_original;      // Original IP address (unencrypted)
    std::string userAgent_original;      // Original User-Agent (unencrypted)
    std::string referrer_original;       // Original referrer (unencrypted)
    
    // Associated profile/view data
    std::string profileId;               // Profile involved
    std::string viewId;                  // Link to analytics view
    std::chrono::system_clock::time_point timestamp;
    
    // Access control and audit
    std::string authorizedBy;            // CEO + Legal Officer names
    std::chrono::system_clock::time_point accessedAt;
    std::string accessReason;            // Legal justification
    std::string extractionMethod;        // How data was extracted
    std::string dataDestination;         // Where data was sent (court, law enforcement)
    
    // Lifecycle management
    std::chrono::system_clock::time_point dataDestructionDate;
    bool isDestroyed = false;
    std::string destructionMethod;       // Secure deletion method used
};
```

## Access Protocol

### Step 1: Court Order Validation

1. Valid court order received by Legal Department
2. Legal Officer validates:
   - Jurisdiction is correct
   - Order specifies exact data required
   - Time period covered by order
   - Legal basis is sound
3. CEO notified of court order

### Step 2: Dual Authorization

**Requirements:**
- CEO physical signature on court order
- Legal Officer physical signature on court order
- Both signatures must be on same document
- Document scanned and archived

**Authorization Form:**
```
LEGAL VAULT ACCESS AUTHORIZATION

Case Number: [___________]
Court: [___________]
Order Date: [___________]

Data Requested:
□ IP addresses for profile: [___________]
□ User-Agent strings for profile: [___________]
□ Referrer URLs for profile: [___________]
□ Time period: [___________] to [___________]

Legal Justification:
[___________________________________________]

Authorized By:
CEO Signature: _____________ Date: _______
Legal Officer Signature: _____________ Date: _______

Witness: _____________ Date: _______
```

### Step 3: Data Extraction

1. **Physical Security:**
   - Access to Legal Vault server requires physical key
   - Server room access logged with biometrics
   - Two-person rule: Both CEO and Legal Officer must be present

2. **Technical Extraction:**
   ```bash
   # Decrypt Tier 2 compliance logs using emergency key
   ./legal-vault-extract \
     --case-number "CASE-2024-0001" \
     --profile-id "507f1f77bcf86cd799439011" \
     --start-date "2024-01-01" \
     --end-date "2024-12-31" \
     --court-order "/secure/court-orders/order-001.pdf" \
     --output "/secure/extractions/case-2024-0001.json"
   ```

3. **Data Format:**
   ```json
   {
     "caseNumber": "CASE-2024-0001",
     "courtOrder": "ORDER-2024-XYZ",
     "extractionDate": "2024-12-15T10:30:00Z",
     "authorizedBy": "John Doe (CEO), Jane Smith (Legal Officer)",
     "dataExtracted": {
       "profileId": "507f1f77bcf86cd799439011",
       "views": [
         {
           "viewId": "1639392000000-1234",
           "timestamp": "2024-06-15T14:23:11Z",
           "ipAddress": "203.0.113.42",
           "userAgent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)...",
           "referrer": "https://google.com/search?q=example"
         }
       ]
     }
   }
   ```

### Step 4: Data Transfer

1. Data extracted to encrypted USB drive (FIPS 140-2 compliant)
2. USB drive sealed in tamper-evident envelope
3. Hand-delivered to court by Legal Officer
4. Delivery receipt obtained and archived

### Step 5: Audit Trail

Every extraction creates an audit log entry:

```cpp
struct LegalVaultAuditLog {
    std::string auditId;
    std::string caseNumber;
    std::chrono::system_clock::time_point accessTime;
    std::string accessedBy;              // CEO + Legal Officer
    std::string dataExtracted;           // Summary of data
    std::string courtOrder;              // Reference to order
    std::string deliveryReceipt;         // Proof of delivery
    std::string witnessName;             // Third-party witness
};
```

### Step 6: Data Destruction

After legal requirement is fulfilled:

1. Court case closed or data retention period expired
2. CEO + Legal Officer authorize destruction
3. Three-pass secure wipe of extracted data
4. Destruction certificate generated
5. Audit log updated with destruction date

## Security Measures

### Physical Security

- **Air-gapped server:** No network connection to Legal Vault
- **Secure room:** Biometric + physical key access
- **24/7 monitoring:** Video surveillance of server room
- **Two-person rule:** Never accessed by single individual

### Logical Security

- **Encryption at rest:** AES-256 disk encryption
- **Emergency key:** Split into three parts (CEO, Legal Officer, Board Chair)
- **Access logs:** Tamper-proof append-only audit trail
- **No remote access:** Physical presence required

### Compliance

- **GDPR Article 6(1)(c):** Processing for compliance with legal obligation
- **Local data retention laws:** Maximum retention as required by law
- **Court order documentation:** Complete paper trail maintained
- **Annual audit:** Independent security review

## Implementation Status

**Current (Task 01c):**
- ✅ Schema defined
- ✅ Access protocol documented
- ✅ Audit requirements specified

**Future Tasks:**
- ⏳ Physical infrastructure setup
- ⏳ Extraction tool development
- ⏳ Dual authorization system
- ⏳ Biometric access control
- ⏳ Audit log implementation

## Emergency Contacts

In case of court order:

1. **Legal Officer:** [Contact TBD]
2. **CEO:** [Contact TBD]
3. **External Legal Counsel:** [Contact TBD]
4. **Security Officer:** [Contact TBD]

## Annual Review

This protocol must be reviewed annually and updated as needed:
- Legal compliance requirements change
- New court precedents established
- Technology improvements available
- Security vulnerabilities discovered

---

**Document Version:** 1.0  
**Last Updated:** [Task 01c Implementation]  
**Next Review:** [One year from deployment]  
**Owner:** Legal Department + Security Team
