#!/usr/bin/env python3
"""
Redis Sync Service - Syncs MongoDB indexed_pages to Redis for fast search
"""
import os
import time
import logging
from datetime import datetime, timedelta
from typing import Dict, List, Optional
import pymongo
import redis
from redis.commands.search.field import TextField, NumericField, TagField
from redis.commands.search.indexDefinition import IndexDefinition, IndexType

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class RedisSync:
    """Handles syncing MongoDB indexed_pages to Redis"""
    
    def __init__(self):
        # MongoDB configuration
        self.mongo_uri = os.getenv('MONGODB_URI', 'mongodb://admin:password123@mongodb:27017')
        self.mongo_db = os.getenv('MONGODB_DB', 'search-engine')
        
        # Redis configuration
        redis_host = os.getenv('REDIS_HOST', 'redis')
        redis_port = int(os.getenv('REDIS_PORT', '6379'))
        self.redis_db = int(os.getenv('REDIS_DB', '0'))
        
        # Search configuration
        self.index_name = os.getenv('SEARCH_INDEX_NAME', 'search_index')
        self.key_prefix = os.getenv('KEY_PREFIX', 'doc:')
        self.batch_size = int(os.getenv('BATCH_SIZE', '100'))
        self.max_content_size = int(os.getenv('MAX_CONTENT_SIZE', '50000'))
        
        # Sync configuration
        self.sync_mode = os.getenv('SYNC_MODE', 'full')  # full or incremental
        self.sync_interval = int(os.getenv('SYNC_INTERVAL_SECONDS', '3600'))  # Default 1 hour
        self.incremental_window_hours = int(os.getenv('INCREMENTAL_WINDOW_HOURS', '24'))
        
        # Initialize connections
        logger.info(f"Connecting to MongoDB: {self.mongo_uri}")
        self.mongo_client = pymongo.MongoClient(self.mongo_uri)
        self.db = self.mongo_client[self.mongo_db]
        self.collection = self.db['indexed_pages']
        
        logger.info(f"Connecting to Redis: {redis_host}:{redis_port}/{self.redis_db}")
        self.redis_client = redis.Redis(
            host=redis_host,
            port=redis_port,
            db=self.redis_db,
            decode_responses=True
        )
        
        # Test connections
        self._test_connections()
        
        # Create search index
        self._create_search_index()
    
    def _test_connections(self):
        """Test MongoDB and Redis connections"""
        try:
            self.mongo_client.admin.command('ping')
            logger.info("✅ MongoDB connection successful")
        except Exception as e:
            logger.error(f"❌ MongoDB connection failed: {e}")
            raise
        
        try:
            self.redis_client.ping()
            logger.info("✅ Redis connection successful")
        except Exception as e:
            logger.error(f"❌ Redis connection failed: {e}")
            raise
    
    def _create_search_index(self):
        """Create RediSearch index if it doesn't exist"""
        try:
            # Check if index exists
            try:
                self.redis_client.ft(self.index_name).info()
                logger.info(f"✅ Search index '{self.index_name}' already exists")
                return
            except redis.ResponseError:
                pass  # Index doesn't exist, create it
            
            # Create index schema (UPDATED - no content field)
            schema = (
                TextField("url", no_stem=True),
                TextField("title", weight=5.0),  # Title is most important for search
                TextField("description", weight=2.0),  # Description for snippets
                TagField("domain", sortable=True),
                TagField("keywords"),
                TagField("language"),
                TagField("category"),
                NumericField("indexed_at", sortable=True),
                NumericField("score", sortable=True)
            )
            
            # Create index
            definition = IndexDefinition(
                prefix=[self.key_prefix],
                index_type=IndexType.HASH
            )
            
            self.redis_client.ft(self.index_name).create_index(
                schema,
                definition=definition
            )
            
            logger.info(f"✅ Created search index '{self.index_name}'")
            
        except Exception as e:
            logger.error(f"❌ Failed to create search index: {e}")
            raise
    
    def _sanitize_text(self, text: str) -> str:
        """Sanitize text to ensure valid UTF-8"""
        if not text:
            return ""
        
        try:
            # Remove invalid UTF-8 characters
            return text.encode('utf-8', errors='ignore').decode('utf-8', errors='ignore')
        except Exception as e:
            logger.warning(f"Failed to sanitize text: {e}")
            return ""
    
    def _extract_content(self, page: Dict) -> str:
        """Extract searchable content from page"""
        content = []
        
        # Add title
        if page.get('title'):
            content.append(self._sanitize_text(page['title']))
        
        # Add description
        if page.get('description'):
            content.append(self._sanitize_text(page['description']))
        
        # Add text content (truncate if too large)
        if page.get('textContent'):
            text = self._sanitize_text(page['textContent'])
            if len(text) > self.max_content_size:
                text = text[:self.max_content_size]
            content.append(text)
        
        return ' '.join(content)
    
    def _generate_doc_key(self, url: str) -> str:
        """Generate Redis key for document"""
        url_hash = str(hash(url))
        return f"{self.key_prefix}{url_hash}"
    
    def _sync_document(self, page: Dict) -> bool:
        """Sync only essential search fields to Redis"""
        try:
            doc_key = self._generate_doc_key(page['url'])

            # Store ONLY critical search fields (minimal memory footprint)
            doc = {
                'url': self._sanitize_text(page.get('url', ''))[:500],  # Truncate long URLs
                'title': self._sanitize_text(page.get('title', ''))[:200],  # Max 200 chars
                'domain': self._sanitize_text(page.get('domain', ''))[:100],
                'score': page.get('contentQuality', 0.0) if page.get('contentQuality') else 0.0,
                'indexed_at': int(page.get('indexedAt', datetime.now()).timestamp())
            }

            # Store truncated description for search snippets (300 chars max)
            if page.get('description'):
                doc['description'] = self._sanitize_text(page['description'])[:300]
            else:
                doc['description'] = ''

            # Store only top 5 keywords
            if page.get('keywords'):
                sanitized_keywords = [self._sanitize_text(k) for k in page['keywords'][:5]]
                doc['keywords'] = '|'.join(sanitized_keywords)[:200]  # Max 200 chars total

            # Optional metadata (small fields)
            if page.get('language'):
                doc['language'] = self._sanitize_text(page['language'])[:10]

            if page.get('category'):
                doc['category'] = self._sanitize_text(page['category'])[:50]

            # DO NOT STORE: content, textContent, full description
            # Full content should be fetched from MongoDB when user clicks result

            self.redis_client.hset(doc_key, mapping=doc)

            return True

        except Exception as e:
            logger.error(f"Failed to sync document {page.get('url')}: {e}")
            return False
    
    def sync_full(self) -> Dict:
        """Perform full sync of all indexed pages"""
        logger.info("🔄 Starting FULL sync from MongoDB to Redis...")
        start_time = time.time()
        
        stats = {
            'total': 0,
            'success': 0,
            'failed': 0,
            'skipped': 0
        }
        
        try:
            # Count total documents
            query = {'isIndexed': True}
            total_docs = self.collection.count_documents(query)
            stats['total'] = total_docs
            
            logger.info(f"📊 Total documents to sync: {total_docs:,}")
            
            # Process in batches
            batch_count = 0
            cursor = self.collection.find(query).sort('indexedAt', -1)
            
            for page in cursor:
                if self._sync_document(page):
                    stats['success'] += 1
                else:
                    stats['failed'] += 1
                
                # Log progress every batch
                if (stats['success'] + stats['failed']) % self.batch_size == 0:
                    batch_count += 1
                    progress = ((stats['success'] + stats['failed']) / total_docs) * 100
                    logger.info(f"📈 Progress: {stats['success'] + stats['failed']:,}/{total_docs:,} ({progress:.1f}%)")
            
            duration = time.time() - start_time
            stats['duration_seconds'] = round(duration, 2)
            
            logger.info(f"✅ FULL sync completed in {duration:.2f}s")
            logger.info(f"   Success: {stats['success']:,}")
            logger.info(f"   Failed: {stats['failed']:,}")
            logger.info(f"   Total: {stats['total']:,}")
            
            return stats
            
        except Exception as e:
            logger.error(f"❌ Full sync failed: {e}")
            raise
    
    def sync_incremental(self) -> Dict:
        """Perform incremental sync of recently modified pages"""
        logger.info("🔄 Starting INCREMENTAL sync from MongoDB to Redis...")
        start_time = time.time()
        
        stats = {
            'total': 0,
            'success': 0,
            'failed': 0,
            'skipped': 0
        }
        
        try:
            # Calculate time threshold
            threshold = datetime.now() - timedelta(hours=self.incremental_window_hours)
            
            # Query for recently modified documents
            query = {
                'isIndexed': True,
                '$or': [
                    {'indexedAt': {'$gte': threshold}},
                    {'lastModified': {'$gte': threshold}}
                ]
            }
            
            total_docs = self.collection.count_documents(query)
            stats['total'] = total_docs
            
            logger.info(f"📊 Documents modified in last {self.incremental_window_hours}h: {total_docs:,}")
            
            if total_docs == 0:
                logger.info("✅ No documents to sync")
                stats['duration_seconds'] = 0
                return stats
            
            # Process documents
            cursor = self.collection.find(query).sort('indexedAt', -1)
            
            for page in cursor:
                if self._sync_document(page):
                    stats['success'] += 1
                else:
                    stats['failed'] += 1
                
                # Log progress
                if (stats['success'] + stats['failed']) % self.batch_size == 0:
                    progress = ((stats['success'] + stats['failed']) / total_docs) * 100
                    logger.info(f"📈 Progress: {stats['success'] + stats['failed']:,}/{total_docs:,} ({progress:.1f}%)")
            
            duration = time.time() - start_time
            stats['duration_seconds'] = round(duration, 2)
            
            logger.info(f"✅ INCREMENTAL sync completed in {duration:.2f}s")
            logger.info(f"   Success: {stats['success']:,}")
            logger.info(f"   Failed: {stats['failed']:,}")
            
            return stats
            
        except Exception as e:
            logger.error(f"❌ Incremental sync failed: {e}")
            raise
    
    def get_status(self) -> Dict:
        """Get sync status and document counts"""
        try:
            # MongoDB count
            mongo_count = self.collection.count_documents({'isIndexed': True})
            
            # Redis count
            try:
                index_info = self.redis_client.ft(self.index_name).info()
                redis_count = int(index_info.get('num_docs', 0))
            except Exception as e:
                logger.warning(f"Failed to get Redis count: {e}")
                redis_count = 0
            
            status = {
                'mongodb_count': mongo_count,
                'redis_count': redis_count,
                'in_sync': mongo_count == redis_count,
                'difference': abs(mongo_count - redis_count),
                'sync_percentage': (redis_count / mongo_count * 100) if mongo_count > 0 else 0
            }
            
            logger.info(f"📊 Status - MongoDB: {mongo_count:,}, Redis: {redis_count:,}, In Sync: {status['in_sync']}")
            
            return status
            
        except Exception as e:
            logger.error(f"Failed to get status: {e}")
            raise
    
    def clear_index(self):
        """Clear Redis search index"""
        try:
            logger.warning("🗑️  Clearing Redis search index...")
            
            # Drop and recreate index
            try:
                self.redis_client.ft(self.index_name).dropindex(delete_documents=True)
                logger.info("✅ Index dropped")
            except redis.ResponseError:
                logger.info("Index doesn't exist, skipping drop")
            
            # Recreate index
            self._create_search_index()
            logger.info("✅ Index cleared and recreated")
            
        except Exception as e:
            logger.error(f"Failed to clear index: {e}")
            raise
    
    def run(self):
        """Main run loop"""
        logger.info("🚀 Redis Sync Service started")
        logger.info(f"   Mode: {self.sync_mode}")
        logger.info(f"   Interval: {self.sync_interval}s")
        logger.info(f"   Batch size: {self.batch_size}")
        
        # Initial sync
        try:
            status = self.get_status()
            
            # If Redis is empty or significantly out of sync, do full sync
            if status['redis_count'] == 0 or status['difference'] > (status['mongodb_count'] * 0.1):
                logger.info("🔄 Redis is empty or significantly out of sync, performing initial full sync...")
                self.sync_full()
            else:
                logger.info("✅ Redis is in sync, skipping initial sync")
        except Exception as e:
            logger.error(f"Initial sync failed: {e}")
        
        # Continuous sync loop
        logger.info(f"⏰ Starting sync loop (every {self.sync_interval}s)")
        
        while True:
            try:
                time.sleep(self.sync_interval)
                
                logger.info("⏰ Sync interval reached")
                
                if self.sync_mode == 'incremental':
                    self.sync_incremental()
                else:
                    self.sync_full()
                
                # Log status after sync
                self.get_status()
                
            except KeyboardInterrupt:
                logger.info("👋 Shutting down Redis Sync Service...")
                break
            except Exception as e:
                logger.error(f"Sync loop error: {e}")
                time.sleep(60)  # Wait before retrying


def main():
    """Main entry point"""
    try:
        sync_service = RedisSync()
        sync_service.run()
    except Exception as e:
        logger.error(f"Fatal error: {e}")
        exit(1)


if __name__ == '__main__':
    main()

