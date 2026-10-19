#include "Crawler.h"
#include "PageFetcher.h"
#include "search_engine/crawler/CrawlerManager.h"

namespace {
class NativeCrawlerRunner final : public CrawlSessionRunner {
  public:
    NativeCrawlerRunner(const CrawlConfig &config,
                        std::shared_ptr<search_engine::storage::ContentStorage> storage,
                        const std::string &id, const std::string &url, bool force)
        : crawler_(config, std::move(storage), id) {
        if (auto *fetcher = crawler_.getPageFetcher()) {
            // Preserve the existing fetcher policy; TLS policy changes are separate.
            fetcher->setVerifySSL(false);
            if (config.spaRenderingEnabled)
                fetcher->setSpaRendering(true, config.browserlessUrl);
        }
        crawler_.addSeedURL(url, force);
    }
    void start() override { crawler_.start(); }
    bool isRunning() const override { return crawler_.isCrawling(); }
    void stop() override { crawler_.stop(); }
    std::vector<CrawlResult> getResults() override { return crawler_.getResults(); }

  private:
    Crawler crawler_;
};
} // namespace

CrawlerManager::CrawlerManager(std::shared_ptr<search_engine::storage::ContentStorage> storage)
    : CrawlerManager(
          storage,
          [storage](const CrawlConfig &config, const std::string &id, const std::string &url,
                    bool force) {
              return std::make_shared<NativeCrawlerRunner>(config, storage, id, url, force);
          },
          environmentLimit("MAX_CONCURRENT_SESSIONS", MAX_CONCURRENT_SESSIONS, 1, 1024),
          environmentLimit("MAX_PENDING_SESSIONS", MAX_PENDING_SESSIONS, 0, 100000)) {}
