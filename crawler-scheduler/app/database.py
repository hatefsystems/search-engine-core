import logging
from datetime import datetime
from typing import Optional
from zoneinfo import ZoneInfo
from pymongo import MongoClient, ASCENDING
from pymongo.errors import DuplicateKeyError
from app.config import Config

logger = logging.getLogger(__name__)

class Database:
    """MongoDB handler for tracking processed files"""
    
    def __init__(self):
        self.client = MongoClient(Config.MONGODB_URI)
        self.db = self.client[Config.MONGODB_DB]
        self.collection = self.db[Config.MONGODB_COLLECTION]
        # Get timezone for timezone-aware datetime
        try:
            self.timezone = ZoneInfo(Config.TIMEZONE)
        except Exception as e:
            logger.warning(f"Failed to load timezone {Config.TIMEZONE}, using UTC: {e}")
            self.timezone = ZoneInfo('UTC')
        self._ensure_indexes()
    
    def _get_current_time(self) -> datetime:
        """Get current time in configured timezone"""
        return datetime.now(self.timezone)
    
    def _ensure_indexes(self):
        """Create necessary indexes"""
        try:
            # Unique index on filename to prevent duplicate processing
            self.collection.create_index([('filename', ASCENDING)], unique=True)
            # Index on status for efficient queries
            self.collection.create_index([('status', ASCENDING)])
            # Index on processed_at for analytics
            self.collection.create_index([('processed_at', ASCENDING)])
            logger.info("Database indexes created successfully")
        except Exception as e:
            logger.error(f"Failed to create indexes: {e}")
    
    def is_file_processed(self, filename: str) -> bool:
        """Check if file has been processed"""
        return self.collection.find_one({'filename': filename}) is not None
    
    def mark_file_as_processing(self, filename: str, file_data: dict) -> bool:
        """
        Mark file as currently being processed
        Returns True if successfully marked, False if already exists
        """
        try:
            self.collection.insert_one({
                'filename': filename,
                'status': 'processing',
                'file_data': file_data,
                'started_at': self._get_current_time(),
                'attempts': 1,
                'error_message': None
            })
            logger.info(f"Marked file as processing: {filename}")
            return True
        except DuplicateKeyError:
            logger.warning(f"File already processed or processing: {filename}")
            return False
        except Exception as e:
            logger.error(f"Failed to mark file as processing: {e}")
            return False
    
    def mark_file_as_processed(self, filename: str, api_response: dict):
        """Mark file as successfully processed"""
        try:
            self.collection.update_one(
                {'filename': filename},
                {
                    '$set': {
                        'status': 'processed',
                        'processed_at': self._get_current_time(),
                        'api_response': api_response
                    }
                }
            )
            logger.info(f"Marked file as processed: {filename}")
        except Exception as e:
            logger.error(f"Failed to mark file as processed: {e}")
    
    def mark_file_as_failed(self, filename: str, error_message: str):
        """Mark file as failed"""
        try:
            self.collection.update_one(
                {'filename': filename},
                {
                    '$set': {
                        'status': 'failed',
                        'failed_at': self._get_current_time(),
                        'error_message': error_message
                    },
                    '$inc': {'attempts': 1}
                }
            )
            logger.error(f"Marked file as failed: {filename} - {error_message}")
        except Exception as e:
            logger.error(f"Failed to mark file as failed: {e}")
    
    def get_processing_stats(self) -> dict:
        """Get statistics about file processing"""
        try:
            total = self.collection.count_documents({})
            processed = self.collection.count_documents({'status': 'processed'})
            processing = self.collection.count_documents({'status': 'processing'})
            failed = self.collection.count_documents({'status': 'failed'})
            
            return {
                'total': total,
                'processed': processed,
                'processing': processing,
                'failed': failed,
                'success_rate': (processed / total * 100) if total > 0 else 0
            }
        except Exception as e:
            logger.error(f"Failed to get stats: {e}")
            return {}
    
    def get_daily_processed_count(self) -> int:
        """Get count of files processed today (in configured timezone)"""
        try:
            # Get start of today in configured timezone
            now = self._get_current_time()
            today_start = now.replace(hour=0, minute=0, second=0, microsecond=0)
            count = self.collection.count_documents({
                'status': 'processed',
                'processed_at': {'$gte': today_start}
            })
            return count
        except Exception as e:
            logger.error(f"Failed to get daily count: {e}")
            return 0
    
    def get_warmup_day(self) -> int:
        """
        Calculate which day of warm-up we're on (1-based)
        Based on when first file was processed (in configured timezone)
        """
        try:
            first_doc = self.collection.find_one(
                {'status': 'processed'},
                sort=[('processed_at', ASCENDING)]
            )
            
            if not first_doc:
                return 1  # First day
            
            # Get dates in configured timezone
            first_datetime = first_doc['processed_at']
            # If stored datetime is timezone-aware, convert to our timezone
            if first_datetime.tzinfo is not None:
                first_datetime = first_datetime.astimezone(self.timezone)
            else:
                # If naive datetime, assume it's in our timezone
                first_datetime = first_datetime.replace(tzinfo=self.timezone)
            
            first_date = first_datetime.date()
            today = self._get_current_time().date()
            days_diff = (today - first_date).days
            
            return days_diff + 1  # 1-based day number
        except Exception as e:
            logger.error(f"Failed to get warmup day: {e}")
            return 1
    
    def close(self):
        """Close database connection"""
        if self.client:
            self.client.close()
            logger.info("Database connection closed")

# Singleton instance
_db_instance: Optional[Database] = None

def get_database() -> Database:
    """Get or create database singleton instance"""
    global _db_instance
    if _db_instance is None:
        _db_instance = Database()
    return _db_instance

