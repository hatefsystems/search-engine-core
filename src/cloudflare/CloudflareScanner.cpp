#include "../../include/search_engine/cloudflare/CloudflareScanner.h"
#include "../../include/Logger.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

#include <sstream>
#include <iomanip>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <ctime>

namespace search_engine {
namespace cloudflare {

// ---------------------------------------------------------------------------
// Known Cloudflare IP ranges (source: https://www.cloudflare.com/ips-v4)
// ---------------------------------------------------------------------------

std::vector<std::string> CloudflareScanner::getDefaultIPv4Ranges() {
    return {
        "173.245.48.0/20",
        "103.21.244.0/22",
        "103.22.200.0/22",
        "103.31.4.0/22",
        "141.101.64.0/18",
        "108.162.192.0/18",
        "190.93.240.0/20",
        "188.114.96.0/20",
        "197.234.240.0/22",
        "198.41.128.0/17",
        "162.158.0.0/15",
        "104.16.0.0/13",
        "104.24.0.0/14",
        "172.64.0.0/13",
        "131.0.72.0/22"
    };
}

std::vector<std::string> CloudflareScanner::getDefaultIPv6Ranges() {
    return {
        "2400:cb00::/32",
        "2606:4700::/32",
        "2803:f800::/32",
        "2405:b500::/32",
        "2405:8100::/32",
        "2a06:98c0::/29",
        "2c0f:f248::/32"
    };
}

// ---------------------------------------------------------------------------
// CIDR utilities
// ---------------------------------------------------------------------------

std::vector<std::string> CloudflareScanner::expandCIDRv4(const std::string& cidr,
                                                          size_t maxIPs) {
    std::vector<std::string> ips;
    const auto slashPos = cidr.find('/');
    if (slashPos == std::string::npos) {
        ips.push_back(cidr);
        return ips;
    }

    const std::string networkStr = cidr.substr(0, slashPos);
    const int prefixLen = std::stoi(cidr.substr(slashPos + 1));
    if (prefixLen < 0 || prefixLen > 32) {
        LOG_WARNING("Invalid CIDR prefix length: " + cidr);
        return ips;
    }

    struct in_addr addr{};
    if (inet_pton(AF_INET, networkStr.c_str(), &addr) != 1) {
        LOG_WARNING("Invalid IPv4 address in CIDR: " + cidr);
        return ips;
    }

    const uint32_t network = ntohl(addr.s_addr);
    const uint32_t mask    = (prefixLen == 0) ? 0u
                           : (prefixLen == 32) ? ~0u
                           : (~0u << (32 - prefixLen));
    const uint32_t first   = network & mask;
    const uint32_t last    = first | (~mask);

    const size_t total = static_cast<size_t>(last - first) + 1;
    const size_t count = std::min(total, maxIPs);

    ips.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t ip = htonl(first + static_cast<uint32_t>(i));
        char buf[INET_ADDRSTRLEN] = {};
        struct in_addr tmp{};
        tmp.s_addr = ip;
        inet_ntop(AF_INET, &tmp, buf, sizeof(buf));
        ips.emplace_back(buf);
    }
    return ips;
}

// ---------------------------------------------------------------------------
// Port check (non-blocking TCP connect with poll timeout)
// ---------------------------------------------------------------------------

bool CloudflareScanner::checkPort(const std::string& ip, int port,
                                   int timeoutMs) {
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    // Set non-blocking
    const int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(sock);
        return false;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        close(sock);
        return false;
    }

    const int ret = connect(sock, reinterpret_cast<struct sockaddr*>(&addr),
                            sizeof(addr));
    bool connected = false;
    if (ret == 0) {
        connected = true;
    } else if (errno == EINPROGRESS) {
        struct pollfd pfd{};
        pfd.fd     = sock;
        pfd.events = POLLOUT;
        const int pollRet = poll(&pfd, 1, timeoutMs);
        if (pollRet > 0) {
            int sockErr = 0;
            socklen_t len = sizeof(sockErr);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &sockErr, &len) == 0 &&
                sockErr == 0) {
                connected = true;
            }
        }
    }

    close(sock);
    return connected;
}

// ---------------------------------------------------------------------------
// TLS hostname extraction
// ---------------------------------------------------------------------------

std::string CloudflareScanner::getHostnameFromTLS(const std::string& ip,
                                                   int port,
                                                   int timeoutMs) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) return {};

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        SSL_CTX_free(ctx);
        return {};
    }

    // Set connect timeout via SO_RCVTIMEO / SO_SNDTIMEO
    struct timeval tv{};
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        close(sock);
        SSL_CTX_free(ctx);
        return {};
    }

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr),
                sizeof(addr)) != 0) {
        close(sock);
        SSL_CTX_free(ctx);
        return {};
    }

    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        close(sock);
        SSL_CTX_free(ctx);
        return {};
    }

    SSL_set_fd(ssl, sock);
    std::string hostname;

    if (SSL_connect(ssl) == 1) {
        X509* cert = SSL_get_peer_certificate(ssl);
        if (cert) {
            // Try Subject Alternative Names first
            GENERAL_NAMES* sans = static_cast<GENERAL_NAMES*>(
                X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));
            if (sans) {
                const int count = sk_GENERAL_NAME_num(sans);
                for (int i = 0; i < count && hostname.empty(); ++i) {
                    GENERAL_NAME* entry = sk_GENERAL_NAME_value(sans, i);
                    if (entry->type == GEN_DNS) {
                        unsigned char* utf8 = nullptr;
                        const int len = ASN1_STRING_to_UTF8(
                            &utf8,
                            entry->d.dNSName);
                        if (utf8) {
                            if (len > 0) {
                                hostname.assign(
                                    reinterpret_cast<const char*>(utf8),
                                    static_cast<size_t>(len));
                            }
                            OPENSSL_free(utf8);
                        }
                    }
                }
                GENERAL_NAMES_free(sans);
            }

            // Fall back to CN
            if (hostname.empty()) {
                X509_NAME* subj = X509_get_subject_name(cert);
                if (subj) {
                    char cn[256] = {};
                    if (X509_NAME_get_text_by_NID(subj, NID_commonName,
                                                   cn, sizeof(cn)) > 0) {
                        hostname = cn;
                    }
                }
            }
            X509_free(cert);
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    return hostname;
}

// ---------------------------------------------------------------------------
// High-level range scanner (thread pool)
// ---------------------------------------------------------------------------

void CloudflareScanner::scanRange(const std::string& cidr,
                                   int port,
                                   std::function<void(const ScanResult&)> callback,
                                   int threads,
                                   int timeoutMs,
                                   size_t maxIPs,
                                   bool reportClosed,
                                   std::atomic<bool>* running) {
    const std::vector<std::string> ips = expandCIDRv4(cidr, maxIPs);
    if (ips.empty()) return;

    std::mutex queueMutex;
    std::queue<std::string> ipQueue;
    for (const auto& ip : ips) ipQueue.push(ip);

    std::mutex callbackMutex;

    const int workerCount = std::max(1, threads);
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(workerCount));

    for (int w = 0; w < workerCount; ++w) {
        workers.emplace_back([&]() {
            while (true) {
                if (running && !running->load()) break;

                std::string ip;
                {
                    std::lock_guard<std::mutex> lk(queueMutex);
                    if (ipQueue.empty()) break;
                    ip = ipQueue.front();
                    ipQueue.pop();
                }

                const auto t0 = std::chrono::steady_clock::now();
                const bool open = checkPort(ip, port, timeoutMs);
                const auto t1 = std::chrono::steady_clock::now();
                const int ms  = static_cast<int>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                        .count());

                if (!open && !reportClosed) continue;

                ScanResult result;
                result.ip            = ip;
                result.port          = port;
                result.open          = open;
                result.responseTimeMs = ms;
                result.cidr          = cidr;
                result.scannedAt     = nowISO8601();

                if (open) {
                    result.hostname = getHostnameFromTLS(ip, port, timeoutMs);
                }

                {
                    std::lock_guard<std::mutex> lk(callbackMutex);
                    callback(result);
                }
            }
        });
    }

    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string CloudflareScanner::nowISO8601() {
    const auto now     = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32] = {};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

} // namespace cloudflare
} // namespace search_engine
