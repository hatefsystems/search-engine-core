"""
FastText Language Detection - Task 01.2

Wrapper for FastText language identification models.
Supports both pre-trained (176 languages) and custom models (250+ languages).

Model Options:
- lid.176.bin: 917KB compressed model
- lid.176.ftz: 126MB full accuracy model (recommended)
- Custom: Your trained model for 250+ languages
"""

import re
from pathlib import Path
from typing import List, Tuple, Optional

try:
    import fasttext
    FASTTEXT_AVAILABLE = True
except ImportError:
    FASTTEXT_AVAILABLE = False

from shared.logger import setup_logger

logger = setup_logger(__name__)


class FastTextDetector:
    """
    FastText-based language detection.
    
    Features:
    - 176 languages out-of-box
    - Supports custom models (250+ languages)
    - High accuracy (95%+)
    - Fast inference (<5ms)
    
    Example:
        >>> detector = FastTextDetector()
        >>> predictions = detector.detect("This is English")
        >>> print(predictions)
        [('en', 0.98), ('fr', 0.01), ...]
    """
    
    # Default model paths
    DEFAULT_MODELS = [
        "models/lid.176.ftz",      # 126MB, best accuracy
        "models/lid.176.bin",      # 917KB, compressed
        "models/custom/model.bin",  # Custom trained model
    ]
    
    def __init__(self, model_path: Optional[Path] = None):
        """
        Initialize FastText detector.
        
        Args:
            model_path: Path to FastText model file
                       If None, searches for default models
        
        Raises:
            ImportError: If fasttext library not installed
            FileNotFoundError: If no model file found
        """
        if not FASTTEXT_AVAILABLE:
            raise ImportError(
                "fasttext library not installed. "
                "Install with: pip install fasttext-wheel"
            )
        
        self.model_path = self._find_model(model_path)
        self.model = self._load_model()
        
        logger.info(
            "FastText detector initialized",
            model_path=str(self.model_path),
            model_size_mb=self.model_path.stat().st_size / (1024 * 1024)
        )
    
    def _find_model(self, model_path: Optional[Path]) -> Path:
        """
        Find FastText model file.
        
        Args:
            model_path: User-provided model path (optional)
            
        Returns:
            Path to model file
            
        Raises:
            FileNotFoundError: If no model found
        """
        if model_path:
            path = Path(model_path)
            if path.exists():
                return path
            raise FileNotFoundError(f"Model not found: {model_path}")
        
        # Search for default models
        base_dir = Path(__file__).parent.parent
        for model_rel_path in self.DEFAULT_MODELS:
            model_full_path = base_dir / model_rel_path
            if model_full_path.exists():
                logger.info("Found model", path=str(model_full_path))
                return model_full_path
        
        # Model not found - provide helpful error message
        raise FileNotFoundError(
            "No FastText model found. Download with:\n"
            "  ./scripts/download_models.sh\n"
            "Or specify model_path explicitly."
        )
    
    def _load_model(self) -> 'fasttext.FastText._FastText':
        """
        Load FastText model from disk.
        
        Returns:
            Loaded FastText model
        """
        logger.info("Loading FastText model", path=str(self.model_path))
        
        # Suppress FastText warnings
        import warnings
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            model = fasttext.load_model(str(self.model_path))
        
        logger.info(
            "FastText model loaded",
            num_labels=len(model.get_labels()),
            model_dimension=model.get_dimension()
        )
        
        return model
    
    def detect(self, text: str, k: int = 3) -> List[Tuple[str, float]]:
        """
        Detect language of text using FastText.
        
        Args:
            text: Input text (any language)
            k: Number of top predictions to return
            
        Returns:
            List of (language_code, probability) tuples
            Sorted by probability descending
            
        Example:
            >>> detector.detect("Hello world", k=2)
            [('en', 0.99), ('fr', 0.005)]
        """
        if not text or not text.strip():
            return []
        
        # Preprocess text for FastText
        # Remove newlines and excessive whitespace
        text = re.sub(r'\s+', ' ', text.strip())
        
        try:
            # FastText predict returns:
            # labels: tuple of strings like ('__label__en',)
            # probabilities: tuple of floats
            labels, probabilities = self.model.predict(text, k=k)
            
            # Parse results
            results = []
            for label, prob in zip(labels, probabilities):
                # Extract language code from '__label__en' format
                lang_code = label.replace('__label__', '')
                results.append((lang_code, float(prob)))
            
            logger.debug(
                "FastText detection",
                text_preview=text[:50],
                top_prediction=results[0] if results else None
            )
            
            return results
            
        except Exception as e:
            logger.error(
                "FastText detection failed",
                error=str(e),
                text_preview=text[:100]
            )
            return []
    
    def detect_batch(self, texts: List[str], k: int = 3) -> List[List[Tuple[str, float]]]:
        """
        Batch detect languages for multiple texts.
        
        Args:
            texts: List of input texts
            k: Number of top predictions per text
            
        Returns:
            List of prediction lists
        """
        results = []
        for text in texts:
            try:
                predictions = self.detect(text, k=k)
                results.append(predictions)
            except Exception as e:
                logger.error("Batch detection error", error=str(e))
                results.append([])
        
        return results
    
    @staticmethod
    def train_custom_model(
        training_file: Path,
        output_model: Path,
        dim: int = 128,
        epoch: int = 25,
        lr: float = 0.1,
        word_ngrams: int = 2,
        loss: str = 'softmax'
    ) -> 'fasttext.FastText._FastText':
        """
        Train custom FastText language detection model.
        Use this to extend to 250+ languages.
        
        Args:
            training_file: Path to training data in FastText format:
                          __label__en This is English text
                          __label__fa این متن فارسی است
            output_model: Path to save trained model
            dim: Dimension of word vectors (default: 128)
            epoch: Number of training epochs (default: 25)
            lr: Learning rate (default: 0.1)
            word_ngrams: Use word n-grams (default: 2)
            loss: Loss function (default: 'softmax')
            
        Returns:
            Trained FastText model
            
        Example:
            >>> model = FastTextDetector.train_custom_model(
            ...     training_file=Path("training_data/corpus.txt"),
            ...     output_model=Path("models/custom/my_model.bin")
            ... )
        """
        if not FASTTEXT_AVAILABLE:
            raise ImportError("fasttext library not installed")
        
        if not training_file.exists():
            raise FileNotFoundError(f"Training file not found: {training_file}")
        
        logger.info(
            "Training custom FastText model",
            training_file=str(training_file),
            output_model=str(output_model),
            dim=dim,
            epoch=epoch
        )
        
        # Train model
        model = fasttext.train_supervised(
            input=str(training_file),
            dim=dim,
            epoch=epoch,
            lr=lr,
            wordNgrams=word_ngrams,
            loss=loss,
            verbose=2
        )
        
        # Save model
        output_model.parent.mkdir(parents=True, exist_ok=True)
        model.save_model(str(output_model))
        
        logger.info(
            "Custom model trained and saved",
            output_model=str(output_model),
            num_labels=len(model.get_labels()),
            model_size_mb=output_model.stat().st_size / (1024 * 1024)
        )
        
        return model
    
    def get_supported_languages(self) -> List[str]:
        """
        Get list of supported language codes.
        
        Returns:
            List of ISO 639-1 language codes
        """
        labels = self.model.get_labels()
        # Extract language codes from '__label__en' format
        return [label.replace('__label__', '') for label in labels]
    
    def get_num_languages(self) -> int:
        """Get number of supported languages."""
        return len(self.model.get_labels())

