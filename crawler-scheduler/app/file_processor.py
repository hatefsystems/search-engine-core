import json
import logging
import os
import shutil
import random
import time
from pathlib import Path
from typing import Optional, List
import requests
from app.config import Config
from app.database import get_database
from app.rate_limiter import get_rate_limiter

logger = logging.getLogger(__name__)

class FileProcessor:
    """Process JSON files and call API"""
    
    def __init__(self):
        self.config = Config
        self.db = get_database()
        self.rate_limiter = get_rate_limiter()
        self.api_url = f"{Config.API_BASE_URL}{Config.API_ENDPOINT}"
    
    def get_pending_files(self) -> List[str]:
        """Get list of unprocessed JSON files from pending directory"""
        try:
            pending_dir = Path(self.config.PENDING_DIR)
            if not pending_dir.exists():
                logger.warning(f"Pending directory does not exist: {pending_dir}")
                return []
            
            # Get all JSON files
            json_files = list(pending_dir.glob('*.txt'))
            
            # Filter out already processed files
            unprocessed_files = [
                str(f) for f in json_files 
                if not self.db.is_file_processed(f.name)
            ]
            
            logger.info(f"Found {len(json_files)} JSON files, {len(unprocessed_files)} unprocessed")
            return unprocessed_files
        
        except Exception as e:
            logger.error(f"Error scanning pending directory: {e}")
            return []
    
    def process_file(self, file_path: str) -> bool:
        """
        Process a single JSON file
        Returns True if successful, False otherwise
        """
        filename = Path(file_path).name
        
        try:
            # Step 1: Read and validate JSON
            logger.info(f"Processing file: {filename}")
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            # Step 2: Check if already processed (double-check)
            if self.db.is_file_processed(filename):
                logger.warning(f"File already processed (duplicate): {filename}")
                self._move_to_processed(file_path)
                return True
            
            # Step 3: Mark as processing (atomic operation)
            if not self.db.mark_file_as_processing(filename, data):
                logger.warning(f"File processing already started by another worker: {filename}")
                return False
            
            # Step 4: Apply jitter (random delay to avoid exact timing)
            jitter = random.randint(
                self.config.JITTER_MIN_SECONDS,
                self.config.JITTER_MAX_SECONDS
            )
            logger.info(f"Applying jitter: {jitter} seconds for {filename}")
            time.sleep(jitter)
            
            # Step 5: Call API
            response = self._call_api(data)
            
            if response:
                # Success
                self.db.mark_file_as_processed(filename, response)
                self._move_to_processed(file_path)
                logger.info(f"✓ Successfully processed: {filename}")
                return True
            else:
                # API call failed
                error_msg = "API call failed or returned error"
                self.db.mark_file_as_failed(filename, error_msg)
                self._move_to_failed(file_path)
                logger.error(f"✗ Failed to process: {filename}")
                return False
        
        except json.JSONDecodeError as e:
            error_msg = f"Invalid JSON: {str(e)}"
            logger.error(f"JSON parsing error in {filename}: {e}")
            self.db.mark_file_as_failed(filename, error_msg)
            self._move_to_failed(file_path)
            return False
        
        except Exception as e:
            error_msg = f"Unexpected error: {str(e)}"
            logger.error(f"Error processing {filename}: {e}", exc_info=True)
            self.db.mark_file_as_failed(filename, error_msg)
            self._move_to_failed(file_path)
            return False
    
    def _call_api(self, data: dict) -> Optional[dict]:
        """
        Call the website profile API
        Returns API response dict if successful, None otherwise
        """
        try:
            logger.info(f"Calling API: {self.api_url}")
            logger.debug(f"Request data: {json.dumps(data, ensure_ascii=False)[:200]}...")
            
            response = requests.post(
                self.api_url,
                json=data,
                headers={'Content-Type': 'application/json'},
                timeout=30
            )
            
            logger.info(f"API response status: {response.status_code}")
            
            if response.status_code == 200:
                response_data = response.json()
                logger.info(f"API call successful: {response_data}")
                return response_data
            else:
                logger.error(f"API returned error status: {response.status_code}")
                logger.error(f"Response body: {response.text[:500]}")
                return None
        
        except requests.exceptions.Timeout:
            logger.error(f"API call timeout after 30 seconds")
            return None
        
        except requests.exceptions.RequestException as e:
            logger.error(f"API request failed: {e}")
            return None
        
        except Exception as e:
            logger.error(f"Unexpected error calling API: {e}", exc_info=True)
            return None
    
    def _move_to_processed(self, file_path: str):
        """Move file to processed directory"""
        try:
            filename = Path(file_path).name
            dest_dir = Path(self.config.PROCESSED_DIR)
            dest_dir.mkdir(parents=True, exist_ok=True)
            dest_path = dest_dir / filename
            
            shutil.move(file_path, dest_path)
            logger.info(f"Moved to processed: {filename}")
        except Exception as e:
            logger.error(f"Failed to move file to processed: {e}")
    
    def _move_to_failed(self, file_path: str):
        """Move file to failed directory"""
        try:
            filename = Path(file_path).name
            dest_dir = Path(self.config.FAILED_DIR)
            dest_dir.mkdir(parents=True, exist_ok=True)
            dest_path = dest_dir / filename
            
            shutil.move(file_path, dest_path)
            logger.info(f"Moved to failed: {filename}")
        except Exception as e:
            logger.error(f"Failed to move file to failed: {e}")
    
    def get_stats(self) -> dict:
        """Get processing statistics"""
        return {
            'database': self.db.get_processing_stats(),
            'rate_limiter': self.rate_limiter.get_status_info()
        }

# Singleton instance
_processor_instance: Optional[FileProcessor] = None

def get_file_processor() -> FileProcessor:
    """Get or create file processor singleton instance"""
    global _processor_instance
    if _processor_instance is None:
        _processor_instance = FileProcessor()
    return _processor_instance

