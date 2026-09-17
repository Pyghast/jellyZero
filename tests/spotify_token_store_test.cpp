#include "spotify_token_store.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

int checks = 0;
void check(bool ok, const char* message) {
    ++checks;
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main() {
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() / "jellyzero-spotify-token-test";
    fs::create_directories(dir);

    // Missing file.
    {
        model::SpotifyTokens tokens;
        std::string error;
        const auto path = dir / "missing.json";
        fs::remove(path);
        check(!model::load_spotify_tokens(path, tokens, error), "missing token file fails to load");
        check(!error.empty(), "missing token file reports an error message");
    }

    // Malformed JSON.
    {
        const auto path = dir / "malformed.json";
        std::ofstream(path) << "{ not valid json";
        model::SpotifyTokens tokens;
        std::string error;
        check(!model::load_spotify_tokens(path, tokens, error), "malformed token file fails to load");
        check(!error.empty(), "malformed token file reports an error message");
    }

    // Missing required field.
    {
        const auto path = dir / "incomplete.json";
        std::ofstream(path) << R"({"client_id":"abc"})";
        model::SpotifyTokens tokens;
        std::string error;
        check(!model::load_spotify_tokens(path, tokens, error), "token file missing fields fails to load");
    }

    // Round-trip save/load, and expiry logic.
    {
        const auto path = dir / "tokens.json";
        model::SpotifyTokens saved;
        saved.client_id = "client-123";
        saved.access_token = "access-abc";
        saved.refresh_token = "refresh-xyz";
        saved.scope = "user-read-private";
        saved.expires_at = 9999999999; // far future

        std::string save_error;
        check(model::save_spotify_tokens(path, saved, save_error), "save_spotify_tokens succeeds");

        model::SpotifyTokens loaded;
        std::string load_error;
        check(model::load_spotify_tokens(path, loaded, load_error), "load_spotify_tokens succeeds after save");
        check(loaded.client_id == saved.client_id, "round-trip preserves client_id");
        check(loaded.access_token == saved.access_token, "round-trip preserves access_token");
        check(loaded.refresh_token == saved.refresh_token, "round-trip preserves refresh_token");
        check(loaded.scope == saved.scope, "round-trip preserves scope");
        check(loaded.expires_at == saved.expires_at, "round-trip preserves expires_at");

        check(!model::spotify_tokens_need_refresh(loaded), "far-future expiry does not need refresh");

        loaded.expires_at = 1; // long past
        check(model::spotify_tokens_need_refresh(loaded), "past expiry needs refresh");
    }

    fs::remove_all(dir);
    std::cout << "PASS: " << checks << " spotify token store checks\n";
    return 0;
}
