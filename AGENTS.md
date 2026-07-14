# Repository Guidelines

## Project Structure & Module Organization

`src/` contains the C++14 service. `main.cpp` is the composition root; routing, upload/download services, configuration, and content-addressed storage live alongside it. Database models and data access objects are in `src/database/`, while background cleanup and deduplication work is in `src/cleaner/`. Keep new production code close to its owning module and add its header beside its implementation.

`tests/` holds GoogleTest unit and integration sources plus shell-based HTTP checks. `config/` contains server and Nginx configuration, `database/migrations/` contains ordered SQL migrations, `web/` holds the static UI, and `docs/` records design decisions. Do not commit generated `build/`, runtime storage, logs, or `.env` credentials.

## Build, Test, and Development Commands

```bash
cmake -S . -B build -DFILELINK_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build -L unit --output-on-failure
```

Use `docker compose up -d mysql` followed by `docker compose run --rm migrate` to prepare MySQL. Enable service-backed checks with `-DFILELINK_BUILD_INTEGRATION_TESTS=ON`, then run `FILELINK_TEST_MYSQL_PASSWORD=... ctest --test-dir build -L integration --output-on-failure`. Start the compiled server through `./scripts/run-dev.sh`; it loads the untracked `.env` and `config/server.toml`.

## Coding Style & Naming Conventions

Follow the surrounding C++ style: four spaces, braces on the same line as declarations, and standard-library types such as `std::string` and `uint64_t`. Use `PascalCase` for classes (`UploadService`), `snake_case` for functions and local variables (`write_session_chunk`), and `camelCase` for data members/config fields where existing APIs use it (`storageRoot_`, `ioThreads`). Keep helpers small, validate external input, and preserve the service/storage/database separation. No formatter or linter is configured; format changed code consistently with nearby files.

## Testing Guidelines

Add focused GoogleTest cases in the matching `tests/*Tests.cpp` file. Name fixtures `*Test` and tests as behavior statements, for example `TEST_F(AppConfigTest, RejectsInvalidNumbers)`. Unit tests must not require MySQL; put database, process, and HTTP coverage behind the `integration` label. Run the relevant CTest label before submitting.

## Commit & Pull Request Guidelines

Recent history uses concise conventional prefixes, commonly `Feat:`, `Fix:`, `Refactor:`, and `Update:`; write an imperative Chinese or English summary after the prefix. Keep commits scoped. Pull requests should explain the behavior and storage/API impact, link the issue when available, list commands run, include screenshots for `web/` changes, and call out migration or configuration changes explicitly.
