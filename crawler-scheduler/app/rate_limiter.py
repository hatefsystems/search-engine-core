import logging
from datetime import datetime, time
from typing import Optional
from app.config import Config
from app.database import get_database

logger = logging.getLogger(__name__)

class RateLimiter:
    """
    Progressive warm-up rate limiter with time window control
    
    Features:
    - Progressive daily limits (50→100→200→400→800)
    - Time window enforcement (10:00-12:00)
    - Automatic day calculation based on first processed file
    """
    
    def __init__(self):
        self.config = Config
        self.warmup_schedule = Config.get_warmup_schedule()
        self.db = get_database()
    
    def can_process_now(self) -> tuple[bool, str]:
        """
        Check if we can process a file right now
        Returns (can_process, reason)
        """
        # Check 1: Is warm-up enabled?
        if not self.config.WARMUP_ENABLED:
            return (True, "Warm-up disabled, no rate limiting")
        
        # Check 2: Are we in the allowed time window?
        if not self._is_in_time_window():
            current_time = datetime.now().strftime('%H:%M')
            return (
                False, 
                f"Outside processing window. Current: {current_time}, "
                f"Allowed: {self.config.WARMUP_START_HOUR}:00-{self.config.WARMUP_END_HOUR}:00"
            )
        
        # Check 3: Have we reached today's limit?
        daily_limit = self._get_current_daily_limit()
        daily_count = self.db.get_daily_processed_count()
        
        if daily_count >= daily_limit:
            return (
                False,
                f"Daily limit reached: {daily_count}/{daily_limit} (Day {self._get_warmup_day()})"
            )
        
        remaining = daily_limit - daily_count
        return (
            True,
            f"Can process. Progress: {daily_count}/{daily_limit}, Remaining: {remaining} (Day {self._get_warmup_day()})"
        )
    
    def _is_in_time_window(self) -> bool:
        """Check if current time is within allowed processing window"""
        now = datetime.now()
        current_time = now.time()
        
        start_time = time(hour=self.config.WARMUP_START_HOUR, minute=0)
        end_time = time(hour=self.config.WARMUP_END_HOUR, minute=0)
        
        return start_time <= current_time < end_time
    
    def _get_warmup_day(self) -> int:
        """Get current warm-up day (1-based)"""
        return self.db.get_warmup_day()
    
    def _get_current_daily_limit(self) -> int:
        """
        Get daily limit for current warm-up day
        If we exceed the schedule length, use the last value
        """
        day = self._get_warmup_day()
        
        if day <= len(self.warmup_schedule):
            limit = self.warmup_schedule[day - 1]  # Convert to 0-based index
        else:
            # After warm-up period, use maximum limit
            limit = self.warmup_schedule[-1]
        
        logger.info(f"Day {day} daily limit: {limit}")
        return limit
    
    def get_status_info(self) -> dict:
        """Get current rate limiter status for monitoring"""
        daily_limit = self._get_current_daily_limit()
        daily_count = self.db.get_daily_processed_count()
        can_process, reason = self.can_process_now()
        
        return {
            'warmup_enabled': self.config.WARMUP_ENABLED,
            'warmup_day': self._get_warmup_day(),
            'daily_limit': daily_limit,
            'daily_processed': daily_count,
            'remaining_today': max(0, daily_limit - daily_count),
            'can_process': can_process,
            'reason': reason,
            'time_window': f"{self.config.WARMUP_START_HOUR}:00-{self.config.WARMUP_END_HOUR}:00",
            'in_time_window': self._is_in_time_window(),
            'warmup_schedule': self.warmup_schedule
        }

# Singleton instance
_rate_limiter_instance: Optional[RateLimiter] = None

def get_rate_limiter() -> RateLimiter:
    """Get or create rate limiter singleton instance"""
    global _rate_limiter_instance
    if _rate_limiter_instance is None:
        _rate_limiter_instance = RateLimiter()
    return _rate_limiter_instance

