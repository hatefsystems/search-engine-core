# Website Profile API Documentation

## Overview
The Website Profile API provides endpoints for managing website profile data from Iranian e-commerce verification system (e-Namad).

**Base URL:** `/api/v2`

**Collection:** `website_profile` (MongoDB database: `search-engine`)

---

## Endpoints

### 1. Save Website Profile

**Endpoint:** `POST /api/v2/website-profile`

**Description:** Save a new website profile to the database.

**Request Headers:**
```
Content-Type: application/json
```

**Request Body:**
```json
{
  "business_name": "فروشگاه نمونه آنلاین",
  "website_url": "example-store.ir",
  "owner_name": "احمد محمدی",
  "grant_date": {
    "persian": "1404/01/01",
    "gregorian": "2025-03-21"
  },
  "expiry_date": {
    "persian": "1406/01/01",
    "gregorian": "2027-03-21"
  },
  "address": "استان : تهران - شهرستان : تهران - بخش : مرکزی - شهر : تهران - خیابان : ولیعصر - پلاک : 123 - طبقه : 2 - واحد : 5",
  "phone": "02112345678",
  "email": "info@example-store.ir",
  "location": {
    "latitude": 35.6892,
    "longitude": 51.3890
  },
  "business_experience": "5 years",
  "business_hours": "9-18",
  "business_services": [
    {
      "row_number": "1",
      "service_title": "فروش محصولات الکترونیکی و لوازم جانبی",
      "permit_issuer": "اداره صنعت، معدن و تجارت",
      "permit_number": "12345",
      "validity_start_date": "2025-01-01",
      "validity_end_date": "2026-01-01",
      "status": "تایید شده"
    }
  ],
  "extraction_timestamp": "2025-10-08T12:00:00.000Z",
  "domain_info": {
    "page_number": 1,
    "row_index": 1,
    "row_number": "100",
    "province": "تهران",
    "city": "تهران",
    "domain_url": "https://trustseal.enamad.ir/?id=123456&code=sample"
  }
}
```

**Success Response:**
```json
{
  "success": true,
  "message": "Profile saved successfully",
  "data": {
    "website_url": "example-store.ir"
  }
}
```

**Error Responses:**

*Missing required field:*
```json
{
  "success": false,
  "message": "Missing required field: website_url",
  "error": "BAD_REQUEST"
}
```

*Duplicate website URL:*
```json
{
  "success": false,
  "message": "Profile with this website URL already exists",
  "error": "BAD_REQUEST"
}
```

**Note:** The API prevents duplicate entries. If a profile with the same `website_url` already exists, the request will be rejected with a `BAD_REQUEST` error.

**Example cURL:**
```bash
curl --location 'http://localhost:3000/api/v2/website-profile' \
--header 'Content-Type: application/json' \
--data-raw '{
  "business_name": "فروشگاه نمونه آنلاین",
  "website_url": "example-store.ir",
  "owner_name": "احمد محمدی",
  "grant_date": {
    "persian": "1404/01/01",
    "gregorian": "2025-03-21"
  },
  "expiry_date": {
    "persian": "1406/01/01",
    "gregorian": "2027-03-21"
  },
  "address": "استان : تهران - شهرستان : تهران - بخش : مرکزی - شهر : تهران - خیابان : ولیعصر - پلاک : 123",
  "phone": "02112345678",
  "email": "info@example-store.ir",
  "location": {
    "latitude": 35.6892,
    "longitude": 51.3890
  },
  "business_experience": "5 years",
  "business_hours": "9-18",
  "business_services": [
    {
      "row_number": "1",
      "service_title": "فروش محصولات الکترونیکی و لوازم جانبی",
      "permit_issuer": "اداره صنعت، معدن و تجارت",
      "permit_number": "12345",
      "validity_start_date": "2025-01-01",
      "validity_end_date": "2026-01-01",
      "status": "تایید شده"
    }
  ],
  "extraction_timestamp": "2025-10-08T12:00:00.000Z",
  "domain_info": {
    "page_number": 1,
    "row_index": 1,
    "row_number": "100",
    "province": "تهران",
    "city": "تهران",
    "domain_url": "https://trustseal.enamad.ir/?id=123456&code=sample"
  }
}'
```

---

### 2. Get Website Profile by URL

**Endpoint:** `GET /api/v2/website-profile/:url`

**Description:** Retrieve a website profile by its URL.

**URL Parameters:**
- `url` (string, required) - The website URL (e.g., `example-store.ir`)

**Success Response:**
```json
{
  "success": true,
  "message": "Profile found",
  "data": {
    "business_name": "فروشگاه نمونه آنلاین",
    "website_url": "example-store.ir",
    "owner_name": "احمد محمدی",
    "grant_date": {
      "persian": "1404/01/01",
      "gregorian": "2025-03-21"
    },
    "expiry_date": {
      "persian": "1406/01/01",
      "gregorian": "2027-03-21"
    },
    "address": "استان : تهران - شهرستان : تهران...",
    "phone": "02112345678",
    "email": "info@example-store.ir",
    "location": {
      "latitude": 35.6892,
      "longitude": 51.3890
    },
    "business_experience": "5 years",
    "business_hours": "9-18",
    "business_services": [...],
    "extraction_timestamp": "2025-10-08T12:00:00.000Z",
    "domain_info": {...},
    "created_at": "2025-10-08T12:30:45.123Z"
  }
}
```

**Error Response:**
```json
{
  "success": false,
  "message": "Profile not found",
  "error": "NOT_FOUND"
}
```

**Example cURL:**
```bash
curl --location 'http://localhost:3000/api/v2/website-profile/example-store.ir'
```

---

### 3. Get All Website Profiles

**Endpoint:** `GET /api/v2/website-profiles`

**Description:** Retrieve all website profiles with pagination support.

**Query Parameters:**
- `limit` (integer, optional) - Maximum number of profiles to return (default: 100)
- `skip` (integer, optional) - Number of profiles to skip for pagination (default: 0)

**Success Response:**
```json
{
  "success": true,
  "message": "Profiles retrieved successfully",
  "data": {
    "profiles": [
      {
        "business_name": "فروشگاه نمونه آنلاین",
        "website_url": "example-store.ir",
        "owner_name": "احمد محمدی",
        ...
      }
    ],
    "count": 1,
    "limit": 100,
    "skip": 0
  }
}
```

**Example cURL:**
```bash
# Get first 10 profiles
curl --location 'http://localhost:3000/api/v2/website-profiles?limit=10&skip=0'

# Get next 10 profiles
curl --location 'http://localhost:3000/api/v2/website-profiles?limit=10&skip=10'
```

---

### 4. Update Website Profile

**Endpoint:** `PUT /api/v2/website-profile/:url`

**Description:** Update an existing website profile.

**URL Parameters:**
- `url` (string, required) - The website URL to update

**Request Headers:**
```
Content-Type: application/json
```

**Request Body:** Same as Save Website Profile (all fields that need updating)

**Success Response:**
```json
{
  "success": true,
  "message": "Profile updated successfully"
}
```

**Error Response:**
```json
{
  "success": false,
  "message": "Profile not found or no changes made",
  "error": "NOT_FOUND"
}
```

**Example cURL:**
```bash
curl --location --request PUT 'http://localhost:3000/api/v2/website-profile/example-store.ir' \
--header 'Content-Type: application/json' \
--data-raw '{
  "business_name": "فروشگاه نمونه آنلاین (به‌روزرسانی شده)",
  "website_url": "example-store.ir",
  "owner_name": "احمد محمدی",
  "phone": "02198765432",
  "email": "updated@example-store.ir"
}'
```

---

### 5. Delete Website Profile

**Endpoint:** `DELETE /api/v2/website-profile/:url`

**Description:** Delete a website profile from the database.

**URL Parameters:**
- `url` (string, required) - The website URL to delete

**Success Response:**
```json
{
  "success": true,
  "message": "Profile deleted successfully"
}
```

**Error Response:**
```json
{
  "success": false,
  "message": "Profile not found",
  "error": "NOT_FOUND"
}
```

**Example cURL:**
```bash
curl --location --request DELETE 'http://localhost:3000/api/v2/website-profile/example-store.ir'
```

---

### 6. Check if Profile Exists

**Endpoint:** `GET /api/v2/website-profile/check/:url`

**Description:** Check if a website profile exists in the database.

**URL Parameters:**
- `url` (string, required) - The website URL to check

**Success Response:**
```json
{
  "success": true,
  "message": "Profile exists",
  "data": {
    "website_url": "example-store.ir",
    "exists": true
  }
}
```

**Example cURL:**
```bash
curl --location 'http://localhost:3000/api/v2/website-profile/check/example-store.ir'
```

---

## Data Model

### WebsiteProfile
```typescript
{
  business_name: string;
  website_url: string;  // Required, unique identifier
  owner_name: string;
  grant_date: {
    persian: string;    // Persian calendar date (e.g., "1404/01/01")
    gregorian: string;  // Gregorian date (e.g., "2025-03-21")
  };
  expiry_date: {
    persian: string;
    gregorian: string;
  };
  address: string;
  phone: string;
  email: string;
  location: {
    latitude: number;
    longitude: number;
  };
  business_experience: string;
  business_hours: string;
  business_services: Array<{
    row_number: string;
    service_title: string;
    permit_issuer: string;
    permit_number: string;
    validity_start_date: string;
    validity_end_date: string;
    status: string;
  }>;
  extraction_timestamp: string;  // ISO 8601 format
  domain_info: {
    page_number: number;
    row_index: number;
    row_number: string;
    province: string;
    city: string;
    domain_url: string;
  };
  created_at: string;  // Auto-generated, ISO 8601 format
}
```

---

## Error Codes

| Code | HTTP Status | Description |
|------|-------------|-------------|
| `BAD_REQUEST` | 400 | Invalid request data or missing required fields |
| `NOT_FOUND` | 404 | Profile not found |
| `INTERNAL_ERROR` | 500 | Database or server error |

---

## MongoDB Collection Schema

**Database:** `search-engine`  
**Collection:** `website_profile`

**Indexes:**
- `website_url` (unique) - for fast lookups
- `created_at` (descending) - for sorted retrieval

---

## Testing

### Test the API with Docker

1. **Start the server:**
```bash
cd /root/search-engine-core
docker compose up
```

2. **Test saving a profile:**
```bash
curl --location 'http://localhost:3000/api/v2/website-profile' \
--header 'Content-Type: application/json' \
--data-raw '{
  "business_name": "Test Store",
  "website_url": "teststore.ir",
  "owner_name": "Test Owner",
  "grant_date": {"persian": "1404/01/01", "gregorian": "2025-03-21"},
  "expiry_date": {"persian": "1405/01/01", "gregorian": "2026-03-21"},
  "address": "Test Address",
  "phone": "02112345678",
  "email": "test@example.com",
  "location": {"latitude": 35.6892, "longitude": 51.3890},
  "business_experience": "",
  "business_hours": "9-18",
  "business_services": [],
  "extraction_timestamp": "2025-10-08T12:00:00.000Z",
  "domain_info": {
    "page_number": 1,
    "row_index": 1,
    "row_number": "1",
    "province": "Tehran",
    "city": "Tehran",
    "domain_url": "https://example.com"
  }
}'
```

3. **Verify in MongoDB:**
```bash
docker exec mongodb_test mongosh --username admin --password password123 \
--eval "use('search-engine'); db.website_profile.find().pretty()"
```

---

## Notes

- All timestamps are stored in ISO 8601 format (UTC)
- The `website_url` field is the **unique identifier** for each profile
- **Duplicate Prevention:** The API automatically prevents duplicate profiles with the same `website_url`
- Persian calendar dates are stored as strings in the format "YYYY/MM/DD"
- The API follows REST conventions with proper HTTP methods
- All endpoints follow lazy initialization pattern for MongoDB connections
- Proper error handling with MongoDB exceptions logged

---

## Version History

- **v1.0** (2025-10-08) - Initial implementation with full CRUD operations

