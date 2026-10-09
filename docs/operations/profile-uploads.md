# Persistent profile uploads

Avatar and cover URLs keep their existing `/uploads/avatars/...` and
`/uploads/covers/...` paths. The application container now mounts the named
`profile_uploads` volume at `/app/uploads`. Project/experience media uses its
existing storage separately.

Before the first replacement of an existing container, stop the application,
copy its uploads, and then recreate it. Stopping prevents uploads arriving after
the snapshot. Do not remove the old container until the copy succeeds.

For the development Compose project:

```sh
docker compose stop search-engine
./scripts/migrate-profile-uploads.sh core search-engine-core_profile_uploads
docker compose up -d --build search-engine
```

For production, use the actual application container and the volume name for its
Compose project (`<project>_profile_uploads`). The script preserves files already
in the destination volume. A container without `/app/uploads` has nothing to
migrate: an existing database URL does not contain the image bytes. Restore
missing files from a backup or upload them again; do not erase profile records.

Verify an uploaded URL returns HTTP 200 before and after recreating the app
container. Never run `docker compose down -v` when retaining uploaded files.

Regression checks:

```sh
node --test tests/profile/test_autosave.mjs tests/profile/test_content_autosave.mjs
NODE_PATH=/tmp/profile-ui-tests/node_modules node tests/profile/media-autosave-browser-check.cjs
```

The browser suite uses an isolated in-memory API and the actual editor assets.
Run C++/API checks against the isolated `docker-compose.profile-test.yml` stack
for backend validation and file persistence. User profiles are not test fixtures.
