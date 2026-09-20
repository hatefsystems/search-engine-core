"""HTTP/DB regression test against a running TEST server; see tests/profile/README.md.

Requires pymongo. Inserts unique fixture IDs and removes only those IDs afterward.
"""
import datetime
import json
import os
import uuid
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, build_opener, HTTPRedirectHandler

from bson import ObjectId
from pymongo import MongoClient


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def main():
    # Explicit test targets prevent accidentally using a developer's default DB.
    base = os.environ["PROFILE_TEST_BASE_URL"].rstrip("/")
    client = MongoClient(os.environ["PROFILE_TEST_MONGODB_URI"], serverSelectionTimeoutMS=3000)
    collection = client[os.environ["PROFILE_TEST_DATABASE"]]["profiles"]
    opener = build_opener(NoRedirect())

    def get(path, accept="application/json", token=None):
        headers = {"Accept": accept}
        if token:
            headers["Authorization"] = "Bearer " + token
        try:
            response = opener.open(Request(base + path, headers=headers), timeout=30)
        except HTTPError as error:
            response = error
        with response:
            return response.status, response.headers, response.read().decode("utf-8")

    suffix = uuid.uuid4().hex[:12]
    slug = "آزمون_هاتف_" + suffix
    old_slug = "old-profile-" + suffix
    ids = [ObjectId(), ObjectId(), ObjectId()]
    fixture = {
        "_id": ids[0], "slug": slug, "type": "PERSON", "name": "Fixture original",
        "displayName": "نام نمایشی آزمایشی", "englishName": "Fixture English",
        "tagline": "Fixture tagline", "bio": "Fixture bio", "isPublic": True,
        "createdAt": datetime.datetime.now(datetime.timezone.utc),
        "avatarUrl": "/uploads/avatars/fixture.png", "coverImageUrl": "/uploads/covers/fixture.png",
        "skillsWithLevel": [{"name": "C++", "level": "EXPERT", "category": "TECHNICAL"}],
        "location": "hidden-location-marker", "availabilityStatus": "AVAILABLE",
        "privacy": {"showEmail": False, "showPhone": False, "showLocation": False, "showAvailability": False},
        "ownerToken": "hidden-owner-marker", "ownerId": "hidden-owner-id",
        "previousSlugs": [old_slug],
    }
    try:
        collection.insert_many([
            fixture,
            {**fixture, "_id": ids[1], "slug": "private-" + suffix, "previousSlugs": [], "isPublic": False},
            {"_id": ids[2], "slug": "business-" + suffix, "name": "Fixture business", "type": "BUSINESS",
             "isPublic": True, "createdAt": fixture["createdAt"]},
        ])
        canonical_data = None
        # A unique first request misses the slug cache; later requests hit it.
        for alias in ["/", "/profiles/"]:
            for _ in range(2):
                status, headers, body = get(alias + quote(slug))
                assert status == 200, (status, body)
                data = json.loads(body)["data"]
                assert data["displayName"] == fixture["displayName"]
                assert data["avatarUrl"] == fixture["avatarUrl"]
                assert data["skillsWithLevel"][0]["name"] == "C++"
                assert "location" not in data and "availabilityStatus" not in data
                assert "hidden-" not in body
                if canonical_data is None:
                    canonical_data = data
                assert data == canonical_data
                assert "no-cache" in headers.get("Cache-Control", "")
                assert "Accept" in headers.get("Vary", "")
            status, headers, body = get(alias + quote(slug), "text/html")
            assert status == 200 and fixture["displayName"] in body
            assert "hidden-" not in body and "آماده همکاری" not in body
            assert "no-cache" in headers.get("Cache-Control", "")
            status, headers, _ = get(alias + old_slug)
            assert status == 301 and headers["Location"] in ["/" + slug, "/" + quote(slug)]
            for malformed in ["%", "%GG", "bad%2Fslug", "%252F", "%FF"]:
                assert get(alias + malformed)[0] == 400
            assert get(alias + "%61pi")[0] == 404
            for accept in ["application/json", "text/html"]:
                assert get(alias + "private-" + suffix, accept)[0] == 403
                assert get(alias + "missing-" + suffix, accept)[0] == 404
            assert get(alias + "business-" + suffix, "text/html")[0] == 200

        status, _, body = get("/api/profiles/" + str(ids[0]))
        assert status == 200 and "hidden-" not in body
        status, _, body = get("/api/profiles/" + str(ids[0]), token=fixture["ownerToken"])
        assert status == 200 and json.loads(body)["data"]["location"] == fixture["location"]
        assert get("/api/profiles/" + str(ids[1]))[0] == 403
        assert get("/api/profiles/" + str(ids[1]), token=fixture["ownerToken"])[0] == 200
        collection.update_one({"_id": ids[0]}, {"$set": {"privacy.showLocation": True}})
        assert json.loads(get("/" + quote(slug))[2])["data"]["location"] == fixture["location"]
        collection.update_one({"_id": ids[0]}, {"$set": {"privacy.showLocation": False, "isPublic": False}})
        assert get("/" + quote(slug), "text/html")[0] == 403
        print("Passed public profile HTTP, privacy, cache, redirect, and business regression checks.")
    finally:
        collection.delete_many({"_id": {"$in": ids}})
        client.close()


if __name__ == "__main__":
    main()
