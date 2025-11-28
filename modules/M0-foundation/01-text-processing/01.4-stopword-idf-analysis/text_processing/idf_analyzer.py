"""
IDF (Inverse Document Frequency) Analyzer - Core Implementation

Calculates IDF scores for terms across a document corpus to identify stopwords.
IDF Formula: IDF = log(N / df)
where N = total documents, df = document frequency

Low IDF (< threshold) → Common word → Likely stopword
High IDF (> threshold) → Rare word → Content word
"""

import math
from collections import Counter, defaultdict
from dataclasses import dataclass
from typing import Dict, List, Set, Tuple, Iterator, Optional
import numpy as np
from scipy.sparse import lil_matrix, csr_matrix

from shared.logger import setup_logger

logger = setup_logger(__name__)


@dataclass
class IDFScore:
    """IDF score result for a term"""
    term: str
    idf: float
    document_frequency: int
    total_documents: int
    is_stopword_candidate: bool = False
    confidence: float = 0.0
    
    def __repr__(self) -> str:
        return (
            f"IDFScore(term='{self.term}', idf={self.idf:.4f}, "
            f"df={self.document_frequency}, N={self.total_documents}, "
            f"stopword={self.is_stopword_candidate}, confidence={self.confidence:.4f})"
        )


class IDFAnalyzer:
    """
    IDF (Inverse Document Frequency) Analyzer
    
    Analyzes document corpus to calculate IDF scores for all terms.
    Identifies stopword candidates based on IDF threshold.
    
    Usage:
        analyzer = IDFAnalyzer(idf_threshold=2.0)
        corpus = ["the quick brown fox", "the lazy dog"]
        idf_scores = analyzer.analyze(corpus)
        
        # Get stopword candidates
        stopwords = analyzer.get_stopword_candidates()
    """
    
    def __init__(
        self,
        idf_threshold: float = 2.0,
        min_df: int = 2,
        max_df_ratio: float = 0.95,
        smoothing: bool = True
    ):
        """
        Initialize IDF Analyzer
        
        Args:
            idf_threshold: IDF threshold for stopword detection (default: 2.0)
                         Terms with IDF < threshold are stopword candidates
            min_df: Minimum document frequency (ignore rare terms)
            max_df_ratio: Maximum document frequency ratio (0.0-1.0)
                         Terms in >95% docs are likely stopwords
            smoothing: Use smoothed IDF calculation (prevents division by zero)
        """
        self.idf_threshold = idf_threshold
        self.min_df = min_df
        self.max_df_ratio = max_df_ratio
        self.smoothing = smoothing
        
        # Internal state
        self.term_doc_freq: Dict[str, int] = {}
        self.total_documents: int = 0
        self.idf_scores: Dict[str, IDFScore] = {}
        self.vocabulary: Set[str] = set()
        
        logger.info(
            "IDF Analyzer initialized",
            idf_threshold=idf_threshold,
            min_df=min_df,
            max_df_ratio=max_df_ratio,
            smoothing=smoothing
        )
    
    def analyze(
        self,
        corpus: List[str],
        language: Optional[str] = None,
        tokenize: bool = True
    ) -> Dict[str, IDFScore]:
        """
        Analyze corpus and calculate IDF scores for all terms
        
        Args:
            corpus: List of documents (strings)
            language: Language code for language-specific processing (optional)
            tokenize: Whether to tokenize documents (default: True, splits on whitespace)
            
        Returns:
            Dictionary mapping terms to IDFScore objects
        """
        logger.info(
            "Starting IDF analysis",
            corpus_size=len(corpus),
            language=language
        )
        
        # Reset state
        self.term_doc_freq = {}
        self.total_documents = len(corpus)
        self.vocabulary = set()
        
        if self.total_documents == 0:
            logger.warning("Empty corpus provided")
            return {}
        
        # Count document frequencies
        logger.debug("Counting document frequencies")
        for doc_idx, doc in enumerate(corpus):
            if tokenize:
                terms = set(doc.lower().split())
            else:
                terms = set(doc.lower())
            
            for term in terms:
                if term.strip():  # Ignore empty terms
                    self.term_doc_freq[term] = self.term_doc_freq.get(term, 0) + 1
                    self.vocabulary.add(term)
            
            if (doc_idx + 1) % 10000 == 0:
                logger.debug(f"Processed {doc_idx + 1}/{self.total_documents} documents")
        
        logger.info(
            "Document frequency counting complete",
            unique_terms=len(self.term_doc_freq),
            total_documents=self.total_documents
        )
        
        # Calculate IDF scores
        logger.debug("Calculating IDF scores")
        self.idf_scores = {}
        for term, df in self.term_doc_freq.items():
            # Apply document frequency filters
            if df < self.min_df:
                continue  # Ignore rare terms
            
            df_ratio = df / self.total_documents
            if df_ratio > self.max_df_ratio:
                # Very common terms (in >95% docs) are likely stopwords
                idf = 0.0
            else:
                idf = self._calculate_idf(df, self.total_documents)
            
            # Determine if stopword candidate
            is_stopword = idf < self.idf_threshold
            confidence = self._calculate_confidence(idf, df_ratio)
            
            self.idf_scores[term] = IDFScore(
                term=term,
                idf=idf,
                document_frequency=df,
                total_documents=self.total_documents,
                is_stopword_candidate=is_stopword,
                confidence=confidence
            )
        
        logger.info(
            "IDF analysis complete",
            analyzed_terms=len(self.idf_scores),
            stopword_candidates=sum(
                1 for score in self.idf_scores.values() 
                if score.is_stopword_candidate
            )
        )
        
        return self.idf_scores
    
    def _calculate_idf(self, df: int, N: int) -> float:
        """
        Calculate IDF score for a term
        
        IDF = log(N / df) for standard IDF
        IDF = log((N + 1) / (df + 1)) + 1 for smoothed IDF
        
        Args:
            df: Document frequency (number of docs containing term)
            N: Total number of documents
            
        Returns:
            IDF score (float)
        """
        if df == 0:
            return 0.0
        
        if self.smoothing:
            # Smoothed IDF (prevents zero division and negative values)
            idf = math.log((N + 1) / (df + 1)) + 1
        else:
            # Standard IDF
            idf = math.log(N / df)
        
        return max(0.0, idf)  # Ensure non-negative
    
    def _calculate_confidence(self, idf: float, df_ratio: float) -> float:
        """
        Calculate confidence score for stopword detection
        
        High confidence = low IDF + high document frequency
        
        Args:
            idf: IDF score
            df_ratio: Document frequency ratio (0.0-1.0)
            
        Returns:
            Confidence score (0.0-1.0)
        """
        if idf >= self.idf_threshold:
            return 0.0  # Not a stopword candidate
        
        # Confidence based on IDF (lower IDF = higher confidence)
        idf_confidence = 1.0 - (idf / self.idf_threshold)
        
        # Boost confidence for very common terms
        # Terms in >70% docs get confidence boost
        if df_ratio > 0.7:
            frequency_boost = (df_ratio - 0.7) / 0.3  # 0-1 scale
            idf_confidence = min(1.0, idf_confidence + frequency_boost * 0.2)
        
        return max(0.0, min(1.0, idf_confidence))
    
    def get_stopword_candidates(
        self,
        min_confidence: float = 0.0,
        limit: Optional[int] = None
    ) -> List[Tuple[str, IDFScore]]:
        """
        Get stopword candidates sorted by confidence
        
        Args:
            min_confidence: Minimum confidence threshold (0.0-1.0)
            limit: Maximum number of stopwords to return
            
        Returns:
            List of (term, IDFScore) tuples sorted by confidence (descending)
        """
        candidates = [
            (term, score)
            for term, score in self.idf_scores.items()
            if score.is_stopword_candidate and score.confidence >= min_confidence
        ]
        
        # Sort by confidence (descending)
        candidates.sort(key=lambda x: x[1].confidence, reverse=True)
        
        if limit:
            candidates = candidates[:limit]
        
        logger.debug(
            "Retrieved stopword candidates",
            total_candidates=len(candidates),
            min_confidence=min_confidence,
            limit=limit
        )
        
        return candidates
    
    def get_idf_score(self, term: str) -> Optional[IDFScore]:
        """
        Get IDF score for a specific term
        
        Args:
            term: Term to look up
            
        Returns:
            IDFScore object or None if term not in vocabulary
        """
        return self.idf_scores.get(term.lower())
    
    def analyze_batch(
        self,
        corpus_iterator: Iterator[str],
        batch_size: int = 10000,
        language: Optional[str] = None
    ) -> Dict[str, IDFScore]:
        """
        Analyze large corpus in batches (memory-efficient)
        
        Args:
            corpus_iterator: Iterator yielding documents
            batch_size: Number of documents per batch
            language: Language code for language-specific processing
            
        Returns:
            Dictionary mapping terms to IDFScore objects
        """
        logger.info(
            "Starting batch IDF analysis",
            batch_size=batch_size,
            language=language
        )
        
        # Phase 1: Count document frequencies in batches
        self.term_doc_freq = {}
        self.total_documents = 0
        
        batch = []
        for doc in corpus_iterator:
            batch.append(doc)
            
            if len(batch) >= batch_size:
                self._process_batch(batch)
                batch = []
        
        # Process remaining documents
        if batch:
            self._process_batch(batch)
        
        logger.info(
            "Batch document frequency counting complete",
            unique_terms=len(self.term_doc_freq),
            total_documents=self.total_documents
        )
        
        # Phase 2: Calculate IDF scores (same as analyze())
        self.idf_scores = {}
        for term, df in self.term_doc_freq.items():
            if df < self.min_df:
                continue
            
            df_ratio = df / self.total_documents
            if df_ratio > self.max_df_ratio:
                idf = 0.0
            else:
                idf = self._calculate_idf(df, self.total_documents)
            
            is_stopword = idf < self.idf_threshold
            confidence = self._calculate_confidence(idf, df_ratio)
            
            self.idf_scores[term] = IDFScore(
                term=term,
                idf=idf,
                document_frequency=df,
                total_documents=self.total_documents,
                is_stopword_candidate=is_stopword,
                confidence=confidence
            )
        
        logger.info(
            "Batch IDF analysis complete",
            analyzed_terms=len(self.idf_scores)
        )
        
        return self.idf_scores
    
    def _process_batch(self, batch: List[str]) -> None:
        """Process a batch of documents for document frequency counting"""
        for doc in batch:
            terms = set(doc.lower().split())
            for term in terms:
                if term.strip():
                    self.term_doc_freq[term] = self.term_doc_freq.get(term, 0) + 1
        
        self.total_documents += len(batch)
        
        if self.total_documents % 50000 == 0:
            logger.debug(
                f"Batch progress: {self.total_documents} documents processed, "
                f"{len(self.term_doc_freq)} unique terms"
            )
    
    def export_statistics(self) -> Dict:
        """
        Export analysis statistics
        
        Returns:
            Dictionary with corpus statistics
        """
        if not self.idf_scores:
            return {}
        
        stopword_count = sum(
            1 for score in self.idf_scores.values() 
            if score.is_stopword_candidate
        )
        
        return {
            "total_documents": self.total_documents,
            "vocabulary_size": len(self.vocabulary),
            "analyzed_terms": len(self.idf_scores),
            "stopword_candidates": stopword_count,
            "stopword_ratio": stopword_count / len(self.idf_scores) if self.idf_scores else 0,
            "idf_threshold": self.idf_threshold,
            "min_df": self.min_df,
            "max_df_ratio": self.max_df_ratio,
        }

