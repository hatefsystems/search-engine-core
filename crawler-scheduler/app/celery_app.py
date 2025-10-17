from celery import Celery
from celery.schedules import crontab
from app.config import Config

# Validate configuration on startup
Config.validate()

# Initialize Celery
app = Celery(
    'crawler_scheduler',
    broker=Config.CELERY_BROKER_URL,
    backend=Config.CELERY_RESULT_BACKEND,
    include=['app.tasks']
)

# Celery Configuration
app.conf.update(
    task_serializer='json',
    accept_content=['json'],
    result_serializer='json',
    timezone='Asia/Tehran',
    enable_utc=False,
    task_track_started=True,
    task_time_limit=300,  # 5 minutes max per task
    task_soft_time_limit=240,  # Soft limit at 4 minutes
    worker_prefetch_multiplier=1,  # Process one task at a time
    worker_max_tasks_per_child=100,  # Restart worker after 100 tasks (memory management)
    result_expires=3600,  # Results expire after 1 hour
)

# Celery Beat Schedule (Periodic Tasks)
app.conf.beat_schedule = {
    'process-pending-files': {
        'task': 'app.tasks.process_pending_files',
        'schedule': Config.TASK_INTERVAL_SECONDS,  # Run every 60 seconds
        'options': {
            'expires': 50,  # Task expires if not executed within 50 seconds
        }
    },
}

if __name__ == '__main__':
    app.start()

