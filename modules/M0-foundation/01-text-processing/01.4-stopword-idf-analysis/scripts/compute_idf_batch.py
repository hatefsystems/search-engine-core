#!/usr/bin/env python3
"""
Batch IDF + Stanza Stopword Detection from MongoDB

This script:
1. Reads documents from MongoDB collection
2. Computes IDF scores for all terms (Layer 1 - Universal)
3. Uses Stanza POS tagging for grammar verification (Layer 2 - Enhanced)
4. Stores results in Redis for fast lookup
5. Generates comprehensive reports

Usage:
    python3 scripts/compute_idf_batch.py \
        --mongodb-uri mongodb://admin:password123@localhost:27017 \
        --database search-engine \
        --collection indexed_pages \
        --text-field textContent \
        --language fa \
        --enable-stanza
"""

import sys
import os
import time
import argparse
from pathlib import Path
from typing import List, Dict, Optional, Set
from collections import defaultdict
import json
from datetime import datetime

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from text_processing import (
    HybridStopwordDetector,
    IDFAnalyzer,
    STANZA_AVAILABLE
)
from shared.logger import setup_logger

logger = setup_logger(__name__)

try:
    from pymongo import MongoClient
    from pymongo.errors import ConnectionFailure, ServerSelectionTimeoutError
    MONGODB_AVAILABLE = True
except ImportError:
    MONGODB_AVAILABLE = False
    logger.error("pymongo not installed. Install with: pip install pymongo")

try:
    import psutil
    PSUTIL_AVAILABLE = True
except ImportError:
    PSUTIL_AVAILABLE = False


class BatchStopwordProcessor:
    """
    Batch processor for IDF + Stanza stopword detection from MongoDB
    """
    
    def __init__(
        self,
        mongodb_uri: str,
        database: str,
        collection: str,
        text_field: str = "textContent",
        language: str = "fa",
        enable_stanza: bool = True,
        redis_url: str = "redis://localhost:6379",
        idf_threshold: float = 2.0,
        confidence_threshold: float = 0.8,
        batch_size: int = 1000
    ):
        """
        Initialize the batch processor
        
        Args:
            mongodb_uri: MongoDB connection URI
            database: Database name
            collection: Collection name
            text_field: Field containing text content
            language: ISO 639-1 language code (e.g., 'fa', 'en')
            enable_stanza: Enable Stanza POS tagging (Layer 2)
            redis_url: Redis connection URL
            idf_threshold: IDF threshold for stopword candidates
            confidence_threshold: Confidence threshold for final stopwords
            batch_size: Number of documents to process in each batch
        """
        if not MONGODB_AVAILABLE:
            raise ImportError("pymongo is required. Install with: pip install pymongo")
        
        self.mongodb_uri = mongodb_uri
        self.database_name = database
        self.collection_name = collection
        self.text_field = text_field
        self.language = language
        self.enable_stanza = enable_stanza and STANZA_AVAILABLE
        self.confidence_threshold = confidence_threshold
        self.batch_size = batch_size
        
        # Initialize MongoDB client
        logger.info(f"Connecting to MongoDB: {mongodb_uri}")
        self.mongo_client = MongoClient(mongodb_uri, serverSelectionTimeoutMS=5000)
        
        # Test connection
        try:
            self.mongo_client.admin.command('ping')
            logger.info("✅ MongoDB connection successful")
        except (ConnectionFailure, ServerSelectionTimeoutError) as e:
            logger.error(f"❌ MongoDB connection failed: {e}")
            raise
        
        self.db = self.mongo_client[database]
        self.collection = self.db[collection]
        
        # Initialize Hybrid Stopword Detector
        logger.info(f"Initializing Hybrid Stopword Detector (Stanza: {self.enable_stanza})")
        self.detector = HybridStopwordDetector(
            redis_url=redis_url,
            enable_stanza=self.enable_stanza,
            default_threshold=confidence_threshold
        )
        
        # Store IDF threshold separately for candidate identification
        self.idf_threshold = idf_threshold
        
        # Statistics
        self.stats = {
            'total_documents': 0,
            'processed_documents': 0,
            'total_terms': 0,
            'unique_terms': 0,
            'stopwords_detected': 0,
            'grammar_verified': 0,
            'start_time': None,
            'end_time': None,
            'duration': 0
        }
    
    def get_memory_usage(self) -> float:
        """Get current process memory usage in MB"""
        if PSUTIL_AVAILABLE:
            process = psutil.Process(os.getpid())
            return process.memory_info().rss / 1024 / 1024
        return 0.0
    
    def extract_terms(self, text: str) -> List[str]:
        """
        Extract terms from text (simple tokenization)
        
        Args:
            text: Input text
            
        Returns:
            List of terms
        """
        if not text:
            return []
        
        # Simple tokenization (split by whitespace and punctuation)
        import re
        # Remove punctuation and split
        terms = re.findall(r'\w+', text.lower())
        return terms
    
    def fetch_documents(self, limit: Optional[int] = None) -> List[Dict]:
        """
        Fetch documents from MongoDB
        
        Args:
            limit: Maximum number of documents to fetch (None for all)
            
        Returns:
            List of documents
        """
        logger.info(f"Fetching documents from {self.database_name}.{self.collection_name}")
        
        query = {}
        projection = {self.text_field: 1, '_id': 0}
        
        cursor = self.collection.find(query, projection)
        if limit:
            cursor = cursor.limit(limit)
        
        documents = list(cursor)
        self.stats['total_documents'] = len(documents)
        
        logger.info(f"Fetched {len(documents)} documents")
        return documents
    
    def compute_idf_scores(self, documents: List[Dict]) -> Dict[str, float]:
        """
        Compute IDF scores for all terms in the corpus (Layer 1)
        
        Args:
            documents: List of documents
            
        Returns:
            Dictionary mapping term -> IDF score
        """
        logger.info("Computing IDF scores (Layer 1: Universal Coverage)...")
        
        # Build corpus as strings for IDFAnalyzer
        corpus_texts = []
        
        for doc in documents:
            text = doc.get(self.text_field, '')
            if text:
                corpus_texts.append(text)
        
        logger.info(f"Corpus statistics:")
        logger.info(f"  - Documents: {len(corpus_texts):,}")
        
        # Compute IDF using IDFAnalyzer
        analyzer = IDFAnalyzer(idf_threshold=self.idf_threshold)
        idf_result = analyzer.analyze(corpus_texts, language=self.language, tokenize=True)
        
        # Convert IDFScore objects to simple dict
        idf_scores = {term: score.idf for term, score in idf_result.items()}
        
        # Update stats
        self.stats['unique_terms'] = len(idf_scores)
        self.stats['total_terms'] = analyzer.total_documents
        
        logger.info(f"  - Total terms: {self.stats['total_terms']:,}")
        logger.info(f"  - Unique terms: {self.stats['unique_terms']:,}")
        logger.info(f"✅ IDF computation complete: {len(idf_scores)} terms analyzed")
        
        return idf_scores
    
    def identify_stopword_candidates(self, idf_scores: Dict[str, float]) -> List[str]:
        """
        Identify stopword candidates based on IDF threshold
        
        Args:
            idf_scores: Dictionary of term -> IDF score
            
        Returns:
            List of stopword candidates
        """
        candidates = [
            term for term, idf in idf_scores.items()
            if idf < self.idf_threshold
        ]
        
        # Sort by IDF (lowest first)
        candidates.sort(key=lambda t: idf_scores[t])
        
        logger.info(f"Identified {len(candidates)} stopword candidates (IDF < {self.idf_threshold})")
        
        return candidates
    
    def verify_with_stanza(self, candidates: List[str]) -> Dict[str, Dict]:
        """
        Verify stopword candidates using Stanza POS tagging (Layer 2)
        
        Args:
            candidates: List of stopword candidates
            
        Returns:
            Dictionary mapping term -> verification result
        """
        if not self.enable_stanza:
            logger.info("Stanza disabled, skipping grammar verification")
            return {}
        
        logger.info(f"Verifying {len(candidates)} candidates with Stanza (Layer 2: Grammar-Aware)...")
        
        results = {}
        verified_count = 0
        
        # Process in batches to show progress
        batch_size = 100
        for i in range(0, len(candidates), batch_size):
            batch = candidates[i:i+batch_size]
            
            for term in batch:
                result = self.detector.is_stopword(term, self.language)
                
                results[term] = {
                    'is_stopword': result.is_stopword,
                    'confidence': result.confidence,
                    'pos_tag': result.pos_tag,
                    'grammar_verified': result.grammar_verified,
                    'detection_method': result.detection_method
                }
                
                if result.grammar_verified:
                    verified_count += 1
            
            if (i + batch_size) % 500 == 0:
                logger.info(f"  Progress: {min(i + batch_size, len(candidates))}/{len(candidates)} terms processed")
        
        self.stats['grammar_verified'] = verified_count
        logger.info(f"✅ Stanza verification complete: {verified_count} terms grammar-verified")
        
        return results
    
    def store_results(self, idf_scores: Dict[str, float], verification_results: Dict[str, Dict]):
        """
        Store results in Redis
        
        Args:
            idf_scores: IDF scores for all terms
            verification_results: Stanza verification results
        """
        logger.info("Storing results in Redis...")
        
        stored_count = 0
        
        for term, idf in idf_scores.items():
            # Get verification result if available
            verification = verification_results.get(term, {})
            
            is_stopword = verification.get('is_stopword', idf < self.idf_threshold)
            confidence = verification.get('confidence', 0.9 if is_stopword else 0.1)
            
            if is_stopword and confidence >= self.confidence_threshold:
                # Store in Redis via detector
                # (detector already stores when we call is_stopword)
                stored_count += 1
        
        self.stats['stopwords_detected'] = stored_count
        logger.info(f"✅ Stored {stored_count} stopwords in Redis")
    
    def generate_report(self, idf_scores: Dict[str, float], verification_results: Dict[str, Dict]) -> Dict:
        """
        Generate comprehensive report
        
        Args:
            idf_scores: IDF scores
            verification_results: Verification results
            
        Returns:
            Report dictionary
        """
        # Top stopwords by confidence
        stopwords_by_confidence = []
        for term, result in verification_results.items():
            if result['is_stopword']:
                stopwords_by_confidence.append({
                    'term': term,
                    'confidence': result['confidence'],
                    'idf': idf_scores.get(term, 0),
                    'pos_tag': result.get('pos_tag'),
                    'grammar_verified': result.get('grammar_verified', False)
                })
        
        stopwords_by_confidence.sort(key=lambda x: x['confidence'], reverse=True)
        
        # POS tag distribution
        pos_distribution = defaultdict(int)
        for result in verification_results.values():
            if result.get('pos_tag'):
                pos_distribution[result['pos_tag']] += 1
        
        report = {
            'metadata': {
                'language': self.language,
                'database': self.database_name,
                'collection': self.collection_name,
                'text_field': self.text_field,
                'stanza_enabled': self.enable_stanza,
                'idf_threshold': self.idf_threshold,
                'confidence_threshold': self.confidence_threshold,
                'timestamp': datetime.now().isoformat()
            },
            'statistics': self.stats,
            'top_stopwords': stopwords_by_confidence[:50],
            'pos_distribution': dict(pos_distribution),
            'sample_stopwords': {
                'high_confidence': stopwords_by_confidence[:10],
                'grammar_verified': [
                    sw for sw in stopwords_by_confidence 
                    if sw['grammar_verified']
                ][:10]
            }
        }
        
        return report
    
    def print_report(self, report: Dict):
        """Print report to console"""
        print("\n" + "=" * 70)
        print("  BATCH STOPWORD DETECTION REPORT")
        print("=" * 70)
        
        print(f"\n📋 Configuration:")
        print(f"   Language: {report['metadata']['language']}")
        print(f"   Database: {report['metadata']['database']}")
        print(f"   Collection: {report['metadata']['collection']}")
        print(f"   Stanza Enabled: {report['metadata']['stanza_enabled']}")
        
        print(f"\n📊 Statistics:")
        stats = report['statistics']
        print(f"   Documents Processed: {stats['processed_documents']:,}")
        print(f"   Total Terms: {stats['total_terms']:,}")
        print(f"   Unique Terms: {stats['unique_terms']:,}")
        print(f"   Stopwords Detected: {stats['stopwords_detected']:,}")
        print(f"   Grammar Verified: {stats['grammar_verified']:,}")
        print(f"   Duration: {stats['duration']:.2f} seconds")
        
        if stats['duration'] > 0:
            print(f"   Throughput: {stats['unique_terms'] / stats['duration']:.1f} terms/second")
        
        print(f"\n🏆 Top 10 Stopwords (by confidence):")
        for i, sw in enumerate(report['sample_stopwords']['high_confidence'][:10], 1):
            pos_str = f"[{sw['pos_tag']}]" if sw['pos_tag'] else ""
            verified_str = "✅" if sw['grammar_verified'] else "📊"
            print(f"   {i:2d}. {verified_str} '{sw['term']}' {pos_str}")
            print(f"       Confidence: {sw['confidence']:.3f}, IDF: {sw['idf']:.4f}")
        
        if report['pos_distribution']:
            print(f"\n📚 POS Tag Distribution:")
            sorted_pos = sorted(report['pos_distribution'].items(), key=lambda x: x[1], reverse=True)
            for pos_tag, count in sorted_pos[:10]:
                print(f"   {pos_tag:10s}: {count:4d} terms")
        
        print("\n" + "=" * 70)
        print("  ✅ Report generation complete!")
        print("=" * 70)
    
    def save_report(self, report: Dict, output_path: str):
        """Save report to JSON file"""
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(report, f, ensure_ascii=False, indent=2)
        
        logger.info(f"Report saved to: {output_path}")
    
    def process(self, limit: Optional[int] = None, output_report: Optional[str] = None):
        """
        Main processing pipeline
        
        Args:
            limit: Maximum number of documents to process
            output_report: Path to save JSON report (optional)
        """
        logger.info("=" * 70)
        logger.info("  BATCH IDF + STANZA STOPWORD DETECTION")
        logger.info("=" * 70)
        
        self.stats['start_time'] = time.time()
        start_mem = self.get_memory_usage()
        
        try:
            # Step 1: Fetch documents
            documents = self.fetch_documents(limit)
            self.stats['processed_documents'] = len(documents)
            
            if not documents:
                logger.warning("No documents found in collection")
                return
            
            # Step 2: Compute IDF scores (Layer 1)
            idf_scores = self.compute_idf_scores(documents)
            
            # Step 3: Identify stopword candidates
            candidates = self.identify_stopword_candidates(idf_scores)
            
            # Step 4: Verify with Stanza (Layer 2)
            verification_results = self.verify_with_stanza(candidates)
            
            # Step 5: Store results
            self.store_results(idf_scores, verification_results)
            
            # Step 6: Generate and print report
            self.stats['end_time'] = time.time()
            self.stats['duration'] = self.stats['end_time'] - self.stats['start_time']
            
            report = self.generate_report(idf_scores, verification_results)
            self.print_report(report)
            
            # Save report if requested
            if output_report:
                self.save_report(report, output_report)
            
            # Memory usage
            end_mem = self.get_memory_usage()
            if start_mem > 0:
                logger.info(f"\n💾 Memory: {start_mem:.1f} MB → {end_mem:.1f} MB (Δ {end_mem - start_mem:+.1f} MB)")
        
        except Exception as e:
            logger.error(f"Error during processing: {e}", exc_info=True)
            raise
        
        finally:
            self.mongo_client.close()
            logger.info("MongoDB connection closed")


def main():
    """Main function"""
    parser = argparse.ArgumentParser(
        description='Batch IDF + Stanza stopword detection from MongoDB',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    # MongoDB arguments
    parser.add_argument(
        '--mongodb-uri',
        required=True,
        help='MongoDB connection URI (e.g., mongodb://admin:password@localhost:27017)'
    )
    parser.add_argument(
        '--database',
        required=True,
        help='Database name'
    )
    parser.add_argument(
        '--collection',
        required=True,
        help='Collection name'
    )
    parser.add_argument(
        '--text-field',
        default='textContent',
        help='Field containing text content (default: textContent)'
    )
    
    # Processing arguments
    parser.add_argument(
        '--language',
        default='fa',
        help='Language code (default: fa for Persian)'
    )
    parser.add_argument(
        '--enable-stanza',
        action='store_true',
        default=False,
        help='Enable Stanza POS tagging for grammar verification'
    )
    parser.add_argument(
        '--idf-threshold',
        type=float,
        default=2.0,
        help='IDF threshold for stopword candidates (default: 2.0)'
    )
    parser.add_argument(
        '--confidence-threshold',
        type=float,
        default=0.8,
        help='Confidence threshold for final stopwords (default: 0.8)'
    )
    parser.add_argument(
        '--limit',
        type=int,
        default=None,
        help='Maximum number of documents to process (default: all)'
    )
    parser.add_argument(
        '--redis-url',
        default='redis://localhost:6379',
        help='Redis connection URL (default: redis://localhost:6379)'
    )
    parser.add_argument(
        '--output-report',
        default=None,
        help='Path to save JSON report (optional)'
    )
    
    args = parser.parse_args()
    
    # Create processor
    processor = BatchStopwordProcessor(
        mongodb_uri=args.mongodb_uri,
        database=args.database,
        collection=args.collection,
        text_field=args.text_field,
        language=args.language,
        enable_stanza=args.enable_stanza,
        redis_url=args.redis_url,
        idf_threshold=args.idf_threshold,
        confidence_threshold=args.confidence_threshold
    )
    
    # Process
    processor.process(
        limit=args.limit,
        output_report=args.output_report
    )


if __name__ == "__main__":
    main()
