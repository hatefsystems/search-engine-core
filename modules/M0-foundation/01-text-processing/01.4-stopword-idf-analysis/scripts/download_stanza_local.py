#!/usr/bin/env python3
"""
Download Stanza Models Using Local Resources File

This script uses the local resources_1.11.0.json file to download models
when GitHub is not accessible.

Usage:
    python scripts/download_stanza_local.py fa
"""

import sys
import os
import json
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

try:
    import stanza
    from stanza.resources.common import DEFAULT_MODEL_DIR
    STANZA_AVAILABLE = True
except ImportError as e:
    STANZA_AVAILABLE = False
    print(f"❌ Error: Stanza not installed! {e}")
    print("Install with: pip install stanza")
    sys.exit(1)


def download_models_from_local_resources(lang: str, resources_file: str, model_dir: str = None):
    """
    Download Stanza models using local resources file
    
    Args:
        lang: Language code (e.g., 'fa' for Persian)
        resources_file: Path to local resources JSON file
        model_dir: Directory to store models (default: DEFAULT_MODEL_DIR)
    """
    if model_dir is None:
        model_dir = DEFAULT_MODEL_DIR
    
    # Create model directory
    os.makedirs(model_dir, exist_ok=True)
    
    # Load local resources file
    print(f"📚 Loading resources from: {resources_file}")
    with open(resources_file, 'r') as f:
        resources = json.load(f)
    
    # Save resources to model directory
    resources_path = os.path.join(model_dir, 'resources.json')
    print(f"💾 Saving resources to: {resources_path}")
    with open(resources_path, 'w') as f:
        json.dump(resources, f, indent=2)
    
    # Check if language is available
    if lang not in resources:
        print(f"❌ Language '{lang}' not found in resources file")
        print(f"Available languages: {', '.join(sorted([k for k in resources.keys() if len(k) == 2]))[:50]}...")
        return False
    
    print(f"✅ Found '{lang}' in resources")
    
    # Download models for tokenize and pos processors
    processors = 'tokenize,pos'
    print(f"\n📥 Downloading models for: {processors}")
    print(f"   Language: {lang}")
    print(f"   Model directory: {model_dir}")
    print()
    
    try:
        # Use stanza's download function
        # Since resources.json is already in model_dir, stanza will use it automatically
        stanza.download(
            lang,
            model_dir=model_dir,
            processors=processors,
            verbose=True,
            logging_level='INFO'
        )
        
        print(f"\n✅ Successfully downloaded models for '{lang}'!")
        return True
        
    except Exception as e:
        print(f"\n❌ Error downloading models: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    """Main function"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Download Stanza models using local resources file',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument(
        'language',
        nargs='?',
        default='fa',
        help='Language code to download (default: fa for Persian)'
    )
    
    parser.add_argument(
        '--resources',
        default='scripts/resources_1.11.0.json',
        help='Path to local resources JSON file'
    )
    
    parser.add_argument(
        '--model-dir',
        default=None,
        help=f'Model directory (default: {DEFAULT_MODEL_DIR})'
    )
    
    args = parser.parse_args()
    
    # Get script directory
    script_dir = Path(__file__).parent.parent
    resources_file = script_dir / args.resources
    
    if not resources_file.exists():
        print(f"❌ Resources file not found: {resources_file}")
        print("\nMake sure resources_1.11.0.json is in the scripts/ directory")
        sys.exit(1)
    
    print("=" * 70)
    print("  Stanza Model Downloader (Local Resources)")
    print("=" * 70)
    print()
    
    success = download_models_from_local_resources(
        args.language,
        str(resources_file),
        args.model_dir
    )
    
    if success:
        print("\n" + "=" * 70)
        print("  🎉 Download Complete!")
        print("=" * 70)
        print("\nYou can now use hybrid stopword detection:")
        print("  from text_processing import HybridStopwordDetector")
        print("  detector = HybridStopwordDetector(enable_stanza=True)")
        print(f"  result = detector.is_stopword('و', '{args.language}')")
    else:
        print("\n" + "=" * 70)
        print("  ❌ Download Failed")
        print("=" * 70)
        sys.exit(1)


if __name__ == "__main__":
    main()

