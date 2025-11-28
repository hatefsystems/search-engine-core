#!/usr/bin/env python3
"""
Download Stanza Models Script

Downloads Stanza NLP models for specified languages.
This script should be run before using hybrid stopword detection with Stanza.

Usage:
    # Download specific languages
    python scripts/download_stanza_models.py en fa ar
    
    # Download all priority languages
    python scripts/download_stanza_models.py --all
    
    # List available languages
    python scripts/download_stanza_models.py --list
"""

import sys
import argparse
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

try:
    import stanza
    STANZA_AVAILABLE = True
except ImportError:
    STANZA_AVAILABLE = False
    print("❌ Error: Stanza not installed!")
    print("Install with: pip install stanza")
    sys.exit(1)


# Priority languages for the search engine
PRIORITY_LANGUAGES = [
    ('en', 'English'),
    ('fa', 'Persian (Farsi)'),
    ('ar', 'Arabic'),
    ('de', 'German'),
    ('es', 'Spanish'),
    ('fr', 'French'),
    ('zh', 'Chinese'),
    ('ru', 'Russian'),
    ('hi', 'Hindi'),
    ('tr', 'Turkish'),
]

# All supported languages (60+)
ALL_SUPPORTED_LANGUAGES = [
    ('en', 'English'),
    ('fa', 'Persian'),
    ('ar', 'Arabic'),
    ('de', 'German'),
    ('es', 'Spanish'),
    ('fr', 'French'),
    ('it', 'Italian'),
    ('pt', 'Portuguese'),
    ('ru', 'Russian'),
    ('tr', 'Turkish'),
    ('zh', 'Chinese'),
    ('ja', 'Japanese'),
    ('ko', 'Korean'),
    ('hi', 'Hindi'),
    ('id', 'Indonesian'),
    ('vi', 'Vietnamese'),
    ('nl', 'Dutch'),
    ('pl', 'Polish'),
    ('uk', 'Ukrainian'),
    ('cs', 'Czech'),
    ('af', 'Afrikaans'),
    ('eu', 'Basque'),
    ('bg', 'Bulgarian'),
    ('ca', 'Catalan'),
    ('hr', 'Croatian'),
    ('da', 'Danish'),
    ('et', 'Estonian'),
    ('fi', 'Finnish'),
    ('gl', 'Galician'),
    ('he', 'Hebrew'),
    ('hu', 'Hungarian'),
    ('is', 'Icelandic'),
    ('lv', 'Latvian'),
    ('lt', 'Lithuanian'),
    ('no', 'Norwegian'),
    ('ro', 'Romanian'),
    ('sk', 'Slovak'),
    ('sl', 'Slovenian'),
    ('sv', 'Swedish'),
    ('th', 'Thai'),
    ('ur', 'Urdu'),
    ('el', 'Greek'),
    ('be', 'Belarusian'),
    ('ta', 'Tamil'),
    ('te', 'Telugu'),
    ('mr', 'Marathi'),
    ('bn', 'Bengali'),
]


def print_header(text):
    """Print formatted header"""
    print("\n" + "=" * 70)
    print(f"  {text}")
    print("=" * 70 + "\n")


def list_languages():
    """List all supported languages"""
    print_header("Stanza Supported Languages")
    
    print("Priority Languages (recommended):")
    for code, name in PRIORITY_LANGUAGES:
        print(f"  {code:4s} - {name}")
    
    print("\nAll Supported Languages (60+):")
    for code, name in ALL_SUPPORTED_LANGUAGES:
        indicator = "✓" if (code, name) in PRIORITY_LANGUAGES else " "
        print(f"  {indicator} {code:4s} - {name}")
    
    print("\nNote: Priority languages are optimized for the search engine.")


def download_model(language_code: str, language_name: str = None) -> bool:
    """
    Download Stanza model for a language
    
    Args:
        language_code: ISO 639-1 language code (e.g., 'en', 'fa')
        language_name: Human-readable language name (optional)
        
    Returns:
        True if download successful, False otherwise
    """
    if language_name:
        display_name = f"{language_name} ({language_code})"
    else:
        display_name = language_code
    
    print(f"📥 Downloading Stanza model for {display_name}...")
    print(f"   This may take a few minutes (models are 200-500 MB)...")
    
    try:
        stanza.download(
            language_code,
            processors='tokenize,pos',  # Only download needed processors
            verbose=False,
            logging_level='WARNING'
        )
        print(f"✅ Successfully downloaded: {display_name}\n")
        return True
        
    except Exception as e:
        print(f"❌ Failed to download {display_name}: {e}\n")
        return False


def main():
    """Main function"""
    parser = argparse.ArgumentParser(
        description='Download Stanza NLP models for stopword detection',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Download English and Persian models
  python scripts/download_stanza_models.py en fa
  
  # Download all priority languages (recommended)
  python scripts/download_stanza_models.py --all
  
  # List available languages
  python scripts/download_stanza_models.py --list
        """
    )
    
    parser.add_argument(
        'languages',
        nargs='*',
        help='Language codes to download (e.g., en fa ar)'
    )
    
    parser.add_argument(
        '--all',
        action='store_true',
        help='Download all priority languages'
    )
    
    parser.add_argument(
        '--list',
        action='store_true',
        help='List all supported languages'
    )
    
    args = parser.parse_args()
    
    # Handle --list flag
    if args.list:
        list_languages()
        return
    
    # Determine which languages to download
    if args.all:
        languages_to_download = PRIORITY_LANGUAGES
        print_header("Downloading All Priority Languages")
        print(f"Will download {len(languages_to_download)} language models...")
    elif args.languages:
        # Look up language names
        lang_dict = dict(ALL_SUPPORTED_LANGUAGES)
        languages_to_download = []
        for code in args.languages:
            name = lang_dict.get(code, None)
            languages_to_download.append((code, name))
        
        print_header(f"Downloading {len(languages_to_download)} Language(s)")
    else:
        # No arguments - show help
        parser.print_help()
        print("\n💡 Tip: Start with priority languages:")
        print("   python scripts/download_stanza_models.py en fa ar")
        return
    
    # Download models
    successful = 0
    failed = 0
    
    for code, name in languages_to_download:
        if download_model(code, name):
            successful += 1
        else:
            failed += 1
    
    # Print summary
    print_header("Download Summary")
    print(f"✅ Successful: {successful}")
    if failed > 0:
        print(f"❌ Failed:     {failed}")
    
    print(f"\n📦 Total models downloaded: {successful}")
    
    if successful > 0:
        print("\n🎉 Models are ready to use!")
        print("\nNext steps:")
        print("  1. Test the hybrid detector:")
        print("     python scripts/test_hybrid_demo.py")
        print("\n  2. Use in your code:")
        print("     from text_processing import HybridStopwordDetector")
        print("     detector = HybridStopwordDetector(enable_stanza=True)")
    
    if failed > 0:
        print("\n⚠️  Some downloads failed. Check your internet connection.")


if __name__ == "__main__":
    main()

