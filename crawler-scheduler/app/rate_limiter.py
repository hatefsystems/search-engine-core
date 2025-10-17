import logging
from datetime import datetime, time
from typing import Optional
from zoneinfo import ZoneInfo
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
        # Get timezone for timezone-aware datetime
        try:
            self.timezone = ZoneInfo(Config.TIMEZONE)
        except Exception as e:
            logger.warning(f"Failed to load timezone {Config.TIMEZONE}, using UTC: {e}")
            self.timezone = ZoneInfo('UTC')
    
    def _get_current_time(self) -> datetime:
        """Get current time in configured timezone"""
        return datetime.now(self.timezone)
    
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
            current_time = self._get_current_time().strftime('%H:%M')
            end_display = self.config.WARMUP_END_HOUR
            # Special formatting for end-of-day cases
            if end_display == 0 or end_display == 24:
                end_display_str = "23:59"
            else:
                end_display_str = f"{end_display}:59"
            return (
                False, 
                f"Outside processing window. Current: {current_time} ({self.config.TIMEZONE}), "
                f"Allowed: {self.config.WARMUP_START_HOUR}:00-{end_display_str}"
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
        """
        Check if current time is within allowed processing window
        
        Note: The end hour is INCLUSIVE. If WARMUP_END_HOUR=23, 
        processing continues through 23:59:59 (entire hour 23).
        Special case: If end hour is 0 or 24, it means end of day (23:59:59).
        """
        now = self._get_current_time()
        current_hour = now.hour
        current_minute = now.minute
        
        start_hour = self.config.WARMUP_START_HOUR
        end_hour = self.config.WARMUP_END_HOUR
        
        # Special case: end hour of 0 or 24 means end of day (23:59)
        if end_hour == 0 or end_hour == 24:
            end_hour = 24  # Will be treated as end of day
        
        # Check if we're in the time window
        # Start hour is inclusive, end hour is INCLUSIVE (entire hour)
        if start_hour <= end_hour:
            # Normal case: e.g., 10:00 to 23:59 (start=10, end=23)
            # Process if current hour is between start and end (inclusive)
            # OR if current hour equals end hour (entire end hour is included)
            return start_hour <= current_hour <= end_hour
        else:
            # Wrap-around case: e.g., 22:00 to 02:59 (start=22, end=2)
            # Process if hour >= start OR hour <= end
            return current_hour >= start_hour or current_hour <= end_hour
    
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
        
        # Format time window display (end hour is inclusive)
        end_display = self.config.WARMUP_END_HOUR
        if end_display == 0 or end_display == 24:
            end_display_str = "23:59"
        else:
            end_display_str = f"{end_display}:59"
        
        return {
            'warmup_enabled': self.config.WARMUP_ENABLED,
            'warmup_day': self._get_warmup_day(),
            'daily_limit': daily_limit,
            'daily_processed': daily_count,
            'remaining_today': max(0, daily_limit - daily_count),
            'can_process': can_process,
            'reason': reason,
            'time_window': f"{self.config.WARMUP_START_HOUR}:00-{end_display_str}",
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

