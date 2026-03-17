#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <chrono>

namespace search_engine {
namespace cloudflare {

struct ScanResult {
    std::string ip;
    int port = 443;
    bool open = false;
    int responseTimeMs = 0;
    std::string hostname; // from TLS certificate CN / SAN, if available
    std::string cidr;
    std::string scannedAt; // ISO-8601 UTC
};

/**
 * Utility class for scanning Cloudflare IP ranges on port 443.
 *
 * Cloudflare IP ranges are embedded as defaults (sourced from the public
 * lists at https://www.cloudflare.com/ips-v4 and /ips-v6) and can be
 * refreshed at runtime if the live endpoints are reachable.
 */
class CloudflareScanner {
public:
    // ---------------------------------------------------------------
    // Known Cloudflare IP ranges (publicly published)
    // ---------------------------------------------------------------
    static std::vector<std::string> getDefaultIPv4Ranges();
    static std::vector<std::string> getDefaultIPv6Ranges();

    // ---------------------------------------------------------------
    // CIDR utilities
    // ---------------------------------------------------------------
    /**
     * Expand a CIDR block into individual IP strings.
     * @param cidr   e.g. "103.21.244.0/22"
     * @param maxIPs Maximum number of IPs to return (safety cap).
     * @return       List of dotted-decimal IPv4 strings.
     */
    static std::vector<std::string> expandCIDRv4(const std::string& cidr,
                                                  size_t maxIPs = 256);

    // ---------------------------------------------------------------
    // Port / TLS probing
    // ---------------------------------------------------------------
    /**
     * Attempt a TCP connection to ip:port within timeoutMs milliseconds.
     * @return true if the connection succeeded (port is open).
     */
    static bool checkPort(const std::string& ip, int port = 443,
                          int timeoutMs = 3000);

    /**
     * Perform a TLS handshake and return the certificate Common Name (CN)
     * or first Subject Alternative Name (SAN) of the server.
     * Returns an empty string when the handshake fails or no name is found.
     */
    static std::string getHostnameFromTLS(const std::string& ip,
                                          int port = 443,
                                          int timeoutMs = 5000);

    // ---------------------------------------------------------------
    // High-level scan helpers
    // ---------------------------------------------------------------
    /**
     * Scan every IP in @p cidr for an open port, calling @p callback for
     * each result (including closed ports when reportClosed == true).
     *
     * @param cidr          CIDR range to scan.
     * @param port          Port to check (default 443).
     * @param callback      Called for every probed IP.
     * @param threads       Degree of parallelism (worker threads).
     * @param timeoutMs     Per-IP TCP connect timeout.
     * @param maxIPs        Maximum IPs to probe from the range.
     * @param reportClosed  If false, callback is only invoked for open ports.
     * @param running       External cancel flag; scan stops when set to false.
     */
    static void scanRange(const std::string& cidr,
                          int port,
                          std::function<void(const ScanResult&)> callback,
                          int threads = 10,
                          int timeoutMs = 3000,
                          size_t maxIPs = 256,
                          bool reportClosed = false,
                          std::atomic<bool>* running = nullptr);

    static std::string nowISO8601();
};

} // namespace cloudflare
} // namespace search_engine
