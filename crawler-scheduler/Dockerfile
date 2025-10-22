FROM python:3.11-slim

WORKDIR /app

# Install system dependencies (tzdata for timezone support)
RUN apt-get update && \
    apt-get install -y --no-install-recommends tzdata && \
    rm -rf /var/lib/apt/lists/*

# Install Python dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application code
COPY app/ ./app/

# Create data directories
RUN mkdir -p /app/data/pending /app/data/processed /app/data/failed

# Set environment variables
ENV PYTHONUNBUFFERED=1
ENV CELERY_BROKER_URL=redis://redis:6379/0
ENV CELERY_RESULT_BACKEND=redis://redis:6379/0
ENV MONGODB_URI=mongodb://admin:password123@mongodb:27017
ENV MONGODB_DB=search-engine
ENV API_BASE_URL=http://core:3000

# Default command (can be overridden in docker-compose)
CMD ["celery", "-A", "app.celery_app", "worker", "--loglevel=info"]

