import logging
from celery import Task
from app.celery_app import app
from app.file_processor import get_file_processor
from app.rate_limiter import get_rate_limiter
from app.database import get_database
from app.config import Config

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Log timezone configuration on startup
logger.info(f"Scheduler timezone configured: {Config.TIMEZONE}")

class BaseTask(Task):
    """Base task with error handling"""
    
    def on_failure(self, exc, task_id, args, kwargs, einfo):
        logger.error(f'Task {task_id} failed: {exc}')
        logger.error(f'Exception info: {einfo}')

@app.task(base=BaseTask, bind=True)
def process_pending_files(self):
    """
    Main periodic task that processes pending JSON files
    Runs every minute as configured in Celery Beat
    """
    logger.info("=" * 80)
    logger.info("Starting periodic file processing task")
    logger.info("=" * 80)
    
    try:
        # Get singletons
        processor = get_file_processor()
        rate_limiter = get_rate_limiter()
        
        # Check rate limiter status
        can_process, reason = rate_limiter.can_process_now()
        logger.info(f"Rate limiter check: {reason}")
        
        if not can_process:
            logger.warning(f"Cannot process files: {reason}")
            return {
                'status': 'skipped',
                'reason': reason,
                'stats': processor.get_stats()
            }
        
        # Get pending files
        pending_files = processor.get_pending_files()
        
        if not pending_files:
            logger.info("No pending files to process")
            return {
                'status': 'no_files',
                'stats': processor.get_stats()
            }
        
        # Process one file per task execution (controlled rate limiting)
        file_to_process = pending_files[0]
        logger.info(f"Processing file: {file_to_process}")
        logger.info(f"Remaining files in queue: {len(pending_files) - 1}")
        
        # Process the file
        success = processor.process_file(file_to_process)
        
        # Get updated stats
        stats = processor.get_stats()
        
        result = {
            'status': 'success' if success else 'failed',
            'file': file_to_process,
            'remaining_files': len(pending_files) - 1,
            'stats': stats
        }
        
        logger.info(f"Task completed: {result['status']}")
        logger.info(f"Daily progress: {stats['rate_limiter']['daily_processed']}/{stats['rate_limiter']['daily_limit']}")
        logger.info("=" * 80)
        
        return result
    
    except Exception as e:
        logger.error(f"Error in process_pending_files task: {e}", exc_info=True)
        return {
            'status': 'error',
            'error': str(e)
        }

@app.task(base=BaseTask)
def get_scheduler_status():
    """
    Get current scheduler status
    Can be called manually from Flower UI or API
    """
    try:
        processor = get_file_processor()
        stats = processor.get_stats()
        
        pending_files = processor.get_pending_files()
        
        return {
            'status': 'healthy',
            'pending_files_count': len(pending_files),
            'database_stats': stats['database'],
            'rate_limiter_stats': stats['rate_limiter']
        }
    except Exception as e:
        logger.error(f"Error getting status: {e}", exc_info=True)
        return {
            'status': 'error',
            'error': str(e)
        }

@app.task(base=BaseTask)
def process_single_file(file_path: str):
    """
    Process a specific file manually
    Can be triggered from Flower UI for testing
    """
    try:
        logger.info(f"Manual processing of file: {file_path}")
        processor = get_file_processor()
        success = processor.process_file(file_path)
        
        return {
            'status': 'success' if success else 'failed',
            'file': file_path
        }
    except Exception as e:
        logger.error(f"Error processing single file: {e}", exc_info=True)
        return {
            'status': 'error',
            'file': file_path,
            'error': str(e)
        }

@app.task(base=BaseTask)
def reset_warmup_schedule():
    """
    Reset warm-up schedule (for testing)
    Clears all processing history
    """
    try:
        logger.warning("Resetting warm-up schedule - clearing all processing history!")
        db = get_database()
        result = db.collection.delete_many({})
        
        return {
            'status': 'success',
            'deleted_count': result.deleted_count,
            'message': 'Warm-up schedule reset successfully'
        }
    except Exception as e:
        logger.error(f"Error resetting schedule: {e}", exc_info=True)
        return {
            'status': 'error',
            'error': str(e)
        }

