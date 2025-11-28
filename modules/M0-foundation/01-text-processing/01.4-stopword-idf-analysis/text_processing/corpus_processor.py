"""
Corpus Processor - MongoDB Integration

Reads document corpus from MongoDB for IDF analysis.
Supports batch processing for large corpora.
"""

from typing import Iterator, Optional, Dict, List
from pymongo import MongoClient
from pymongo.collection import Collection

from shared.logger import setup_logger

logger = setup_logger(__name__)


class CorpusProcessor:
    """
    MongoDB Corpus Processor
    
    Reads documents from MongoDB collections for IDF analysis.
    Supports efficient batch processing for large corpora.
    
    Usage:
        processor = CorpusProcessor(
            mongodb_uri="mongodb://localhost:27017",
            database="search-engine",
            collection="documents"
        )
        
        # Iterate over documents
        for doc_text in processor.iterate_documents(batch_size=1000):
            # Process document
            pass
    """
    
    def __init__(
        self,
        mongodb_uri: str,
        database: str,
        collection: str,
        text_field: str = "content"
    ):
        """
        Initialize Corpus Processor
        
        Args:
            mongodb_uri: MongoDB connection URI
            database: Database name
            collection: Collection name
            text_field: Field name containing document text
        """
        self.mongodb_uri = mongodb_uri
        self.database_name = database
        self.collection_name = collection
        self.text_field = text_field
        
        # Initialize MongoDB connection
        try:
            self.client = MongoClient(mongodb_uri, serverSelectionTimeoutMS=5000)
            # Test connection
            self.client.server_info()
            
            self.db = self.client[database]
            self.collection: Collection = self.db[collection]
            
            logger.info(
                "MongoDB connection established",
                database=database,
                collection=collection,
                text_field=text_field
            )
        except Exception as e:
            logger.error(f"Failed to connect to MongoDB: {e}")
            raise
    
    def iterate_documents(
        self,
        batch_size: int = 1000,
        query: Optional[Dict] = None,
        language: Optional[str] = None,
        limit: Optional[int] = None
    ) -> Iterator[str]:
        """
        Iterate over documents in corpus
        
        Args:
            batch_size: Number of documents to fetch per batch
            query: MongoDB query filter (default: {})
            language: Filter by language code (optional)
            limit: Maximum number of documents to process
            
        Yields:
            Document text strings
        """
        if query is None:
            query = {}
        
        # Add language filter if specified
        if language:
            query["language"] = language
        
        logger.info(
            "Starting document iteration",
            query=query,
            batch_size=batch_size,
            limit=limit
        )
        
        # Count total documents
        total_docs = self.collection.count_documents(query)
        logger.info(f"Total documents matching query: {total_docs}")
        
        if limit:
            total_docs = min(total_docs, limit)
        
        # Iterate in batches
        processed = 0
        cursor = self.collection.find(query).batch_size(batch_size)
        
        if limit:
            cursor = cursor.limit(limit)
        
        for doc in cursor:
            text = doc.get(self.text_field, "")
            
            if text and isinstance(text, str):
                yield text
                processed += 1
                
                if processed % 10000 == 0:
                    logger.debug(f"Processed {processed}/{total_docs} documents")
        
        logger.info(f"Document iteration complete: {processed} documents processed")
    
    def get_corpus_list(
        self,
        query: Optional[Dict] = None,
        language: Optional[str] = None,
        limit: Optional[int] = None
    ) -> List[str]:
        """
        Get corpus as list of documents (loads all into memory)
        
        Args:
            query: MongoDB query filter
            language: Filter by language code
            limit: Maximum number of documents
            
        Returns:
            List of document text strings
        """
        documents = list(self.iterate_documents(
            query=query,
            language=language,
            limit=limit
        ))
        
        logger.info(f"Loaded {len(documents)} documents into memory")
        return documents
    
    def get_language_statistics(self) -> Dict[str, int]:
        """
        Get document count by language
        
        Returns:
            Dictionary mapping language codes to document counts
        """
        pipeline = [
            {"$group": {
                "_id": "$language",
                "count": {"$sum": 1}
            }},
            {"$sort": {"count": -1}}
        ]
        
        results = self.collection.aggregate(pipeline)
        
        language_stats = {}
        for result in results:
            lang = result.get("_id", "unknown")
            count = result.get("count", 0)
            language_stats[lang] = count
        
        logger.info(
            "Language statistics retrieved",
            language_count=len(language_stats),
            total_documents=sum(language_stats.values())
        )
        
        return language_stats
    
    def get_corpus_statistics(self) -> Dict:
        """
        Get overall corpus statistics
        
        Returns:
            Dictionary with corpus statistics
        """
        total_docs = self.collection.count_documents({})
        language_stats = self.get_language_statistics()
        
        # Calculate average document length (sample)
        sample_docs = list(self.collection.find().limit(1000))
        avg_length = 0
        
        if sample_docs:
            lengths = [
                len(doc.get(self.text_field, "").split())
                for doc in sample_docs
                if doc.get(self.text_field)
            ]
            avg_length = sum(lengths) / len(lengths) if lengths else 0
        
        return {
            "total_documents": total_docs,
            "languages": len(language_stats),
            "language_distribution": language_stats,
            "avg_document_length_words": int(avg_length),
            "database": self.database_name,
            "collection": self.collection_name
        }
    
    def close(self) -> None:
        """Close MongoDB connection"""
        if self.client:
            self.client.close()
            logger.info("MongoDB connection closed")

