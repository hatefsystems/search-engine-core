#!/usr/bin/env python3
"""
Train Custom Language Detection Model
Task 01.2 - Language Detection

Train FastText model on your own corpus to extend beyond 176 languages.
Support for 250+ languages through custom training.

Usage:
    python scripts/train_custom_model.py \\
        --corpus training_data/my_corpus.json \\
        --output models/custom/my_model.bin \\
        --languages 250

Example corpus.json format:
{
    "en": ["English text sample 1", "English text sample 2", ...],
    "fa": ["نمونه متن فارسی ۱", "نمونه متن فارسی ۲", ...],
    "custom_lang": ["Custom language samples", ...]
}
"""

import argparse
import json
import sys
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from text_processing.model_trainer import ModelTrainer
from shared.logger import setup_logger

logger = setup_logger(__name__)


def load_corpus(corpus_path: Path) -> dict:
    """
    Load corpus from JSON file.
    
    Args:
        corpus_path: Path to JSON corpus file
        
    Returns:
        Dictionary mapping language codes to text samples
    """
    logger.info("Loading corpus", path=str(corpus_path))
    
    with open(corpus_path, 'r', encoding='utf-8') as f:
        corpus = json.load(f)
    
    # Validate corpus
    if not isinstance(corpus, dict):
        raise ValueError("Corpus must be a dictionary")
    
    total_samples = sum(len(texts) for texts in corpus.values())
    logger.info(
        "Corpus loaded",
        num_languages=len(corpus),
        total_samples=total_samples
    )
    
    return corpus


def main():
    """Main training function."""
    parser = argparse.ArgumentParser(
        description="Train custom language detection model (up to 250+ languages)"
    )
    
    parser.add_argument(
        '--corpus',
        type=Path,
        required=True,
        help='Path to corpus JSON file'
    )
    
    parser.add_argument(
        '--output',
        type=Path,
        default=Path('models/custom/custom_model.bin'),
        help='Output model path (default: models/custom/custom_model.bin)'
    )
    
    parser.add_argument(
        '--training-data',
        type=Path,
        default=Path('training_data/prepared_corpus.txt'),
        help='Path to save prepared training data'
    )
    
    parser.add_argument(
        '--dim',
        type=int,
        default=128,
        help='Word vector dimension (default: 128)'
    )
    
    parser.add_argument(
        '--epoch',
        type=int,
        default=25,
        help='Number of training epochs (default: 25)'
    )
    
    parser.add_argument(
        '--lr',
        type=float,
        default=0.1,
        help='Learning rate (default: 0.1)'
    )
    
    parser.add_argument(
        '--min-samples',
        type=int,
        default=100,
        help='Minimum samples per language (default: 100)'
    )
    
    parser.add_argument(
        '--max-samples',
        type=int,
        default=10000,
        help='Maximum samples per language for balancing (default: 10000)'
    )
    
    parser.add_argument(
        '--languages',
        type=int,
        help='Expected number of languages (for logging)'
    )
    
    args = parser.parse_args()
    
    # Load corpus
    try:
        corpus = load_corpus(args.corpus)
    except Exception as e:
        logger.error("Failed to load corpus", error=str(e))
        return 1
    
    # Validate corpus
    trainer = ModelTrainer()
    stats = trainer.validate_corpus(corpus)
    
    logger.info("Corpus statistics", **stats)
    
    if stats.get('imbalanced'):
        logger.warning(stats['warning'])
    
    if args.languages and stats['num_languages'] < args.languages:
        logger.warning(
            "Corpus has fewer languages than expected",
            expected=args.languages,
            actual=stats['num_languages']
        )
    
    # Confirm training
    print("\n" + "=" * 60)
    print("🎯 Custom Language Detection Model Training")
    print("=" * 60)
    print(f"📊 Languages: {stats['num_languages']}")
    print(f"📝 Total samples: {stats['total_samples']:,}")
    print(f"📉 Min samples per language: {stats['min_samples']}")
    print(f"📈 Max samples per language: {stats['max_samples']}")
    print(f"📁 Output model: {args.output}")
    print("=" * 60)
    
    response = input("\n🚀 Start training? [y/N]: ")
    if response.lower() != 'y':
        print("❌ Training cancelled")
        return 0
    
    print("\n⚙️  Training in progress...\n")
    
    # Prepare and train
    try:
        model = trainer.prepare_and_train(
            corpus=corpus,
            output_model=args.output,
            training_data_path=args.training_data,
            dim=args.dim,
            epoch=args.epoch,
            lr=args.lr,
            min_count=1,
            word_ngrams=2
        )
        
        print("\n" + "=" * 60)
        print("✅ Training Complete!")
        print("=" * 60)
        print(f"📦 Model saved to: {args.output}")
        print(f"🌍 Languages supported: {len(model.get_labels())}")
        print(f"💾 Model size: {args.output.stat().st_size / (1024*1024):.1f} MB")
        print("=" * 60)
        print("\n🎉 Your custom model is ready for 250+ language detection!")
        print(f"\nUsage:")
        print(f"  detector = UniversalLanguageDetector(model_path='{args.output}')")
        print("")
        
        return 0
        
    except Exception as e:
        logger.error("Training failed", error=str(e))
        print(f"\n❌ Training failed: {e}")
        return 1


if __name__ == '__main__':
    sys.exit(main())

