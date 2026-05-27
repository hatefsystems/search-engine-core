"""
Model Training Utilities - Task 01.2

Tools for training custom language detection models to extend beyond 176 languages.
Enables scaling to 250+ languages using your own training corpus.

Example:
    >>> trainer = ModelTrainer()
    >>> corpus = {
    ...     'en': ['English text samples...'],
    ...     'fa': ['نمونه‌های متن فارسی...'],
    ...     # ... up to 250+ languages
    ... }
    >>> trainer.prepare_and_train(corpus, 'models/custom/250lang.bin')
"""

import random
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from collections import Counter

try:
    import fasttext
    FASTTEXT_AVAILABLE = True
except ImportError:
    FASTTEXT_AVAILABLE = False

from shared.logger import setup_logger

logger = setup_logger(__name__)


class ModelTrainer:
    """
    Training utilities for custom language detection models.
    
    Features:
    - Prepare training data from corpus
    - Train FastText supervised models
    - Evaluate model accuracy
    - Data validation and balancing
    - Cross-validation support
    
    Example:
        >>> trainer = ModelTrainer()
        >>> trainer.prepare_training_data(
        ...     corpus={'en': texts_en, 'fa': texts_fa},
        ...     output_path='training_data/corpus.txt'
        ... )
        >>> model = trainer.train_model(
        ...     training_file='training_data/corpus.txt',
        ...     output_model='models/custom/my_model.bin'
        ... )
    """
    
    def __init__(self, seed: int = 42):
        """
        Initialize model trainer.
        
        Args:
            seed: Random seed for reproducibility
        """
        if not FASTTEXT_AVAILABLE:
            raise ImportError(
                "fasttext library required for training. "
                "Install with: pip install fasttext-wheel"
            )
        
        random.seed(seed)
        logger.info("Model trainer initialized", seed=seed)
    
    def prepare_training_data(
        self,
        corpus: Dict[str, List[str]],
        output_path: Path,
        min_samples: int = 100,
        max_samples: Optional[int] = 10000,
        train_split: float = 0.8
    ) -> Tuple[Path, Path]:
        """
        Prepare training data in FastText format from language corpus.
        
        Args:
            corpus: Dictionary mapping language codes to text samples
                   Example: {'en': ['text1', 'text2'], 'fa': ['متن۱', 'متن۲']}
            output_path: Path to save training data
            min_samples: Minimum samples required per language
            max_samples: Maximum samples to use per language (for balancing)
            train_split: Train/validation split ratio (0.0-1.0)
            
        Returns:
            Tuple of (train_file_path, validation_file_path)
            
        Raises:
            ValueError: If corpus is invalid or insufficient
        """
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Validate corpus
        valid_corpus = {}
        for lang_code, texts in corpus.items():
            if len(texts) < min_samples:
                logger.warning(
                    "Insufficient samples for language",
                    language=lang_code,
                    num_samples=len(texts),
                    min_required=min_samples
                )
                continue
            valid_corpus[lang_code] = texts
        
        if not valid_corpus:
            raise ValueError("No valid language data in corpus")
        
        logger.info(
            "Preparing training data",
            num_languages=len(valid_corpus),
            output_path=str(output_path)
        )
        
        # Balance dataset
        balanced_corpus = self._balance_corpus(
            valid_corpus,
            max_samples=max_samples
        )
        
        # Split train/validation
        train_data = []
        val_data = []
        
        for lang_code, texts in balanced_corpus.items():
            # Shuffle texts
            shuffled = texts.copy()
            random.shuffle(shuffled)
            
            # Split
            split_idx = int(len(shuffled) * train_split)
            train_samples = shuffled[:split_idx]
            val_samples = shuffled[split_idx:]
            
            # Format for FastText: __label__en Text content here
            for text in train_samples:
                # Clean text (remove newlines, extra spaces)
                clean_text = ' '.join(text.split())
                if clean_text:
                    train_data.append(f"__label__{lang_code} {clean_text}")
            
            for text in val_samples:
                clean_text = ' '.join(text.split())
                if clean_text:
                    val_data.append(f"__label__{lang_code} {clean_text}")
        
        # Shuffle combined data
        random.shuffle(train_data)
        random.shuffle(val_data)
        
        # Write train file
        train_file = output_path
        with open(train_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(train_data))
        
        # Write validation file
        val_file = train_file.parent / f"{train_file.stem}_val{train_file.suffix}"
        with open(val_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(val_data))
        
        logger.info(
            "Training data prepared",
            train_file=str(train_file),
            val_file=str(val_file),
            train_samples=len(train_data),
            val_samples=len(val_data),
            languages=len(balanced_corpus)
        )
        
        return train_file, val_file
    
    def _balance_corpus(
        self,
        corpus: Dict[str, List[str]],
        max_samples: Optional[int]
    ) -> Dict[str, List[str]]:
        """
        Balance corpus by limiting samples per language.
        
        Args:
            corpus: Original corpus
            max_samples: Maximum samples per language
            
        Returns:
            Balanced corpus
        """
        if max_samples is None:
            return corpus
        
        balanced = {}
        for lang_code, texts in corpus.items():
            if len(texts) > max_samples:
                # Randomly sample max_samples
                balanced[lang_code] = random.sample(texts, max_samples)
            else:
                balanced[lang_code] = texts
        
        return balanced
    
    def train_model(
        self,
        training_file: Path,
        output_model: Path,
        validation_file: Optional[Path] = None,
        dim: int = 128,
        epoch: int = 25,
        lr: float = 0.1,
        word_ngrams: int = 2,
        loss: str = 'softmax',
        min_count: int = 1,
        verbose: int = 2
    ) -> 'fasttext.FastText._FastText':
        """
        Train FastText supervised model for language detection.
        
        Args:
            training_file: Path to training data (FastText format)
            output_model: Path to save trained model
            validation_file: Optional validation data for accuracy check
            dim: Word vector dimension (default: 128)
            epoch: Number of training epochs (default: 25)
            lr: Learning rate (default: 0.1)
            word_ngrams: Use word n-grams (default: 2)
            loss: Loss function (default: 'softmax')
            min_count: Minimum word frequency (default: 1)
            verbose: Verbosity level (default: 2)
            
        Returns:
            Trained FastText model
        """
        training_file = Path(training_file)
        output_model = Path(output_model)
        
        if not training_file.exists():
            raise FileNotFoundError(f"Training file not found: {training_file}")
        
        output_model.parent.mkdir(parents=True, exist_ok=True)
        
        logger.info(
            "Training FastText model",
            training_file=str(training_file),
            output_model=str(output_model),
            dim=dim,
            epoch=epoch,
            lr=lr
        )
        
        # Train model
        model = fasttext.train_supervised(
            input=str(training_file),
            dim=dim,
            epoch=epoch,
            lr=lr,
            wordNgrams=word_ngrams,
            loss=loss,
            minCount=min_count,
            verbose=verbose
        )
        
        # Evaluate on training data
        train_result = model.test(str(training_file))
        train_precision = train_result[1]
        train_recall = train_result[2]
        
        logger.info(
            "Training complete",
            num_labels=len(model.get_labels()),
            train_precision=train_precision,
            train_recall=train_recall
        )
        
        # Evaluate on validation data if provided
        if validation_file and Path(validation_file).exists():
            val_result = model.test(str(validation_file))
            val_precision = val_result[1]
            val_recall = val_result[2]
            
            logger.info(
                "Validation results",
                val_precision=val_precision,
                val_recall=val_recall
            )
            
            if val_precision < 0.90:
                logger.warning(
                    "Low validation accuracy",
                    val_precision=val_precision,
                    recommendation="Consider adding more training data or adjusting hyperparameters"
                )
        
        # Save model
        model.save_model(str(output_model))
        model_size_mb = output_model.stat().st_size / (1024 * 1024)
        
        logger.info(
            "Model saved",
            output_model=str(output_model),
            model_size_mb=model_size_mb,
            num_languages=len(model.get_labels())
        )
        
        return model
    
    def evaluate_model(
        self,
        model_path: Path,
        test_file: Path,
        k: int = 1
    ) -> Dict[str, float]:
        """
        Evaluate trained model on test data.
        
        Args:
            model_path: Path to trained model
            test_file: Path to test data (FastText format)
            k: Number of predictions to consider (default: 1 for top-1)
            
        Returns:
            Dictionary with evaluation metrics
        """
        if not model_path.exists():
            raise FileNotFoundError(f"Model not found: {model_path}")
        
        if not test_file.exists():
            raise FileNotFoundError(f"Test file not found: {test_file}")
        
        logger.info(
            "Evaluating model",
            model_path=str(model_path),
            test_file=str(test_file)
        )
        
        # Load model
        model = fasttext.load_model(str(model_path))
        
        # Test model
        result = model.test(str(test_file), k=k)
        num_samples = result[0]
        precision = result[1]
        recall = result[2]
        
        metrics = {
            'num_samples': num_samples,
            'precision': precision,
            'recall': recall,
            'f1_score': 2 * (precision * recall) / (precision + recall) if (precision + recall) > 0 else 0.0
        }
        
        logger.info("Evaluation results", **metrics)
        
        return metrics
    
    def prepare_and_train(
        self,
        corpus: Dict[str, List[str]],
        output_model: Path,
        training_data_path: Optional[Path] = None,
        **train_kwargs
    ) -> 'fasttext.FastText._FastText':
        """
        Convenience method: prepare data and train model in one step.
        
        Args:
            corpus: Language corpus dictionary
            output_model: Path to save trained model
            training_data_path: Path to save training data (optional)
            **train_kwargs: Additional arguments for train_model()
            
        Returns:
            Trained FastText model
        """
        # Default training data path
        if training_data_path is None:
            training_data_path = Path("training_data/corpus.txt")
        
        # Prepare data
        train_file, val_file = self.prepare_training_data(
            corpus=corpus,
            output_path=training_data_path
        )
        
        # Train model
        model = self.train_model(
            training_file=train_file,
            output_model=output_model,
            validation_file=val_file,
            **train_kwargs
        )
        
        return model
    
    @staticmethod
    def validate_corpus(corpus: Dict[str, List[str]]) -> Dict[str, any]:
        """
        Validate corpus and return statistics.
        
        Args:
            corpus: Language corpus to validate
            
        Returns:
            Dictionary with corpus statistics
        """
        stats = {
            'num_languages': len(corpus),
            'total_samples': sum(len(texts) for texts in corpus.values()),
            'samples_per_language': {
                lang: len(texts) for lang, texts in corpus.items()
            },
            'min_samples': min(len(texts) for texts in corpus.values()) if corpus else 0,
            'max_samples': max(len(texts) for texts in corpus.values()) if corpus else 0,
            'avg_samples': sum(len(texts) for texts in corpus.values()) / len(corpus) if corpus else 0
        }
        
        # Check for imbalanced dataset
        if stats['max_samples'] > stats['min_samples'] * 10:
            stats['imbalanced'] = True
            stats['warning'] = "Dataset is highly imbalanced. Consider balancing before training."
        else:
            stats['imbalanced'] = False
        
        return stats

