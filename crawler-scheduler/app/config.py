import os
from typing import List


def _detect_timezone() -> str:
    """
    Detect timezone from environment or system configuration
    Priority: SCHEDULER_TIMEZONE > TZ > /etc/timezone > /etc/localtime > UTC
    """
    # Priority 1: SCHEDULER_TIMEZONE environment variable
    env_tz = os.getenv('SCHEDULER_TIMEZONE', '').strip()
    if env_tz:
        return env_tz
    
    # Priority 2: TZ environment variable (standard Unix way)
    try:
        if 'TZ' in os.environ and os.environ['TZ'].strip():
            return os.environ['TZ'].strip()
        
        # Priority 3: Read from /etc/timezone (Debian/Ubuntu)
        if os.path.exists('/etc/timezone'):
            with open('/etc/timezone', 'r') as f:
                tz = f.read().strip()
                if tz:
                    return tz
        
        # Priority 4: Read from /etc/localtime symlink (modern systems)
        if os.path.islink('/etc/localtime'):
            link = os.readlink('/etc/localtime')
            # Extract timezone from path like /usr/share/zoneinfo/Asia/Tehran
            if '/zoneinfo/' in link:
                return link.split('/zoneinfo/')[-1]
    except Exception:
        pass
    
    # Default to UTC if all detection methods fail
    return 'UTC'


class Config:
    """Configuration for crawler scheduler"""
    
    # Celery Configuration
    CELERY_BROKER_URL = os.getenv('CELERY_BROKER_URL', 'redis://redis:6379/1')
    CELERY_RESULT_BACKEND = os.getenv('CELERY_RESULT_BACKEND', 'redis://redis:6379/1')
    
    # Timezone Configuration
    TIMEZONE = _detect_timezone()
    
    # MongoDB Configuration
    MONGODB_URI = os.getenv('MONGODB_URI', 'mongodb://admin:password123@mongodb_test:27017')
    MONGODB_DB = os.getenv('MONGODB_DB', 'search-engine')
    MONGODB_COLLECTION = 'crawler_scheduler_tracking'
    
    # API Configuration
    API_BASE_URL = os.getenv('API_BASE_URL', 'http://core:3000')
    API_ENDPOINT = '/api/v2/website-profile'
    
    # File Processing Configuration
    PENDING_DIR = os.getenv('PENDING_DIR', '/app/data/pending')
    PROCESSED_DIR = os.getenv('PROCESSED_DIR', '/app/data/processed')
    FAILED_DIR = os.getenv('FAILED_DIR', '/app/data/failed')
    
    # Warm-up Configuration
    WARMUP_ENABLED = os.getenv('WARMUP_ENABLED', 'true').lower() == 'true'
    WARMUP_SCHEDULE_RAW = os.getenv('WARMUP_SCHEDULE', '50,100,200,400,800')
    WARMUP_START_HOUR = int(os.getenv('WARMUP_START_HOUR', '10'))
    WARMUP_END_HOUR = int(os.getenv('WARMUP_END_HOUR', '12'))
    
    @classmethod
    def get_warmup_schedule(cls) -> List[int]:
        """Parse warmup schedule from environment variable"""
        return [int(x.strip()) for x in cls.WARMUP_SCHEDULE_RAW.split(',')]
    
    # Jitter Configuration (Randomization to avoid exact timing)
    JITTER_MIN_SECONDS = int(os.getenv('JITTER_MIN_SECONDS', '30'))
    JITTER_MAX_SECONDS = int(os.getenv('JITTER_MAX_SECONDS', '60'))
    
    # Task Configuration
    TASK_INTERVAL_SECONDS = int(os.getenv('TASK_INTERVAL_SECONDS', '60'))
    MAX_RETRIES = int(os.getenv('MAX_RETRIES', '3'))
    RETRY_DELAY_SECONDS = int(os.getenv('RETRY_DELAY_SECONDS', '300'))
    
    # Logging
    LOG_LEVEL = os.getenv('LOG_LEVEL', 'info').upper()
    
    @classmethod
    def validate(cls):
        """Validate configuration"""
        assert cls.WARMUP_START_HOUR < cls.WARMUP_END_HOUR, "Start hour must be before end hour"
        assert cls.JITTER_MIN_SECONDS < cls.JITTER_MAX_SECONDS, "Min jitter must be less than max jitter"
        assert cls.TASK_INTERVAL_SECONDS > 0, "Task interval must be positive"
        schedule = cls.get_warmup_schedule()
        assert len(schedule) > 0, "Warmup schedule cannot be empty"
        assert all(x > 0 for x in schedule), "All warmup values must be positive"

